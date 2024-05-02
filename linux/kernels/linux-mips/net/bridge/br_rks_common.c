/*
 * Copyright 2014 Ruckus Wireless, Inc. All rights reserved.
 *
 * RUCKUS WIRELESS, INC. CONFIDENTIAL -
 * This is an unpublished, proprietary work of Ruckus Wireless, Inc., and is
 * fully protected under copyright and trade secret laws. You may not view,
 * use, disclose, copy, or distribute this file or any information contained
 * herein except pursuant to a valid license from Ruckus.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/capability.h>
#include <linux/kernel.h>
#include <linux/if_bridge.h>
#include <linux/netdevice.h>
#include <linux/slab.h>
#include <linux/times.h>
#include <net/net_namespace.h>
#include <asm/uaccess.h>
#include "br_private.h"
#include <ruckus/rks_api_ver.h>
#include <ruckus/rks_if_bridge.h>
#include <ruckus/rks_parse.h>
#include <linux/in.h>
#include <net/addrconf.h>


#ifdef CONFIG_BRIDGE_PACKET_INSPECTION_FILTER_MODULE


int (*br_pif_parse_ra_rs_msg_hook)(struct sk_buff *skb, struct ra_rs_parsed *ra_rs_info) __read_mostly = NULL;
EXPORT_SYMBOL(br_pif_parse_ra_rs_msg_hook);

int (*br_pif_if_ingress_hook)(struct sk_buff *, unsigned int ) = NULL;
EXPORT_SYMBOL(br_pif_if_ingress_hook);
#endif
int rks_get_if_index(const unsigned char *if_name,
             unsigned short *byte_offset, unsigned short *bit_offset);
EXPORT_SYMBOL(rks_get_if_index);
#define MF_WLAN_NAME          "wlan"
#define MF_ETH_NAME           "eth"
#define MF_MAX_WIF              61
#define MF_MAX_EIF              8
int rks_ratelimit_token_bucket_util(struct sk_buff *skb, struct ratelimit_cfg *bucket);
EXPORT_SYMBOL(rks_ratelimit_token_bucket_util);
int data_is_mdns_packet(rks_pkt_info_t *pkt_info)
{
    if ((__constant_htons(PORT_MDNS) == pkt_info->l4_dst_port) ||
            (__constant_htons(PORT_MDNS) == pkt_info->l4_src_port)) {
        return 1;
    }
    return 0;
}
EXPORT_SYMBOL(data_is_mdns_packet);

void *br_attached(const struct net_device *dev)
{
	struct net_bridge_port *br_port;
	const struct net_device *master_dev;

	if (!dev)
		return NULL;

	if (dev->priv_flags & IFF_EBRIDGE)
		return netdev_priv(dev);


	if (netif_is_bond_slave((struct net_device *)dev))
		master_dev = RKS_MASTER_DEV(dev);
	else
		master_dev = dev;

	br_port = br_port_get_rcu(master_dev);
	return br_port ? br_port->br : NULL;
}
EXPORT_SYMBOL(br_attached);

struct net_device *br_fdb_dev_get(const struct net_device *br_dev, const unsigned char *mac, __u16 vlan, int *tagging)
{
	struct net_bridge_fdb_entry *fdb;

	fdb = __BR_FDB_GET((struct net_bridge *)netdev_priv(br_dev), mac, vlan);
    if ((NULL != fdb) && (NULL != fdb->dst)) {

        *tagging = (fdb->dst->vlan.untagged == vlan) ? 0 : 1;
        return fdb->dst->dev;
    } else {
        return NULL;
    }
}
EXPORT_SYMBOL(br_fdb_dev_get);

struct net_device *br_fdb_dev_get_and_hold(const struct net_device *br_dev, const unsigned char *mac, __u16 vlan, int *tagging)
{
    struct net_device *dev = NULL;

	rcu_read_lock_bh();
    dev = br_fdb_dev_get(br_dev, mac, vlan, tagging);
	if (NULL != dev) {
        dev_hold(dev);
	}
    rcu_read_unlock_bh();
    return dev;
}
EXPORT_SYMBOL(br_fdb_dev_get_and_hold);


void br_rks_prvt_setup(struct br_rks_prvt *rksprvt)
{
}


int br_set_port_type(struct net_bridge *br, int ifindex, int port_type)
{
        struct net_device *dev = NULL;
        struct net_bridge_port *p = NULL;
        int ret_val = 0;

	if((port_type < PORT_TYPE_MIN) || (port_type > PORT_TYPE_MAX)) {
		ret_val = -EINVAL;
		goto done;
	}

        dev = dev_get_by_index(dev_net(br->dev), ifindex);
        if (unlikely(dev == NULL)) {
                ret_val = -EINVAL;
                goto done;
        }

        p = br_port_get_rcu(dev);
        if (unlikely(!p || p->br != br)) {
                ret_val = -EINVAL;
                goto put_dev;
        }

        dev->port_type = (unsigned char)port_type;

        printk("[%s:%d] - Port type configuration applied, dev - %s, port type - %d\n",
               __func__, __LINE__, dev->name, dev->port_type);
put_dev:
        dev_put(dev);

done :
        return ret_val;
}

int br_mf_get_index(const unsigned char *ifname, int *ifidx)
{
        unsigned char  *pIf  = NULL;
        int rv = 1;

        if(unlikely((NULL == ifname) || (NULL == ifidx))) {
                rv = 0;
                goto done;
        }

        pIf = strstr(ifname, MF_WLAN_NAME);

        if(likely(pIf)) {
                *ifidx = simple_strtol(pIf + strlen(MF_WLAN_NAME), NULL, 0);
                if(unlikely(*ifidx > MF_MAX_WIF)) {
                    rv = 0;
                }
        }else{
            pIf = strstr(ifname, MF_ETH_NAME);

            if(likely(pIf)) {
                *ifidx = simple_strtol(pIf + strlen(MF_ETH_NAME), NULL, 0);
                if(unlikely(*ifidx > MF_MAX_EIF)) {
                    rv = 0;
                }
            }
        }

done:
        return rv;
}
EXPORT_SYMBOL(br_mf_get_index) ;
int rks_ratelimit_token_bucket_util(struct sk_buff *skb, struct ratelimit_cfg *bucket)
{
        int action = RKS_MCAST_ACTION_ALLOW;
        unsigned long long  time_now = 0;

        if (unlikely(!skb) || unlikely(!bucket)) {
                goto allow;
        }

        time_now = jiffies_to_msecs(jiffies);
        if (time_after_eq(time_now, bucket->mcast_timestamp + 1000)) {
                 bucket->mcast_timestamp = time_now;
                bucket->mcast_token_count = bucket->mcast_rl_val;
        } else {
		if (bucket->mcast_token_count < skb->len) {
                        action = RKS_MCAST_ACTION_DROP;
                } else {
                        bucket->mcast_token_count -= skb->len;
                }
        }

allow:
        return action;
}
static inline int br_set_eapol_filter(struct net_bridge *br, int eapol_filter)
{
	spin_lock_bh(&br->lock);
	br->eapol_filter = eapol_filter ? 1 : 0;
	spin_unlock_bh(&br->lock);
	return 0;
}

int rks_get_if_index(const unsigned char *if_name,
             unsigned short *byte_offset, unsigned short *bit_offset) {

        unsigned char  *pif  = 0;
        int            ifidx = -1;
        int            err = -1;


        if ((!if_name) || (!byte_offset) || (!bit_offset))
                goto ret_err;

        *byte_offset = *bit_offset = 0;

        pif = strstr(if_name, RKS_WIF_STR);
        if (!pif) {
                pif = strstr(if_name, RKS_ETH_STR);
                if (pif) {
                        ifidx = simple_strtol(pif + strlen(RKS_ETH_STR), NULL, 0);

                        if ((ifidx >= RKS_MAX_EIF) || (ifidx == -1))
                                goto ret_err;

                        *byte_offset = RKS_ETH_BIT_OFFSET;
                }
        } else {
                ifidx = simple_strtol(pif + strlen(RKS_WIF_STR), NULL, 0);
                if ((ifidx >= RKS_MAX_WIF) || (ifidx == -1))
                        goto ret_err;

                *byte_offset = ifidx / CHAR_BIT;
        }

        *bit_offset  = ifidx % CHAR_BIT;

        return ifidx;

ret_err:
        return err;
}
static inline int __br_rks_ioctl(unsigned int cmd, struct net_bridge *br, unsigned long args[])
{
	switch (cmd) {




	case BR_IOCTL_SET_PORT_STP_FORWARD_DROP:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		return br_set_port_stp_forward_drop(br, args[0], args[1]);

	case BR_IOCTL_SET_BRIDGE_DESIRE_MTU:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		br->des_mtu = args[0];
		dev_set_mtu(br->dev, br_min_mtu(br));
		return 0;


	case BR_IOCTL_GET_PORT_ATTRIB_FOR_MAC: {
		struct net_bridge_fdb_entry *fdb;
		int rc = 0, port_num;
		unsigned char mac_addr[ETH_ALEN];
		unsigned int vid;

		if (copy_from_user(mac_addr, (void __user *)args[0], ETH_ALEN))
			return -EFAULT;

#ifdef CONFIG_BRIDGE_VLAN
		if (copy_from_user(&vid, (void __user *)args[1], sizeof(vid)))
			return -EFAULT;

		if (vid == 0) {
			vid = BR_DEFAULT_VLAN;
		}
		fdb = __br_fdb_get(br, vid, mac_addr);
#else
		fdb = __br_fdb_get(br, mac_addr, vid);
#endif

		if (!fdb || !fdb->dst || !fdb->dst->dev) {
			return -EFAULT;
			/* Pass ifindex back */
		} else if (copy_to_user((void __user *)args[1],
				&(fdb->dst->dev->ifindex), sizeof(int))) {
			rc = -EFAULT;
			/* Pass port number as well */
		} else {
			port_num = fdb->dst->port_no;
			if (copy_to_user((void __user *)args[2], &port_num, sizeof(int)))
				rc = -EFAULT;
		}

		return rc;
	}

