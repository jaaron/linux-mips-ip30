/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2004-2007 Stanislaw Skowronek <skylark@linux-mips.org>
 *		 2007 Joshua Kinard <kumba@gentoo.org>
 *		 2009 Johannes Dickgreber <tanzy@gmx.de>
 */

#ifndef __ASM_MACH_IP30_HEART_H
#define __ASM_MACH_IP30_HEART_H

#include <linux/types.h>

/* HEART internal register space */
#define HEART_PIU_BASE		0x900000000ff00000
#define HEART_IMR_BASE		0x900000000ff10000

/* HEART Register */
#define HEART_MODE		((void *)(HEART_PIU_BASE + 0x00))
#define HEART_SDRAM_MODE	((void *)(HEART_PIU_BASE + 0x08))
#define HEART_MEM_REF		((void *)(HEART_PIU_BASE + 0x10))
#define HEART_MEM_REQ_ARB	((void *)(HEART_PIU_BASE + 0x18))
#define HEART_MEMCFG0		((void *)(HEART_PIU_BASE + 0x20))
#define HEART_MEMCFG1		((void *)(HEART_PIU_BASE + 0x28))
#define HEART_MEMCFG2		((void *)(HEART_PIU_BASE + 0x30))
#define HEART_MEMCFG3		((void *)(HEART_PIU_BASE + 0x38))
#define HEART_FC_MODE		((void *)(HEART_PIU_BASE + 0x40))
#define HEART_FC_TIMER_LIMIT	((void *)(HEART_PIU_BASE + 0x48))
#define HEART_FC0_ADDR		((void *)(HEART_PIU_BASE + 0x50))
#define HEART_FC1_ADDR		((void *)(HEART_PIU_BASE + 0x58))
#define HEART_FC0_CR_CNT	((void *)(HEART_PIU_BASE + 0x60))
#define HEART_FC1_CR_CNT	((void *)(HEART_PIU_BASE + 0x68))
#define HEART_FC0_TIMER 	((void *)(HEART_PIU_BASE + 0x70))
#define HEART_FC1_TIMER 	((void *)(HEART_PIU_BASE + 0x78))
#define HEART_STATUS		((void *)(HEART_PIU_BASE + 0x80))
#define HEART_BERR_ADDR 	((void *)(HEART_PIU_BASE + 0x88))
#define HEART_BERR_MISC 	((void *)(HEART_PIU_BASE + 0x90))
#define HEART_MEMERR_ADDR	((void *)(HEART_PIU_BASE + 0x98))
#define HEART_MEMERR_DATA	((void *)(HEART_PIU_BASE + 0xa0))
#define HEART_PIUR_ACC_ERR	((void *)(HEART_PIU_BASE + 0xa8))
#define HEART_MLAN_CLK_DIV	((void *)(HEART_PIU_BASE + 0xb0))
#define HEART_MLAN_CTL		((void *)(HEART_PIU_BASE + 0xb8))

/* IRQ Register */
#define HEART_IMR(x)		((void *)(HEART_IMR_BASE + (8 * (x))))
#define HEART_SET_ISR		((void *)(HEART_IMR_BASE + 0x20))
#define HEART_CLR_ISR		((void *)(HEART_IMR_BASE + 0x28))
#define HEART_ISR		((void *)(HEART_IMR_BASE + 0x30))
#define HEART_IMSR		((void *)(HEART_IMR_BASE + 0x38))
#define HEART_CAUSE		((void *)(HEART_IMR_BASE + 0x40))

/* 52-bit counter */
#define HEART_COUNT		((void *)(HEART_PIU_BASE + 0x20000))

/* 24-bit compare */
#define HEART_COMPARE		((void *)(HEART_PIU_BASE + 0x30000))
#define HEART_TRIGGER		((void *)(HEART_PIU_BASE + 0x40000))
#define HEART_PRID		((void *)(HEART_PIU_BASE + 0x50000))
#define HEART_SYNC		((void *)(HEART_PIU_BASE + 0x60000))

