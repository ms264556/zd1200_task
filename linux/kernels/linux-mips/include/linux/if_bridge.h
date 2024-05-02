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

#ifndef _LINUX_IF_BRIDGE_H
#define _LINUX_IF_BRIDGE_H

#include <linux/types.h>

#define SYSFS_BRIDGE_ATTR	"bridge"
#define SYSFS_BRIDGE_FDB	"brforward"
#define SYSFS_BRIDGE_PORT_SUBDIR "brif"
#define SYSFS_BRIDGE_PORT_ATTR	"brport"
#define SYSFS_BRIDGE_PORT_LINK	"bridge"

#define BRCTL_VERSION 2

#define BRCTL_GET_VERSION 0
#define BRCTL_GET_BRIDGES 1
#define BRCTL_ADD_BRIDGE 2
#define BRCTL_DEL_BRIDGE 3
#define BRCTL_ADD_IF 4
#define BRCTL_DEL_IF 5
#define BRCTL_GET_BRIDGE_INFO 6
#define BRCTL_GET_PORT_LIST 7
#define BRCTL_SET_BRIDGE_FORWARD_DELAY 8
#define BRCTL_SET_BRIDGE_HELLO_TIME 9
#define BRCTL_SET_BRIDGE_MAX_AGE 10
#define BRCTL_SET_AGEING_TIME 11
#define BRCTL_SET_GC_INTERVAL 12
#define BRCTL_GET_PORT_INFO 13
#define BRCTL_SET_BRIDGE_STP_STATE 14
#define BRCTL_SET_BRIDGE_PRIORITY 15
#define BRCTL_SET_PORT_PRIORITY 16
#define BRCTL_SET_PATH_COST 17
#define BRCTL_GET_FDB_ENTRIES 18

#if 1 /* V54_BSP */
#define BRCTL_GET_PORT_ATTRIB_FOR_MAC 19 //get forwarding port attributes for a MAC
#ifdef CONFIG_BRIDGE_VLAN
#define BRCTL_SET_PORT_UNTAGGED_VLAN 20
#define BRCTL_ADD_PORT_VLAN 21
#define BRCTL_DEL_PORT_VLAN 22
#define BRCTL_GET_PORT_VLAN_INFO 23
#define BRCTL_SET_PORT_TAGGED_VLAN 24
#endif

#define BRCTL_SET_BRIDGE_MODE 33 // CONFIG_BRIDGE_LOOP_GUARD
#define BRCTL_SET_PORT_MODE 34   // CONFIG_BRIDGE_PORT_MODE
#define BRCTL_SET_PORT_STP_FORWARD_DROP 35
#define BRCTL_SET_BRIDGE_EAPOL_FILTER 36
#define BRCTL_SET_BRIDGE_DESIRE_MTU 37
#define BRCTL_SET_PORT_CI_STATE 38

#if 1 || defined(CONFIG_BRIDGE_LOOP_GUARD)
#define BR_BRIDGE_MODE_NORMAL 0
#define BR_BRIDGE_MODE_HOST_ONLY 1
#define BR_BRIDGE_MODE_SET_LOOP_HOLD 0x11
#define BR_BRIDGE_MODE_SET_FLAP_THRESHOLD 0x12
#endif

#if 1 || defined(CONFIG_BRIDGE_PORT_MODE)
#define BR_PORT_MODE_ALL       0
#define BR_PORT_MODE_NNI       0    // NNI port. will forward everything
#define BR_PORT_MODE_HOST_ONLY 1    // bridge port only pass host traffic
#define BR_PORT_MODE_UUNI      2    // Unrestricted UNI port. will only forward to non-UNI ports
#define BR_PORT_MODE_RUNI      3    // Restricted UNI port (Client Isolation enabled)
#define BR_PORT_MODE_MAX BR_PORT_MODE_RUNI

#define BR_PORT_CI_INIT 0
#define BR_PORT_CI_LEARNING 1
#define BR_PORT_CI_ISOLATION 2
#endif

#if 1 || defined(CONFIG_BRIDGE_MESH_PKT_FORWARDING_FILTER)
#define BRCTL_CLEAR_MACS       40
#define BRCTL_ADD_MAC          41
#define BRCTL_SET_LEARNING     42
#define BRCTL_SET_PFF          43
#endif

