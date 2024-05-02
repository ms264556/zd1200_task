/*
 * Copyright 2014 Ruckus Wireless, Inc. All rights reserved.
 *
 * RUCKUS WIRELESS, INC. CONFIDENTIAL -
 * This is an unpublished, proprietary work of Ruckus Wireless, Inc., and is
 * fully protected under copyright and trade secret laws. You may not view,
 * use, disclose, copy, or distribute this file or any information contained
 * herein except pursuant to a valid license from Ruckus.
 */

#ifndef __BR_RKS_PRIVATE_H__
#define __BR_RKS_PRIVATE_H__

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/list.h>
#include <linux/timer.h>



#define BR_IPDB_HASH_SIZE       (1 << 8)
#define BR_IPV6DB_HASH_SIZE     (1 << 8)


struct net_bridge;
struct net_bridge_fdb_entry;
struct net_port_vlans;

struct mac_addr
{
    unsigned char   addr[6];
};


struct arp_pkt
{
    unsigned short  ar_hrd;
    unsigned short  ar_pro;
    unsigned char   ar_hln;
    unsigned char   ar_pln;
    unsigned short  ar_op;
    unsigned char   ar_sha[6];
    unsigned int    ar_sip;
    unsigned char   ar_tha[6];
    unsigned int    ar_tip;
} __attribute__ ((packed));

#ifdef CONFIG_BRIDGE_LOOP_GUARD
struct net_bridge_mac_change
{
    unsigned long   track_window;
    int             flap_cnt;
};
#endif

#ifdef RKS_TUBE_SUPPORT
struct tube_hook {
    int    (*func)(struct net_device *dev, struct sk_buff *skb, void *opaque);
    void    *opaque;
};
#endif


static inline int ipv6_addr_is_zero(const struct in6_addr *addr)
{
    return !(addr->s6_addr32[0] | addr->s6_addr32[1] | addr->s6_addr32[2] | addr->s6_addr32[3]);
}

#ifdef CONFIG_BRIDGE_LOOP_GUARD

#define FLAP_THRESHOLD          20
#define LOOP_DETECT_WINDOW      (3*HZ / 100)    // 30 ms
#define LOOP_HOLD_JIFFIES       (HZ * 10)       // 10 seconds

extern unsigned int br_loop_hold;
#endif



struct net_device *br_net_device(struct net_device *dev);

void br_rks_prvt_setup();


extern int (*br_ipdb_get_mac_hook)(struct net_bridge *, unsigned int ,
                                   unsigned int , unsigned char **,
                                   unsigned int *);


extern const u8 br_group_address[ETH_ALEN];
int should_deliver(const struct net_bridge_port *dport, const struct sk_buff *skb);

#ifdef CONFIG_BRIDGE_VLAN
#include <linux/if_vlan.h>

#define BR_DEFAULT_VLAN 1
#define BR_ALL_VLAN     0

struct net_bridge_port_vlan
{
    int             untagged;
    int             pvid_fork_bridge;
    u8              filter[4096/8];
    u8              dvlan_mask[4096/8];
};




int br_vlan_dev_queue_xmit(struct sk_buff *skb, struct net_device *in_dev);
int br_vlan_dev_mcast_xmit(struct sk_buff *skb, struct net_device *out_dev);



int br_vlan_filter(const struct sk_buff *skb,
                   const struct net_bridge_port_vlan *vlan);
void br_vlan_init(struct net_bridge_port_vlan *vlan);



void br_fdb_foreach(struct net_bridge *br,
                    void (*callback)(struct net_bridge_fdb_entry *f, void *arg1,
                                     void *arg2, void *arg3, void *arg4),
                    void *arg1, void *arg2, void *arg3, void *arg4);

void br_vlan_pull_8021q_header(struct sk_buff *skb);
int br_vlan_input_frame(struct sk_buff *skb, struct net_bridge_port_vlan *vlan);
int br_vlan_output_frame(struct sk_buff **pskb,
                         unsigned int untagged, unsigned int dvlan);

int br_vlan_set_dvlan(struct net_bridge *br,
                      unsigned int port, unsigned int enable);
void br_vlan_add_del_dvlan(struct net_device *dev, unsigned short vid, int add);

int br_vlan_set_untagged(struct net_bridge *br,
                         unsigned int port, unsigned int vid);
int br_vlan_set_tagged(struct net_bridge *br,
                       unsigned int port, unsigned int vid);
int br_vlan_set_filter(struct net_bridge *br,
                       unsigned int cmd, unsigned int port,
                       unsigned int vid1, unsigned int vid2);

int br_vlan_get_untagged(struct sk_buff *skb);
int br_vlan_get_info(struct net_bridge *br,
                     void *user_mem, unsigned long port);

int br_vlan_set_vlan_fork_br(struct net_bridge *br,
                             unsigned int port, unsigned int enable);
int br_vlan_set_pvid_fork_br(struct net_bridge *br,
                             unsigned int port, unsigned int vid);



int br_vlan_pass_frame_up(struct sk_buff **pskb, unsigned int untagged);


bool br_is_pvid_set(struct net_bridge_port *p);

#define LIMIT_PRINTK(fmt, args...) \
    if (net_ratelimit()) {printk(fmt, ## args);}


#else /* CONFIG_BRIDGE_VLAN */


#define LIMIT_PRINTK(...)

#define br_vlan_filter(skb, vlan)               (0)
#define br_vlan_input_frame(skb, vlan)          (0)
#define br_vlan_output_frame(pskb, untagged)    (0)
#define br_vlan_init(vlan)                      do {} while(0)
#endif /* CONFIG_BRIDGE_VLAN */

struct net_bridge_fdb_entry *__br_fdb_get_no_check_dst_brport(struct net_bridge *br,
#ifdef CONFIG_BRIDGE_VLAN
                                                              unsigned int vlan_id,
#endif
                                                              const unsigned char *addr);

#ifdef CONFIG_BRIDGE_LOOP_GUARD
int br_loop_detected(struct net_bridge *br, const unsigned char *addr);
#endif




#define __BR_FDB_GET(br, addr, vid) __br_fdb_get((br), (vid), (addr))

#endif /* __BR_RKS_PRIVATE_H__ */
