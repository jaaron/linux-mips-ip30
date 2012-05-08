/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Definitions for the built-in PCI bridge
 * Copyright (C) 2004-2007 Stanislaw Skowronek
 */
#ifndef __ASM_MACH_IP30_PCIBR_H
#define __ASM_MACH_IP30_PCIBR_H

//#include <asm/mach-ip30/addrs.h>
#include <asm/pci/bridge.h>
#include <asm/mach-ip30/irq.h>

/* Used by pci-ip30.c and ip30-irq.c */
extern bridge_t *ip30_irq_to_bridge[NR_IRQS];
extern unsigned int ip30_irq_to_slot[NR_IRQS];


/* Xtalk */
#define PCIBR_XTALK_MFGR	0x036
#define PCIBR_XTALK_PART	0xc002

#define PCIBR_OFFSET_MEM	0x200000
#define PCIBR_OFFSET_IO		0xa00000
#define PCIBR_OFFSET_END	0xc00000

#define PCIBR_DIR_ALLOC_BASE	0x1000000

#define PCIBR_XIO_SEES_HEART	0x00000080	/* This is how XIO sees HEART_ISR */

#define PCIBR_IRQ_BASE		8

#define PCIBR_MAX_PCI_BUSSES	8		/* Max PCI Busses/Bridges */
#define PCIBR_MAX_DEV_PCIBUS	8		/* Max # of devices per bus */


/* Occult Numbers of the Black Priesthood of Ancient Mu
 *
 * Meticuously derived by studying dissassembly,
 * patents, and random guesses
 */
#define PCIBR_ANCIENT_MU_EVEN_RESP	0xddcc9988
#define PCIBR_ANCIENT_MU_ODD_RESP	0xddcc9988
#define PCIBR_ANCIENT_MU_INT_DEVICE	0xff000000
#define PCIBR_ANCIENT_MU_INT_ENABLE	0x7ffffe00
#define PCIBR_ANCIENT_MU_INT_MODE	0x00000000

/* Used by pci-ip30.c and ip30-irq.c */
/* XXX */
//extern unsigned int ip30_irq_in_bridge[PCIBR_MAX_PCI_BUSSES * PCIBR_MAX_DEV_PCIBUS];
//extern bridge_t *ip30_irq_bridge[PCIBR_MAX_PCI_BUSSES * PCIBR_MAX_DEV_PCIBUS];

#endif /* __ASM_MACH_IP30_PCIBR_H */
