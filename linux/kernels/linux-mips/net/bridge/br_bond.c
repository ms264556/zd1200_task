/*------------------------------------------------------------------------------
 * Copyright (c), 2005-2011, Ruckus Wireless, Inc.
 * File       : br_bond.c
 * Version    :
 * Author     : qingjie.xing
 * Date       : 2015-07-12
 * After bond was added to bridge. When bond changes active slave, make bond port send
 * gratuitous ARP
 *----------------------------------------------------------------------------*/

#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/skbuff.h>
#include <linux/if_vlan.h>
#include <net/arp.h>

#include "br_private.h"

static void br_bond_arp_send(struct net_device *bond_dev, int arp_op, __be32 dest_ip, __be32 src_ip, unsigned short vlan_id)
{
	struct sk_buff *skb;

	printk(KERN_DEBUG "arp %d on slave %s: dst %x src %x vid %d\n", arp_op,
				bond_dev->name, dest_ip, src_ip, vlan_id);

	skb = arp_create(arp_op, ETH_P_ARP, dest_ip, bond_dev, src_ip,
			 NULL, bond_dev->dev_addr, NULL);

	if (!skb) {
		printk(KERN_ERR ": ARP packet allocation failed\n");
		return;
	}
	if (vlan_id) {
		skb = vlan_put_tag(skb, vlan_id);
		if (!skb) {
			printk(KERN_ERR ": failed to insert VLAN tag\n");
			return;
		}
	}
	printk(KERN_DEBUG "br_bond_arp_send: vlan:%d, skb->vlan_tci:%d, skb->pro:%d\n", vlan_id, skb->vlan_tci, skb->protocol);
	arp_xmit(skb);
}

void br_bond_send_gratuitous_arp(struct net_bridge *br, struct net_device *dev)
{
	struct net_device *br_dev, *br_vlan_dev;
	struct in_device *in_dev;
	struct vlan_group *vg;
	struct in_ifaddr *ifa;
	u16 i;

	br_dev = br->dev;
	if ((in_dev = __in_dev_get_rtnl(br_dev)) != NULL) {
		if ((ifa = in_dev->ifa_list) != NULL) {
			br_bond_arp_send(dev, ARPOP_REPLY, ifa->ifa_address, ifa->ifa_address, 0);
		}
	}

	if ((vg = vlan_find_group(br_dev)) != NULL) {
		for (i = 0; i < VLAN_GROUP_ARRAY_LEN; i++) {
			br_vlan_dev = vlan_group_get_device(vg, i);
			if (!br_vlan_dev) {
				continue;
			}

			if ((in_dev = __in_dev_get_rtnl(br_vlan_dev)) != NULL) {
				if ((ifa = in_dev->ifa_list) != NULL) {
					br_bond_arp_send(dev, ARPOP_REPLY, ifa->ifa_address, ifa->ifa_address, i);
				}
			}
		}
	}
}
