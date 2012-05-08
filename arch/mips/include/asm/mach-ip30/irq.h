/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2009 Johannes Dickgreber <tanzy@gmx.de>
 */

#ifndef __ASM_MACH_IP30_IRQ_H
#define __ASM_MACH_IP30_IRQ_H

#define NR_IRQS 		128

#define HEART_IRQS		64

#define HEART_IRQ_BASE		0

/* 0...7 special heart irqs */

/* 8..15 base bridge irqs */
#define IP30_POWER_IRQ		(HEART_IRQ_BASE + 14)
#define IP30_ACFAIL_IRQ 	(HEART_IRQ_BASE + 15)

/* 16..31 bridge irqs for max 2 bridges */

/* 32..47 ipi irqs */
#define IP30_IPI_MASK		0xfUL
#define IP30_IPI_BASE		(HEART_IRQ_BASE + 32)
#define IP30_IPI_SHIFT(x)	(IP30_IPI_BASE + (x) * 4)
#define IP30_IPI_RESCH(x)	(IP30_IPI_BASE + 0 + (x) * 4)
#define IP30_IPI_TIMER(x)	(IP30_IPI_BASE + 1 + (x) * 4)
#define IP30_IPI_CALLF(x)	(IP30_IPI_BASE + 2 + (x) * 4)

/* 48..49 free irqs */

/* 50 counter irq */
#define IP30_COUNTER_IRQ	(HEART_IRQ_BASE + 50)

/* 51..63 heart error irqs */

#define MIPS_CPU_IRQ_BASE	(HEART_IRQ_BASE + 64)

#include_next <irq.h>

#endif /* __ASM_MACH_IP30_IRQ_H */
