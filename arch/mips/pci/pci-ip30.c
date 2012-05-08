/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2004-2007 Stanislaw Skowronek (skylark@linux-mips.org)
 *		 2009 Johannes Dickgreber <tanzy@gmx.de>
 *
 * Based on pci-ip27.c by
 *  Copyright (C) 2003 Christoph Hellwig (hch@lst.de)
 *  Copyright (C) 1999, 2000, 04 Ralf Baechle (ralf@linux-mips.org)
 *  Copyright (C) 1999, 2000 Silicon Graphics, Inc.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/platform_device.h>

#include <asm/pci/bridge.h>
#include <asm/mach-ip30/addrs.h>
#include <asm/mach-ip30/pcibr.h>

/*
 * XXX: No kzalloc available when we do our crosstalk scan,
 *     we should try to move it later in the boot process.
 */
static struct bridge_controller bridges[PCIBR_MAX_PCI_BUSSES];

static unsigned int ip30_irq_assigned = PCIBR_IRQ_BASE;

/* OK, spikey dildo time */
#define AT_FAIL	0
#define AT_D32	1
#define AT_D64	2
#define AT_DIO	3
#define AT_WIN	4

static char *at_names[] = {"failed",
			"Direct 32-bit",
			"Direct 64-bit",
			"Direct I/O",
			"Window"};

static unsigned int align(unsigned int ptr, unsigned int size)
{
	return (ptr + size - 1) & ~(size - 1);
}

static unsigned int win_size(int n)
{
	return (n < 2) ? 0x200000 : 0x100000;
}

static unsigned int win_base(int n)
{
	return (n < 3) ? (0x200000 * (n + 1)) : (0x100000 * (n + 4));
}

static int startup_resource(struct pci_controller *hose,
				struct pci_dev *dev, int res)
{
	struct bridge_controller *bc = (struct bridge_controller *)hose;
	struct resource *rs = &dev->resource[res];
	bridge_t *bridge = bc->base;
	int slot = PCI_SLOT(dev->devfn);
	int is_io = !!(rs->flags & IORESOURCE_IO);
	unsigned int size = rs->end - rs->start + 1;
	int at = AT_FAIL;
	unsigned int base = 0;
	unsigned long vma = 0;
	unsigned int devio;
	int i, j;

	/* check for nonexistant resources */
	if (size < 2)
		return 0;

	/* try direct mappings first */
	if (!is_io) {
		base = align(bc->d32_p, size);
		vma = base + BRIDGE_PCI_MEM32_BASE;
		bc->d32_p = base + size;
		at = AT_D32;
	}
	if (is_io) {
		if (XWIDGET_REV_NUM(bridge->b_wid_id) >= BRIDGE_REV_D) {
			base = align(bc->dio_p, size);
			vma = base + BRIDGE_PCI_IO_BASE;
			bc->dio_p = base + size;
			at = AT_DIO;
		}
	}

	/* OK, that failed, try finding a compatible DevIO */
	if (at == AT_FAIL)
		for (j = 0; j < 8; j++) {
			i = (j + slot) & 7;
			if (bc->win_p[i] && bc->win_io[i] == is_io)
				if (align(bc->win_p[i], size) +
						size <= win_size(i)) {
					base = align(bc->win_p[i], size);
					bc->win_p[i] = base + size;
					base += win_base(i);
					vma = base;
					at = AT_WIN;
					break;
				}
		}

	/* if everything else fails, allocate a new DevIO */
	if (at == AT_FAIL)
		for (j = 0; j < 8; j++) {
			i = (j + slot) & 7;
			if (!bc->win_p[i] && size <= win_size(i)) {
				bc->win_p[i] = size;
				bc->win_io[i] = is_io;
				base = win_base(i);
				vma = base;
				at = AT_WIN;
				/* set the DevIO params */
				devio = bridge->b_device[i].reg;
				if (is_io)
					devio &= ~BRIDGE_DEV_DEV_IO_MEM;
				else
					devio |=  BRIDGE_DEV_DEV_IO_MEM;
				devio &= ~BRIDGE_DEV_OFF_MASK;
				devio |= win_base(i) >>
						BRIDGE_DEV_OFF_ADDR_SHFT;
				bridge->b_device[i].reg = devio;
				break;
			}
		}

