/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * ip30-err.c: HEART error handling for IP30 architecture.
 *
 * Copyright (C) 2004-2007 Stanislaw Skowronek <skylark@linux-mips.org>
 *		 2007 Joshua Kinard <kumba@gentoo.org>
 */

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/time.h>
#include <linux/timex.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/kernel_stat.h>
#include <linux/delay.h>

#include <asm/mach-ip30/heart.h>

void ip30_do_err(void)
{
	unsigned long errors = readq(HEART_ISR);
	int i;

	irq_enter();

	writeq(HEART_INT_LEVEL4, HEART_CLR_ISR);

	printk("IP30: HEART ATTACK! Caught errors: 0x%04x!\n",
		(int)((errors >> HEART_ERR_MASK_START) & HEART_ERR_MASK));

	for(i = HEART_ERR_MASK_END; i >= HEART_ERR_MASK_START; i--)
		if ((errors >> i) & 1)
			printk("    interrupt #%d\n", i);
	irq_exit();
}
