/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * ip30-irq.c: Highlevel interrupt handling for IP30 architecture.
 *
 * Copyright (C) 2004-2007 Stanislaw Skowronek <skylark@linux-mips.org>
 *		 2007 Joshua Kinard <kumba@gentoo.org>
 *		 2009 Johannes Dickgreber <tanzy@gmx.de>
 *
 * Inspired by ip27-irq.c and ip32-irq.c
 */

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/tick.h>

#include <asm/irq_cpu.h>
#include <asm/pci/bridge.h>

#include <asm/mach-ip30/heart.h>
#include <asm/mach-ip30/pcibr.h>
#include <asm/mach-ip30/racermp.h>

#undef DEBUG_IRQ
#define DEBUG_IRQ_SET 1

#define DYNAMIC_IRQ_START	64
#define SOFT_IRQ_COUNT		50	/* This is probably wrong */

#ifndef CONFIG_SMP
#define cpu_logical_map(x) 0
#define cpu_next_pcpu(x) 0
#else
extern int cpu_next_pcpu(int pcpu);
#endif

static DEFINE_SPINLOCK(heart_lock);
static int heart_irq_owner[NR_IRQS];
bridge_t *ip30_irq_to_bridge[NR_IRQS];
unsigned int ip30_irq_to_slot[NR_IRQS];

/* /\* CPU IRQ *\/ */

/* static void enable_cpu_irq(struct irq_data *d) */
/* { */
/* 	set_c0_status(STATUSF_IP7); */
/* } */

/* static void disable_cpu_irq(struct irq_data *d) */
/* { */
/* 	clear_c0_status(STATUSF_IP7); */
/* } */

/* //static void end_cpu_irq(struct irq_data *d) */
/* //{ */
/* //	if (!(irq_to_desc(d->irq)->status & (IRQ_DISABLED | IRQ_INPROGRESS))) */
/* //		enable_cpu_irq(d); */
/* //} */

/* static struct irq_chip ip30_cpu_irq = { */
/* 	.name = "IP30 CPU", */
/* 	.irq_enable = enable_cpu_irq, */
/* 	.irq_disable = disable_cpu_irq, */
/* 	.irq_ack = disable_cpu_irq, */
/* 	.irq_eoi = enable_cpu_irq, */
/* }; */



/* static noinline void ip30_do_irq(unsigned int irq) */
/* { */
/* 	irq_enter(); */
/* 	generic_handle_irq(irq); */
/* 	irq_exit(); */
/* } */


static noinline void ip30_do_error_irq(void)
{
	unsigned long errors;
	unsigned long cause;
	int i;

	irq_enter();
	errors = readq(HEART_ISR);
	cause = readq(HEART_CAUSE);
	writeq(HEART_INT_LEVEL4, HEART_CLR_ISR);

	printk(KERN_WARNING "IP30: HEART ATTACK! ISR = 0x%.16lx CAUSE = 0x%.16lx\n", 
		(long unsigned int)((errors >> HEART_ERR_MASK_START) & HEART_ERR_MASK),
	        (long unsigned int)((cause >> HEART_ERR_MASK_START) & HEART_ERR_MASK));

	for (i = HEART_ERR_MASK_END; i >= HEART_ERR_MASK_START; i--)
		if ((errors >> i) & 1)
			printk(KERN_CONT "    heart error irq #%d\n", i);

	irq_exit();
}

/* real HEART IRQs */
int heart_irq_thisowner;





/* XXX: This helps us to debug print IRQs of interest. */
static inline unsigned int ok_to_debug_irq(unsigned int irq)
{
	switch (irq) {
	case 8:			/* scsi0 */
	case 9:			/* scsi1 */
	case 10:		/* eth0 */
		return 0;	/* Non-Zero is TRUE in C! */
		break;
	default:
		return 1;	/* Zero is FALSE in C! */
		break;
	}
}


static int log2(int x)
{
	int r = 0;

	while(x >>= 1)
		r++;

	return r;
}


static noinline void ip30_do_heart_irq(void)
{
	unsigned long irqs;
	unsigned long irqsel = NON_HEART_IRQ_ST;
	int irqnum = SOFT_IRQ_COUNT;
	int cpu = smp_processor_id();
	#include <asm/mach-ip30/addrs.h>
	bridge_t *bvma = (bridge_t *)RAW_NODE_SWIN_BASE(0, 15);

	irqs = readq(HEART_ISR);
	irqs &= HEART_ATK_MASK;
//	irqs &= (HEART_INT_LEVEL1 | HEART_INT_LEVEL0);  /* Appears to lock HEART into only seeing level0/level1 IRQs, which never fire (SW IRQs in MIPS). */
	irqs &= readq(HEART_IMR(cpu));

//#ifdef DEBUG_IRQ
	if (ok_to_debug_irq(log2((readq(HEART_ISR) & 0xffffffff))))
		if (irqs & ~(15UL << IRQ_TIMER_P(0)))
			printk("IP30: received HEART IRQs: 0x%016lx (mask 0x%016lx) PCPU%d BRIDGE %08x\n",
			       (long unsigned int)readq(HEART_ISR), (long unsigned int)readq(HEART_IMR(cpu)), cpu, bvma->b_int_status);
//#endif

	/* poll all IRQs in decreasing priority order */
	while (irqsel) {
		if (irqs & irqsel)
			do_IRQ(irqnum);
		irqsel >>= 1;
		irqnum--;
	}
}