	/* get real VMA */
	if (vma < PCIBR_OFFSET_END)
		vma += NODE_SWIN_BASE(bc->nasid, bc->widget_id);
	else
		vma += NODE_BWIN_BASE(bc->nasid, bc->widget_id);

	/* dump useless info to console */
	printk(KERN_INFO "pci-ip30: %s Bar %d with size 0x%08x"
			 " at bus 0x%08x vma 0x%016lx is %s.\n",
			pci_name(dev), res, size,
			base, vma, at_names[at]);

	if (at == AT_FAIL)
		return -ENOMEM;

	/* set the device resource to the new address */
	rs->start = vma;
	rs->end = vma + size - 1;

	pci_read_config_dword(dev, PCI_BASE_ADDRESS_0 + (4 * res), &devio);
	devio &= 0xf;
	devio |= base & ~0xf;
	pci_write_config_dword(dev, PCI_BASE_ADDRESS_0 + (4 * res), devio);

	return 0;
}

static int num_bridges;

int bridge_probe(nasid_t nasid, int widget_id, int masterwid)
{
	struct bridge_controller *bc;
//	static int num_bridges = 0;
	bridge_t *bridge;
	int slot;

	/* XXX: kludge alert.. */
	if (!num_bridges)
		ioport_resource.end = ~0UL;

	bc = &bridges[num_bridges];

	bc->pc.pre_enable	= startup_resource;
	bc->pc.pci_ops		= &bridge_pci_ops;

	bc->pc.mem_resource	= &bc->mem;
	bc->pc.mem_offset	= 0;

	bc->pc.io_resource	= &bc->io;
	bc->pc.io_offset	= 0;

	bc->pc.index		= num_bridges;
	bc->pc.io_map_base	= NODE_SWIN_BASE(nasid, widget_id);

	bc->mem.name		= "Bridge MEM";
	bc->mem.start		= NODE_SWIN_BASE(nasid, widget_id) +
						PCIBR_OFFSET_MEM;
	bc->mem.end		= NODE_SWIN_BASE(nasid, widget_id) +
						PCIBR_OFFSET_IO - 1;
	bc->mem.flags		= IORESOURCE_MEM;

	bc->io.name		= "Bridge IO";
	bc->io.start		= NODE_SWIN_BASE(nasid, widget_id) +
						PCIBR_OFFSET_IO;
	bc->io.end		= NODE_SWIN_BASE(nasid, widget_id) +
						PCIBR_OFFSET_END - 1;
	bc->io.flags		= IORESOURCE_IO;

	bc->irq_cpu = smp_processor_id();
	bc->widget_id = widget_id;
	bc->nasid = nasid;

	/* set direct allocation base */
	bc->dio_p = PCIBR_DIR_ALLOC_BASE;
	bc->d32_p = PCIBR_DIR_ALLOC_BASE;

	bc->baddr = (u64)masterwid << 60 | PCI64_ATTR_BAR;

	/*
	 * point to this bridge
	 */
	bridge = (bridge_t *) RAW_NODE_SWIN_BASE(nasid, widget_id);

	/*
	 * Clear all pending interrupts.
	 */
	bridge->b_int_rst_stat = BRIDGE_IRR_ALL_CLR;

	/*
	 * Until otherwise set up, assume all interrupts are from slot 0
	 */
	bridge->b_int_device = 0x0;

	/* Fix the initial b_device configuration. */
	bridge->b_wid_control &= ~(BRIDGE_CTRL_IO_SWAP | BRIDGE_CTRL_MEM_SWAP);
#ifdef CONFIG_PAGE_SIZE_4KB
	bridge->b_wid_control &= ~BRIDGE_CTRL_PAGE_SIZE;
#else /* 16kB or larger */
	bridge->b_wid_control |= BRIDGE_CTRL_PAGE_SIZE;
#endif

	for (slot = 0; slot < BRIDGE_DEV_CNT; slot++) {
		bridge->b_device[slot].reg = BRIDGE_DEV_ERR_LOCK_EN |
					     BRIDGE_DEV_VIRTUAL_EN |
					     BRIDGE_DEV_PMU_BITS |
					     BRIDGE_DEV_D32_BITS;
		/* We map the IRQs to slots in a straightforward way. */
		bridge->b_int_addr[slot].addr = ip30_irq_assigned;
		bc->pci_int[slot] = ip30_irq_assigned;
		ip30_irq_to_bridge[ip30_irq_assigned] = bridge;
		ip30_irq_to_slot[ip30_irq_assigned] = slot;
		ip30_irq_assigned++;
	}

	/* Configure direct-mapped DMA */
	bridge->b_dir_map = (masterwid << BRIDGE_DIRMAP_W_ID_SHFT) |
				BRIDGE_DIRMAP_ADD512;

	/*
	 * Allocate the RRBs randomly.
	 *
	 * No, I'm joking :)
	 * These are occult numbers of the Black Priesthood of Ancient Mu.
	 */
	bridge->b_even_resp = PCIBR_ANCIENT_MU_EVEN_RESP;
	bridge->b_odd_resp  = PCIBR_ANCIENT_MU_ODD_RESP;

	/*
	 * Route all PCI bridge interrupts to the HEART ASIC. The idea is
	 * that we cause the bridge to send an Xtalk write to a specified
	 * interrupt register (0x80 for HEART, 0x90 for HUB) in a defined
	 * widget. The actual IRQ support and masking is done elsewhere.
	 */
	bridge->b_wid_int_upper = masterwid << WIDGET_TARGET_ID_SHFT;
	bridge->b_wid_int_lower = PCIBR_XIO_SEES_HEART;

	bridge->b_int_device = PCIBR_ANCIENT_MU_INT_DEVICE;
	bridge->b_int_enable = PCIBR_ANCIENT_MU_INT_ENABLE;
	bridge->b_int_mode   = PCIBR_ANCIENT_MU_INT_MODE;

	bridge->b_wid_tflush;     /* wait until Bridge PIO complete */

	bc->base = bridge;

	register_pci_controller(&bc->pc);

	num_bridges++;

	return 0;
}

