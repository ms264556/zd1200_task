#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#if 1 /* V54_BSP */
#include <asm/uaccess.h>
#endif

#if 1 /* V54_BSP */
extern int MAX_SOFTIRQ_RESTART;

/* *grunt* We have to do atoi ourselves :-( */
static int softirqs_atoi(const char *s)
{
        int k = 0;

        k = 0;
        while (*s != '\0' && *s >= '0' && *s <= '9') {
                k = 10 * k + (*s - '0');
                s++;
        }
        return k;
}
#endif

/*
 * /proc/softirqs  ... display the number of softirqs
 */
static int show_softirqs(struct seq_file *p, void *v)
{
	int i, j;

#if 1 /* V54_BSP */
	seq_printf(p, "MAX_SOFTIRQ_RESTART=%d\n", MAX_SOFTIRQ_RESTART);

	seq_printf(p, " Softirq        ");
#else
	seq_printf(p, "                ");
#endif
	for_each_possible_cpu(i)
		seq_printf(p, "CPU%-8d", i);
	seq_printf(p, "\n");

	for (i = 0; i < NR_SOFTIRQS; i++) {
		seq_printf(p, "%8s:", softirq_to_name[i]);
		for_each_possible_cpu(j)
			seq_printf(p, " %10u", kstat_softirqs_cpu(i, j));
		seq_printf(p, "\n");
	}
	return 0;
}

static int softirqs_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_softirqs, NULL);
}

#if 1 /* V54_BSP */
static ssize_t
softirqs_write(struct file *file,
               const char __user * buffer, size_t count, loff_t * ppos)
{
	char val[16];
	int i;

        if (!capable(CAP_SYS_ADMIN))
                return -EACCES;
	if (count > sizeof(val))
		count = sizeof(val);
	if (copy_from_user(val, buffer, count))
		return -EFAULT;
	/* Rid of the \n at the end */
	val[count-1] = 0;

	i = softirqs_atoi(val);
	if (i > 0)
		MAX_SOFTIRQ_RESTART = i;

	return count;
}
#endif

static const struct file_operations proc_softirqs_operations = {
	.open		= softirqs_open,
	.read		= seq_read,
#if 1 /* V54_BSP */
	.write		= softirqs_write,
#endif
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_softirqs_init(void)
{
	proc_create("softirqs", 0, NULL, &proc_softirqs_operations);
	return 0;
}
module_init(proc_softirqs_init);
