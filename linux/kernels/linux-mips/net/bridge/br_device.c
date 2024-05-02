/*
 *	Device handling code
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
#include <linux/ethtool.h>

#include <asm/uaccess.h>
#include "br_private.h"

/* net device transmit always called with no BH (preempt_disabled) */
netdev_tx_t br_dev_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct net_bridge *br = netdev_priv(dev);
	const unsigned char *dest = skb->data;
	struct net_bridge_fdb_entry *dst;
#if 1 /* defined(V54_BSP) */
	if ( skb->input_dev == NULL )
		skb->input_dev = dev;
#endif
	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;

	skb_reset_mac_header(skb);
	skb_pull(skb, ETH_HLEN);

#ifdef CONFIG_BRIDGE_VLAN
	skb->mac_len = skb->network_header - skb->mac_header;
#endif

#if 1 /* defined(V54_BSP) */
	TX_BROADCAST_STATS((&dev->stats), skb, 1);
#endif
#ifdef CONFIG_BRIDGE_VLAN
	if ( skb->tag_vid == 0 ) {
		skb->tag_priority = 0;
		skb->tag_vid = BR_DEFAULT_VLAN;
	}
#endif

#if defined(CONFIG_BRIDGE_DHCP_HOOK) || defined(CONFIG_BRIDGE_DHCP_HOOK_MODULE)
	if ((br->hotspot) && (NULL != br_dhcp_handler_hook)) {
		skb = br_dhcp_handler_hook(skb);
		if (NULL == skb)
			return 0;
	}
#endif

	if (dest[0] & 1)
		br_flood_deliver(br, skb);
#ifdef BR_FDB_VLAN
	else if ((dst = __br_fdb_get(br, (unsigned int)skb->tag_vid, dest)) != NULL)
#else
	else if ((dst = __br_fdb_get(br, dest)) != NULL)
#endif
		br_deliver(dst->dst, skb);
	else
		br_flood_deliver(br, skb);

	return NETDEV_TX_OK;
}

static int br_dev_open(struct net_device *dev)
{
	struct net_bridge *br = netdev_priv(dev);

	br_features_recompute(br);
	netif_start_queue(dev);
	br_stp_enable_bridge(br);

	return 0;
}

static void br_dev_set_multicast_list(struct net_device *dev)
{
}

static int br_dev_stop(struct net_device *dev)
{
	br_stp_disable_bridge(netdev_priv(dev));

	netif_stop_queue(dev);

	return 0;
}

static int br_change_mtu(struct net_device *dev, int new_mtu)
{
	struct net_bridge *br = netdev_priv(dev);
	if (new_mtu < 68 || new_mtu > br_min_mtu(br))
		return -EINVAL;

#if 1 /* V54_BSP */
	if (br->des_mtu && br->des_mtu < new_mtu) new_mtu = br->des_mtu;
#endif
	dev->mtu = new_mtu;

#ifdef CONFIG_BRIDGE_NETFILTER
	/* remember the MTU in the rtable for PMTU */
	br->fake_rtable.u.dst.metrics[RTAX_MTU - 1] = new_mtu;
#endif

	return 0;
}

/* Allow setting mac address to any valid ethernet address. */
static int br_set_mac_address(struct net_device *dev, void *p)
{
	struct net_bridge *br = netdev_priv(dev);
	struct sockaddr *addr = p;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EINVAL;

	spin_lock_bh(&br->lock);
#if 1 /* yun.wu, 2010-12-03 */
	br_fdb_changeaddr_br(br, (unsigned char *)addr->sa_data); /* Add bridge mac address to FDB. */
#endif
	memcpy(dev->dev_addr, addr->sa_data, ETH_ALEN);
	br_stp_change_bridge_id(br, addr->sa_data);
	br->flags |= BR_SET_MAC_ADDR;
	spin_unlock_bh(&br->lock);

	return 0;
}

static void br_getinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
	strcpy(info->driver, "bridge");
	strcpy(info->version, BR_VERSION);
	strcpy(info->fw_version, "N/A");
	strcpy(info->bus_info, "N/A");
}

static int br_set_sg(struct net_device *dev, u32 data)
{
	struct net_bridge *br = netdev_priv(dev);

	if (data)
		br->feature_mask |= NETIF_F_SG;
	else
		br->feature_mask &= ~NETIF_F_SG;

	br_features_recompute(br);
	return 0;
}

