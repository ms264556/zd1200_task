#ifndef _LINUX_KERNEL_STAT_H
#define _LINUX_KERNEL_STAT_H

#include <linux/list.h>
#include <linux/kallsyms.h>
#include <linux/smp.h>
#include <linux/threads.h>
#include <linux/percpu.h>
#include <linux/cpumask.h>
#include <linux/interrupt.h>
#include <asm/irq.h>
#include <asm/cputime.h>

/*
 * 'kernel_stat.h' contains the definitions needed for doing
 * some kernel statistics (CPU usage, context switches ...),
 * used by rstatd/perfmeter
 */

struct cpu_usage_stat {
	cputime64_t user;
	cputime64_t nice;
	cputime64_t system;
	cputime64_t softirq;
	cputime64_t irq;
	cputime64_t idle;
	cputime64_t iowait;
	cputime64_t steal;
	cputime64_t guest;
};

#ifdef CONFIG_PROC_TASKLETS
struct tasklet_stat {
	struct list_head list;
	void *func;
	atomic_t count;
};
#endif

struct kernel_stat {
	struct cpu_usage_stat	cpustat;
#ifndef CONFIG_GENERIC_HARDIRQS
       unsigned int irqs[NR_IRQS];
#endif
	unsigned int softirqs[NR_SOFTIRQS];
#ifdef CONFIG_PROC_TASKLETS
	struct tasklet_stat tasklets[256];
#endif
};

DECLARE_PER_CPU(struct kernel_stat, kstat);

#define kstat_cpu(cpu)	per_cpu(kstat, cpu)
/* Must have preemption disabled for this to be meaningful. */
#define kstat_this_cpu	__get_cpu_var(kstat)

extern unsigned long long nr_context_switches(void);

#ifndef CONFIG_GENERIC_HARDIRQS
#define kstat_irqs_this_cpu(irq) \
	(kstat_this_cpu.irqs[irq])

struct irq_desc;

static inline void kstat_incr_irqs_this_cpu(unsigned int irq,
					    struct irq_desc *desc)
{
	kstat_this_cpu.irqs[irq]++;
}

static inline unsigned int kstat_irqs_cpu(unsigned int irq, int cpu)
{
       return kstat_cpu(cpu).irqs[irq];
}
#else
#include <linux/irq.h>
extern unsigned int kstat_irqs_cpu(unsigned int irq, int cpu);
#define kstat_irqs_this_cpu(DESC) \
	((DESC)->kstat_irqs[smp_processor_id()])
#define kstat_incr_irqs_this_cpu(irqno, DESC) \
	((DESC)->kstat_irqs[smp_processor_id()]++)

#endif

static inline void kstat_incr_softirqs_this_cpu(unsigned int irq)
{
	kstat_this_cpu.softirqs[irq]++;
}

static inline unsigned int kstat_softirqs_cpu(unsigned int irq, int cpu)
{
       return kstat_cpu(cpu).softirqs[irq];
}

#ifdef CONFIG_PROC_TASKLETS
static inline unsigned int kstat_hash_tasklets(void *func)
{
	unsigned char *c = (unsigned char *)&func;

	/* Poor man's hash algorithm. Note XOR of the MSB 2 bits is to
	 * get rid of a hash collision observed on Ruckus AP.
	 */
	return  (c[0] ^ c[1]) ^ (c[2] ^ c[3]) ^ (c[0] >> 6);
}

static inline char *kstat_func2name(void *func, char *buf, int buflen)
{
	const char *name;
	unsigned long size, offset;
	char *modname, namebuf[KSYM_NAME_LEN+1];

	name = kallsyms_lookup((unsigned long)func, &size, &offset, &modname, namebuf);
	if (name) {
		snprintf(buf, buflen, "%s", name);
	} else {
		snprintf(buf, buflen, "%p", func);
	}
	return buf;
}

extern int kstat_do_tasklets;
static inline int kstat_do_tasklets_get(void)
{
	return kstat_do_tasklets;
}

