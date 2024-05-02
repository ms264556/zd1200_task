/*
 *	Handle incoming frames
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

#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/netfilter_bridge.h>
#include <linux/jhash.h>
#include <linux/if_arp.h>
#include <linux/jiffies.h>
#include "br_private.h"

/* Bridge group multicast address 802.1d (pg 51). */
const u8 br_group_address[ETH_ALEN] = { 0x01, 0x80, 0xc2, 0x00, 0x00, 0x00 };
#if 1 /* V54_BSP */
const unsigned char bridge_pae[6] = { 0x01, 0x80, 0xc2, 0x00, 0x00, 0x03 };
#endif

#ifdef CONFIG_ANTI_SPOOF
int br_ci_is_spoofed_mac(const struct sk_buff *skb);
#endif  /* CONFIG_ANTI_SPOOF */

static int br_pass_frame_up_finish(struct sk_buff *skb)
{
#ifdef RKS_TUBE_SUPPORT
	struct net_bridge *br = netdev_priv(skb->dev);

	if (br->tube.func && br->tube.func(br->dev, skb, br->tube.opaque)) {
		return 0;
	}
#endif

#if defined(CONFIG_BRIDGE_VLAN)
	/* Ruckus: To make an untagged frame pass up to br.vlan interface,
	 * need to dot1q encap the frame per its ingress untagged VLAN id.
	 * Note that VLAN ID 1 is specially chosen so that br interface is
	 * still L3 reachable, with untagged VLAN ID 1 for the ingress port.
	 */
#endif
	netif_receive_skb(skb);
	return 0;
}

static void br_pass_frame_up(struct net_bridge *br, struct sk_buff *skb)
{
	struct net_device *indev, *brdev = br->dev;

	brdev->stats.rx_packets++;
	brdev->stats.rx_bytes += skb->len;
#if 1 /* V54_BSP */
	RX_BROADCAST_STATS((&brdev->stats), skb);
#endif

	indev = skb->dev;
	skb->dev = brdev;

	NF_HOOK(PF_BRIDGE, NF_BR_LOCAL_IN, skb, indev, NULL,
			br_pass_frame_up_finish);
}


#if defined(CONFIG_BRIDGE_DHCP_HOOK) || defined(CONFIG_BRIDGE_DHCP_HOOK_MODULE)
struct sk_buff * (*br_dhcp_handler_hook)(struct sk_buff *) = NULL;
EXPORT_SYMBOL(br_dhcp_handler_hook);
#endif

#if defined(CONFIG_BRIDGE_DNAT_HOOK) || defined(CONFIG_BRIDGE_DNAT_HOOK_MODULE)
struct sk_buff * (*br_dnat_in_handler_hook)(struct sk_buff *, struct sk_buff **) = NULL;
EXPORT_SYMBOL(br_dnat_in_handler_hook);
#endif

#ifdef CONFIG_BRIDGE_PACKET_INSPECTION_FILTER_MODULE
int (*br_pif_inspection_hook)(struct sk_buff *, unsigned int) = NULL;
EXPORT_SYMBOL(br_pif_inspection_hook);
#endif

/* note: already called with rcu_read_lock (preempt_disabled) */
int br_handle_frame_finish(struct sk_buff *skb)
{
	const unsigned char *dest = eth_hdr(skb)->h_dest;
	struct net_bridge_port *p = rcu_dereference(skb->dev->br_port);
	struct net_bridge *br;
	struct net_bridge_fdb_entry *dst;
	struct sk_buff *skb2;
	int do_local_up_only = 0;
	int my_addr;
#if defined(CONFIG_BRIDGE_DNAT_HOOK) || defined(CONFIG_BRIDGE_DNAT_HOOK_MODULE)
	struct sk_buff *nskb = NULL;
#endif

	if (!p || p->state == BR_STATE_DISABLED)
		goto drop;
	br = p->br;

	/* insert into forwarding database after filtering to avoid spoofing */
	my_addr = br_fdb_update(p, skb);
	if (my_addr) {
		if( my_addr == BR_FDB_UPDATE_UP_ONLY ) {
			// Allow unicast packets to forward - likely local
			do_local_up_only = 1;
		} else {
			// received source address that is mine, drop it.
			goto drop;
		}
	}

#if defined(CONFIG_BRIDGE_DHCP_HOOK) || defined(CONFIG_BRIDGE_DHCP_HOOK_MODULE)
	if (NULL != br_dhcp_handler_hook) {
		skb = br_dhcp_handler_hook(skb);
		if (NULL == skb)
			goto out;
	}
#endif

	if (p->state == BR_STATE_LEARNING)
		goto drop;

#if defined(CONFIG_BRIDGE_DNAT_HOOK) || defined(CONFIG_BRIDGE_DNAT_HOOK_MODULE)
	if (br_dnat_in_handler_hook != NULL) {
		skb = br_dnat_in_handler_hook(skb, &nskb);
		if (skb == NULL)
			goto out;
	}
send_fragment:
#endif

	/* The packet skb2 goes to the local host (NULL to skip). */
	skb2 = NULL;

	if (br->dev->flags & IFF_PROMISC)
		skb2 = skb;

	dst = NULL;

	if (is_multicast_ether_addr(dest)) {
		br->dev->stats.multicast++;
		skb2 = skb;
	} else {
#ifdef BR_FDB_VLAN
		dst = __br_fdb_get(br, skb->tag_vid, dest);
#else
		dst = __br_fdb_get(br, dest);
#endif
		if (dst != NULL && dst->is_local) {
			skb2 = skb;
			/* Do not forward the packet since it's local. */
			skb = NULL;
		}
	}

	if (skb2 == skb)
		skb2 = skb_clone(skb, GFP_ATOMIC);

	if (skb2)
		br_pass_frame_up(br, skb2);

	if (skb) {

#ifdef CONFIG_BRIDGE_PACKET_INSPECTION_FILTER_MODULE
		if (NULL != br_pif_inspection_hook) {
			if( br->is_pif_enabled &&
			    br_pif_inspection_hook(skb, skb->tag_vid) ) {
				br->pif_drop++;
				goto drop;
			}
		}
#endif
		if ( do_local_up_only )
			goto drop;

#ifdef CONFIG_BRIDGE_BR_MODE
		if (br->br_mode == BR_BRIDGE_MODE_HOST_ONLY) {
			goto drop;
		}
#endif
#ifdef CONFIG_BRIDGE_LOOP_GUARD
		if (br->hostonly) { // bridge in hostonly mode
			goto drop;
		}
#endif
#if 1 /* V54_BSP */
		// 802.1X pae eapol frame filter
		if (br->eapol_filter && !memcmp(dest, bridge_pae, ETH_ALEN))
			goto drop;
#endif
		if (dst)
			br_forward(dst->dst, skb);
		else
			br_flood_forward(br, skb);
	}

#if defined(CONFIG_BRIDGE_DNAT_HOOK) || defined(CONFIG_BRIDGE_DNAT_HOOK_MODULE)
	if (nskb != NULL) {
		skb = nskb;
		nskb = NULL;
		dest = eth_hdr(skb)->h_dest;
		goto send_fragment;
	}
#endif

out:
	return 0;
drop:
	kfree_skb(skb);
#if defined(CONFIG_BRIDGE_DNAT_HOOK) || defined(CONFIG_BRIDGE_DNAT_HOOK_MODULE)
	if (nskb != NULL)
		kfree_skb(nskb);
#endif
	goto out;
}

