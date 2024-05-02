/*
 *	Handle incoming frames
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Ruckus Wireless
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


#ifdef CONFIG_BRIDGE_ARP_BROADCAST_FILTER


#define MAC_FMT "%02x:%02x:%02x:%02x:%02x:%02x"

void br_abf_token_replenisher(struct net_bridge *br)
{
	unsigned long msec_passed, tokens_to_add;
	if( br->abf_token_count >= br->abf_token_max ) {
		return;
	}
	msec_passed = jiffies_to_msecs(jiffies - br->abf_token_timestamp);
	tokens_to_add = msec_passed / (1000/br->abf_token_max);
	if( tokens_to_add > 0 ) {
		br->abf_token_timestamp = jiffies;
		if( (br->abf_token_count + tokens_to_add) >= br->abf_token_max ) {
			br->abf_token_count = br->abf_token_max;
		} else {
			br->abf_token_count += tokens_to_add;
		}
	}
}


int br_arp_filter(struct sk_buff *skb, unsigned int vlan)
{
    struct ethhdr   *eth = eth_hdr(skb);
    unsigned short  ether_type;
	struct net_bridge_port *p = skb->dev->br_port;
	struct net_bridge *br = p->br;
    unsigned int    len = skb->len;
    struct arp_pkt *parp;
	unsigned char allff = 0xFF, allzero = 0;
	int i;
    struct net_bridge_port *in_p = skb->input_dev?skb->input_dev->br_port:NULL;

    // Check for VLAN tag
    if (eth->h_proto == __constant_htons(ETH_P_8021Q)) {
      ether_type = ((struct vlan_ethhdr *)eth_hdr(skb))->h_vlan_encapsulated_proto;
      parp = (struct arp_pkt *)(skb_mac_header(skb) + sizeof(struct vlan_ethhdr));
      len -= (skb_mac_header(skb) + sizeof(struct vlan_ethhdr)) - skb->data;
    } else {
      ether_type = eth->h_proto;
      parp = (struct arp_pkt *)(skb_mac_header(skb) + sizeof(struct ethhdr));
      len -= (skb_mac_header(skb) + sizeof(struct ethhdr)) - skb->data;
    }

    // Check only ARP packets
    if( ether_type != __constant_htons(ETH_P_ARP) ) {
        return 0;
    }

    // Validate ARP
    if( len < sizeof(struct arp_pkt) ||
	parp->ar_hrd != __constant_htons(ETH_P_802_3) ||
	parp->ar_pro != __constant_htons(ETH_P_IP) ||
	parp->ar_hln != ETH_ALEN ||
	parp->ar_pln != 4 ||
	(parp->ar_op != __constant_htons(ARPOP_REQUEST) &&
	 parp->ar_op != __constant_htons(ARPOP_REPLY))) {
#if ABF_DEBUG
        printk(KERN_DEBUG "%s: Malformed ARP: " MAC_FMT "->" MAC_FMT ", len %d (exp %d), vlan %d, hrd %X, pro %X, hln %d, pln %d, op %X\n",
                    __func__,
                    MAC_ARG(eth_hdr(skb)->h_source),
                    MAC_ARG(eth_hdr(skb)->h_dest),
                    len, sizeof(struct arp_pkt),
                    vlan,
                    parp->ar_hrd,
                    parp->ar_pro,
                    parp->ar_hln,
                    parp->ar_pln,
                    parp->ar_op );
#endif
	return 0;
    }

#ifdef ABF_DEBUG
    printk(KERN_DEBUG "%s: ARP: " MAC_FMT "->" MAC_FMT ", len %d (exp %d), vlan %d, hrd %X, pro %X, hln %d, pln %d, op %X, sha " MAC_FMT ", sip " IPADDR_FMT ", tha " MAC_FMT ", tip " IPADDR_FMT "\n",
                __func__,
                MAC_ARG(eth_hdr(skb)->h_source),
                MAC_ARG(eth_hdr(skb)->h_dest),
                len, sizeof(struct arp_pkt),
                vlan,
                parp->ar_hrd,
                parp->ar_pro,
                parp->ar_hln,
                parp->ar_pln,
                parp->ar_op,
                MAC_ARG(parp->ar_sha),
                NIPQUAD(parp->ar_sip),
                MAC_ARG(parp->ar_tha),
                NIPQUAD(parp->ar_tip));
#endif

	// First, check if we know he sender - and refresh our table
	for( i = 0; i < ETH_ALEN; i++ ) {
		allff &= parp->ar_sha[i];
		allzero |= parp->ar_sha[i];
	}
	if( parp->ar_sip != 0 && parp->ar_sip != 0xFFFFFFFF &&
		allff != 0xFF && allzero != 0 ) {
#ifdef ABF_DEBUG
        printk(KERN_DEBUG "%s: Add IP: " MAC_FMT "->" MAC_FMT ", sip %d.%d.%d.%d, sha " MAC_FMT " \n",
                    __func__,
                    MAC_ARG(eth_hdr(skb)->h_source),
                    MAC_ARG(eth_hdr(skb)->h_dest),
                    (parp->ar_sip >> 24) & 0xFF, (parp->ar_sip >> 16) & 0xFF, (parp->ar_sip >> 8) & 0xFF, (parp->ar_sip) & 0xFF,
                    MAC_ARG(parp->ar_sha) );
#endif
		br_ipdb_insert(br, parp->ar_sip, vlan, parp->ar_sha);
	}

	// Now, for a request, check if we know the destination
	// If we do - translate to unicast
	// If we don't - ensure this is not a flood using a DoS protection mechanism
    if( parp->ar_op == __constant_htons(ARPOP_REQUEST) ) {
	// Check if this is going to a known IP
	unsigned char *macaddr;
	if( br_ipdb_get_mac( br, parp->ar_tip, vlan, &macaddr ) == 0 ) {
		// IP found - use MAC and convert from vroadcast to unicast
		memcpy(parp->ar_tha, macaddr, ETH_ALEN );
		memcpy(eth_hdr(skb)->h_dest, macaddr, ETH_ALEN );
	} else if(in_p && in_p->abf_ifmode == BR_ABF_IFMODE_DOWNLINK ){
			// Just let it through as broadcast - but rate limit the broadcasts
		if( br->abf_token_count <= 0 )
			br_abf_token_replenisher(br);
		if( br->abf_token_count <= 0 ) {
#ifdef ABF_DEBUG
		    printk(KERN_DEBUG "%s: ABF - Dropping ARP: " MAC_FMT "->" MAC_FMT ", " IPADDR_FMT "->" IPADDR_FMT ", count %u, max %u  ,abf_ifmode %u\n",
		                __func__,
		                MAC_ARG(parp->ar_sha),
		                MAC_ARG(parp->ar_tha),
		                NIPQUAD(parp->ar_sip),
		                NIPQUAD(parp->ar_tip),
		                br->abf_token_count,
		                br->abf_token_max,
                             in_p?in_p->abf_ifmode:-1);
#endif
			return 1; // No tokens left - drop the packet
		}
		br->abf_token_count--;
	}
#ifdef ABF_DEBUG
	    printk(KERN_DEBUG "%s: Sending ARP: ETH: " MAC_FMT "->" MAC_FMT " ARP: " MAC_FMT "->" MAC_FMT ", " IPADDR_FMT "->" IPADDR_FMT ", count %u, max %u, br %p, brname %s  from %s, abf_ifmode %u \n",
	                __func__,
	                MAC_ARG(eth_hdr(skb)->h_source),
	                MAC_ARG(eth_hdr(skb)->h_dest),
	                MAC_ARG(parp->ar_sha),
	                MAC_ARG(parp->ar_tha),
	                NIPQUAD(parp->ar_sip),
	                NIPQUAD(parp->ar_tip),
	                br->abf_token_count,
	                br->abf_token_max,
	                br,
	                br->dev->name,
                    skb->input_dev->name?skb->input_dev->name:"internal",
                    in_p?in_p->abf_ifmode:-1
	                );
#endif
    }

    return 0;
}


#endif // CONFIG_BRIDGE_ARP_BROADCAST_FILTER