static unsigned int startup_heart_irq(struct irq_data *d)
{
	bridge_t *bridge;
	unsigned int device, slot;
	unsigned int irq = d->irq;
	unsigned int irq_owner = 0;
	int cpu = smp_processor_id();
	unsigned long *imr;

	spin_lock(&heart_lock);

	/* Hack to force certain IRQ's to certain CPUs until sense is made of things. */
	switch (d->irq) {
	case 42:		irq_owner = 0;	break;
	case 43:		irq_owner = 1;	break;
	case 45:		irq_owner = 0;	break;
	case 46:		irq_owner = 0;	break;
	case 47:		irq_owner = 1;	break;
	case IRQ_HEART_CC:	irq_owner = 0;	break;
	default:
		irq_owner = 0;
		break;
	}

	if (heart_irq_owner[irq] != -1) {
		printk(KERN_EMERG "DEBUG: startup_heart_irq: bad IRQ "
		       "startup request for IRQ %d on CPU %d "
		       "(already assigned to %d)!\n",
		       irq, cpu, heart_irq_owner[irq]);
		goto out;
	}

//#ifdef DEBUG_IRQ_SET
	printk(KERN_EMERG "DEBUG: startup_heart_irq: start up IRQ%d for "
	       "CPU %d\n", irq, irq_owner);
//#endif

	/* store which CPU owns this IRQ */
	heart_irq_owner[irq] = irq_owner;

	/* clear the IRQ */
	writeq(HEART_VEC_TO_IBIT(irq), HEART_CLR_ISR);

	/* unmask IRQ */
	imr = HEART_IMR(irq_owner);
	writeq(readq(imr) | HEART_VEC_TO_IBIT(irq), imr);

	/* Handle BRIDGE IRQs. */
	bridge = ip30_irq_to_bridge[irq];
	if (bridge) {
		slot = ip30_irq_to_slot[irq];
		bridge->b_int_enable |= (1 << slot);
		bridge->b_int_mode |= (1 << slot);
		device = bridge->b_int_device;
		device &= ~BRIDGE_INT_DEV_MASK(slot);
		device |=  BRIDGE_INT_DEV_SET(slot, slot);
		bridge->b_int_device = device;
		bridge->b_widget.w_tflush;
	}

 out:
	spin_unlock(&heart_lock);

	/* XXX: This is probably not right; we could have pending irqs */
	return 0;
}


static void shutdown_heart_irq(struct irq_data *d)
{
	bridge_t *bridge;
	unsigned long *imr;

	if (ok_to_debug_irq(d->irq))
		printk(KERN_EMERG "DEBUG: shutdown_heart_irq: shutting down IRQ %d on CPU %d!\n", d->irq, heart_irq_owner[d->irq]);

	spin_lock(&heart_lock);

	imr = HEART_IMR(heart_irq_owner[d->irq]);
	writeq(readq(imr) & ~(HEART_VEC_TO_IBIT(d->irq)), imr);

	bridge = ip30_irq_to_bridge[d->irq];
	if (bridge)
		bridge->b_int_enable &= ~(1UL << ip30_irq_to_slot[d->irq]);

	heart_irq_owner[d->irq] = -1;

	spin_unlock(&heart_lock);
}


static void ack_heart_irq(struct irq_data *d)
{
	if (ok_to_debug_irq(d->irq))
		printk(KERN_EMERG "DEBUG: ack_heart_irq: acked IRQ %d on CPU %d!\n", d->irq, heart_irq_owner[d->irq]);

	spin_lock(&heart_lock);
//	if (!ip30_irq_to_bridge[d->irq])
//		writeq(HEART_VEC_TO_IBIT(d->irq), HEART_ISR);
//	else

	writeq(HEART_VEC_TO_IBIT(d->irq), HEART_CLR_ISR);
	spin_unlock(&heart_lock);
}

