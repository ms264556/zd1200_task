/*
 *	Forwarding decision
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
#include <linux/skbuff.h>
#include <linux/if_vlan.h>
#include <linux/netfilter_bridge.h>
#include "br_private.h"


#if 1 /* V54_BSP */

#ifdef CONFIG_BRIDGE_PACKET_INSPECTION_FILTER_MODULE
int (*br_pif_filter_hook)(const struct sk_buff *, const struct net_bridge_port *) = NULL;
EXPORT_SYMBOL(br_pif_filter_hook);
#endif

#if defined(CONFIG_CI_WHITELIST) || defined (CONFIG_SOURCE_MAC_GUARD)
int (*br_ci_filter_hook)(const struct sk_buff *, const struct net_bridge_port *, const unsigned short) = NULL;
EXPORT_SYMBOL(br_ci_filter_hook);

extern int br_ci_filter(const struct sk_buff *skb, const struct net_bridge_port *port, const unsigned short);
#endif /* CONFIG_CI_WHITELIST || CONFIG_SOURCE_MAC_GUARD */

#if defined(CONFIG_BRIDGE_DNAT_HOOK) || defined(CONFIG_BRIDGE_DNAT_HOOK_MODULE)
struct sk_buff * (*br_dnat_out_handler_hook)(struct sk_buff *) = NULL;
EXPORT_SYMBOL(br_dnat_out_handler_hook);
#endif


const unsigned char bridge_ula_cisco1[6] = { 0x01, 0x00, 0x0c, 0xcc, 0xcc, 0xcd };
const unsigned char bridge_ula_cisco2[6] = { 0x01, 0x00, 0x0c, 0xcd, 0xcd, 0xcd };
int is_bridge_ula(const unsigned char *dest)
{
	if (!memcmp(dest, br_group_address, ETH_ALEN) ||
	    !memcmp(dest, bridge_ula_cisco1, ETH_ALEN) ||
	    !memcmp(dest, bridge_ula_cisco2, ETH_ALEN))
		return 1;

	return 0;
}

#endif


/* Don't forward packets to originating port or forwarding diasabled    */
/* For the caller - if the return value is zero - it indicates that the */
/* skb needs to be dropped, a return value of 1 - means that the skb    */
/* is good for further processing                                       */

/* TBD : Add counters when telling the caller to drop skbs              */

int should_deliver(const struct net_bridge_port *dport,
				 const struct sk_buff         *skb)
{
	struct net_bridge_port *sport;
	const unsigned char    *dest = eth_hdr(skb)->h_dest;
	int                     is_stp_frame = 0, ret = 0;

	if ((!(dport->flags & BR_HAIRPIN_MODE) && skb->dev == dport->dev) ||
	    dport->state != BR_STATE_FORWARDING) {
		return ret;
	}

	if (skb->pkt_type == PACKET_MULTICAST) {
		is_stp_frame = is_bridge_ula(dest);
		if (dport->stp_forward_drop &&
		    is_stp_frame)
			return ret;
	}

#ifdef CONFIG_BRIDGE_PORT_MODE
	/*
	 * -- block traffic where 'x' is indicated --
	 * (RUNI <=> NNI only)
         *
	 *       RUNI   UUNI
	 * ==================
	 * RUNI   x      x
	 * UUNI   x
	 */
	if ((skb->dev) && (skb->dev->br_port)) {
		sport = rcu_dereference(skb->dev->br_port);

		if ((sport->port_mode == BR_PORT_MODE_RUNI && dport->port_mode == BR_PORT_MODE_UUNI) ||
		    (sport->port_mode == BR_PORT_MODE_UUNI && dport->port_mode == BR_PORT_MODE_RUNI) ||
		    (sport->port_mode == BR_PORT_MODE_RUNI && dport->port_mode == BR_PORT_MODE_RUNI) ) {
			/* permit STP regardless of port type */
			if (!is_stp_frame) return ret;
		} else if ( sport->port_mode == BR_PORT_MODE_HOST_ONLY ) {
			return ret;
		} else if ( dport->port_mode == BR_PORT_MODE_HOST_ONLY ) {
			return ret;
		}
	}
#endif


#ifdef CONFIG_BRIDGE_VLAN
	if (skb->tag_vid && br_vlan_filter(skb, &dport->vlan))
		return ret;
#endif

#if defined(CONFIG_CI_WHITELIST) || defined (CONFIG_SOURCE_MAC_GUARD)
	if (br_ci_filter_hook) {
		if (0 == br_ci_filter_hook(skb, dport, 0))
			return ret;
	}
#endif /* CONFIG_CI_WHITELIST || CONFIG_SOURCE_MAC_GUARD */

#ifdef CONFIG_BRIDGE_PACKET_INSPECTION_FILTER_MODULE
	if (br_pif_filter_hook) {
		if (dport->br->is_pif_enabled &&
		    br_pif_filter_hook(skb, dport)) {
			return ret;
		}
	}
#endif

	return 1;
}