#if 1 || defined(CONFIG_BRIDGE_PACKET_INSPECTION_FILTER_MODULE)
#define BRCTL_SET_PIF                   51
#define BRCTL_GET_IPDB_ENTRIES          52
#define BRCTL_GET_IPV6DB_ENTRIES        53
#define BRCTL_SET_IP_AGEING             54
#define BRCTL_DEL_IPDB_ENTRY_BY_MAC     55
#define BRCTL_DEL_IPV6DB_ENTRY_BY_MAC   56

#define PIF_CTL_SET_PIF                 0
#define PIF_CTL_SET_IF_TYPE             1
#define PIF_CTL_SET_DIRECTED_ARP        2
#define PIF_CTL_SET_IF_MODE             3
#define PIF_CTL_SET_RATE_LIMIT          4
#define PIF_CTL_SET_PROC_UPLINK         5
#define PIF_CTL_SET_IF_LEARNING         6

#define BR_PIF_TYPE_DIRECTED_ARP        0
#define BR_PIF_TYPE_PROXY_ARP           1
#define BR_PIF_TYPE_NONE                2

#define BR_PIF_IFMODE_MIN               0
#define BR_PIF_IFMODE_UPLINK_PORT       0
#define BR_PIF_IFMODE_DNLINK_PORT       1
#define BR_PIF_IFMODE_MAX               1

#define BR_PIF_LEARNING_OFF             0
#define BR_PIF_LEARNING_ALL             1
#define BR_PIF_LEARNING_GW_ONLY         2
#endif
#endif

#if 1 || defined(CONFIG_CI_WHITELIST) || defined (CONFIG_SOURCE_MAC_GUARD)  || defined (CONFIG_ANTI_SPOOF)

#define BRCTL_SET_CI_WL                 57

#define CI_STATUS_ENABLE                1
#define CI_STATUS_DISABLE               0

#define CI_ADD_WL_IP_MAC                0
#define CI_DEL_WL_IP_MAC                1
#define CI_FSH_WL_IP_MAC                2
#define CI_PRG_WL_IP_MAC                3
#define CI_SET_WL_IFNAME                4
#define CI_GET_WL_SIZE                  5
#define CI_REFRESH_WAN_IP_MAC           6
#define CI_SET_WAN_IF                   7
#define CI_CLR_CTR                      8
#define CI_SET_WL_CTR_IFNAME            9

#define CI_MAX_OPCODE                   9 /* Set the highest CI_ # here */

#endif  /* CONFIG_CI_WHITELIST || CONFIG_SOURCE_MAC_GUARD || CONFIG_ANTI_SPOOF */

#define BRCTL_GET_WHICH_BRIDGE 48
#define BRCTL_SET_HOTSPOT_FLAG 49
#define BRCTL_SET_PORT_EAPOL_PASSTHRU 50


#define BR_STATE_DISABLED 0
#define BR_STATE_LISTENING 1
#define BR_STATE_LEARNING 2
#define BR_STATE_FORWARDING 3
#define BR_STATE_BLOCKING 4

struct __bridge_info
{
	__u64 designated_root;
	__u64 bridge_id;
	__u32 root_path_cost;
	__u32 max_age;
	__u32 hello_time;
	__u32 forward_delay;
	__u32 bridge_max_age;
	__u32 bridge_hello_time;
	__u32 bridge_forward_delay;
	__u8 topology_change;
	__u8 topology_change_detected;
	__u8 root_port;
	__u8 stp_enabled;
#if 1 /* V54_BSP */
	__u8 eapol_filter;
	__u8 unused0;
	__u16 des_mtu;
#endif
	__u32 ageing_time;
	__u32 gc_interval;
	__u32 hello_timer_value;
	__u32 tcn_timer_value;
	__u32 topology_change_timer_value;
	__u32 gc_timer_value;
#if 1 || defined(CONFIG_BRIDGE_LOOP_GUARD)
	__u32 br_mode;
	__u32 hostonly;
	__u32 jiffies;
	__u32 hostonly_cnt;
	__u32 loop_hold;
	__u32 flap_threshold;
#endif
#if 1 || defined(CONFIG_BRIDGE_MESH_PKT_FORWARDING_FILTER)
	__u32 pff_drop;
	__u32 skip_learning;
	__u32 drop_from_self;
#endif
#if 1 || defined(CONFIG_BRIDGE_PACKET_INSPECTION_FILTER_MODULE)
	__u32 pif;
	__u32 pif_drop;
	__u32 pif_proc_uplink;
	__u32 pif_token_count;
	__u32 pif_token_max;
	__u32 pif_proxy_arp_count;
	__u32 pif_directed_arp_count;
	__u32 pif_directed_arp;
	__u32 ip_ageing_time;
	__u32 ipdb_gc_timer_value;
	__u32 ipv6db_gc_timer_value;
#endif

};

