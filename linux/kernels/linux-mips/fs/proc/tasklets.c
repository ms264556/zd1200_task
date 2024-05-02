#include <linux/module.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/kernel_stat.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <asm/uaccess.h>

int kstat_do_tasklets = 0;
EXPORT_SYMBOL(kstat_do_tasklets);

static char *left_aligned_str(char *str, int width,
                              char *buf, int buflen)
{
	int i, len;

	memset(buf, 0, buflen);

	if (width >= buflen)
		width = buflen - 1;

	len = snprintf(buf, buflen-1, "%s", str);
	if (len < 0)
		return buf;

	for (i=len; i<width; i++) {
		snprintf(buf+i, buflen-i, " ");
	}
	buf[i] = 0;

	return buf;
}

#define FUNCNAME_WIDTH    36

/*
 * /proc/tasklets  ... display the number of tasklets
 */
static int show_tasklets(struct seq_file *p, void *v)
{
	int i, h;
	struct list_head *pos;
	struct tasklet_stat *t, *stat;
	char *str, name[64], buf[128];

	seq_printf(p, "Stats: %s\n",
	           kstat_do_tasklets_get() ? "enabled" : "disabled");

	for_each_possible_cpu(i) {
		str = left_aligned_str("Tasklet", FUNCNAME_WIDTH, buf, sizeof(buf));
		seq_printf(p, "%s CPU%-8d\n", str, i);

		for (h=0; h<ARRAY_SIZE(kstat_cpu(i).tasklets); h++) {
			t = &kstat_cpu(i).tasklets[h];
			if (!t->func)
				continue;
			kstat_func2name(t->func, name, sizeof(name));
			str = left_aligned_str(name, FUNCNAME_WIDTH, buf, sizeof(buf));
			seq_printf(p, "%s %d\n", str, atomic_read(&t->count));
			list_for_each(pos, &t->list) {
				stat = (struct tasklet_stat *)pos;
				kstat_func2name(stat->func, name, sizeof(name));
				str = left_aligned_str(name, FUNCNAME_WIDTH, buf, sizeof(buf));
				seq_printf(p, "%s %d\n", str, atomic_read(&stat->count));
			}
		}
	}

	return 0;
}

static int tasklets_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_tasklets, NULL);
}

static ssize_t
tasklets_write(struct file *file,
               const char __user * buffer, size_t count, loff_t * ppos)
{
	char enable[16];

        if (!capable(CAP_SYS_ADMIN))
                return -EACCES;
	if (count > sizeof(enable))
		count = sizeof(enable);
	if (copy_from_user(enable, buffer, count))
		return -EFAULT;
	/* Rid of the \n at the end */
	enable[count-1] = 0;

	kstat_do_tasklets_set(enable[0] == '0' ? 0 : 1);

	return count;
}

static const struct file_operations proc_tasklets_operations = {
	.open		= tasklets_open,
	.read		= seq_read,
	.write		= tasklets_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_tasklets_init(void)
{
	struct proc_dir_entry *res;

	res = create_proc_entry("tasklets", 0, NULL);
	if (res)
		res->proc_fops = (struct file_operations *)&proc_tasklets_operations;
	return 0;
}

module_init(proc_tasklets_init);
