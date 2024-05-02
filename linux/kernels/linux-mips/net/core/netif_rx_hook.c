/*
 * Copyright 2014 Ruckus Wireless, Inc. All rights reserved.
 *
 * RUCKUS WIRELESS, INC. CONFIDENTIAL -
 * This is an unpublished, proprietary work of Ruckus Wireless, Inc., and is
 * fully protected under copyright and trade secret laws. You may not view,
 * use, disclose, copy, or distribute this file or any information contained
 * herein except pursuant to a valid license from Ruckus.
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>


#include <ruckus/rks_network.h>

#include <linux/if_bridge.h>

#if 0
#define RKS_TIMESTAMP_START(_id) printk(KERN_INFO "RKS_TIMESTAMP_START: %s\n", _id)
#define RKS_TIMESTAMP_STOP(_id)  printk(KERN_INFO "RKS_TIMESTAMP_STOP: %s\n", _id)
#else // not def CONFIG_RKS_TIMESTAMP
#define RKS_TIMESTAMP_START(_id)
#define RKS_TIMESTAMP_STOP(_id)
#endif

#include <linux/udp.h>
#include <net/ip.h>
#include <linux/etherdevice.h>
#include <linux/if_vlan.h>





#ifdef CONFIG_BRIDGE_PACKET_INSPECTION_FILTER_MODULE
#include <ruckus/rks_if_bridge.h>
#endif



#ifdef CONFIG_BRIDGE_VLAN
extern void skb_set_vlan_info(struct sk_buff *skb);
#endif


void netif_rx_capture_for_fastpath(struct sk_buff *skb)
{
	struct list_head *ptype_all = rks_ptype_all();
	struct packet_type *ptype = NULL, *pt_prev = NULL;
	struct net_device *orig_dev = skb->dev;
	struct net_device *dev = netif_is_bond_slave(orig_dev) ? RKS_MASTER_DEV(orig_dev) : orig_dev;

	list_for_each_entry_rcu(ptype, ptype_all, list) {
		if (!ptype->dev || ptype->dev == orig_dev || ptype->dev == dev) {
			if (pt_prev)
				(void)rks_deliver_skb(skb, pt_prev, orig_dev);
			pt_prev = ptype;
		}
	}

	if (pt_prev)
		(void) rks_deliver_skb(skb, pt_prev, orig_dev);
}
EXPORT_SYMBOL(netif_rx_capture_for_fastpath);



struct sk_buff * netif_rx_hook(struct sk_buff *skb)
{
	int ret = 0;
	struct net_device_stats *stats;
	struct net_device *cfg_dev = skb->dev;

	if (!skb->input_dev) {
		skb->input_dev = skb->dev;
	}


	rcu_read_lock();

	skb_set_vlan_info(skb);


	if (br_pif_if_ingress_hook && br_pif_if_ingress_hook(skb, 0)) {
		goto drop;
	}


quit:
	rcu_read_unlock();
	return skb;

drop:
	if (NULL != (stats = get_dev_stats(skb->dev)))
		stats->rx_dropped++;
	kfree_skb(skb);
consumed:
	skb = NULL;
	goto quit;

bypass_backlog:
	netif_receive_skb(skb);
	goto consumed;
}

int netif_rx_no_backlog(struct sk_buff *skb)
{

	if ((skb = netif_rx_hook(skb)) == NULL)
		return NET_RX_DROP;


	return netif_receive_skb(skb);

}
EXPORT_SYMBOL(netif_rx_no_backlog);


int netif_rx_rks_timestamp(struct sk_buff *skb, __u16 id)
{
	int ret = 0;

	RKS_TIMESTAMP_START(id);
	ret = netif_rx(skb);
	RKS_TIMESTAMP_STOP(id);

	return ret;
}
EXPORT_SYMBOL(netif_rx_rks_timestamp);
