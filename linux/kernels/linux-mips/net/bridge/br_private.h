/*
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek		<buytenh@gnu.org>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#ifndef _BR_PRIVATE_H
#define _BR_PRIVATE_H

#include <linux/netdevice.h>
#include <linux/if_bridge.h>
#include <net/route.h>

#define BR_FDB_VLAN

#include "br_rks_private.h"



#define BR_HASH_BITS 8
#define BR_HASH_SIZE (1 << BR_HASH_BITS)

#define BR_HOLD_TIME (1*HZ)

#define BR_PORT_BITS	10
#define BR_MAX_PORTS	(1<<BR_PORT_BITS)

#define BR_VERSION	"2.3"

/* Path to usermode spanning tree program */
#define BR_STP_PROG	"/sbin/bridge-stp"
#define BR_PACKET_MARK_L2UF    0xaa

#if 1 /* V54_BSP */
#define MAC_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_ARG(x) ((u8*)(x))[0],((u8*)(x))[1],((u8*)(x))[2],((u8*)(x))[3],((u8*)(x))[4],((u8*)(x))[5]
#define IPADDR_FMT "%d.%d.%d.%d"
#define IPV6_ARG(x) ((u16*)(x))[0],((u16*)(x))[1],((u16*)(x))[2],((u16*)(x))[3],((u16*)(x))[4],((u16*)(x))[5],((u16*)(x))[6],((u16*)(x))[7]
#define IPV6ADDR_FMT "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x"
#define IPV6ADDR_EQUAL(x,y) (((x[0] == y[0]) && (x[1] == y[1]) && (x[2] == y[2]) && (x[3] == y[3])) ? 1 : 0)
//#define IPV6_ALEN    16
#endif






typedef struct bridge_id bridge_id;
typedef struct mac_addr mac_addr;
typedef __u16 port_id;



struct bridge_id
{
	unsigned char	prio[2];
	unsigned char	addr[6];
};


struct ra_cache_info
{
    unsigned char   *router_mac;
    struct in6_addr *router_ip6addr;
    unsigned char   pref;
    unsigned short  len_ra_opt;
    unsigned char   *ra_opt;
};

#define IPV6DB_FLAG_FROM_ROUTER    (1 << 0)
#define IPV6DB_FLAG_ADDR_PROBE     (1 << 1)
#define IPV6DB_FLAG_ADDR_REACHABLE (1 << 2)
#define IPV6DB_FLAG_ADDR_FROM_RUDB (1 << 3)
#define IPV6DB_FLAG_DAD_DELETE     (1 << 4)
#define RA_OPT_MAX_BUF_SIZE  536

struct ra_cached_info
{
    unsigned char  pref;
    unsigned char  rsvd;
    unsigned short len_ra_opt;
    unsigned char  ra_opt[0];
};




extern int _br_loop_hold;
#define BR_SET_LOOP_HOLD(val)		do {_br_loop_hold = val;} while(0)
#define BR_GET_LOOP_HOLD()		(_br_loop_hold)




#if 1 /* V54_BSP */

struct net_bridge_ipdb_entry
{
	struct hlist_node		hlist;
	struct rcu_head			rcu;
	atomic_t			use_count;
	struct net_bridge_port		*br_port;
	unsigned long			ageing_timer;
	unsigned int			ipaddr;
	unsigned int			vlan;
	mac_addr			mac_addr;
    unsigned int            xid;
};

struct net_bridge_ipv6db_entry
{
	struct hlist_node		hlist;
	struct rcu_head			rcu;
	atomic_t			use_count;
	struct net_bridge_port		*br_port;
	unsigned long			ageing_timer;
	unsigned int			ipv6addr[4];
	unsigned int			vlan;
	mac_addr			mac_addr;
	unsigned int            flags;
    struct ra_cached_info *ra_info;
};
#endif

struct net_bridge_fdb_entry
{
	struct hlist_node		hlist;
	struct net_bridge_port		*dst;

