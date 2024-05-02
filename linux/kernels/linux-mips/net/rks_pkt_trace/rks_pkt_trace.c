/*
 * Copyright 2017 Ruckus Wireless, Inc. All rights reserved.
 *
 * RUCKUS WIRELESS, INC. CONFIDENTIAL -
 * This is an unpublished, proprietary work of Ruckus Wireless, Inc., and is
 * fully protected under copyright and trade secret laws. You may not view,
 * use, disclose, copy, or distribute this file or any information contained
 * herein except pursuant to a valid license from Ruckus.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/etherdevice.h>
#include <linux/if_arp.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <linux/skbuff.h>
#include <asm/unaligned.h>
#include <ruckus/rks_utility.h>
#include <ruckus/rks_pkt_trace.h>

struct pkt_trace_info_s {
    unsigned int log_len;
    char log[0];
};

int pkt_trace_enable = 0;
struct net_device *tif;

#define PKT_TRACE_BUF_SIZE   1024
#define MAX_PKT_TRACE_LOG_LEN    (PKT_TRACE_BUF_SIZE - sizeof(struct pkt_trace_info_s))

void rks_pkt_trace_log(struct sk_buff *skb, const char *fmt, ...)
{
    struct pkt_trace_info_s *trace_info;
    va_list args;

    if (!pkt_trace_enable || !skb)
        return;

    if (!skb->pkt_trace_log) {
        skb->pkt_trace_log = kzalloc(PKT_TRACE_BUF_SIZE, GFP_ATOMIC);
        if (!skb->pkt_trace_log) {
            printk(KERN_ERR "%s: failed to allocate pkt trace buffer\n", __FUNCTION__);
            return;
        }
    }

    trace_info = (struct pkt_trace_info_s *)skb->pkt_trace_log;
    if (trace_info->log_len >= MAX_PKT_TRACE_LOG_LEN)
        return;

    va_start(args, fmt);
    trace_info->log_len += vsnprintf(trace_info->log + trace_info->log_len, MAX_PKT_TRACE_LOG_LEN - trace_info->log_len, fmt, args);
    va_end(args);

    if (trace_info->log_len > MAX_PKT_TRACE_LOG_LEN)
        trace_info->log_len = MAX_PKT_TRACE_LOG_LEN;
}

void rks_pkt_trace_copy(struct sk_buff *new, const struct sk_buff *old)
{
    struct pkt_trace_info_s *trace_info;

    if (!old->pkt_trace_log)
        return;

    trace_info = (struct pkt_trace_info_s *)old->pkt_trace_log;
    if (trace_info->log_len > MAX_PKT_TRACE_LOG_LEN)
        return;

    if (new->pkt_trace_log) {
        kfree(new->pkt_trace_log);
        new->pkt_trace_log = NULL;
    }

    new->pkt_trace_log = kzalloc(PKT_TRACE_BUF_SIZE, GFP_ATOMIC);
    if (!new->pkt_trace_log) {
        printk(KERN_ERR "%s: failed to allocate pkt trace buffer\n", __FUNCTION__);
        return;
    }

    memcpy(new->pkt_trace_log, old->pkt_trace_log, trace_info->log_len + sizeof(struct pkt_trace_info_s));
}

static struct sk_buff *new_skb_with_log_after_data(struct sk_buff *skb)
{
    struct sk_buff *new_skb = NULL;
    struct pkt_trace_info_s *trace_info;
    char *log_buf, *magic;
    int *log_len_field;
    unsigned int log_len_be;


    trace_info = (struct pkt_trace_info_s *)skb->pkt_trace_log;
    if (trace_info->log_len > MAX_PKT_TRACE_LOG_LEN)
        return NULL;

    skb->pkt_trace_log = NULL;

    new_skb = skb_copy_expand(skb, skb_headroom(skb), trace_info->log_len + 8, GFP_ATOMIC);
    skb->pkt_trace_log = (char *)trace_info;
    if (unlikely(new_skb == NULL)) {
       printk(KERN_ERR "%s failed to alloc skb.\n", __func__);
       return NULL;
    }

    if(unlikely(skb_is_nonlinear(new_skb))) {
        if (unlikely(__skb_linearize(new_skb))) {
            printk(KERN_ERR "skb(%p) linerize failed.\n", new_skb);
            goto error;
        }
    }

    log_buf = skb_tail_pointer(new_skb);
    memcpy(log_buf, trace_info->log, trace_info->log_len);

    log_len_field = (int *)(log_buf + trace_info->log_len);
    log_len_be = htonl(trace_info->log_len);
    put_unaligned (log_len_be, log_len_field);

    magic = (char *)log_len_field + 4;
    magic[0] = 'R';
    magic[1] = 'K';
    magic[2] = 'U';
    magic[3] = 'S';

    skb_put(new_skb, trace_info->log_len + 8);

    if (skb_mac_header(new_skb) < new_skb->head || skb_mac_header(new_skb) > new_skb->tail) {
        if (net_ratelimit())
            printk(KERN_WARNING "captured bad pkt? protocol:%04x, dev:%s, mac header:%p, skb->data:%p, skb->head:%p, skb->tail:%p\n",
                   ntohs(new_skb->protocol), new_skb->dev ? new_skb->dev->name : "(null)", skb_mac_header(new_skb), new_skb->data, new_skb->head, new_skb->tail);
        skb_reset_mac_header(new_skb);
    }

    if (skb_mac_header(new_skb) < new_skb->data)
        skb_push(new_skb, new_skb->data - skb_mac_header(new_skb));
    else
        skb_pull(new_skb, skb_mac_header(new_skb) - new_skb->data);

    return new_skb;

error:
    if (new_skb)
        dev_kfree_skb_any(new_skb);
    return NULL;
}

static inline deliver_skb_2_tif(struct sk_buff *skb,
                  struct packet_type *pt_prev,
                  struct net_device *orig_dev)
{
    struct sk_buff *new_skb;

    if (unlikely(!skb_mac_header_was_set(skb))) {
        tif->stats.rx_dropped++;
        return;
    }

    if (skb->data > skb->tail || skb->tail > skb->end || skb->data < skb->head) {
        if (net_ratelimit())
            printk(KERN_WARNING "captured bad pkt: protocol %04x, skb->data:%p, skb->tail:%p, skb->head:%p, skb->end:%p\n",
                   ntohs(skb->protocol), skb->data, skb->tail, skb->head, skb->end);
        tif->stats.rx_dropped++;
        return;
    }

    new_skb = new_skb_with_log_after_data(skb);
    if (new_skb) {
        unsigned long flags;
        tif->stats.rx_packets++;

        if (!new_skb->dev)
            new_skb->dev = tif;
         local_irq_save(flags);
        pt_prev->func(new_skb, tif, pt_prev, orig_dev);
        local_irq_restore(flags);
    }

    return;
}

extern struct list_head *rks_ptype_all(void);
void rks_pkt_trace(struct sk_buff *skb)
{
    struct packet_type *ptype;
    struct list_head *ptype_all;

    if (!skb->pkt_trace_log)
        return;

    ptype_all = rks_ptype_all();
    if (list_empty(ptype_all))
        goto free_buf;

    rcu_read_lock();
    list_for_each_entry_rcu(ptype, ptype_all, list) {
        if (ptype->dev == tif)
            deliver_skb_2_tif(skb, ptype, skb->dev ? skb->dev : tif);
    }
    rcu_read_unlock();

free_buf:
    kfree(skb->pkt_trace_log);
    skb->pkt_trace_log = NULL;
}

void tif_destructor (struct net_device *dev)
{
}

void tif_uninit (struct net_device *dev)
{
  dev_put(dev);
}

int tif_init (struct net_device *dev)
{

  dev->type             = ARPHRD_ETHER;
  dev->hard_header_len  = ETH_HLEN;
  dev->mtu              = 3000;
  dev->flags            = IFF_BROADCAST | IFF_MULTICAST;
  dev->iflink           = 0;
  dev->addr_len         = 6;
  dev->tx_queue_len     = 0;
  memset(dev->broadcast, 0xff, ETH_ALEN);
  dev->destructor       = tif_destructor;
  random_ether_addr(dev->dev_addr);

  return(0);
}

int tif_open (struct net_device *dev)
{
  return(0);
}


int tif_close (struct net_device *dev)
{
  return(0);
}

int tif_xmit (struct sk_buff *skb, struct net_device *dev)
{
    dev_kfree_skb(skb);
    tif->stats.tx_dropped++;
    return NETDEV_TX_OK;
}

static const struct net_device_ops tif_netdev_ops = {
  .ndo_uninit           = tif_uninit,
  .ndo_start_xmit       = tif_xmit,
  .ndo_get_stats        = NULL,
  .ndo_do_ioctl         = NULL,
  .ndo_change_mtu       = NULL,
  .ndo_open             = tif_open,
  .ndo_stop             = tif_close,
  .ndo_init             = tif_init,
};

static void tif_alloc_setup (struct net_device *dev)
{
  dev->netdev_ops = &tif_netdev_ops;
  return;
}


#define PKT_TRACE_IF_NAME "tif0"
int create_tif(void)
{
  struct net_device *dev;
  int               err = 0;

  dev = alloc_netdev(0, PKT_TRACE_IF_NAME,
               tif_alloc_setup);

  if (dev == NULL) {
      return (-ENOMEM);
  }

  rtnl_lock();

  if (__dev_get_by_name(&init_net, dev->name)) {
    err = -EEXIST;
    goto failed;
  }

  if ((err = register_netdevice(dev)) < 0) {
    goto failed;
  }

  if ((err = dev_open(dev))) {
    (void)unregister_netdevice(dev);
    goto failed;
  }

  rtnl_unlock();
  tif = dev;
  return(0);

failed:
  rtnl_unlock();
  free_netdev(dev);

  return(err);
}



static ssize_t pkt_trace_debug_read_proc(struct file *filp, char __user *buffer,
			size_t count, loff_t *pos)
{
	int len=0;

	char buf[256];
	len = snprintf(buf, sizeof(buf), "%d\n", pkt_trace_enable);
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

	return len;
}


static int pkt_trace_debug_write_proc(struct file* file, const char* buffer,
                             unsigned long count, void *data)
{
    int len=count;
    char buf[5] = {0};

    if ( count < 1 ) {
        printk(KERN_ERR "%s: no data for write", __FUNCTION__);
        len = 0;
        goto finish;
    }

    if ( len > sizeof(buf) ) len = sizeof(buf);
    if ( copy_from_user(buf, buffer, len) ) {
        len = -EFAULT;
        goto finish;
    }
    buf[sizeof(buf) - 1] = '\0';

    sscanf(buf, "%d", &pkt_trace_enable);

    len = count;
 finish:
    return len;
}



static struct file_operations fops_pkt_trace_enable;
void pkt_trace_init_proc(void)
{
    struct proc_dir_entry *entry;

    entry = rks_create_rdwr_proc(&fops_pkt_trace_enable, "pkt_trace_enable", pkt_trace_debug_read_proc,
                                 pkt_trace_debug_write_proc, NULL, NULL, init_net.proc_net);
}

void pkt_trace_cleanup_proc(void)
{
    remove_proc_entry("pkt_trace_enable", init_net.proc_net);
}

static int __init rks_pkt_trace_init(void)
{
    if (create_tif())
        printk(KERN_ERR "%s failed to create tif0.\n", __func__);
    else
        printk(KERN_ERR "%s create tif0 successfully.\n", __func__);

    pkt_trace_init_proc();
    return 0;
}
late_initcall(rks_pkt_trace_init);

EXPORT_SYMBOL(pkt_trace_enable);
EXPORT_SYMBOL(rks_pkt_trace_log);
EXPORT_SYMBOL(rks_pkt_trace_copy);
EXPORT_SYMBOL(rks_pkt_trace);