static void unmask_heart_irq(struct irq_data *d)
{
	unsigned long *imr;

	if (ok_to_debug_irq(d->irq))
		printk(KERN_EMERG "DEBUG: unmask_heart_irq: unmasked IRQ %d on CPU %d!\n", d->irq, heart_irq_owner[d->irq]);

	spin_lock(&heart_lock);
	imr = HEART_IMR(heart_irq_owner[d->irq]);
	writeq(readq(imr) | HEART_VEC_TO_IBIT(d->irq), imr);
	spin_unlock(&heart_lock);
}

static void mask_heart_irq(struct irq_data *d)
{
	unsigned long *imr;

	if (ok_to_debug_irq(d->irq))
		printk(KERN_EMERG "DEBUG: mask_heart_irq: masked IRQ %d on CPU %d!\n", d->irq, heart_irq_owner[d->irq]);

	spin_lock(&heart_lock);
	imr = HEART_IMR(heart_irq_owner[d->irq]);
	writeq(readq(imr) & ~(HEART_VEC_TO_IBIT(d->irq)), imr);
	spin_unlock(&heart_lock);
}

static void mask_and_ack_heart_irq(struct irq_data *d)
{
        unsigned long *imr;

	if (ok_to_debug_irq(d->irq))
		printk(KERN_EMERG "DEBUG: mask_and_ack_heart_irq: masked/acked IRQ %d on CPU %d!\n", d->irq, heart_irq_owner[d->irq]);

        spin_lock(&heart_lock);

        imr = HEART_IMR(heart_irq_owner[d->irq]);
//	printk(KERN_EMERG "DEBUG: mask_and_ack_heart_irq: %d    IMR =  0x%.16lx\n", heart_irq_owner[d->irq],
//	       (long unsigned int)imr);
	writeq(readq(imr) & ~(HEART_VEC_TO_IBIT(d->irq)), imr);
	writeq(HEART_VEC_TO_IBIT(d->irq), HEART_CLR_ISR);

        spin_unlock(&heart_lock);
}

