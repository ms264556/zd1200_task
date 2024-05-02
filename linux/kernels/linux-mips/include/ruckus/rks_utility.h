/*
 * Copyright 2014 Ruckus Wireless, Inc. All rights reserved.
 *
 * RUCKUS WIRELESS, INC. CONFIDENTIAL -
 * This is an unpublished, proprietary work of Ruckus Wireless, Inc., and is
 * fully protected under copyright and trade secret laws. You may not view,
 * use, disclose, copy, or distribute this file or any information contained
 * herein except pursuant to a valid license from Ruckus.
 */
#ifndef __RKS_UTILITY_H__
#define __RKS_UTILITY_H__

#include <linux/types.h>
#ifdef __KERNEL__
#include <linux/version.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>


struct netlink_kernel_cfg {
        unsigned int    groups;
        unsigned int    flags;
        void            (*input)(struct sk_buff *skb);
        struct mutex    *cb_mutex;
        void            (*bind)(int group);
};

#endif
#ifdef __KERNEL__
#include <linux/in.h>
#include <linux/in6.h>
#else
#include <netinet/in.h>
#endif

extern unsigned int   tx_tot_radio1; // total tx from radio1
extern unsigned int   tx_tot_radio2; // total tx from radio2

#ifndef FMT_MAC
#define FMT_MAC         "%02x:%02x:%02x:%02x:%02x:%02x"
#define FMT_MAC_U       "%02X:%02X:%02X:%02X:%02X:%02X"
#define FMT_MAC_HYPHEN   "%02x-%02x-%02x-%02x-%02x-%02x"
#define FMT_MAC_HYPHEN_U "%02X-%02X-%02X-%02X-%02X-%02X"
#define FMT_MAC_NODELIMITER   "%02x%02x%02x%02x%02x%02x"
#define FMT_MAC_NODELIMITER_U "%02X%02X%02X%02X%02X%02X"
#define ARG_MAC(ptr)    ((unsigned char *)(ptr))[0], \
                        ((unsigned char *)(ptr))[1], \
                        ((unsigned char *)(ptr))[2], \
                        ((unsigned char *)(ptr))[3], \
                        ((unsigned char *)(ptr))[4], \
                        ((unsigned char *)(ptr))[5]
#endif

#ifndef FMT_IPV4
#define FMT_IPV4        "%u.%u.%u.%u"
#define ARG_IPV4(ptr)   ((unsigned char *)(ptr))[0], \
                        ((unsigned char *)(ptr))[1], \
                        ((unsigned char *)(ptr))[2], \
                        ((unsigned char *)(ptr))[3]
#endif

#ifndef FMT_IPV6
#define FMT_IPV6        "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x"
#define ARG_IPV6(ptr)   ((unsigned char *)(ptr))[ 0], \
                        ((unsigned char *)(ptr))[ 1], \
                        ((unsigned char *)(ptr))[ 2], \
                        ((unsigned char *)(ptr))[ 3], \
                        ((unsigned char *)(ptr))[ 4], \
                        ((unsigned char *)(ptr))[ 5], \
                        ((unsigned char *)(ptr))[ 6], \
                        ((unsigned char *)(ptr))[ 7], \
                        ((unsigned char *)(ptr))[ 8], \
                        ((unsigned char *)(ptr))[ 9], \
                        ((unsigned char *)(ptr))[10], \
                        ((unsigned char *)(ptr))[11], \
                        ((unsigned char *)(ptr))[12], \
                        ((unsigned char *)(ptr))[13], \
                        ((unsigned char *)(ptr))[14], \
                        ((unsigned char *)(ptr))[15]
#endif

union union_sockaddr {
	struct sockaddr     s;
	struct sockaddr_in  sin;
	struct sockaddr_in6 sin6;
};

union union_in_addr {
	struct in_addr  v4;
	struct in6_addr v6;
};

static inline void rks_set_bit(unsigned char *bits, unsigned int idx)
{
	bits[idx >> 3] |= (1 << (idx & 7));
}

static inline int rks_bit_is_set(const unsigned char *bits, unsigned int idx)
{
	return (bits[idx >> 3] & (1 << (idx & 7))) ? 1 : 0;
}

#ifdef __KERNEL__
#include <linux/seq_file.h>
#include <linux/proc_fs.h>

void rks_dump_memory(void *mem, size_t len);

int rks_get_nexthop(unsigned short family, const union union_in_addr *p_daddr, int query,
    struct dst_entry **p_dst, struct net_device **p_xmitdev, unsigned int *p_pmtu,
    union union_in_addr *p_saddr, union union_in_addr *p_nexthopaddr,
    __u8 *p_smac, __u8 *p_nexthopmac);