static inline unsigned packet_length(const struct sk_buff *skb)
{
	return skb->len - (skb->protocol == htons(ETH_P_8021Q) ? VLAN_HLEN : 0);
}

int br_dev_queue_push_xmit(struct sk_buff *skb)
{
	/* drop mtu oversized packets except gso */
	if (packet_length(skb) > skb->dev->mtu &&
#if 1 // V54_TUNNELMGR
	    !(skb->dev->features & NETIF_F_FRAGINNER) &&
#endif // V54_TUNNELMGR
	    !skb_is_gso(skb)) {
		printk(KERN_DEBUG "[bridge forward] packet size(%u) exceed dev(%s) "
		       "mtu(%u)... drop it...\n",
		       packet_length(skb), skb->dev->name, skb->dev->mtu);
		kfree_skb(skb);
	} else {
		/* ip_refrag calls ip_fragment, doesn't copy the MAC header. */
		if (nf_bridge_maybe_copy_header(skb))
			kfree_skb(skb);
		else {
#if defined(CONFIG_BRIDGE_DNAT_HOOK) || defined(CONFIG_BRIDGE_DNAT_HOOK_MODULE)
			if (br_dnat_out_handler_hook != NULL) {
				skb = br_dnat_out_handler_hook(skb);
				if (skb == NULL)
					return 0;
			}
#endif

#ifndef CONFIG_BRIDGE_VLAN
			skb_push(skb, ETH_HLEN);
#endif

			dev_queue_xmit(skb);
		}
	}

	return 0;
}

int br_forward_finish(struct sk_buff *skb)
{
	return NF_HOOK(PF_BRIDGE, NF_BR_POST_ROUTING, skb, NULL, skb->dev,
		       br_dev_queue_push_xmit);

}

static void __br_deliver(const struct net_bridge_port *to, struct sk_buff *skb)
{
	skb->dev = to->dev;
	NF_HOOK(PF_BRIDGE, NF_BR_LOCAL_OUT, skb, NULL, skb->dev,
			br_forward_finish);
}

static void __br_forward(const struct net_bridge_port *to, struct sk_buff *skb)
{
	struct net_device *indev;

	if (skb_warn_if_lro(skb)) {
		kfree_skb(skb);
		return;
	}

#if 1 /* defined(V54_BSP) */
	if (skb->input_dev == NULL)
		skb->input_dev = skb->dev;
#endif

	indev = skb->dev;
	skb->dev = to->dev;
	skb_forward_csum(skb);

	NF_HOOK(PF_BRIDGE, NF_BR_FORWARD, skb, indev, skb->dev,
			br_forward_finish);
}

/* called with rcu_read_lock */
void br_deliver(const struct net_bridge_port *to, struct sk_buff *skb)
{
	if (should_deliver(to, skb)) {
		__br_deliver(to, skb);
		return;
	}
	to->dev->stats.tx_dropped++;
	kfree_skb(skb);
}

/* called with rcu_read_lock */
void br_forward(const struct net_bridge_port *to, struct sk_buff *skb)
{
	if (should_deliver(to, skb)) {
		__br_forward(to, skb);
		return;
	}
	to->dev->stats.tx_dropped++;
	kfree_skb(skb);
}

/* called under bridge lock */
static void br_flood(struct net_bridge *br, struct sk_buff *skb,
	void (*__packet_hook)(const struct net_bridge_port *p,
			      struct sk_buff *skb))
{
	struct net_bridge_port *p;
	struct net_bridge_port *prev;

	prev = NULL;

	list_for_each_entry_rcu(p, &br->port_list, list) {
		if (should_deliver(p, skb)) {
			if (prev != NULL) {
				struct sk_buff *skb2;
				if ((skb2 = skb_clone(skb, GFP_ATOMIC)) == NULL) {
					br->dev->stats.tx_dropped++;
					kfree_skb(skb);
					return;
				}
				__packet_hook(prev, skb2);
			}
			prev = p;
		}
	}

	if (prev) {
		__packet_hook(prev, skb);
		return;
	}

	kfree_skb(skb);
}


/* called with rcu_read_lock */
void br_flood_deliver(struct net_bridge *br, struct sk_buff *skb)
{
	br_flood(br, skb, __br_deliver);
}

/* called under bridge lock */
void br_flood_forward(struct net_bridge *br, struct sk_buff *skb)
{
	br_flood(br, skb, __br_forward);
}