/* HEART Masks */
#define HEART_ATK_MASK		0x0007ffffffffffff	/* HEART Attack Mask */
#define HEART_ACK_ALL_MASK	0xffffffffffffffff	/* Ack everything */
#define HEART_CLR_ALL_MASK	0x0000000000000000	/* Clear all */
#define HEART_BR_ERR_MASK	0x7ff8000000000000	/* BRIDGE Error Mask */
#define HEART_ERR_MASK		0x1ff			/* HEART Error Mask */
#define HEART_ERR_MASK_START	51			/* HEART Error start */
#define HEART_ERR_MASK_END	63			/* HEART Error end */
#define NON_HEART_IRQ_ST	0x0004000000000000	/* Where non-HEART IRQs begin */

/* bits in the HEART_MODE registers */
#define HM_PROC_DISABLE_SHFT		60
#define HM_PROC_DISABLE_MSK		((ulong) 0xf << HM_PROC_DISABLE_SHFT)
#define HM_PROC_DISABLE(x)		((ulong) 0x1 << (x) + HM_PROC_DISABLE_SHFT)
#define HM_MAX_PSR			((ulong) 0x7 << 57)
#define HM_MAX_IOSR			((ulong) 0x7 << 54)
#define HM_MAX_PEND_IOSR		((ulong) 0x7 << 51)

#define HM_TRIG_SRC_SEL_MSK		((ulong) 0x7 << 48)
#define HM_TRIG_HEART_EXC		((ulong) 0x0 << 48)	/* power-up default */
#define HM_TRIG_REG_BIT 		((ulong) 0x1 << 48)
#define HM_TRIG_SYSCLK			((ulong) 0x2 << 48)
#define HM_TRIG_MEMCLK_2X		((ulong) 0x3 << 48)
#define HM_TRIG_MEMCLK			((ulong) 0x4 << 48)
#define HM_TRIG_IOCLK			((ulong) 0x5 << 48)

#define HM_PIU_TEST_MODE		((ulong) 0xf << 40)

#define HM_GP_FLAG_MSK			((ulong) 0xf << 36)
#define HM_GP_FLAG(x)			((ulong) 0x1 << (x) + 36)

#define HM_MAX_PROC_HYST		((ulong) 0xf << 32)
#define HM_LLP_WRST_AFTER_RST		((ulong) 0x1 << 28)
#define HM_LLP_LINK_RST 		((ulong) 0x1 << 27)
#define HM_LLP_WARM_RST 		((ulong) 0x1 << 26)
#define HM_COR_ECC_LCK			((ulong) 0x1 << 25)
#define HM_REDUCED_PWR			((ulong) 0x1 << 24)
#define HM_COLD_RST			((ulong) 0x1 << 23)
#define HM_SW_RST			((ulong) 0x1 << 22)
#define HM_MEM_FORCE_WR 		((ulong) 0x1 << 21)
#define HM_DB_ERR_GEN			((ulong) 0x1 << 20)
#define HM_SB_ERR_GEN			((ulong) 0x1 << 19)
#define HM_CACHED_PIO_EN		((ulong) 0x1 << 18)
#define HM_CACHED_PROM_EN		((ulong) 0x1 << 17)
#define HM_PE_SYS_COR_ERE		((ulong) 0x1 << 16)
#define HM_GLOBAL_ECC_EN		((ulong) 0x1 << 15)
#define HM_IO_COH_EN			((ulong) 0x1 << 14)
#define HM_INT_EN			((ulong) 0x1 << 13)
#define HM_DATA_CHK_EN			((ulong) 0x1 << 12)
#define HM_REF_EN			((ulong) 0x1 << 11)
#define HM_BAD_SYSWR_ERE		((ulong) 0x1 << 10)
#define HM_BAD_SYSRD_ERE		((ulong) 0x1 << 9)
#define HM_SYSSTATE_ERE 		((ulong) 0x1 << 8)
#define HM_SYSCMD_ERE			((ulong) 0x1 << 7)
#define HM_NCOR_SYS_ERE 		((ulong) 0x1 << 6)
#define HM_COR_SYS_ERE			((ulong) 0x1 << 5)
#define HM_DATA_ELMNT_ERE		((ulong) 0x1 << 4)
#define HM_MEM_ADDR_PROC_ERE		((ulong) 0x1 << 3)
#define HM_MEM_ADDR_IO_ERE		((ulong) 0x1 << 2)
#define HM_NCOR_MEM_ERE 		((ulong) 0x1 << 1)
#define HM_COR_MEM_ERE			((ulong) 0x1 << 0)

