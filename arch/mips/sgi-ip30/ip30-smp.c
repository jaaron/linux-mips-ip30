/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * ip30-smp.c: SMP on IP30 architecture.
 *
 * Copyright (C) 2005-2007 Stanislaw Skowronek <skylark@linux-mips.org>
 *               2006-2007 Joshua Kinard <kumba@gentoo.org>
 *               2009 Johannes Dickgreber <tanzy@gmx.de>
 *               2011 Joshua Kinard <kumba@gentoo.org>
 */

#include <linux/init.h>
//#include <linux/delay.h>
#include <linux/smp.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/spinlock_types.h>

#include <asm/mmu_context.h>
#include <asm/bootinfo.h>
#include <asm/time.h>
#include <asm/mach-ip30/heart.h>
#include <asm/mach-ip30/racermp.h>
#include <asm/mach-ip30/addrs.h>

#include <linux/delay.h>

#undef DEBUG_IPI

extern void asmlinkage ip30_smp_bootstrap(void);
extern void plat_time_init(void);
extern void heart_clockevent_init(void);

//static spinlock_t ipi_mbx_lock = SPIN_LOCK_UNLOCKED;
static DEFINE_PER_CPU(spinlock_t, ipi_mbx_lock);
static volatile unsigned int ipi_mailbox[NR_CPUS];

extern unsigned int (*mips_hpt_read)(void);
extern void (*mips_hpt_init)(unsigned int);
extern void ip30_init_secondary_irq(void);

static void ip30_send_ipi_single(int cpu, unsigned int action)
{
	unsigned long flags;

//#ifdef DEBUG_IPI
//	if(action == SMP_CALL_FUNCTION)
//		printk(KERN_EMERG "IPI call_function me: %d -> %d\n", smp_processor_id(), cpu);
//#endif

	spin_lock_irqsave(&ipi_mbx_lock, flags);
	ipi_mailbox[cpu] |= action;
	writeq(HEART_VEC_TO_IBIT(IRQ_IPI_P(cpu)), HEART_SET_ISR);
	spin_unlock_irqrestore(&ipi_mbx_lock, flags);

	printk(KERN_EMERG "DEBUG: ip30_send_ipi_single: CPU %d -> IRQ_IPI_P(%d)!\n", smp_processor_id(), cpu);
}

static void ip30_send_ipi_mask(const struct cpumask *mask, unsigned int action)
{
        unsigned int i;

        for_each_cpu(i, mask)
                ip30_send_ipi_single(i, action);
}

int cpu_next_pcpu(int pcpu)
{
	int i;
	for (i = (pcpu + 1) % MP_NCPU; !cpu_possible(i); i = (i + 1) % MP_NCPU)
		if(i == pcpu)
			return pcpu;
	return i;
}

irqreturn_t ip30_ipi_mailbox_irq(int irq, void *dev_id)
{
	int cpu = smp_processor_id();
	int mbx;

	printk(KERN_EMERG "DEBUG: ip30_ipi_mailbox_irq: checking mailbox on CPU %d\n", cpu);

	spin_lock(&ipi_mbx_lock);
	mbx = ipi_mailbox[cpu];
	ipi_mailbox[cpu] = 0;
	writeq(HEART_VEC_TO_IBIT(IRQ_IPI_P(cpu)), HEART_CLR_ISR);
	spin_unlock(&ipi_mbx_lock);

	switch (mbx) {
	case SMP_RESCHEDULE_YOURSELF:
		/* do nothing */
		printk(KERN_EMERG "DEBUG: ip30_mailbox_irq: Got SMP_RESCHEDULE_YOURSELF on CPU #%d!\n", cpu);
		break;

	case SMP_CALL_FUNCTION:
		printk(KERN_EMERG "DEBUG: ip30_mailbox_irq: Got SMP_CALL_FUNCTION on CPU #%d!\n", cpu);
		smp_call_function_interrupt();
		break;

	default:
//		printk(KERN_EMERG "DEBUG: ip30_mailbox_irq: Got 0x%x on CPU #%d!\n", mbx, cpu);
		break;
	}

	return IRQ_HANDLED;
}

static void __init ip30_prepare_cpus(unsigned int max_cpus)
{
	int cpu;

	for_each_possible_cpu(cpu)
		spin_lock_init(&per_cpu(ipi_mbx_lock, cpu));
}

static void __init ip30_smp_setup(void)
{
        int i, j;

	//	cpumask_clear(cpu_possible_mask);
	for (i = 0, j = 0; (i < MP_NCPU) && (j < NR_CPUS); i++) {
		if (readl(MP_MAGIC(i)) == MPCONF_MAGIC) {
		    // cpu_set(i, cpu_possible_map);
			set_cpu_possible(i, true);
			set_cpu_present(i, true);
			printk(KERN_INFO "Slot: %d PrID: %x PhyID: %x VirtID: %x CPU is present.\n",
				i, readl(MP_PRID(i)), readl(MP_PHYSID(i)), readl(MP_VIRTID(i)));
			j++;
		}
	}
	printk(KERN_INFO "Detected %d CPU(s) present.\n", j);
}

static void __cpuinit ip30_boot_secondary(int cpu, struct task_struct *idle)
{
	writeq(__KSTK_TOS(idle), MP_STACKADDR(cpu));
	writeq((unsigned long) task_thread_info(idle), MP_LPARM(cpu));
	writeq((unsigned long) ip30_smp_bootstrap, MP_LAUNCH(cpu));
}

static void __cpuinit ip30_init_secondary(void)
{
	ip30_init_secondary_irq();
}


struct irqaction
ip30_ipi_irqaction = {
	.name		= "IP30 IPI 46",
	.flags		= IRQF_PERCPU,
	.handler	= ip30_ipi_mailbox_irq,
};

static void __cpuinit ip30_smp_finish(void)
{
	/* Called by secondary+ CPU(s)! */
	int cpu = smp_processor_id();

	printk(KERN_EMERG "DEBUG: ip30_smp_finish: called by cpu %d!\n", cpu);

//	setup_irq(IRQ_IPI_P(cpu), &ip30_ipi_irqaction);

//	printk(KERN_EMERG "DEBUG: ip30_smp_finish: calling r4k_clockevent_init!\n");
//	heart_clockevent_init();			/* Does not belong on CPU#1? */
	local_irq_enable();
}

static void __init ip30_cpus_done(void)
{
	/* Called by primary CPU! */
	int cpu = smp_processor_id();
	printk(KERN_EMERG "DEBUG: ip30_cpus_done: called by cpu %d!\n", cpu);

//	setup_irq(IRQ_IPI_P(cpu), &ip30_ipi_irqaction);
}

struct plat_smp_ops ip30_smp_ops = {
	.send_ipi_single	= ip30_send_ipi_single,
	.send_ipi_mask		= ip30_send_ipi_mask,
	.init_secondary		= ip30_init_secondary,
	.smp_finish		= ip30_smp_finish,
	.cpus_done		= ip30_cpus_done,
	.boot_secondary		= ip30_boot_secondary,
	.smp_setup		= ip30_smp_setup,
	.prepare_cpus		= ip30_prepare_cpus,
};