#if 0
static void end_of_heart_irq(struct irq_data *d)
{
	if (ok_to_debug_irq(d->irq))
		printk(KERN_EMERG "DEBUG: end_of_heart_irq: ended IRQ %d on CPU %d!\n", d->irq, heart_irq_owner[d->irq]);

	if (!(irq_to_desc(d->irq)->status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		unmask_heart_irq(d);
}
#endif

static struct irq_chip ip30_heart_irq = {
	.name = "HEART",
	.irq_startup	= startup_heart_irq,
	.irq_shutdown	= shutdown_heart_irq,
	.irq_enable	= unmask_heart_irq,
	.irq_disable	= mask_heart_irq,
	.irq_ack	= ack_heart_irq,
	.irq_mask	= mask_heart_irq,
	.irq_mask_ack	= mask_and_ack_heart_irq,
	.irq_unmask	= unmask_heart_irq,
	.irq_eoi	= unmask_heart_irq,
};


/* setup procedure */

asmlinkage void plat_irq_dispatch(void)
{
	unsigned long pending;
//	int cpu = smp_processor_id();
//	extern void ip30_mailbox_irq(void);

	pending = read_c0_cause() & read_c0_status();
	if (pending & CAUSEF_IP7) {			/* IBIT8 - cpu cnt/cmp */
//		printk(KERN_EMERG "DEBUG: plat_irq_dispatch: CPU %d: CAUSEF_IP7!\n", smp_processor_id());
		do_IRQ(MIPS_CPU_IRQ_BASE + 7);
	} else if (unlikely(pending & CAUSEF_IP6)) {	/* IBIT7 - lvl4 - ERR */
		printk(KERN_EMERG "DEBUG: plat_irq_dispatch: CPU %d: CAUSEF_IP6!\n", smp_processor_id());
		ip30_do_error_irq();
	} else if (pending & CAUSEF_IP5) {		/* IBIT6 - lvl3 - HEART TIMER */
		printk(KERN_EMERG "DEBUG: plat_irq_dispatch: CPU %d: CAUSEF_IP5!\n", smp_processor_id());
//		ip30_do_heart_irq();
		do_IRQ(IRQ_HEART_CC);
	} else if (pending & CAUSEF_IP4) {		/* IBIT5 - lvl2 - IPI, Misc */
		printk(KERN_EMERG "DEBUG: plat_irq_dispatch: CPU %d: CAUSEF_IP4!\n", smp_processor_id());
		ip30_do_heart_irq();
////		ip30_mailbox_irq();
	} else if (pending & CAUSEF_IP3) {		/* IBIT4 - lvl1 - HW1 */
		printk(KERN_EMERG "DEBUG: plat_irq_dispatch: CPU %d: CAUSEF_IP3!\n", smp_processor_id());
		ip30_do_heart_irq();
	} else if (pending & CAUSEF_IP2) {		/* IBIT3 - lvl0 - HW0 */
//		printk(KERN_EMERG "DEBUG: plat_irq_dispatch: CPU %d: CAUSEF_IP2!\n", smp_processor_id());
		ip30_do_heart_irq();
	}
}



static irqreturn_t
null_irq_handler(int irq, void *dev_id)
{
	printk(KERN_EMERG "DEBUG: null_irq_handler: Got IRQ %d!\n", irq);
	return IRQ_HANDLED;
}

static struct irqaction
misc_irqaction = {
	.name		= "MISC",
	.flags		= IRQF_PERCPU,
	.handler	= &null_irq_handler,
};



/* XXX: Hack alert. */
static inline unsigned int irq_is_reserved(unsigned int irq)
{
	switch (irq) {
//	case 7:
//		return 1;	/* Non-Zero is TRUE in C! */
//		break;
	default:
		return 0;	/* Zero is FALSE in C! */
		break;
	}
}


void __init arch_init_irq(void)
{
//	int cpu = smp_processor_id();
	int i;
//	unsigned long *imr;
//	extern struct irqaction ip30_ipi_irqaction;

	/* Disable CPU IRQs. */
	clear_c0_status(ST0_IM);

	/* acknowledge everything */
	writeq(HEART_ACK_ALL_MASK, HEART_CLR_ISR);

	/* mask all IRQs */
	writeq(HEART_CLR_ALL_MASK, HEART_IMR(0));
	writeq(HEART_CLR_ALL_MASK, HEART_IMR(1));
	writeq(HEART_CLR_ALL_MASK, HEART_IMR(2));
	writeq(HEART_CLR_ALL_MASK, HEART_IMR(3));

	/* leave errors on */
	writeq(HEART_BR_ERR_MASK, HEART_IMR(0));

	/* IP30 CPU Timer */
	// irq_set_chip_and_handler(63, &ip30_cpu_irq, handle_percpu_irq);


	/* XXX: Hardcoded IRQs. This is a mess. */
//	printk(KERN_EMERG "DEBUG: arch_init_irq: IRQ 46!\n");
//	irq_set_chip_and_handler(46, &ip30_heart_irq, handle_percpu_irq);
//	setup_irq(46, &ip30_ipi_irqaction);

	for (i = 0; i < SOFT_IRQ_COUNT; i++) {
		heart_irq_owner[i] = -1;

		irq_set_chip_and_handler(i, &ip30_heart_irq, handle_percpu_irq);
		if (irq_is_reserved(i)) {
			setup_irq(i, &misc_irqaction);
			printk(KERN_EMERG "DEBUG: arch_init_irq: Called setup_irq(%d, <func>)!\n", i);
		}
		printk(KERN_EMERG "DEBUG: arch_init_irq: Assigned IRQ %d to ip30_heart_irq!\n", i);
	}


	/* XXX: Need to setup IRQ map like IP32, AFTER fixing ioc3. */
//	for ( i = DYNAMIC_IRQ_START; i < NR_IRQS; i++) {
//		irq_set_chip_and_handler(i, &dynamic_free_irq, handle_percpu_irq);
//		printk(KERN_EMERG "DEBUG: plat_irq_dispatch: Assigned IRQ %d to dynamic_free_irq!\n", i);
//	}

	/* Setup MIPS CPU IRQ. */
	mips_cpu_irq_init();

	/* mask IP0, IP1 (sw int).  IP7 is brought up in generic MIPS code. */
	change_c0_status(ST0_IM, STATUSF_IP2 | STATUSF_IP3 | STATUSF_IP4 |
				 STATUSF_IP5 | STATUSF_IP6);
	set_c0_status(ST0_IE);
	printk("IP30: interrupt controller initialized.\n");
}

#ifdef CONFIG_SMP
void ip30_init_secondary_irq(void)
{
	int cpu = smp_processor_id();
	extern struct irqaction ip30_ipi_irqaction;

	/* XXX: Hardcoded IRQs */
//	printk(KERN_EMERG "DEBUG: arch_init_irq: IRQ 47!\n");
	irq_set_chip_and_handler(43, &ip30_heart_irq, handle_percpu_irq);

	irq_set_chip_and_handler(47, &ip30_heart_irq, handle_percpu_irq);
	setup_irq(47, &ip30_ipi_irqaction);

	writeq(HEART_CLR_ALL_MASK, HEART_IMR(cpu));
	change_c0_status(ST0_IM, STATUSF_IP2 | STATUSF_IP3 | STATUSF_IP4 |
				 STATUSF_IP5 | STATUSF_IP6);// | STATUSF_IP7);
	set_c0_status(ST0_IE);
}
#endif

//EXPORT_SYMBOL(new_dynamic_irq);
//EXPORT_SYMBOL(call_dynamic_irq);
//EXPORT_SYMBOL(delete_dynamic_irq);