int pcibios_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	struct bridge_controller *bc = BRIDGE_CONTROLLER(dev->bus);

	return bc->pci_int[slot];
}

/* Do platform specific device initialization at pci_enable_device() time */
int pcibios_plat_dev_init(struct pci_dev *dev)
{
	return 0;
}

static void pci_disable_swapping_dma(struct pci_dev *dev)
{
	struct bridge_controller *bc = BRIDGE_CONTROLLER(dev->bus);
	bridge_t *bridge = bc->base;
	unsigned int devio;
	int slot = PCI_SLOT(dev->devfn);

	devio = bridge->b_device[slot].reg;
	devio &= ~(BRIDGE_DEV_SWAP_PMU | BRIDGE_DEV_SWAP_DIR);
	bridge->b_device[slot].reg = devio;
}

DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_SGI, PCI_DEVICE_ID_SGI_IOC3,
	pci_disable_swapping_dma);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_SGI, PCI_DEVICE_ID_SGI_RAD1,
	pci_disable_swapping_dma);

static int ip30_bridge_probe(struct platform_device *dev)
{
	return bridge_probe(0, dev->id, IP30_WIDGET_HEART);
}

static struct platform_driver bridge_driver = {
	.probe = ip30_bridge_probe,
	/* add remove someday */
	.driver = {
		.name = "bridge",
	},
};

static int __init bridge_init(void)
{
	return platform_driver_register(&bridge_driver);
}

arch_initcall(bridge_init);