/* bits in the memory refresh register */
#define HEART_MEMREF_REFS(x)		((ulong) (0xf & (x)) << 16)
#define HEART_MEMREF_PERIOD(x)		((ulong) (0xffff & (x)))
#define HEART_MEMREF_REFS_VAL		HEART_MEMREF_REFS(8)
#define HEART_MEMREF_PERIOD_VAL 	HEART_MEMREF_PERIOD(0x4000)
#define HEART_MEMREF_VAL		(HEART_MEMREF_REFS_VAL | \
					HEART_MEMREF_PERIOD_VAL)

/* bits in the memory request arbitrarion register */
#define HEART_MEMARB_IODIS		(1  << 20)
#define HEART_MEMARB_MAXPMWRQS		(15 << 16)
#define HEART_MEMARB_MAXPMRRQS		(15 << 12)
#define HEART_MEMARB_MAXPMRQS		(15 << 8)
#define HEART_MEMARB_MAXRRRQS		(15 << 4)
#define HEART_MEMARB_MAXGBRRQS		(15)

/* bits in the memory configuration registers */
/* bank valid bit */
#define HEART_MEMCFG_VLD		0x80000000
/* RAM mask */
#define HEART_MEMCFG_RAM_MSK		0x003f0000
/* RAM density */
#define HEART_MEMCFG_DENSITY		0x01c00000
#define HEART_MEMCFG_RAM_SHFT		16
/* base address mask */
#define HEART_MEMCFG_ADDR_MSK		0x000001ff
/* 32 MB is the HEART's basic memory unit */
#define HEART_MEMCFG_UNIT_SHFT		25

/* bits in the status register */
#define HEART_STAT_HSTL_SDRV		((ulong) 0x1 << 14)
#define HEART_STAT_FC_CR_OUT(x) 	((ulong) 0x1 << (x) + 12)
#define HEART_STAT_DIR_CNNCT		((ulong) 0x1 << 11)
#define HEART_STAT_TRITON		((ulong) 0x1 << 10)
#define HEART_STAT_R4K			((ulong) 0x1 << 9)
#define HEART_STAT_BIG_ENDIAN		((ulong) 0x1 << 8)
#define HEART_STAT_PROC_SHFT		4
#define HEART_STAT_PROC_MSK		((ulong) 0xf << HEART_STAT_PROC_SHFT)
#define HEART_STAT_PROC_ACTIVE(x)	((ulong) 0x1 << \
						(x) + HEART_STAT_PROC_SHFT)
#define HEART_STAT_WIDGET_ID		0xf

/* interrupt handling */
#define HEART_VEC_TO_IBIT(vec)		((ulong) 1 << (vec))
#define HEART_INT_VECTORS		64	/* how many vectors we manage */

/* IP7 is the CPU count/compare, IP1 and IP0 are SW2 and SW1 */
#define HEART_INT_LEVEL4		0xfff8000000000000	/* IP6 */
#define HEART_INT_LEVEL3		0x0004000000000000	/* IP5 */
#define HEART_INT_LEVEL2		0x0003ffff00000000	/* IP4 */
#define HEART_INT_LEVEL1		0x00000000ffff0000	/* IP3 */
#define HEART_INT_LEVEL0		0x000000000000ffff	/* IP2 */
#define HEART_INT_L4SHIFT		51
#define HEART_INT_L4MASK		0x1fff
#define HEART_INT_L3SHIFT		50
#define HEART_INT_L3MASK		0x1
#define HEART_INT_L2SHIFT		32
#define HEART_INT_L2MASK		0x3ffff
#define HEART_INT_L1SHIFT		16
#define HEART_INT_L1MASK		0xffff
#define HEART_INT_L0SHIFT		0
#define HEART_INT_L0MASK		0xffff