struct __port_info
{
	__u64 designated_root;
	__u64 designated_bridge;
	__u16 port_id;
	__u16 designated_port;
	__u32 path_cost;
	__u32 designated_cost;
	__u8 state;
	__u8 top_change_ack;
	__u8 config_pending;
#if 1 /* V54_BSP */
	__u8 stp_forward_drop;
	__u8 eapol_passthru;
	__u8 unused2;
	__u8 unused1;
	__u8 unused0;
#endif
	__u32 message_age_timer_value;
	__u32 forward_delay_timer_value;
	__u32 hold_timer_value;
#if 1 || defined(CONFIG_BRIDGE_PORT_MODE)
	__u32 port_mode;
	__u8   port_ci_state;
#endif
#if 1 || defined(CONFIG_BRIDGE_MESH_PKT_FORWARDING_FILTER)
	__u32 learning;
	__u32 pff;
#endif
#if 1 || defined(CONFIG_BRIDGE_PACKET_INSPECTION_FILTER_MODULE)
	__u32 pif_iftype;
	__u32 pif_ifmode;
	__u32 pif_learning;
#endif
};

#ifdef CONFIG_BRIDGE_VLAN
struct __vlan_info
{
	__u32 untagged;
	__u8 filter[4096/8];
};
#endif

struct __fdb_entry
{
	__u8 mac_addr[6];
	char port_name[16];
	__u8 port_no;
	__u8 is_local;
	__u32 ageing_timer_value;
	__u8 port_hi;
	__u8 pad0;
#ifdef CONFIG_BRIDGE_VLAN
	__u16 vlan_id;
#else
	__u16 unused;
#endif
};

#if 1 || defined(CONFIG_BRIDGE_PACKET_INSPECTION_FILTER_MODULE)
struct __ipdb_entry
{
	__u32 ipaddr;
	char port_name[16];
	__u32 vlan;
	__u8 mac_addr[6];
	__u32 ageing_timer_value;
	__u32 iftype;
};

struct __ipv6db_entry
{
	__u32 ipv6addr[4];
	char port_name[16];
	__u32 vlan;
	__u8 mac_addr[6];
	__u32 ageing_timer_value;
	__u32 iftype;
};
#endif

#ifdef __KERNEL__

#include <linux/netdevice.h>

extern void brioctl_set(int (*ioctl_hook)(struct net *, unsigned int, void __user *));
extern struct sk_buff *(*br_handle_frame_hook)(struct net_bridge_port *p,
					       struct sk_buff *skb);
extern int (*br_should_route_hook)(struct sk_buff *skb);
#ifdef CONFIG_BRIDGE_VLAN
extern int (*br_vlan_get_untagged_hook)(struct sk_buff *skb);
#endif
#if defined(CONFIG_BRIDGE_DHCP_HOOK) || defined(CONFIG_BRIDGE_DHCP_HOOK_MODULE)
extern struct sk_buff * (*br_dhcp_handler_hook)(struct sk_buff *);
#endif
#if defined(CONFIG_BRIDGE_DNAT_HOOK) || defined(CONFIG_BRIDGE_DNAT_HOOK_MODULE)
extern struct sk_buff * (*br_dnat_in_handler_hook)(struct sk_buff *, struct sk_buff **);
extern struct sk_buff * (*br_dnat_out_handler_hook)(struct sk_buff *);
#endif
#if defined(CONFIG_BRIDGE_PACKET_INSPECTION_FILTER_MODULE)
extern int (*br_pif_filter_hook)(const struct sk_buff *, const struct net_bridge_port *);
extern int (*br_pif_inspection_hook)(struct sk_buff *, unsigned int);


extern int (*br_pif_if_ingress_hook)(struct sk_buff *, unsigned int);

#endif

extern int is_bridge_device(struct net_device *dev);

#ifdef V54_BSP
typedef void (* BrDbUpdate_CB_type)(const unsigned char * macAddr);
extern void register_br_db_update_hook( BrDbUpdate_CB_type cb_func);
#endif

#endif

#endif