	struct rcu_head			rcu;
	unsigned long			ageing_timer;
	mac_addr			addr;
	unsigned char			is_local;
	unsigned char			is_static;
#ifdef CONFIG_BRIDGE_VLAN
	unsigned int			vlan_id; /* track vlan id */
#endif
#ifdef CONFIG_BRIDGE_LOOP_GUARD
	struct net_bridge_mac_change	mac_track;
#endif
};

struct net_bridge_port
{
	struct net_bridge		*br;
	struct net_device		*dev;
	struct list_head		list;

	/* STP */
	u8				priority;
	u8				state;
	u16				port_no;
	unsigned char			topology_change_ack;
	unsigned char			config_pending;
	port_id				port_id;
	port_id				designated_port;
	bridge_id			designated_root;
	bridge_id			designated_bridge;
	u32				path_cost;
	u32				designated_cost;

	struct timer_list		forward_delay_timer;
	struct timer_list		hold_timer;
	struct timer_list		message_age_timer;
	struct kobject			kobj;
	struct rcu_head			rcu;

	unsigned long 			flags;
#define BR_HAIRPIN_MODE		0x00000001
#define BR_DVLAN_PORT_MODE	0x00000002

#define BR_VLAN_FORK_BR_MODE	0x00000004



#ifdef CONFIG_BRIDGE_VLAN
	struct net_bridge_port_vlan	vlan;
#endif
#ifdef CONFIG_BRIDGE_PORT_MODE
	u32				port_mode; //packet forwarding mode for port, e.g. allow hostonly traffic.
	u8				port_ci_state;//init/learning/isolation
#endif
#if 1//def RKS_KERNEL
	u8              stp_forward_drop;
	u8              l2uf_forwarding_drop;
#endif

	u32             pif_iftype;
	u32	            pif_ifmode;
	u32             pif_learning;
	u32             pif_ipv6type;
    u16             pif_ra_throttle_count;
    u16             pif_ra_throttle_interval;


};

#if 1 /* V54_BSP */

static inline struct net_bridge_port *br_port_get_rcu(const struct net_device *dev)
{
	return dev ? rcu_dereference(dev->br_port) : NULL;
}
#endif

struct net_bridge
{
	spinlock_t			lock;
	struct list_head		port_list;
	struct net_device		*dev;
	spinlock_t			hash_lock;
	struct hlist_head		hash[BR_HASH_SIZE];
	struct list_head		age_list;
	unsigned long			feature_mask;
#ifdef CONFIG_BRIDGE_NETFILTER
	struct rtable 			fake_rtable;
#endif
	unsigned long			flags;
#define BR_SET_MAC_ADDR		0x00000001

	/* STP */
	bridge_id			designated_root;
	bridge_id			bridge_id;
	u32				root_path_cost;
	unsigned long			max_age;
	unsigned long			hello_time;
	unsigned long			forward_delay;
	unsigned long			bridge_max_age;
	unsigned long			ageing_time;
	unsigned long			bridge_hello_time;
	unsigned long			bridge_forward_delay;

	/* br dev's mtu set to this unless min mtu of ports is smaller */
	u16				des_mtu;

	u8				group_addr[ETH_ALEN];
	u16				root_port;

	enum {
		BR_NO_STP, 		/* no spanning tree */
		BR_KERNEL_STP,		/* old STP in kernel */
		BR_USER_STP,		/* new RSTP in userspace */
	} stp_enabled;

