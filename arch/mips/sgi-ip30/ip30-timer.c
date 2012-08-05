/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * ip30-timer.c: Clocksource/clockevent support for the 
 *               HEART chip in SGI Octane (IP30) systems.
 *
 * Copyright (C) 2004-2007 Stanislaw Skowronek <skylark@linux-mips.org>
 * Copyright (C) 2009 Johannes Dickgreber <tanzy@gmx.de>
 * Copyright (C) 2011 Joshua Kinard <kumba@gentoo.org>
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clocksource.h>
#include <linux/cpumask.h>

#include <asm/time.h>
#include <asm/mach-ip30/heart.h>


/* ----------------------------------------------------------------------- */
/* HEART Clocksource setup. */

#define HEART_NS_PER_CYCLE	80
#define HEART_CYCLES_PER_SEC	(NSEC_PER_SEC / HEART_NS_PER_CYCLE)

/**
 * heart_counter_read - read HEART counter register (52-bit).
 * @clocksource: pointer to clocksource struct.
 */
static cycle_t
heart_counter_read(struct clocksource *cs)
{
	return readq(HEART_COUNT);
}


/**
 * struct heart_clocksource - HEART clocksource definition.
 * @name: self-explanatory.
 * @rating: quality of this clocksource (HEART has 80ns cycle time).
 * @read: pointer to function to read the counter register.
 * @mask: bitmask for the counter (52bit counter/24bit compare).
 * @flags: clocksource flags.
 */
static struct clocksource
heart_clocksource = {
	.name	= "HEART",
	.rating	= 400,
	.read	= heart_counter_read,
	.mask	= CLOCKSOURCE_MASK(52),
	.flags	= CLOCK_SOURCE_IS_CONTINUOUS, // | CLOCK_SOURCE_VALID_FOR_HRES,
};


/**
 * heart_clocksource_init - init the clocksource for HEART.
 */
static void __init heart_clocksource_init(void)
{
	clocksource_register_hz(&heart_clocksource, HEART_CYCLES_PER_SEC);
}

/* ----------------------------------------------------------------------- */


/* ----------------------------------------------------------------------- */
/* HEART Clockevent setup. */

/*
 * HEART clockevent structure.
 */
static struct clock_event_device heart_clockevent;


/**
 * heart_compare_irq - IRQ handler for the HEART compare interrupt.
 * @irq: IRQ number.
 * @dev_id: void pointer to the clock_event_device struct.
 */
static irqreturn_t
heart_compare_irq(int irq, void *dev_id)
{
	struct clock_event_device *cd = dev_id;

	/* Ack the IRQ. */
	writeq(HEART_VEC_TO_IBIT(irq), HEART_CLR_ISR);
	cd->event_handler(cd);

	return IRQ_HANDLED;
}


/**
 * struct heart_counter_irqaction - irqaction block for HEART.
 * @name: self-explanatory.
 * @flags: HEART counter IRQ flags.
 * @handler: pointer to IRQ handler for the counter interrupt.
 * @dev_id: void pointer to the HEART device cookie.
 * @irq: HEART's counter IRQ number.
 */
static struct irqaction
heart_counter_irqaction = {
	.name		= "HEART",
	.flags		= IRQF_TIMER | IRQF_PERCPU,
	.handler	= heart_compare_irq,
	.dev_id		= &heart_clockevent,
	.irq		= IRQ_HEART_CC,
};


/**
 * heart_next_event - resets the compare bit on HEART.
 * @delta: difference between count and compare.
 * @evt: pointer to clock_event_device struct.
 */
static int
heart_next_event(unsigned long delta, struct clock_event_device *evt)
{
	unsigned long cnt;

	/*
	 * Read HEART's current counter, add the delta and write that 
	 * back to HEART's compare register.
	 */
	cnt = readq(HEART_COUNT);
	cnt += delta;
	writeq(cnt, HEART_COMPARE);

	return ((readq(HEART_COUNT) >= cnt) ? -ETIME : 0);
}


/**
 * heart_set_mode - Change the clock event mode on HEART.
 * @mode: mode to change to.
 * @evt: pointer to clock_event_device struct.
 *
 * Not supported on HEART.
 */
static void
heart_set_mode(enum clock_event_mode mode, struct clock_event_device *evt)
{
	/* Nothing to do  */
}


/**
 * heart_event_handler - Clock event handler on HEART.
 * @cd: pointer to clock_event_device struct.
 *
 * Not supported on HEART.
 */
static void
heart_event_handler(struct clock_event_device *cd)
{
	/* Nothing to do  */
}


/**
 * heart_clockevent_init -  HEART clockevent initialization.
 */
void __init
heart_clockevent_init(void)
{
	struct clock_event_device *cd = &heart_clockevent;

	cd->name		= "HEART";
	cd->features		= CLOCK_EVT_FEAT_ONESHOT;
	clockevent_set_clock(cd, HEART_CYCLES_PER_SEC);
	cd->max_delta_ns        = clockevent_delta2ns(0xffffff, cd);
	cd->min_delta_ns        = clockevent_delta2ns(0x0000fa, cd);
	cd->rating		= 200;
	cd->irq			= IRQ_HEART_CC;
	cd->cpumask		= cpu_all_mask;
	cd->set_next_event	= heart_next_event;
	cd->set_mode		= heart_set_mode;
	cd->event_handler	= heart_event_handler;
	clockevents_register_device(cd);

	setup_irq(IRQ_HEART_CC, &heart_counter_irqaction);
}

/* ----------------------------------------------------------------------- */


/* ----------------------------------------------------------------------- */

/**
 * plat_time_init - platform time initialization.
 */
void __init
plat_time_init(void)
{
	unsigned long heart_compare;

	heart_compare = (readq(HEART_COUNT) + (HEART_CYCLES_PER_SEC / 10));
	write_c0_count(0);
	while ((readq(HEART_COUNT) - heart_compare) & 0x800000);
	mips_hpt_frequency = (read_c0_count() * 10);
	printk(KERN_INFO "%d MHz CPU detected\n",
	       (mips_hpt_frequency * 2) / 1000000);

	heart_clocksource_init();
	heart_clockevent_init();
}


unsigned int get_c0_compare_int(void) {
////        unsigned int cpu = smp_processor_id();
 
	/* Return Octane's Timer IRQ */
////	return IRQ_TIMER_P(cpu);
	return MIPS_CPU_IRQ_BASE + 7;
}


void ip30_timer_bcast(void)
{
	int i;
	for (i = 1; i < NR_CPUS; i++)
		if (cpu_online(i))
			writeq(HEART_VEC_TO_IBIT(IRQ_TIMER_P(i)),
			       HEART_SET_ISR);
}