/* note: already called with rcu_read_lock (preempt_disabled) */
static int br_handle_local_finish(struct sk_buff *skb)
{
	struct net_bridge_port *p = rcu_dereference(skb->dev->br_port);

	if (p)
		(void) br_fdb_update(p, skb);
	return 0;	 /* process further */
}

/* Does address match the link local multicast address.
 * 01:80:c2:00:00:0X
 */
static inline int is_link_local(const unsigned char *dest)
{
	__be16 *a = (__be16 *)dest;
	static const __be16 *b = (const __be16 *)br_group_address;
	static const __be16 m = cpu_to_be16(0xfff0);

	return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | ((a[2] ^ b[2]) & m)) == 0;
}

/*
 * Called via br_handle_frame_hook.
 * Return NULL if skb is handled
 * note: already called with rcu_read_lock (preempt_disabled)
 */
struct sk_buff *br_handle_frame(struct net_bridge_port *p, struct sk_buff *skb)
{
	const unsigned char *dest = eth_hdr(skb)->h_dest;
	int (*rhook)(struct sk_buff *skb);

	if (!is_valid_ether_addr(eth_hdr(skb)->h_source))
		goto drop;

#ifdef CONFIG_BRIDGE_LOOP_GUARD
	// reset hostonly after hold time
	if (p->br->hostonly && time_after(jiffies, p->br->hostonly)) {
		p->br->hostonly = 0;
		//if (net_ratelimit()) {
			printk(KERN_WARNING "%s: Terminate host only mode (%lu)\n",
				p->br->dev->name, p->br->hostonly_cnt);
		//}
	}
#endif

	skb = skb_share_check(skb, GFP_ATOMIC);
	if (!skb)
		return NULL;

#if defined(CONFIG_BRIDGE_VLAN) && defined(BR_FDB_VLAN)
	// figured out what vlan frame is for.

	/* allow untagged unknown vlan traffic to be handled by the real device.
	 * This maps to brctl output case of 'none' for 'untagged vlan' column */
	if ( skb->tag_vid == 0 ) {
		return skb;
	}
#endif


	if (unlikely(is_link_local(dest))) {
		/* Pause frames shouldn't be passed up by driver anyway */
		if (skb->protocol == htons(ETH_P_PAUSE))
			goto drop;

		/* If STP is turned off, then forward */
		if (p->br->stp_enabled == BR_NO_STP && dest[5] == 0)
			goto forward;

		if (NF_HOOK(PF_BRIDGE, NF_BR_LOCAL_IN, skb, skb->dev,
			    NULL, br_handle_local_finish))
			return NULL;	/* frame consumed by filter */
		else
			return skb;	/* continue processing */
	}

forward:
	switch (p->state) {
	case BR_STATE_FORWARDING:
		rhook = rcu_dereference(br_should_route_hook);
		if (rhook != NULL) {
			if (rhook(skb))
				return skb;
			dest = eth_hdr(skb)->h_dest;
		}
		/* fall through */
	case BR_STATE_LEARNING:
		if (!compare_ether_addr(p->br->dev->dev_addr, dest))
			skb->pkt_type = PACKET_HOST;

		NF_HOOK(PF_BRIDGE, NF_BR_PRE_ROUTING, skb, skb->dev, NULL,
			br_handle_frame_finish);
		break;
	default:
drop:
		kfree_skb(skb);
	}
	return NULL;
}
