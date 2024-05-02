/*
 * Copyright (c), 2005-2011, Ruckus Wireless, Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <ruckus/rks_ioctl.h>
#include <linux/list.h>
#include <linux/version.h>

#include <linux/rculist.h>

#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/rcupdate.h>
#include <asm/uaccess.h>

struct rks_ioctl_handler_entry
{
	struct list_head    list;
	unsigned int        module;
	int                 (*handler)(unsigned int, unsigned int, void *);
};

static DEFINE_SPINLOCK(rks_ioctl_lock);
static struct list_head rks_ioctl_handlers __read_mostly = LIST_HEAD_INIT(rks_ioctl_handlers);

int rks_ioctl_register(unsigned int module, int (*handler)(unsigned int, unsigned int, void *))
{
	int rv = 0;
	struct rks_ioctl_handler_entry *pos, *entry;

	if (NULL == (entry = kmalloc(sizeof(*entry), GFP_KERNEL))) {
		return -ENOMEM;
	}
	entry->module = module;
	entry->handler = handler;

	spin_lock(&rks_ioctl_lock);
	list_for_each_entry_rcu(pos, &rks_ioctl_handlers, list) {
		if (pos->module == module) {
			rv = -EEXIST;
			break;
		}
	}
	if (likely(0 == rv))
		list_add_rcu(&entry->list, &rks_ioctl_handlers);
	spin_unlock(&rks_ioctl_lock);

	if (unlikely(0 != rv))
		kfree(entry);
	return rv;
}
EXPORT_SYMBOL(rks_ioctl_register);

int rks_ioctl_deregister(unsigned int module)
{
	struct rks_ioctl_handler_entry *pos, *entry = NULL;

	spin_lock(&rks_ioctl_lock);
	list_for_each_entry_rcu(pos, &rks_ioctl_handlers, list) {
		if (pos->module == module) {
			entry = pos;
			list_del_rcu(&entry->list);
			break;
		}
	}
	spin_unlock(&rks_ioctl_lock);

	if (unlikely(NULL == entry))
		return -ENOENT;

	might_sleep();
	synchronize_rcu();
	kfree(entry);
	return 0;
}
EXPORT_SYMBOL(rks_ioctl_deregister);

int rks_ioctl(void __user *arg)
{
	int rv = 0;
	struct rks_ioctl_req req;
	struct rks_ioctl_handler_entry *entry;
	unsigned char buf[512];
	unsigned char *data = buf;

	if (copy_from_user(&req, arg, sizeof(req))) {
		rv = -EFAULT;
		goto quit;
	}

	if (unlikely(req.length > sizeof(buf))) {
		if (NULL == (data = kmalloc(req.length, GFP_KERNEL))) {
			rv = -ENOMEM;
			goto quit;
		}
	}

	if (copy_from_user(data, req.data, req.length)) {
		rv = -EFAULT;
		goto quit;
	}

	list_for_each_entry_rcu(entry, &rks_ioctl_handlers, list) {
		if (entry->module == req.module) {
			rv = entry->handler(req.cmd, req.length, (void *)data);
			goto quit;
		}
	}

	printk(KERN_WARNING "rks_ioctl: module %u cmd %u not supported\n", req.module, req.cmd);
	rv = -ENOIOCTLCMD;

quit:
	if ((buf != data) && (NULL != data))
		kfree(data);
	return rv;
}
