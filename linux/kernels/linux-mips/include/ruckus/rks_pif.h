/*
 * Copyright 2014 Ruckus Wireless, Inc. All rights reserved.
 *
 * RUCKUS WIRELESS, INC. CONFIDENTIAL -
 * This is an unpublished, proprietary work of Ruckus Wireless, Inc., and is
 * fully protected under copyright and trade secret laws. You may not view,
 * use, disclose, copy, or distribute this file or any information contained
 * herein except pursuant to a valid license from Ruckus.
 */

#ifndef __RKS_PIF_H__
#define __RKS_PIF_H__


#define PIF_IOCTL_SET_PIF                    0
#define PIF_IOCTL_SET_IF_TYPE                1
#define PIF_IOCTL_SET_DIRECTED_ARP           2
#define PIF_IOCTL_SET_IF_MODE                3
#define PIF_IOCTL_SET_RATE_LIMIT             4
#define PIF_IOCTL_SET_PROC_UPLINK            5
#define PIF_IOCTL_SET_IF_LEARNING            6
#define PIF_IOCTL_GET_IPDB_ENTRIES           7
#define PIF_IOCTL_GET_IPV6DB_ENTRIES         8
#define PIF_IOCTL_SET_IP_AGEING              9
#define PIF_IOCTL_DEL_IPDB_ENTRY_BY_MAC     10
#define PIF_IOCTL_DEL_IPV6DB_ENTRY_BY_MAC   11


enum {
    BR_PIF_IPV6_TYPE_MIN         = 0,
    BR_PIF_IPV6_TYPE_ND_PROXY    = 1,
    BR_PIF_IPV6_TYPE_NS_SUPPRESS = 2,
    BR_PIF_IPV6_TYPE_RA_PROXY    = 4,
    BR_PIF_IPV6_TYPE_RSRA_GUARD  = 8,
    BR_PIF_IPV6_TYPE_EXPERIMENTAL= 16,
    BR_PIF_IPV6_TYPE_RA_THROTTLE = 32,
    BR_PIF_IPV6_TYPE_RA_THROTTLE_PARAM = 64,
    BR_PIF_IPV6_FILTER_ALL       = 128,
    BR_PIF_IPV6_FILTER_MCAST     = 256,
    BR_PIF_IPV6_TYPE_MAX         = 512
};

union throttle_params
{
    unsigned int u32_params;
    struct _u16_params {
        unsigned short num_ra_allowed;
        unsigned short interval;
    }u16_params;
};

struct br_pif_ipv6_filter_pvt {
    uint16_t type;
    unsigned long long counter;
    int	lock;
};

#define BR_PIF_IFTYPE_DIRECTEDARP            0
#define BR_PIF_IFTYPE_PROXYARP               1
#define BR_PIF_IFTYPE_NONE                   2

#define BR_PIF_IFMODE_MIN                    0
#define BR_PIF_IFMODE_UPLINK_PORT            0
#define BR_PIF_IFMODE_DNLINK_PORT            1
#define BR_PIF_IFMODE_MAX                    1

#define BR_PIF_LEARNING_OFF                  0
#define BR_PIF_LEARNING_ALL                  1
#define BR_PIF_LEARNING_GW_ONLY              2

#ifndef IFNAMSIZ
#define	IFNAMSIZ	16
#endif

struct req_pif_update_ipdb {
    unsigned char           macaddr[6];
    unsigned short          family;
    int                     ifindex;
    unsigned short          vlanID;
    union {
        struct in_addr  v4;
        struct in6_addr v6;
    } ipaddr;
} __attribute__((packed));

struct req_pif_ioctl {
	char            brname[IFNAMSIZ];
	unsigned long   args[3];
};

#endif /* __RKS_PIF_H__ */