/* errors */
#define IRQ_HEART_ERR			63
#define IRQ_BUS_ERR_P(x)		(59 + (x))
#define IRQ_XT_ERR(xid) 		(51 + ((xid) - 1) & 7)
#define IRQ_XT_ERR_XBOW 		58
#define IRQ_XT_ERR_F			57
#define IRQ_XT_ERR_E			56
#define IRQ_XT_ERR_D			55
#define IRQ_XT_ERR_C			54
#define IRQ_XT_ERR_B			53
#define IRQ_XT_ERR_A			52
#define IRQ_XT_ERR_9			51
/* count/compare timer */
#define IRQ_HEART_CC			50
/* level 2 interrupts */
#define IRQ_IPI_P(x)			(46 + (x))
#define IRQ_TIMER_P(x)			(42 + (x))
#define IRQ_LOCAL_L2_BASE		32
#define IRQ_LOCAL_L2_NUM		9
/* level 1 interrupts */
#define IRQ_LOCAL_L1_BASE		16
#define IRQ_LOCAL_L1_NUM		16
/* level 0 interrupts */
#define IRQ_LOCAL_L0_BASE		3
#define IRQ_LOCAL_L0_NUM		13
#define IRQ_FC_HIGH			2
#define IRQ_FC_LOW			1
#define IRQ_GENERIC			0

/* bits in the HEART_CAUSE register */
#define HC_PE_SYS_COR_ERR_MSK		((ulong) 0xf << 60)
#define HC_PE_SYS_COR_ERR(x)		((ulong) 0x1 << (x) + 60)
#define HC_PIOWDB_OFLOW 		((ulong) 0x1 << 44)
#define HC_PIORWRB_OFLOW		((ulong) 0x1 << 43)
#define HC_PIUR_ACC_ERR 		((ulong) 0x1 << 42)
#define HC_BAD_SYSWR_ERR		((ulong) 0x1 << 41)
#define HC_BAD_SYSRD_ERR		((ulong) 0x1 << 40)
#define HC_SYSSTATE_ERR_MSK		((ulong) 0xf << 36)
#define HC_SYSSTATE_ERR(x)		((ulong) 0x1 << (x) + 36)
#define HC_SYSCMD_ERR_MSK		((ulong) 0xf << 32)
#define HC_SYSCMD_ERR(x)		((ulong) 0x1 << (x) + 32)
#define HC_NCOR_SYSAD_ERR_MSK		((ulong) 0xf << 28)
#define HC_NCOR_SYSAD_ERR(x)		((ulong) 0x1 << (x) + 28)
#define HC_COR_SYSAD_ERR_MSK		((ulong) 0xf << 24)
#define HC_COR_SYSAD_ERR(x)		((ulong) 0x1 << (x) + 24)
#define HC_DATA_ELMNT_ERR_MSK		((ulong) 0xf << 20)
#define HC_DATA_ELMNT_ERR(x)		((ulong) 0x1 << (x) + 20)
#define HC_WIDGET_ERR			((ulong) 0x1 << 16)
#define HC_MEM_ADDR_ERR_PROC_MSK	((ulong) 0xf << 4)
#define HC_MEM_ADDR_ERR_PROC(x) 	((ulong) 0x1 << (x) + 4)
#define HC_MEM_ADDR_ERR_IO		((ulong) 0x1 << 2)
#define HC_NCOR_MEM_ERR 		((ulong) 0x1 << 1)
#define HC_COR_MEM_ERR			((ulong) 0x1 << 0)

#endif /* __ASM_MACH_IP30_HEART_H */
