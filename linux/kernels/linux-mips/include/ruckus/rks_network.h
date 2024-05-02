/*
 * Copyright (c), 2005-2015, Ruckus Wireless, Inc.
 * Ruckus related network definitions
 */

#ifndef __RKS_NETWORK_H__
#define __RKS_NETWORK_H__

#ifdef __KERNEL__
#include "rks_api_ver.h"

static inline int is_lag_master(struct net_device *dev)
{
	return (dev->flags & IFF_MASTER) && (dev->priv_flags & IFF_BONDING);
}

static inline int is_lag_slave(struct net_device *dev)
{
	return (dev->flags & IFF_SLAVE) && (dev->priv_flags & IFF_BONDING);
}

static inline struct net_device* netif_master_dev(struct net_device *dev)
{
	return is_lag_slave(dev) ? RKS_MASTER_DEV(dev) : dev;
}




extern int rks_deliver_skb(struct sk_buff *skb,
			   struct packet_type *pt_prev,
			   struct net_device *orig_dev);
extern struct list_head *rks_ptype_all(void);

extern void netif_rx_capture_for_fastpath(struct sk_buff *skb);
extern int netif_rx_no_backlog(struct sk_buff *skb);
extern int netif_rx_rks_timestamp(struct sk_buff *skb, __u16 id);

#endif


#endif /* __RKS_NETWORK_H__ */