static int br_set_tso(struct net_device *dev, u32 data)
{
	struct net_bridge *br = netdev_priv(dev);

	if (data)
		br->feature_mask |= NETIF_F_TSO;
	else
		br->feature_mask &= ~NETIF_F_TSO;

	br_features_recompute(br);
	return 0;
}

static int br_set_tx_csum(struct net_device *dev, u32 data)
{
	struct net_bridge *br = netdev_priv(dev);

	if (data)
		br->feature_mask |= NETIF_F_NO_CSUM;
	else
		br->feature_mask &= ~NETIF_F_ALL_CSUM;

	br_features_recompute(br);
	return 0;
}

static const struct ethtool_ops br_ethtool_ops = {
	.get_drvinfo    = br_getinfo,
	.get_link	= ethtool_op_get_link,
	.get_tx_csum	= ethtool_op_get_tx_csum,
	.set_tx_csum 	= br_set_tx_csum,
	.get_sg		= ethtool_op_get_sg,
	.set_sg		= br_set_sg,
	.get_tso	= ethtool_op_get_tso,
	.set_tso	= br_set_tso,
	.get_ufo	= ethtool_op_get_ufo,
	.get_flags	= ethtool_op_get_flags,
};

static const struct net_device_ops br_netdev_ops = {
	.ndo_open		 = br_dev_open,
	.ndo_stop		 = br_dev_stop,
	.ndo_start_xmit		 = br_dev_xmit,
	.ndo_set_mac_address	 = br_set_mac_address,
	.ndo_set_multicast_list	 = br_dev_set_multicast_list,
	.ndo_change_mtu		 = br_change_mtu,
	.ndo_do_ioctl		 = br_dev_ioctl,
};

void br_dev_setup(struct net_device *dev)
{
	random_ether_addr(dev->dev_addr);
	ether_setup(dev);

	dev->netdev_ops = &br_netdev_ops;
	dev->destructor = free_netdev;
	SET_ETHTOOL_OPS(dev, &br_ethtool_ops);
	dev->tx_queue_len = 0;
	dev->priv_flags = IFF_EBRIDGE;

	dev->features = NETIF_F_SG | NETIF_F_FRAGLIST | NETIF_F_HIGHDMA |
			NETIF_F_GSO_MASK | NETIF_F_NO_CSUM | NETIF_F_LLTX |
			NETIF_F_NETNS_LOCAL | NETIF_F_GSO;
}

/* Function to get device's owing bridge's net device */
struct net_device *br_net_device(struct net_device *dev)
{
	if (dev && dev->br_port && dev->br_port->br)
		return dev->br_port->br->dev;
	else
		return NULL;
}
EXPORT_SYMBOL(br_net_device);

#ifdef RKS_TUBE_SUPPORT
void br_tube_setup(struct net_device *dev, void *func, void *opaque)
{
	struct net_bridge *br = netdev_priv(dev);

	br->tube.func = func;
	br->tube.opaque = opaque;
}
EXPORT_SYMBOL(br_tube_setup);

void br_tube_clean(struct net_device *dev)
{
	struct net_bridge *br = netdev_priv(dev);

	br->tube.func = NULL;
	br->tube.opaque = NULL;
}
EXPORT_SYMBOL(br_tube_clean);
#endif

#ifdef CONFIG_BRIDGE_VLAN
int
br_vlan_outport_filter(struct sk_buff *skb, struct net_device *out_dev)
{
	return br_vlan_filter(skb, &skb->dev->br_port->vlan);
}
EXPORT_SYMBOL(br_vlan_outport_filter);

static inline  void
br_vlan_skb_set_vlan_tag(struct sk_buff *skb, struct net_bridge_port_vlan *vlan)
{
	if ( eth_hdr(skb)->h_proto != htons(ETH_P_8021Q) ) {
		// skb->tag_priority = 0;	// if tag_priority is non-zero, leave it alone
		// set to default vlan if 0
		skb->tag_vid = vlan->untagged;
	} else {
		// tagged frames
		struct vlan_ethhdr *vhdr;
		unsigned short vlan_TCI;
		unsigned short vid;

		vhdr = (struct vlan_ethhdr *)eth_hdr(skb);
		vlan_TCI = ntohs(vhdr->h_vlan_TCI);
		vid = (vlan_TCI & VLAN_VID_MASK);

		if ( skb->tag_priority == 0 ) {	// if not already set. (may be by smartcast?)
			skb->tag_priority = ((vlan_TCI >> 13) & 7);
		}
		skb->tag_vid = vid ? vid : vlan->untagged;
	}
	return;
}

