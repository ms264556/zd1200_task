/*
 * Copyright 2014 Ruckus Wireless, Inc. All rights reserved.
 *
 * RUCKUS WIRELESS, INC. CONFIDENTIAL -
 * This is an unpublished, proprietary work of Ruckus Wireless, Inc., and is
 * fully protected under copyright and trade secret laws. You may not view,
 * use, disclose, copy, or distribute this file or any information contained
 * herein except pursuant to a valid license from Ruckus.
 */

#ifndef __RKS_IF_BRIDGE_H__
#define __RKS_IF_BRIDGE_H__

#ifdef __KERNEL__
#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/in.h>
#include <linux/in6.h>
#else
#include <netinet/in.h>
#endif

#ifndef IFNAMSIZ
#define	IFNAMSIZ	16
#endif
#define RKS_WIF_STR           "wlan"
#define RKS_ETH_STR           "eth"
#define RKS_MAX_WIF            64
#define RKS_MAX_EIF            8
#define RKS_MAX_IFCOUNT       (RKS_MAX_WIF + RKS_MAX_EIF)
#define RKS_ETH_BIT_OFFSET     8
#define RKS_MAX_IFCOUNT_BYTE   9
#ifndef CHAR_BIT
#define CHAR_BIT               8
#endif /* CHAR_BIT */

struct req_br_rks_ioctl {
	char            brname[IFNAMSIZ];
	unsigned long   args[3];
};



enum {
    BRCTL_MAX = 99,
    BR_IOCTL_SET_PORT_UNTAGGED_VLAN,
    BR_IOCTL_ADD_PORT_VLAN,
    BR_IOCTL_DEL_PORT_VLAN,
    BR_IOCTL_GET_PORT_VLAN_INFO,
    BR_IOCTL_SET_PORT_TAGGED_VLAN,
    BR_IOCTL_SET_PORT_DVLAN,

    BR_IOCTL_GET_PORT_ATTRIB_FOR_MAC,
    BR_IOCTL_SET_PORT_MODE,
    BR_IOCTL_SET_PORT_STP_FORWARD_DROP,
    BR_IOCTL_SET_BRIDGE_EAPOL_FILTER,
    BR_IOCTL_SET_BRIDGE_DESIRE_MTU,
    BR_IOCTL_SET_PORT_VLAN_FORK_BRIDGE,
    BR_IOCTL_SET_PORT_PVID_FORK_BRIDGE,
};
enum {
	RKS_MCAST_ACTION_ALLOW = 0,
	RKS_MCAST_ACTION_DROP,
	RKS_MCAST_ACTION_UL_RL,
	RKS_MCAST_ACTION_DL_RL
};

enum {
        RKS_MCAST_IGNORE_RL = 0,
        RKS_MCAST_APPLY_RL
};
enum {
    PORT_TYPE_MIN    = 0,
    PORT_TYPE_ACCESS = 0,
    PORT_TYPE_TRUNK,
    PORT_TYPE_GENERAL,
    PORT_TYPE_MAX    = PORT_TYPE_GENERAL,
};

#if 1 || defined(CONFIG_BRIDGE_PORT_MODE)
#define BR_PORT_MODE_ALL                     0
#define BR_PORT_MODE_NNI                     0
#define BR_PORT_MODE_HOST_ONLY               1
#define BR_PORT_MODE_UUNI                    2
#define BR_PORT_MODE_RUNI                    3
#define BR_PORT_MODE_MAX BR_PORT_MODE_RUNI
#endif







#ifdef __KERNEL__

struct net_bridge_port;


static inline int is_bridge_port(struct net_device *dev)
{
	return (NULL != dev->br_port);
}



extern int br_vlan_handler_for_fastpath(struct sk_buff **pskb);






int br_rks_ioctl(unsigned int cmd, unsigned int length, void *data);

extern void *br_attached(const struct net_device *dev);
extern struct net_device *br_fdb_dev_get(const struct net_device *br_dev, const unsigned char *mac, __u16 vlan, int *tagging);
extern struct net_device *br_fdb_dev_get_and_hold(const struct net_device *br_dev, const unsigned char *mac, __u16 vlan, int *tagging);

extern struct net_device *br_net_device(struct net_device *dev);

#endif /* __KERNEL__ */

#endif /* __RKS_IF_BRIDGE_H__ */
