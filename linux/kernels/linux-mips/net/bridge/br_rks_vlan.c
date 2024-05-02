/*
 *     VLAN support
 *     Linux ethernet bridge
 *
 *     Authors:
 *     Simon Barber        <simon@xxxxxxxxxxxxxxx>
 *
 *     This program is free software; you can redistribute it and/or
 *     modify it under the terms of the GNU General Public License
 *     as published by the Free Software Foundation; either version
 *     2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/rtnetlink.h>
#include <linux/skbuff.h>
#include <linux/if_bridge.h>
#include <linux/if_ether.h>
#include <linux/netfilter_bridge.h>
#include <asm/uaccess.h>
#include "br_private.h"
#include <ruckus/rks_if_bridge.h>
#include <ruckus/rks_api_ver.h>


int br_vlan_filter(const struct sk_buff *skb, const struct net_bridge_port_vlan *vlan)
{
    return !(vlan->filter[skb->tag_vid / 8] & (1 << (skb->tag_vid & 7)));
}

int br_vlan_input_frame(struct sk_buff *skb, struct net_bridge_port_vlan *vlan)
{
    struct vlan_ethhdr *vhdr;
    int pull_vhdr = 0;
    unsigned short vlan_TCI;
    unsigned short vid;

    if (skb->protocol != __constant_htons(ETH_P_8021Q)) {
        if (unlikely(0 == vlan->untagged)) {

            skb->tag_priority = 0;
            skb->tag_vid = 0;
            return 0;
        } else if (skb->tag_vid == vlan->untagged) {
        } else if (skb->tag_vid) {
        } else {
            skb->tag_priority = 0;
            skb->tag_vid = vlan->untagged;
        }
    } else {
        vhdr = vlan_eth_hdr(skb);
        vlan_TCI = ntohs(vhdr->h_vlan_TCI);
        vid = (vlan_TCI & VLAN_VID_MASK);

        skb->tag_priority = ((vlan_TCI >> 13) & 7);
        skb->tag_vid = vid ? vid : vlan->untagged;

        if (skb->tag_vid == 0)
            goto err;

        pull_vhdr = 1;
    }

    if (br_vlan_filter(skb, vlan)) {
        goto err;
    }

    if (pull_vhdr) br_vlan_pull_8021q_header(skb);
    return 0;

 err:
    kfree_skb(skb);
    return 1;
}


static inline int _br_vlan_untagged_frame(struct sk_buff **pskb)
{
    struct sk_buff *skb = *pskb;

    if (eth_hdr(skb)->h_proto == __constant_htons(ETH_P_8021Q)) {
        if (skb_cloned(skb) || skb_shared(skb)) {
            struct sk_buff *new_skb;

            new_skb = skb_copy(skb, GFP_ATOMIC);
            kfree_skb(skb);
            if (!new_skb)
                return 1;
            *pskb = skb = new_skb;
        }


        skb->mac_header += VLAN_HLEN;
        memmove(skb_mac_header(skb), skb_mac_header(skb) - VLAN_HLEN, ETH_ALEN * 2);

        if (ntohs(eth_hdr(skb)->h_proto) >= 1536)
            skb->protocol = eth_hdr(skb)->h_proto;
        else if (*((unsigned short *)skb->data) == 0xFFFF)
            skb->protocol = __constant_htons(ETH_P_802_3);
        else
            skb->protocol = __constant_htons(ETH_P_802_2);
    }
    return 0;
}


int br_vlan_output_frame(struct sk_buff **pskb, unsigned int untagged, unsigned int dvlan)
{
    struct sk_buff *skb = *pskb;

    skb_push(skb, ETH_HLEN);
    if (eth_hdr(skb)->h_proto == __constant_htons(ETH_P_8021Q)) {
        skb->mac_len -= VLAN_HLEN;
    }

    if (skb->tag_vid == 0)
        return 0;

    if (skb->tag_vid == untagged || dvlan) {
        if (_br_vlan_untagged_frame(pskb)) {
            return 1;
        }
    } else {
        if (eth_hdr(skb)->h_proto != __constant_htons(ETH_P_8021Q)) {
            struct vlan_ethhdr *vhdr;
            if (skb_cloned(skb) || skb_shared(skb)) {
                struct sk_buff *new_skb;

                new_skb = skb_copy(skb, GFP_ATOMIC);
                kfree_skb(skb);
                if (!new_skb)
                    return 1;
                *pskb = skb = new_skb;
            }

            if (skb_headroom(skb) < VLAN_HLEN) {
                if (pskb_expand_head(skb, VLAN_HLEN, 0,
                                     GFP_ATOMIC)) {
                    kfree_skb(skb);
                    return 1;
                }
            }

            skb_push(skb, VLAN_HLEN);

            skb->mac_header -= VLAN_HLEN;
            memmove(skb_mac_header(skb), skb_mac_header(skb) + VLAN_HLEN, ETH_ALEN * 2);

            vhdr = (struct vlan_ethhdr *)skb_mac_header(skb);
            vhdr->h_vlan_proto = __constant_htons(ETH_P_8021Q);
            vhdr->h_vlan_TCI = htons(skb->tag_priority << 13 | skb->tag_vid);
        } else {
            struct vlan_ethhdr *vhdr =
                (struct vlan_ethhdr *)skb_mac_header(skb);
            vhdr->h_vlan_TCI = htons(skb->tag_priority << 13 | skb->tag_vid);
            skb_push(skb, VLAN_HLEN);
        }
        skb->protocol = __constant_htons(ETH_P_8021Q);
    }

    return 0;
}


int br_vlan_pass_frame_up(struct sk_buff **pskb, unsigned int untagged)
{
    struct sk_buff *skb = *pskb;

    if (skb->tag_vid == 0)
        return 0;

    if (eth_hdr(skb)->h_proto == __constant_htons(ETH_P_EAPOL)
        || (eth_hdr(skb)->h_proto == __constant_htons(ETH_P_LLDP))) {
        return 0;
    }

    if (skb->tag_vid == untagged) {

        if (_br_vlan_untagged_frame(pskb)) {
            return 1;
        }
    } else {

        if (eth_hdr(skb)->h_proto != __constant_htons(ETH_P_8021Q)) {
            skb->protocol = __constant_htons(ETH_P_8021Q);
        } else {
            struct vlan_ethhdr *vhdr =
                (struct vlan_ethhdr *)skb_mac_header(skb);
            vhdr->h_vlan_TCI = htons(skb->tag_priority << 13 | skb->tag_vid);
            skb_push(skb, VLAN_HLEN);
            skb->protocol = __constant_htons(ETH_P_8021Q);
        }
    }
    return 0;
}


static void br_vlan_reset_dvlan(struct net_bridge_port_vlan *vlan, int on)
{
     if (!on) {
        memset(vlan->dvlan_mask, 0xff, sizeof(vlan->dvlan_mask));
        vlan->dvlan_mask[0] &= 0xfe;
        vlan->dvlan_mask[sizeof(vlan->dvlan_mask)-1] &= 0x7f;
    } else {
        memset(vlan->dvlan_mask, 0, sizeof(vlan->dvlan_mask));
    }
}


void br_vlan_init(struct net_bridge_port_vlan *vlan)
{
    vlan->untagged = BR_DEFAULT_VLAN;
    vlan->filter[0] = 1 << BR_DEFAULT_VLAN;
    br_vlan_reset_dvlan(vlan, 0);
}


void br_vlan_add_del_dvlan(struct net_device *dev, unsigned short vid, int add)
{
    struct net_bridge_port *p = br_port_get_rcu(dev);

    if (!p || !(p->flags & BR_DVLAN_PORT_MODE)) return;

    if (add)
        p->vlan.dvlan_mask[vid / 8] |= 1 << (vid & 7);
    else
        p->vlan.dvlan_mask[vid / 8] &= ~(1 << (vid & 7));
}


int br_vlan_set_dvlan(struct net_bridge *br,
                  unsigned int port, unsigned int enable)
{
    struct net_bridge_port *p;

    if ((port == 0) || (p = br_get_port(br, port)) == NULL)
        return -EINVAL;

    if (enable) {
        if (0 == (p->flags & BR_DVLAN_PORT_MODE))
            br_vlan_reset_dvlan(&p->vlan, 1);
        p->flags |= BR_DVLAN_PORT_MODE;
    } else {
        if ((p->flags & BR_DVLAN_PORT_MODE))
            br_vlan_reset_dvlan(&p->vlan, 0);
        p->flags &= ~BR_DVLAN_PORT_MODE;
    }

    return 0;
}



int br_vlan_set_vlan_fork_br(struct net_bridge *br,
                      unsigned int port, unsigned int enable)
{
    struct net_bridge_port *p;

    if ((port == 0) || (p = br_get_port(br, port)) == NULL)
        return -EINVAL;

    if (enable) {
        p->flags |= BR_VLAN_FORK_BR_MODE;
    } else {
        p->flags &= ~BR_VLAN_FORK_BR_MODE;
    }

    return 0;
}


int br_vlan_set_pvid_fork_br(struct net_bridge *br,
                             unsigned int port, unsigned int vid)
{
    struct net_bridge_port *p;
    struct net_bridge_port_vlan *vlan;

    if (vid > 4094)
        return -EINVAL;

    if ((port == 0) || (p = br_get_port(br, port)) == NULL)
        return -EINVAL;

    vlan = &p->vlan;

    vlan->pvid_fork_bridge = vid;
    return 0;
}


int br_vlan_set_untagged(struct net_bridge *br,
                     unsigned int port, unsigned int vid)
{
    struct net_bridge_port *p;
    struct net_bridge_port_vlan *vlan;

    if (vid > 4094)
        return -EINVAL;

    if ((port == 0) || (p = br_get_port(br, port)) == NULL)
        return -EINVAL;

    vlan = &p->vlan;

    vlan->filter[vid / 8] |= 1 << (vid & 7);

    vlan->untagged = vid;

    return 0;
}



int br_vlan_set_tagged(struct net_bridge *br,
                   unsigned int port, unsigned int vid)
{
    struct net_bridge_port *p;
    struct net_bridge_port_vlan *vlan;

    if (vid > 4094)
        return -EINVAL;

    if ((port == 0) || (p = br_get_port(br, port)) == NULL)
        return -EINVAL;

    vlan = &p->vlan;
    if (vlan->untagged == vid)
        vlan->untagged = 0;

    return 0;
}


int br_vlan_set_filter(struct net_bridge *br,
                   unsigned int cmd, unsigned int port,
                   unsigned int vid1, unsigned int vid2)
{
    unsigned int vid;
    struct net_bridge_port *p;
    struct net_bridge_port_vlan *vlan;
    int add = (cmd == BR_IOCTL_ADD_PORT_VLAN);

    if (vid1 < 1 || vid2 > 4094)
        return -EINVAL;

    if ((port == 0) || (p = br_get_port(br, port)) == NULL)
        return -EINVAL;
    vlan = &p->vlan;

    for (vid = vid1; vid <= vid2; vid++) {
        if (add) {
            vlan->filter[vid / 8] |= 1 << (vid & 7);
        } else {
            vlan->filter[vid / 8] &= ~(1 << (vid & 7));
            if (vid == vlan->untagged) vlan->untagged = 0;
        }
    }

    return 0;
}

int br_vlan_get_untagged(struct sk_buff *skb)
{
    struct net_bridge_port *port;

    if (unlikely(skb->pkt_type == PACKET_LOOPBACK)) {
        return 0;
    } else if (skb->tag_vid != 0) {
        return skb->tag_vid;
    } else {
        port = br_port_get_rcu(netif_is_bond_slave(skb->dev) ? RKS_MASTER_DEV(skb->dev) : skb->dev);
        return (port == NULL) ? 0 : port->vlan.untagged;
    }
}


void skb_set_vlan_info(struct sk_buff *skb)
{
    struct net_bridge_port *port;
    struct vlan_ethhdr *vhdr;
    unsigned short vlan_TCI, vid;

    if (__constant_htons(ETH_P_8021Q) == skb->protocol) {
        vhdr = vlan_eth_hdr(skb);
        vlan_TCI = ntohs(vhdr->h_vlan_TCI);

        skb->tag_priority = ((vlan_TCI >> VLAN_PRIO_SHIFT) & 7);

        vid = (vlan_TCI & VLAN_VID_MASK);
        if (unlikely(vid == 0)) {
            port = br_port_get_rcu(netif_is_bond_slave(skb->dev) ?
                                   RKS_MASTER_DEV(skb->dev) : skb->dev);
            if (NULL != port) {
                vid = port->vlan.untagged;
            }
        }
        skb->tag_vid = vid;
    } else {
        if (skb->tag_vid == 0) {
            skb->tag_priority = 0;
            port = br_port_get_rcu(netif_is_bond_slave(skb->dev) ?
                                   RKS_MASTER_DEV(skb->dev) : skb->dev);

            if (unlikely(NULL == port)) {
            } else if (port->flags & BR_VLAN_FORK_BR_MODE) {

                skb->tag_vid = port->vlan.pvid_fork_bridge;

                if (unlikely(skb->protocol == __constant_htons(ETH_P_EAPOL)) ||
                    unlikely(skb->protocol == __constant_htons(ETH_P_LLDP))) {
                    skb->tag_vid = 1;
                } else if (skb->tag_vid == 0) {
                } else {
                    skb->protocol = __constant_htons(ETH_P_8021Q);
                    skb->tag_cfi = 1;
                }
            } else {
                skb->tag_vid = port->vlan.untagged;
            }
        } else {
        }
    }

}

int br_vlan_get_info(struct net_bridge *br, void *user_mem, unsigned long port)
{
    struct net_bridge_port *p;
    struct net_bridge_port_vlan *vlan;
    struct __vlan_info v;

    if ((port == 0) || (p = br_get_port(br, port)) == NULL)
        return -EINVAL;
    vlan = &p->vlan;

    memset(&v, 0, sizeof(v));
    v.untagged = vlan->untagged;
    if (p->flags & BR_DVLAN_PORT_MODE)
        memcpy(v.filter, vlan->dvlan_mask, 4096 / 8);
    else
        memcpy(v.filter, vlan->filter, 4096 / 8);

    if (copy_to_user((void __user *)user_mem, &v, sizeof(v)))
        return -EFAULT;

    return 0;
}


static inline void br_vlan_skb_set_vlan_tag(struct sk_buff *skb, struct net_bridge_port_vlan *vlan)
{
	if (eth_hdr(skb)->h_proto != __constant_htons(ETH_P_8021Q)) {
		skb->tag_vid = vlan->untagged;
	} else {
		struct vlan_ethhdr *vhdr;
		unsigned short vlan_TCI;
		unsigned short vid;

		vhdr = (struct vlan_ethhdr *)eth_hdr(skb);
		vlan_TCI = ntohs(vhdr->h_vlan_TCI);
		vid = (vlan_TCI & VLAN_VID_MASK);

		if (skb->tag_priority == 0) {
			skb->tag_priority = ((vlan_TCI >> 13) & 7);
		}
		skb->tag_vid = vid ? vid : vlan->untagged;
	}
	return;
}

bool br_is_pvid_set(struct net_bridge_port *p)
{
	if (p->vlan.untagged)
		return true;

	return false;
}
EXPORT_SYMBOL(br_is_pvid_set);



void br_vlan_pull_8021q_header(struct sk_buff *skb)
{
    struct vlan_ethhdr *vhdr;

    skb_pull(skb, VLAN_HLEN);
    skb_reset_network_header(skb);
    skb_reset_transport_header(skb);
    skb->mac_len = skb->network_header - skb->mac_header;
    vhdr = vlan_eth_hdr(skb);
    skb->protocol = vhdr->h_vlan_encapsulated_proto;
}

//-----------------------------------------------------------

static int _br_vlan_if_mcast(struct net_bridge_fdb_entry *dst, struct sk_buff *skb, int vlan_id)
{
	struct sk_buff *skb2;
	struct net_bridge_port *to = dst->dst;

	skb2 = skb_copy(skb, GFP_ATOMIC);
	if (!skb2) {
		LIMIT_PRINTK("%s: skb_copy failed\n", __FUNCTION__);
		return -1;
	}

	skb2->tag_vid = vlan_id;

	br_forward(to, skb2);

	return 0;
}




static inline void rks_set_bit(unsigned char *bits, unsigned int idx)
{
	bits[idx / 8] |= (1 << (idx & 7));
}

static inline int rks_bit_is_set(const unsigned char *bits, unsigned int idx)
{
	return (bits[idx / 8] & (1 << (idx & 7))) ? 1 : 0;
}

static void __br_vlan_dev_mcast_xmit_callback(struct net_bridge_fdb_entry *f,
	void *arg1, void *arg2, void *arg3, void *arg4)
{
	struct sk_buff *skb = (struct sk_buff *)arg1;
	struct net_bridge_port *br_port = (struct net_bridge_port *)arg2;
	struct net_bridge_port_vlan *done_vlans = (struct net_bridge_port_vlan *)arg3;
	unsigned int *num = (unsigned int *)arg4;
	unsigned int vlan_id = f->vlan_id;

	if (f->is_local) {
	} else if (f->dst != br_port) {
	} else if (rks_bit_is_set(done_vlans->filter, vlan_id)) {
	} else {
		rks_set_bit(done_vlans->filter, vlan_id);
		if (0 == _br_vlan_if_mcast(f, skb, vlan_id)) {
			(*num)++;
		}
	}
}

#ifdef CONFIG_BRIDGE_VLAN

int br_vlan_handler_for_fastpath(struct sk_buff **pskb)
{
	unsigned int dvlan = 0, untagged = 0;
	struct sk_buff *skb = *pskb;
	struct net_bridge_port *to = br_port_get_rcu(skb->dev);

	if (likely(to)) {
		untagged = to->vlan.untagged;
		dvlan = to->flags & BR_DVLAN_PORT_MODE;
	} else {
		skb->tag_vid = 0;
	}

	if (eth_hdr(skb)->h_proto == __constant_htons(ETH_P_8021Q))
		br_vlan_pull_8021q_header(skb);

	return br_vlan_output_frame(pskb, untagged, dvlan);
}

#else /*CONFIG_BRIDGE_VLAN*/
int br_vlan_handler_for_fastpath(struct sk_buff **pskb)
{
	skb_push(*pskb, ETH_HLEN);
	return 0;
}
#endif /*CONFIG_BRIDGE_VLAN*/
EXPORT_SYMBOL(br_vlan_handler_for_fastpath);