//
// called from smartcast for directed MCast packets.
// The assumption here is that skb->data will be pointing to the
// Layer2 header similar to if dev_queue_xmit() is called.
// The current convention of packets traversing the bridge is that:
//
//   when packets enter bridge (after processed by br_vlan_input_frame):
//      skb->mac.raw points to mac header
//      skb->data, skb->h.raw and skb->nh.raw point to network header after vlan (if there is vlan header).
//      skb->protocol indicates the protocol encapsulated in vlan header (if there is vlan header).
//      skb->mac_len == 18 (if there is vlan header) or 14 (no vlan header).
//
//    br_vlan_dev_queue_xmit, br_dev_xmit and br_vlan_dev_mcast_xmit don't call br_vlan_input_frame,
//       so we need to do similar processing to conform above convertion.
//
//   when packets are passed up (after br_vlan_pass_frame_up):
//      skb->mac.raw points to mac header
//      skb->data points to vlan header (if there is vlan header) or network header (if there is no vlan header)
//      skb->h.raw and skb->nh.raw still point to network header after vlan (if there is vlan header).
//      skb->protocol indicates the protocol encapsulated in mac header.
//      skb->mac_len not touched.
//
//   when packets are xmitted out (after br_vlan_output_frame):
//      skb->mac.raw points to mac header
//      skb->data points to mac header
//      skb->h.raw and skb->nh.raw still point to network header after vlan (if there is vlan header).
//      skb->protocol indicates the protocol encapsulated in mac header.
//      skb->mac_len == 14 (even if there is vlan header).
int
br_vlan_dev_queue_xmit(struct sk_buff *skb, struct net_device *in_dev)
{
	int rc = 0;

	rcu_read_lock();

#if 0
	// skb Param checks_
	if ( skb == NULL || in_dev == NULL || skb->dev == NULL ) {
		LIMIT_PRINTK("*** %s %s is NULL\n", __FUNCTION__, (skb == NULL) ? "skb" : (in_dev == NULL) ? "in_dev": "skb->dev" );
		rc = -ENOBUFS;
		goto drop_it;
	}
#endif

	// check to see if the device is on the bridge.
	if ( in_dev->br_port == NULL || skb->dev->br_port == NULL ) {
		// input or output device not on the bridge
		// just send it out on the real device.
		LIMIT_PRINTK("*** %s %s not on bridge?\n", __FUNCTION__, (in_dev->br_port == NULL) ? "in_dev" : "skb->dev");
		rc = dev_queue_xmit(skb);
		goto done;
	}

	if ( skb->dev->br_port->state != BR_STATE_FORWARDING) {
		// LIMIT_PRINTK("*** %s.%d not forwarding\n", skb->dev->name, skb->tag_vid);
		rc = -ENETUNREACH;
		goto drop_it;
	}

	if ( skb->tag_vid == 0 ) {      // vlan tag unset
		// assumption: if skb->tag_vid is set, then skb->tag_priority is also set.
		br_vlan_skb_set_vlan_tag(skb, &in_dev->br_port->vlan);
#if 0   // DEBUG
	} else {
		LIMIT_PRINTK("*** %s.%d -> %s %s\n", in_dev->name, skb->tag_vid, skb->dev,
                     skb->dev->br_port->vlan.untagged == skb->tag_vid)? "ut" : "t");
#endif
	}

	if (br_vlan_filter(skb, &skb->dev->br_port->vlan)) {
		// not ours. Destination port not member of our VLAN.
#if 0
		LIMIT_PRINTK("*** bad vid: %s.%d -> %s %s\n", in_dev->name, skb->tag_vid,
			skb->dev->name,
			(skb->dev->br_port->vlan.untagged == skb->tag_vid)? "ut" : "t");
#endif
		rc = -ENETUNREACH;
		goto drop_it;
	}

	skb_reset_mac_header(skb);
	skb->mac_len = skb->network_header - skb->mac_header;
	skb_pull(skb, ETH_HLEN);

	rc = dev_queue_xmit(skb);
	goto done;