	unsigned char			topology_change;
	unsigned char			topology_change_detected;
#if 1 /* V54_BSP */
	unsigned char			eapol_filter;
	unsigned char			hotspot:1,
					unused0:7;
	unsigned char			unused1;
#endif
	struct timer_list		hello_timer;
	struct timer_list		tcn_timer;
	struct timer_list		topology_change_timer;
	struct timer_list		gc_timer;
	struct kobject			*ifobj;
#ifdef RKS_TUBE_SUPPORT
	struct tube_hook		tube;
#endif
#ifdef CONFIG_BRIDGE_BR_MODE
	int				br_mode; // bridge forwarding mode, administrative value
#endif
#ifdef CONFIG_BRIDGE_LOOP_GUARD
	struct net_bridge_mac_change	mac_track;
	unsigned long			hostonly;     // non-zero means end time (jiffies) of hostonly mode.
	unsigned long			hostonly_cnt; // times of hostonly tripped.
	unsigned long			flap_threshold;
#endif
#ifdef CONFIG_BRIDGE_MESH_PKT_FORWARDING_FILTER
	unsigned long			pff_drop;
	unsigned long			skip_learning;
	unsigned long			drop_from_self;
#endif
#ifdef CONFIG_BRIDGE_PACKET_INSPECTION_FILTER_MODULE
	unsigned long			pif_drop;
	u32				is_pif_enabled;
	u32				is_pif_proc_uplink;
#define PIF_TOKEN_MAX_DROPALL       65535 // drop all
#define PIF_TOKEN_MAX_IGNORE        0     // ignore rate limit, fall through all
#define PIF_TOKEN_COUNT_INIT        1000  // Allow more broadcast ARPs to go through after boot
#define PIF_TOKEN_MAX_INIT          PIF_TOKEN_MAX_IGNORE
	unsigned int			pif_token_count;
	unsigned int			pif_token_max;
	unsigned long			pif_token_timestamp;
	unsigned int			pif_proxy_arp_count;
	unsigned int			pif_directed_arp_count;
	u32				pif_directed_arp;

	spinlock_t			ip_hash_lock;
	struct hlist_head		ip_hash[BR_HASH_SIZE];
	struct list_head		ip_age_list;
#define IP_AGEING_INIT  (10*60 * HZ) // 10 mins
	unsigned long			ip_ageing_time;
	struct timer_list		ipdb_gc_timer;

	spinlock_t			ipv6_hash_lock;
	struct hlist_head		ipv6_hash[BR_HASH_SIZE];
	struct list_head		ipv6_age_list;
	struct timer_list		ipv6db_gc_timer;

	struct timer_list       ipv6_ra_throttle_timer;
#endif
};

extern struct notifier_block br_device_notifier;
extern const u8 br_group_address[ETH_ALEN];

/* called under bridge lock */
static inline int br_is_root_bridge(const struct net_bridge *br)
{
	return !memcmp(&br->bridge_id, &br->designated_root, 8);
}

/* br_device.c */
extern void br_dev_setup(struct net_device *dev);
extern netdev_tx_t br_dev_xmit(struct sk_buff *skb,
			       struct net_device *dev);

/* br_fdb.c */
extern int br_fdb_init(void);
extern void br_fdb_fini(void);
extern void br_fdb_flush(struct net_bridge *br);
extern void br_fdb_changeaddr(struct net_bridge_port *p,
#ifdef CONFIG_BRIDGE_VLAN
			      unsigned int vlan_id,
#endif
			      const unsigned char *newaddr);
#if 1 /* yun.wu, 2010-12-03 */
extern void br_fdb_changeaddr_br(struct net_bridge *br, const unsigned char *newaddr);
#endif
extern void br_fdb_cleanup(unsigned long arg);
extern void br_fdb_delete_by_port(struct net_bridge *br,
				  const struct net_bridge_port *p, int do_all);
extern struct net_bridge_fdb_entry *__br_fdb_get(struct net_bridge *br,
#ifdef CONFIG_BRIDGE_VLAN
						 unsigned int vlan_id,
#endif
						 const unsigned char *addr);
extern int br_fdb_test_addr(struct net_device *dev, unsigned char *addr);
extern int br_fdb_fillbuf(struct net_bridge *br, void *buf,
			  unsigned long count, unsigned long off);
extern int br_fdb_insert(struct net_bridge *br,
			 struct net_bridge_port *source,
#ifdef CONFIG_BRIDGE_VLAN
			 unsigned int vlan_id,
#endif
			 const unsigned char *addr,
			 int is_local );

int br_fdb_update(struct net_bridge_port *source, const struct sk_buff *skb);

#if 1 /* V54_BSP */
#define BR_FDB_UPDATE_OK		0
#define BR_FDB_UPDATE_DROP		1
#define BR_FDB_UPDATE_UP_ONLY		2
#endif

/* br_forward.c */
extern void br_deliver(const struct net_bridge_port *to,
		       struct sk_buff *skb);
extern int br_dev_queue_push_xmit(struct sk_buff *skb);
extern void br_forward(const struct net_bridge_port *to,
		       struct sk_buff *skb);
