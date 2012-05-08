/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * SGI IP30 specific setup.
 *
 * Copyright (C) 2004-2007 Stanislaw Skowronek <skylark@linux-mips.org>
 *               2007 Joshua Kinard <kumba@gentoo.org>
 *		 2009 Johannes Dickgreber <tanzy@gmx.de>
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/pm.h>

#include <asm/bootinfo.h>
#include <asm/reboot.h>
#include <asm/smp-ops.h>

#include <asm/mach-ip30/addrs.h>
#include <asm/mach-ip30/heart.h>

extern void ip30_machine_restart(char *command);
extern void ip30_machine_halt(void);
extern void ip30_machine_power_off(void);
extern void __init ip30_xtalk_setup(void);

extern struct plat_smp_ops ip30_smp_ops;

static unsigned long __init ip30_size_memory(void)
{
	unsigned long result = 0;
	unsigned int mem;
	int i;

	for (i = 0; i < 8; i++) {
		mem = readl(HEART_MEMCFG0 + i * 4);
		if (mem & HEART_MEMCFG_VLD)
			result += ((mem & HEART_MEMCFG_RAM_MSK)
			       >> HEART_MEMCFG_RAM_SHFT) + 1;
	}

	return result << HEART_MEMCFG_UNIT_SHFT;
}


static void __init ip30_fix_memory(void)
{
	unsigned long size = ip30_size_memory();
	printk(KERN_INFO "Detected %ld MB of physical memory.\n", size >> 20);

	if(size > IP30_MAX_PROM_MEM) {
		printk(KERN_INFO "Updating PROM memory size.\n");
		add_memory_region((IP30_MAX_PROM_MEM + IP30_MEM_BASE),
					size - IP30_MAX_PROM_MEM, BOOT_MEM_RAM);
	}
}

void __init plat_mem_setup(void)
{
	printk(KERN_INFO "SGI Octane (IP30) support: (c) 2004-2007 Stanislaw Skowronek.\n");
	set_io_port_base(IP30_IO_PORT_BASE);
	_machine_restart  = ip30_machine_restart;
	_machine_halt = ip30_machine_halt;
	pm_power_off = ip30_machine_power_off;
	ip30_fix_memory();
	ip30_xtalk_setup();
}