#ifdef CONFIG_BRIDGE_VLAN
	case BR_IOCTL_SET_PORT_UNTAGGED_VLAN:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		return br_vlan_set_untagged(br, args[0], args[1]);

	case BR_IOCTL_ADD_PORT_VLAN:
	case BR_IOCTL_DEL_PORT_VLAN:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		return br_vlan_set_filter(br, cmd, args[0], args[1], args[2]);

	case BR_IOCTL_GET_PORT_VLAN_INFO:
		return br_vlan_get_info(br, (void *)args[0], args[1]);

	case BR_IOCTL_SET_PORT_TAGGED_VLAN:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		return br_vlan_set_tagged(br, args[0], args[1]);

	case BR_IOCTL_SET_PORT_DVLAN:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		return br_vlan_set_dvlan(br, args[0], args[1]);

	case BR_IOCTL_SET_PORT_VLAN_FORK_BRIDGE:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		return br_vlan_set_vlan_fork_br(br, args[0], args[1]);

	case BR_IOCTL_SET_PORT_PVID_FORK_BRIDGE:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		return br_vlan_set_pvid_fork_br(br, args[0], args[1]);
#endif

	default:
		return -EOPNOTSUPP;
	}
}

static inline int br_rks_ioctl_get_which_bridge(unsigned int length, void *data)
{
	struct req_br_rks_ioctl *rq = (typeof(rq))data;
	struct net_device *dev;
	struct net_bridge_port *br_port;
	unsigned long ifidx = 0;

	if (rq->brname[0] != '\0') {
		dev = dev_get_by_name(
	                          &init_net,
	                          rq->brname);
	} else {
		dev = dev_get_by_index(
	                           &init_net,
			                   (int)rq->args[0]);
	}
	if (NULL == dev) {
		return -ENODEV;
	}

	if (NULL != (br_port = br_port_get_rcu(dev))) {
		ifidx = (unsigned long)br_port->br->dev->ifindex;
	}
	dev_put(dev);

	if (copy_to_user((void __user *)rq->args[1], &ifidx, sizeof(ifidx))) {
		return -EFAULT;
	}
	return 0;
}

int br_rks_ioctl(unsigned int cmd, unsigned int length, void *data)
{
	struct req_br_rks_ioctl *rq = (typeof(rq))data;
	struct net_device *dev;
	int rv;

	if (NULL == (dev = dev_get_by_name(
	                                   &init_net,
	                                   rq->brname))) {
		return -ENODEV;
	}

	rtnl_lock();
	rv = __br_rks_ioctl(cmd, netdev_priv(dev), rq->args);
	rtnl_unlock();
	dev_put(dev);
	return rv;
}
EXPORT_SYMBOL(br_rks_ioctl);



unsigned int br_loop_hold = LOOP_HOLD_JIFFIES;


int br_loop_detected(struct net_bridge *br, const unsigned char *addr)
{
	int detected = false;
	struct net_bridge_mac_change *mac_track = &br->mac_track;

	if (time_after(jiffies, mac_track->track_window)) {
		mac_track->track_window = jiffies + LOOP_DETECT_WINDOW;
		mac_track->flap_cnt = 1;
	} else {
		if (++mac_track->flap_cnt > br->flap_threshold) {
			detected = true;
		}
	}
	return detected;
}