static ssize_t noop_read(struct file *file, char __user *buf,
			size_t count, loff_t *pos)
{
	return 0;
}

#define MODE_READ   0444
#define MODE_WRITE  0222
#define MODE_RW     0644

static inline struct proc_dir_entry *
rks_create_read_proc(struct file_operations *proc_fops
                  , char * name
                  , ssize_t (*read) (struct file *, char __user *, size_t, loff_t *)
                  , int (*open) (struct inode *, struct file *)
                  , void *data
                  , struct proc_dir_entry *parent
                  )
{
	struct proc_dir_entry *entry = NULL;

	entry = proc_create_data(name, MODE_READ, parent, proc_fops, data);
	if (NULL == entry)
		return NULL;

	if (open) {
		proc_fops->open = open;
		proc_fops->read = seq_read;
		proc_fops->llseek = seq_lseek;
		proc_fops->release = single_release;
	} else {
		proc_fops->read = read;
		proc_fops->llseek = default_llseek;
	}

	return entry;
}

static inline struct proc_dir_entry *
rks_create_write_proc(struct file_operations *proc_fops
                  , char * name
                  , ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *)
                  , void *data
                  , struct proc_dir_entry *parent
                  )
{
	struct proc_dir_entry *entry = NULL;

	entry = proc_create_data(name, MODE_WRITE, parent, proc_fops, data);
	if (NULL == entry)
		return NULL;

	proc_fops->write = write;
	proc_fops->llseek = noop_llseek;
	proc_fops->read = noop_read;

	return entry;
}

static inline struct proc_dir_entry *
rks_create_rdwr_proc(struct file_operations *proc_fops
                  , char *name
                  , ssize_t (*read) (struct file *, char __user *, size_t, loff_t *)
                  , ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *)
                  , int (*open) (struct inode *, struct file *)
                  , void *data
                  , struct proc_dir_entry *parent
                  )
{
	struct proc_dir_entry *entry = NULL;

	entry = proc_create_data(name, MODE_RW, parent, proc_fops, data);
	if (NULL == entry)
		return NULL;

	proc_fops->write = write;
	if (open) {
		proc_fops->open = open;
		proc_fops->read = seq_read;
		proc_fops->llseek = seq_lseek;
		proc_fops->release = single_release;
	} else {
		proc_fops->read = read ? read : noop_read;
		proc_fops->llseek = default_llseek;
	}

	return entry;
}
static inline struct proc_dir_entry *
rks_create_dir_proc(struct file_operations *proc_fops
                  , char *name
                  , ssize_t (*read) (struct file *, char __user *, size_t, loff_t *)
                  , ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *)
                  , int (*open) (struct inode *, struct file *)
                  , void *data
                  , struct proc_dir_entry *parent
                  )
{
	struct proc_dir_entry *entry = NULL;

	entry = proc_create_data(name, S_IFDIR, parent, proc_fops, data);
	if (NULL == entry)
		return NULL;

	proc_fops->write = write;
	if (open) {
		proc_fops->open = open;
		proc_fops->read = seq_read;
		proc_fops->llseek = seq_lseek;
		proc_fops->release = single_release;
	} else {
		proc_fops->read = read ? read : noop_read;
		proc_fops->llseek = default_llseek;
	}

	return entry;
}

static inline struct proc_dir_entry *
rks_create_reg_proc(struct file_operations *proc_fops
                  , char *name
                  , ssize_t (*read) (struct file *, char __user *, size_t, loff_t *)
                  , ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *)
                  , int (*open) (struct inode *, struct file *)
                  , void *data
                  , struct proc_dir_entry *parent
                  )
{
	struct proc_dir_entry *entry = NULL;

	entry = proc_create_data(name, S_IFDIR, parent, proc_fops, data);
	if (NULL == entry)
		return NULL;

	proc_fops->write = write;
	if (open) {
		proc_fops->open = open;
		proc_fops->read = seq_read;
		proc_fops->llseek = seq_lseek;
		proc_fops->release = single_release;
	} else {
		proc_fops->read = read ? read : noop_read;
		proc_fops->llseek = default_llseek;
	}

	return entry;
}

static inline struct sock *
rks_netlink_kernel_create(struct net *net
                          , int unit
                          , struct netlink_kernel_cfg *cfg
                          , struct mutex *cb_mutex
                          , void (*input)(struct sk_buff *skb)
                          , unsigned int groups)
{
	struct sock *sk;
	sk = netlink_kernel_create(net,unit,groups,cfg->input,cb_mutex,THIS_MODULE);
	return sk;
}

#endif


extern int rks_deliver_skb(struct sk_buff *skb,
			   struct packet_type *pt_prev,
			   struct net_device *orig_dev);
extern struct list_head *rks_ptype_all(void);
#endif /* __RKS_UTILITY_H__ */