extern int br_forward_finish(struct sk_buff *skb);
extern void br_flood_deliver(struct net_bridge *br, struct sk_buff *skb);
extern void br_flood_forward(struct net_bridge *br, struct sk_buff *skb);

/* br_if.c */
extern void br_port_carrier_check(struct net_bridge_port *p);
extern int br_add_bridge(struct net *net, const char *name);
extern int br_del_bridge(struct net *net, const char *name);
extern void br_net_exit(struct net *net);
extern int br_add_if(struct net_bridge *br,
		     struct net_device *dev);
extern int br_del_if(struct net_bridge *br,
		     struct net_device *dev);
extern void br_bond_send_gratuitous_arp(struct net_bridge *br, struct net_device *dev);
extern int br_min_mtu(const struct net_bridge *br);
extern void br_features_recompute(struct net_bridge *br);
#if defined(CONFIG_BRIDGE_BR_MODE) || defined(CONFIG_BRIDGE_LOOP_GUARD)
extern int br_set_br_mode(struct net_bridge *br, int param, int value);
#endif
#ifdef CONFIG_BRIDGE_PORT_MODE
extern int br_set_port_mode(struct net_bridge *br, int ifindex, int mode);
extern int br_set_port_ci_state(struct net_bridge * br,int ifidx ,int state);
#endif
#if 1 /* V54_BSP */
extern int br_set_port_stp_forward_drop(struct net_bridge *br, int ifindex, int drop);
extern int br_set_port_eapol_passthru(struct net_bridge *br, int ifindex, int passthru);
#endif

#ifdef CONFIG_BRIDGE_MESH_PKT_FORWARDING_FILTER
extern int br_clear_macs(struct net_bridge *br);
extern int br_add_mac(struct net_bridge *br, int ifindex, unsigned char *mac);
extern int br_set_learning(struct net_bridge *br, int ifindex, int learning);
extern int br_set_pff(struct net_bridge *br, int ifindex, int pff);
#endif

/* br_input.c */
extern int br_handle_frame_finish(struct sk_buff *skb);
extern struct sk_buff *br_handle_frame(struct net_bridge_port *p,
				       struct sk_buff *skb);

/* br_ioctl.c */
extern int br_dev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
extern int br_ioctl_deviceless_stub(struct net *net, unsigned int cmd, void __user *arg);

/* br_netfilter.c */
#ifdef CONFIG_BRIDGE_NETFILTER
extern int br_netfilter_init(void);
extern void br_netfilter_fini(void);
extern void br_netfilter_rtable_init(struct net_bridge *);
#else
#define br_netfilter_init()	(0)
#define br_netfilter_fini()	do { } while(0)
#define br_netfilter_rtable_init(x)
#endif

/* br_stp.c */
extern void br_log_state(const struct net_bridge_port *p);
extern struct net_bridge_port *br_get_port(struct net_bridge *br,
					   u16 port_no);
extern void br_init_port(struct net_bridge_port *p);
extern void br_become_designated_port(struct net_bridge_port *p);

/* br_stp_if.c */
extern void br_stp_enable_bridge(struct net_bridge *br);
extern void br_stp_disable_bridge(struct net_bridge *br);
extern void br_stp_set_enabled(struct net_bridge *br, unsigned long val);
extern void br_stp_enable_port(struct net_bridge_port *p);
extern void br_stp_disable_port(struct net_bridge_port *p);
extern void br_stp_recalculate_bridge_id(struct net_bridge *br);
extern void br_stp_change_bridge_id(struct net_bridge *br, const unsigned char *a);
extern void br_stp_set_bridge_priority(struct net_bridge *br,
				       u16 newprio);
extern void br_stp_set_port_priority(struct net_bridge_port *p,
				     u8 newprio);
extern void br_stp_set_path_cost(struct net_bridge_port *p,
				 u32 path_cost);
extern ssize_t br_show_bridge_id(char *buf, const struct bridge_id *id);

/* br_stp_bpdu.c */
struct stp_proto;
extern void br_stp_rcv(const struct stp_proto *proto, struct sk_buff *skb,
		       struct net_device *dev);

