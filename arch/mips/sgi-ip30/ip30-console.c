/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001, 2002 Ralf Baechle
 */
#include <linux/init.h>

#include <asm/page.h>
#include <asm/sn/ioc3.h>
#include <asm/sn/sn_private.h>

#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/delay.h>

#define IOC3_CLK	(22000000 / 3)
#define IOC3_FLAGS	(0)

static inline struct ioc3_uartregs *console_uart(void)
{
	struct ioc3 *ioc3;

	ioc3 = (struct ioc3 *)((void *)(0x900000001F600000));

	return &ioc3->sregs.uarta;
}

void __init prom_putchar(char c)
{
	struct ioc3_uartregs *uart = console_uart();

	while ((uart->iu_lsr & 0x20) == 0);
	uart->iu_thr = c;
}
