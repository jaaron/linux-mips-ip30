/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2006  Ralf Baechle <ralf@linux-mips.org>
 *               2007  Joshua Kinard <kumba@gentoo.org>
 *               2009  Johannes Dickgreber <tanzy@gmx.de>
 *
 * derived from include/asm-mips/mach-ip27/dma-coherence.h
 * and based on code found in the old dma-ip30.c, which is
 * Copyright (C) 2004-2007 Stanislaw Skowronek <skylark@linux-mips.org>
 */
#ifndef __ASM_MACH_IP30_DMA_COHERENCE_H
#define __ASM_MACH_IP30_DMA_COHERENCE_H

#include <asm/mach-ip30/addrs.h>
#include <asm/pci/bridge.h>

static inline dma_addr_t pdev_to_baddr(struct pci_dev *dev, dma_addr_t addr,
					int size)
{
	if (dev->dma_mask == DMA_BIT_MASK(64))
		return (0x8UL<<60) | (0x0UL<<56) | (addr);
		/*      Heart Id     PCI64_ATTR    Phys Addr */

	if (dev->dma_mask == DMA_BIT_MASK(32))
		if (addr >= 0x20000000 && (addr+size) < 0xA0000000)
			return PCI32_DIRECT_BASE + addr - 0x20000000;

	printk(KERN_ERR "BRIDGE: DMA Mapping can't be realized.\n");
	return -1;
}

static inline dma_addr_t dev_to_baddr(struct device *dev, dma_addr_t addr,
					int size)
{
	addr &= 0xffffffffff; /* Only 40 Bits */

	if (dev)
		return pdev_to_baddr(to_pci_dev(dev), addr, size);

	return addr;
}

static inline dma_addr_t plat_map_dma_mem(struct device *dev, void *addr,
					size_t size)
{
	dma_addr_t pa = dev_to_baddr(dev, virt_to_phys(addr), size);

	return pa;
}

static inline dma_addr_t plat_map_dma_mem_page(struct device *dev,
						struct page *page)
{
	dma_addr_t pa = dev_to_baddr(dev, page_to_phys(page), PAGE_SIZE);

	return pa;
}

static inline unsigned long plat_dma_addr_to_phys(struct device *dev,
	dma_addr_t dma_addr)
{
	if (dma_addr > 0xffffffffUL)
		return dma_addr & 0xffffffffffUL;
	else
		return dma_addr + 0x20000000 - PCI32_DIRECT_BASE;
}

static inline void plat_unmap_dma_mem(struct device *dev, dma_addr_t dma_addr,
	size_t size, enum dma_data_direction direction)
{
	/* Empty */
}

static inline int plat_dma_supported(struct device *dev, u64 mask)
{
	/*
	 * we fall back to GFP_DMA when the mask isn't all 1s,
	 * so we can't guarantee allocations that must be
	 * within a tighter range than GFP_DMA..
	 */
	if (mask < DMA_BIT_MASK(24))
		return 0;

	return 1;
}

static inline void plat_extra_sync_for_device(struct device *dev)
{
}

static inline int plat_dma_mapping_error(struct device *dev,
					 dma_addr_t dma_addr)
{
	return 0;
}

static inline int plat_device_is_coherent(struct device *dev)
{
	return 1;		/* IP30 non-cohernet mode is unsupported
				 * (does it even have one?) */
}

#endif /* __ASM_MACH_IP30_DMA_COHERENCE_H */