/* br_stp_timer.c */
extern void br_stp_timer_init(struct net_bridge *br);
extern void br_stp_port_timer_init(struct net_bridge_port *p);
extern unsigned long br_timer_value(const struct timer_list *timer);

/* br.c */
#if defined(CONFIG_ATM_LANE) || defined(CONFIG_ATM_LANE_MODULE)
extern int (*br_fdb_test_addr_hook)(struct net_device *dev, unsigned char *addr);
#endif

/* br_netlink.c */
extern int br_netlink_init(void);
extern void br_netlink_fini(void);
extern void br_ifinfo_notify(int event, struct net_bridge_port *port);

#ifdef CONFIG_SYSFS
/* br_sysfs_if.c */
extern struct sysfs_ops brport_sysfs_ops;
extern int br_sysfs_addif(struct net_bridge_port *p);

/* br_sysfs_br.c */
extern int br_sysfs_addbr(struct net_device *dev);
extern void br_sysfs_delbr(struct net_device *dev);

#else

#define br_sysfs_addif(p)		(0)
#define br_sysfs_addbr(dev)		(0)
#define br_sysfs_delbr(dev)		do { } while(0)
#endif /* CONFIG_SYSFS */

typedef	int (*_br_vlan_func)(struct net_bridge_fdb_entry *dst, struct sk_buff *skb, int vlan_id);
extern int br_fdb_vlan_exec(struct net_bridge_port *br_port,
		struct sk_buff *skb, _br_vlan_func my_func);
#ifdef CONFIG_BRIDGE_PACKET_INSPECTION_FILTER_MODULE
extern void br_ipdb_init(void);
extern void br_ipdb_fini(void);
extern void br_ipdb_cleanup(unsigned long _data);
extern void br_ipdb_clear(struct net_bridge *br);
extern int br_ipdb_delete_by_mac(struct net_bridge *br, int ifidx, char *macaddr);
extern int br_ipdb_delete_by_ifmode(struct net_bridge * br, int ifmode);
extern int br_ipdb_delete_by_port(struct net_bridge * br, int port);
extern int br_ipdb_insert(struct net_bridge *br, const unsigned int ipaddr,
			  const unsigned int vlan, const unsigned char *macaddr,
			  struct net_bridge_port *br_port);
extern int br_ipdb_get_mac(struct net_bridge *br, unsigned int ipaddr,
			   unsigned int vlan, unsigned char **macaddr,
			   unsigned int *iptype);
extern int br_ipdb_fillbuf(struct net_bridge *br, void *buf,
			   unsigned long count, unsigned long off);

extern int br_ipdb_insert_dummy_entry(struct net_bridge *br,
				      const unsigned int vlan,
				      const unsigned char *macaddr,
				      struct net_bridge_port *br_port);
extern int br_ipdb_upd_dummy_entry(struct net_bridge *br, unsigned int ipaddr,
				   unsigned int vlan, unsigned char *macaddr);

extern void br_ipv6db_init(void);
extern void br_ipv6db_fini(void);
extern void br_ipv6db_cleanup(unsigned long _data);
extern void br_ipv6db_clear(struct net_bridge *br);
extern int br_ipv6db_delete_by_mac(struct net_bridge *br, int ifidx, char *macaddr);
extern int br_ipv6db_delete_by_port(struct net_bridge *br, int port);
extern int br_ipv6db_delete_by_ifmode(struct net_bridge *br, int ifmode);

extern int br_ipv6db_insert(struct net_bridge *br, const unsigned int *ipv6addr,
			    const unsigned int vlan, const unsigned char *macaddr,
			    struct net_bridge_port *br_port);
extern int br_ipv6db_get_mac(struct net_bridge *br, unsigned int *ipaddr,
			     unsigned int vlan, unsigned char **macaddr,
			     unsigned int *iptype);

extern int br_ipv6db_fillbuf(struct net_bridge *br, void *buf,
			     unsigned long count, unsigned long off);
#endif

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif
static inline int
ether_addr_equal(char *mac1 , char *mac2)
{
    return(memcmp(mac2, mac1, ETH_ALEN) == 0);
}
#endif