static inline void kstat_do_tasklets_set(int doit)
{
	kstat_do_tasklets = doit;
}

static inline void kstat_init_tasklets_this_cpu(void *func, int count)
{
	int h;
	struct list_head *pos;
	struct tasklet_stat *t;
	char buf[128];

	h = kstat_hash_tasklets(func);
	t = &kstat_this_cpu.tasklets[h];
	if (!t->func) {
		/* Hash array entry is available. */
		t->func = func;
		atomic_set(&t->count, count);
		INIT_LIST_HEAD(&t->list);
		printk(KERN_INFO "kstat_init_tasklets: hash=0x%02x, "
		       "func=0x%p %s\n",
		       h, func, kstat_func2name(func, buf, sizeof(buf)));
	} else if (t->func == func) {
		/* Hash array entry matches. */
	} else {
		/* Need to traverse hash linked list. */
		struct tasklet_stat *stat;
		list_for_each(pos, &t->list) {
			stat = (struct tasklet_stat *)pos;
			if (stat->func == func) {
				/* Hash linked list entry matches. */
				return;
			}
		}
		/* Allocate a new hash linked list entry and add in. */
		stat = kmalloc(sizeof(struct tasklet_stat), GFP_ATOMIC);
		if (stat) {
			stat->func = func;
			atomic_set(&stat->count, count);
			list_add_tail(&stat->list, &t->list);
			printk(KERN_INFO "kstat_init_tasklets: hash=0x%02x, "
			       "func=0x%p %s in linked list\n",
			       h, func, kstat_func2name(func, buf, sizeof(buf)));
		}
	}
}

static inline void kstat_incr_tasklets_this_cpu(void *func)
{
	int h;
	struct list_head *pos;
	struct tasklet_stat *t;

	if (!kstat_do_tasklets_get())
		return;

	h = kstat_hash_tasklets(func);
	t = &kstat_this_cpu.tasklets[h];
	if (t->func == func) {
		/* Sure-ting branch */
		atomic_inc(&t->count);
	} else {
		struct tasklet_stat *stat;
		list_for_each(pos, &t->list) {
			stat = (struct tasklet_stat *)pos;
			if (stat->func == func) {
				atomic_inc(&stat->count);
				return;
			}
		}
		printk(KERN_INFO "Having kstat for tasklets init and incr "
		       "at the same time\n");
		kstat_init_tasklets_this_cpu(func, 1);
	}
}

static inline void kstat_kill_tasklets_this_cpu(void *func)
{
	int h;
	struct list_head *pos;
	struct tasklet_stat *t;

	h = kstat_hash_tasklets(func);
	t = &kstat_this_cpu.tasklets[h];
	if (t->func == func) {
		t->func = NULL;
		atomic_set(&t->count, 0);
	} else {
		struct tasklet_stat *stat;
		list_for_each(pos, &t->list) {
			stat = (struct tasklet_stat *)pos;
			if (stat->func == func) {
				list_del(&stat->list);
				kfree(stat);
				return;
			}
		}
	}
}
#endif

/*
 * Number of interrupts per specific IRQ source, since bootup
 */
static inline unsigned int kstat_irqs(unsigned int irq)
{
	unsigned int sum = 0;
	int cpu;

	for_each_possible_cpu(cpu)
		sum += kstat_irqs_cpu(irq, cpu);

	return sum;
}


/*
 * Lock/unlock the current runqueue - to extract task statistics:
 */
extern unsigned long long task_delta_exec(struct task_struct *);

extern void account_user_time(struct task_struct *, cputime_t, cputime_t);
extern void account_system_time(struct task_struct *, int, cputime_t, cputime_t);
extern void account_steal_time(cputime_t);
extern void account_idle_time(cputime_t);

extern void account_process_tick(struct task_struct *, int user);
extern void account_steal_ticks(unsigned long ticks);
extern void account_idle_ticks(unsigned long ticks);

#endif /* _LINUX_KERNEL_STAT_H */
