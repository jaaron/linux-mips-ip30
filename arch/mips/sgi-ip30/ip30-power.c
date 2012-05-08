/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * ip30-power.c: Software powerdown and reset handling for IP30 architecture.
 *
 * Copyright (C) 2004-2007 Stanislaw Skowronek <skylark@linux-mips.org>
 *		 2007 Joshua Kinard <kumba@gentoo.org>
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/time.h>
#include <linux/console.h>
#include <linux/tty.h>
#include <linux/delay.h>

#include <asm/mach-ip30/addrs.h>
#include <asm/mach-ip30/heart.h>
#include <asm/mach-ip30/racermp.h>
#include <asm/reboot.h>
#include <asm/time.h>
#include <asm/io.h>
#include <asm/pci/bridge.h>

void ip30_machine_restart(char *command)
{
	printk("Rebooting...");
#ifdef CONFIG_SMP
	smp_send_stop();
	udelay(1000);
#endif
	/* execute HEART cold reset
	 *   Yes, it's cold-HEARTed!
	 */
	writeq(readq(HEART_MODE) | (1UL << 23), HEART_MODE);
}

void ip30_soft_powerdown(void);
int ip30_clear_power_irq(void);
int ip30_can_powerdown(void);

void ip30_machine_power_off(void)
{
#ifdef CONFIG_SGI_IP30_RTC	
	int i;

	if (!ip30_can_powerdown())
		return;
	printk("Powering down, please wait...");

#ifdef CONFIG_SMP
	smp_send_stop();
	udelay(1000);
#endif

	/* kill interrupts */
	writeq(HEART_ACK_ALL_MASK, HEART_CLR_ISR);
        for (i = 0; i < MP_NCPU; i++)
		writeq(HEART_CLR_ALL_MASK, HEART_IMR(i));

	/* execute RTC powerdown */
	ip30_soft_powerdown();
#else
	printk("RTC support is required to power down.\n");
	printk("System halted.\n");
	while (1);
#endif
}

void ip30_machine_halt(void)
{
	ip30_machine_power_off();
}

/* power button */
static struct timer_list power_timer;

static int is_shutdown;

static void power_timeout(unsigned long data)
{
	ip30_machine_power_off();
}

static irqreturn_t power_irq(int irq, void *dev_id)
{
	/* prepare for next IRQs */
#ifdef CONFIG_SGI_IP30_RTC
	if (!ip30_clear_power_irq())
#endif
		disable_irq_nosync(irq);

	/* button pressed twice or no init */
	if (is_shutdown || kill_cad_pid(SIGINT, 1)) {
		printk(KERN_INFO "Immediate powerdown...\n");
		ip30_machine_power_off();
		return IRQ_HANDLED;
	}

	/* power button, set LEDs if we can */
	is_shutdown = 1;
	printk(KERN_INFO "Power button pressed, shutting down...\n");

	init_timer(&power_timer);
	power_timer.function = power_timeout;
	power_timer.expires = jiffies + (30 * HZ);
	add_timer(&power_timer);

	return IRQ_HANDLED;
}

static irqreturn_t acfail_irq(int irq, void *dev_id)
{
	/* we have a bit of time here */
	return IRQ_HANDLED;
}

static int __init reboot_setup(void)
{
	/* XXX: check return value! */
	int ret;

	ret = request_irq(IP30_POWER_IRQ, power_irq, 0, "powerbtn", NULL);
	ret = request_irq(IP30_ACFAIL_IRQ, acfail_irq, 0, "acfail", NULL);
	return 0;
}

subsys_initcall(reboot_setup);