drop_it:
	kfree_skb(skb);
done:
	rcu_read_unlock();
	return rc;
}
EXPORT_SYMBOL(br_vlan_dev_queue_xmit);

//-----------------------------------------------------------
// send out a multicast packet on one vlan on
// a specific dev (specified bys kb->dev)
// assumption:
// the skb contains a packet to a multicast/broadcast address.
// returns -1 if failed
// return 0 if success.
static int
_br_vlan_if_mcast(struct net_bridge_fdb_entry *dst, struct sk_buff *skb, int vlan_id)
{
	struct sk_buff *skb2 ;
	struct net_bridge_port *to = dst->dst;

	// vlan header could change, make copy
	skb2 = skb_copy(skb, GFP_ATOMIC);
	if ( ! skb2 ) {
		LIMIT_PRINTK("%s: skb_copy failed\n", __FUNCTION__);
		return -1 ;
	}
	skb2->tag_vid = vlan_id;
	// assumed that skb->tag_priority has been set.
#if 0
	skb2->dev = to->dev;
	skb2->ip_summed = CHECKSUM_NONE;
	br_dev_queue_push_xmit(skb2);
#else
//	LIMIT_PRINTK("%s: to %s.%d\n", __FUNCTION__, to->dev->name, vlan_id);
	br_forward(to, skb2);
#endif
	return 0;
}

// API for sending multicast packets on all VLANs that
// the destination interface is a member of.
// for trunk ports, the packet will be send out on all "active" VLANs
// i.e VLANs for that interface that shows up on the FDB
//
// This function is intended to be called for locally generated
// multi-cast packets.
//
// dev is the outbound device interface
// and skb->dev should be the bridge device;
//
// int br_dev_mcast_xmit(struct sk_buff *skb, struct net_device *dev)
// non multicast packets will be dropped.
//
// returns < 0 for failure
// returns 0 for success.
// In all cases the skb is consumed.
int
br_vlan_dev_mcast_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct net_bridge *br;
	const unsigned char *dest = skb->data;
	int rc = 0;
	int num;

	if ( ! dev ) {
		rc = -ENODEV;
		goto drop_it;
	}

	if ( dest[0] & 1 ) {
	} else {
		// not a multicast packet
		rc = -ENXIO;
		goto drop_it;
	}

	if ( dev->br_port == NULL ) {
		// output device not on the bridge
		// just send it out on the real device.
		LIMIT_PRINTK("*** %s dev not on bridge?\n", __FUNCTION__);
		rc = -ENODEV;
		goto drop_it;
	}

	if ( dev->br_port->state != BR_STATE_FORWARDING ) {
//		LIMIT_PRINTK("*** %s.%d not forwarding\n", dev->name, skb->tag_vid);
		rc = -ENETUNREACH;
		goto drop_it;
	}
//	rcu_read_lock(); // lock taken in br_fdb_vlan_exec()

	br = dev->br_port->br;

	if ( ! skb->dev ) {
		// set it to the bridge device
		skb->dev = br->dev;
		skb->input_dev = br->dev;
	}
	if ( ! skb->input_dev ) {
		skb->input_dev = skb->dev;
	}

	skb_reset_mac_header(skb);
	skb_pull(skb, ETH_HLEN);

	skb->mac_len = skb->network_header - skb->mac_header;

	num = br_fdb_vlan_exec(dev->br_port, skb, _br_vlan_if_mcast);
	// _br_vlan_mcast does an skb_copy before sending, need to free
	// original skb
	if ( num <= 0 ) {
		// did not send any
		rc = -EFAULT;
	} else {
		br->dev->stats.tx_packets += num;
		br->dev->stats.tx_bytes += (num * skb->len);
		TX_BROADCAST_STATS((&br->dev->stats), skb, num);
	}

drop_it:
	kfree_skb(skb);
// done:
//	rcu_read_unlock(); // lock taken in br_fdb_vlan_exec()
	return rc;
}
EXPORT_SYMBOL(br_vlan_dev_mcast_xmit);
#endif
