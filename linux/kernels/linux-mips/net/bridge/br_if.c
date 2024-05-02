/*
 *	Userspace interface
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
#include <linux/ethtool.h>
#include <linux/if_arp.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/rtnetlink.h>
#include <linux/if_ether.h>
#include <net/sock.h>

#include "br_private.h"

#if defined(CONFIG_CI_WHITELIST) || defined (CONFIG_SOURCE_MAC_GUARD)
extern int (*br_ci_filter_hook)(const struct sk_buff *, const struct net_bridge_port *,unsigned short );
extern int br_ci_filter(const struct sk_buff *, const struct net_bridge_port *, const unsigned short);
#endif /* CONFIG_CI_WHITELIST || CONFIG_SOURCE_MAC_GUARD */

/*
 * Determine initial path cost based on speed.
 * using recommendations from 802.1d standard
 *
 * Since driver might sleep need to not be holding any locks.
 */
static int port_cost(struct net_device *dev)
{
	if (dev->ethtool_ops && dev->ethtool_ops->get_settings) {
		struct ethtool_cmd ecmd = { .cmd = (u32) ETHTOOL_GSET, };

		if (!dev->ethtool_ops->get_settings(dev, &ecmd)) {
			switch(ecmd.speed) {
			case SPEED_10000:
				return 2;
			case SPEED_1000:
				return 4;
			case SPEED_100:
				return 19;
			case SPEED_10:
				return 100;
			}
		}
	}

	/* Old silly heuristics based on name */
	if (!strncmp(dev->name, "lec", 3))
		return 7;

	if (!strncmp(dev->name, "plip", 4))
		return 2500;

	return 100;	/* assume old 10Mbps */
}


/*
 * Check for port carrier transistions.
 * Called from work queue to allow for calling functions that
 * might sleep (such as speed check), and to debounce.
 */
void br_port_carrier_check(struct net_bridge_port *p)
{
	struct net_device *dev = p->dev;
	struct net_bridge *br = p->br;

	if (netif_carrier_ok(dev))
		p->path_cost = port_cost(dev);

	if (netif_running(br->dev)) {
		spin_lock_bh(&br->lock);
		if (netif_carrier_ok(dev)) {
			if (p->state == BR_STATE_DISABLED)
				br_stp_enable_port(p);
		} else {
			if (p->state != BR_STATE_DISABLED)
				br_stp_disable_port(p);
		}
		spin_unlock_bh(&br->lock);
	}
}

static void release_nbp(struct kobject *kobj)
{
	struct net_bridge_port *p
		= container_of(kobj, struct net_bridge_port, kobj);
	kfree(p);
}

static struct kobj_type brport_ktype = {
#ifdef CONFIG_SYSFS
	.sysfs_ops = &brport_sysfs_ops,
#endif
	.release = release_nbp,
};

static void destroy_nbp(struct net_bridge_port *p)
{
	struct net_device *dev = p->dev;

	p->br = NULL;
	p->dev = NULL;
	dev_put(dev);

	kobject_put(&p->kobj);
}

static void destroy_nbp_rcu(struct rcu_head *head)
{
	struct net_bridge_port *p =
			container_of(head, struct net_bridge_port, rcu);
	destroy_nbp(p);
}

/* Delete port(interface) from bridge is done in two steps.
 * via RCU. First step, marks device as down. That deletes
 * all the timers and stops new packets from flowing through.
 *
 * Final cleanup doesn't occur until after all CPU's finished
 * processing packets.
 *
 * Protected from multiple admin operations by RTNL mutex
 */
static void del_nbp(struct net_bridge_port *p)
{
	struct net_bridge *br = p->br;
	struct net_device *dev = p->dev;


	sysfs_remove_link(br->ifobj, dev->name);

	/*
	 * Ruckus: With VLAN-aware bridge, need to lock/unlock bottom half
	 * handling throughout for better protection.
	 */
	spin_lock_bh(&br->lock);

	dev_set_promiscuity(dev, -1);

	br_stp_disable_port(p);

	br_ifinfo_notify(RTM_DELLINK, p);

	br_fdb_delete_by_port(br, p, 1);

#ifdef CONFIG_BRIDGE_PACKET_INSPECTION_FILTER_MODULE
	br_ipdb_delete_by_port(br, dev->ifindex);
	br_ipv6db_delete_by_port(br, dev->ifindex);
#endif

	list_del_rcu(&p->list);

	rcu_assign_pointer(dev->br_port, NULL);

	call_rcu(&p->rcu, destroy_nbp_rcu);
	spin_unlock_bh(&br->lock);

	kobject_uevent(&p->kobj, KOBJ_REMOVE);
	kobject_del(&p->kobj);
}

/* called with RTNL */
static void del_br(struct net_bridge *br)
{
	struct net_bridge_port *p, *n;

	list_for_each_entry_safe(p, n, &br->port_list, list) {
		del_nbp(p);
	}

	del_timer_sync(&br->gc_timer);

#ifdef CONFIG_BRIDGE_PACKET_INSPECTION_FILTER_MODULE
	br_ipdb_clear(br);
	del_timer_sync(&br->ipdb_gc_timer);
	br_ipv6db_clear(br);
	del_timer_sync(&br->ipv6db_gc_timer);
#endif

	br_sysfs_delbr(br->dev);
	unregister_netdevice(br->dev);
}

static struct net_device *new_bridge_dev(struct net *net, const char *name)
{
	struct net_bridge *br;
	struct net_device *dev;

	dev = alloc_netdev(sizeof(struct net_bridge), name,
			   br_dev_setup);

	if (!dev)
		return NULL;
	dev_net_set(dev, net);

	br = netdev_priv(dev);
	br->dev = dev;

	spin_lock_init(&br->lock);
	INIT_LIST_HEAD(&br->port_list);
	spin_lock_init(&br->hash_lock);

	br->bridge_id.prio[0] = 0x80;
	br->bridge_id.prio[1] = 0x00;

	memcpy(br->group_addr, br_group_address, ETH_ALEN);

	br->feature_mask = dev->features;
	br->stp_enabled = BR_NO_STP;
	br->designated_root = br->bridge_id;
	br->root_path_cost = 0;
	br->root_port = 0;
	br->bridge_max_age = br->max_age = 20 * HZ;
	br->bridge_hello_time = br->hello_time = 2 * HZ;
	br->bridge_forward_delay = br->forward_delay = 15 * HZ;
	br->topology_change = 0;
	br->topology_change_detected = 0;
#if 1 /* V54_BSP */
	br->eapol_filter = 1; // default: do not forward eapol mcast frame
#endif
	br->ageing_time = 300 * HZ;

	br_netfilter_rtable_init(br);

	INIT_LIST_HEAD(&br->age_list);

#ifdef CONFIG_BRIDGE_PACKET_INSPECTION_FILTER_MODULE
	spin_lock_init(&br->ip_hash_lock);
	INIT_LIST_HEAD(&br->ip_age_list);
	spin_lock_init(&br->ipv6_hash_lock);
	INIT_LIST_HEAD(&br->ipv6_age_list);
	br->ip_ageing_time = IP_AGEING_INIT;
	br->is_pif_enabled = 0;
	br->pif_drop = 0;
	br->pif_token_max = PIF_TOKEN_MAX_INIT;
	br->pif_token_count = PIF_TOKEN_COUNT_INIT;
	br->pif_token_timestamp = 0;
	br->pif_proxy_arp_count = 0;
	br->pif_directed_arp_count = 0;
	br->pif_directed_arp = 0;
	setup_timer(&br->ipdb_gc_timer, br_ipdb_cleanup, (unsigned long) br);
	setup_timer(&br->ipv6db_gc_timer, br_ipv6db_cleanup, (unsigned long) br);
#endif

#ifdef CONFIG_BRIDGE_BR_MODE
	br->br_mode = BR_BRIDGE_MODE_NORMAL;
#endif
#ifdef CONFIG_BRIDGE_LOOP_GUARD
	br->mac_track.track_window = 0;
	br->mac_track.flap_cnt = 0;
	br->hostonly = 0; // initialized to zero for feature off.
	br->hostonly_cnt = 0;
	br->flap_threshold = FLAP_THRESHOLD;
#endif

#ifdef CONFIG_BRIDGE_MESH_PKT_FORWARDING_FILTER
	br->pff_drop = 0;
	br->skip_learning = 0;
	br->drop_from_self = 0;
#endif

	br_stp_timer_init(br);

	return dev;
}

#if 1 /* V54_BSP */
int is_bridge_device(struct net_device *dev)
{
	return (dev->netdev_ops->ndo_start_xmit == br_dev_xmit) ? 1 : 0;
}
EXPORT_SYMBOL(is_bridge_device);
#endif

/* find an available port number */
static int find_portno(struct net_bridge *br)
{
	int index;
	struct net_bridge_port *p;
	unsigned long *inuse;

	inuse = kcalloc(BITS_TO_LONGS(BR_MAX_PORTS), sizeof(unsigned long),
			GFP_KERNEL);
	if (!inuse)
		return -ENOMEM;

	set_bit(0, inuse);	/* zero is reserved */
	list_for_each_entry(p, &br->port_list, list) {
		set_bit(p->port_no, inuse);
	}
	index = find_first_zero_bit(inuse, BR_MAX_PORTS);
	kfree(inuse);

	return (index >= BR_MAX_PORTS) ? -EXFULL : index;
}

/* called with RTNL but without bridge lock */
static struct net_bridge_port *new_nbp(struct net_bridge *br,
				       struct net_device *dev)
{
	int index;
	struct net_bridge_port *p;

	index = find_portno(br);
	if (index < 0)
		return ERR_PTR(index);

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (p == NULL)
		return ERR_PTR(-ENOMEM);

	p->br = br;
	dev_hold(dev);
	p->dev = dev;
	p->path_cost = port_cost(dev);
	p->priority = 0x8000 >> BR_PORT_BITS;
	p->port_no = index;
	p->flags = 0;
	br_init_port(p);
	p->state = BR_STATE_DISABLED;
	br_stp_port_timer_init(p);
#if 1 /* V54_BSP */
	p->stp_forward_drop = 0;
#endif
	return p;
}

static struct device_type br_type = {
	.name	= "bridge",
};

int br_add_bridge(struct net *net, const char *name)
{
	struct net_device *dev;
	int ret;

	dev = new_bridge_dev(net, name);
	if (!dev)
		return -ENOMEM;

	rtnl_lock();
	if (strchr(dev->name, '%')) {
		ret = dev_alloc_name(dev, dev->name);
		if (ret < 0)
			goto out_free;
	}

	SET_NETDEV_DEVTYPE(dev, &br_type);

	ret = register_netdevice(dev);
	if (ret)
		goto out_free;

	ret = br_sysfs_addbr(dev);
	if (ret)
		unregister_netdevice(dev);
 out:
	rtnl_unlock();
	return ret;

out_free:
	free_netdev(dev);
	goto out;
}

int br_del_bridge(struct net *net, const char *name)
{
	struct net_device *dev;
	int ret = 0;

	rtnl_lock();
	dev = __dev_get_by_name(net, name);
	if (dev == NULL)
		ret =  -ENXIO; /* Could not find device */

	else if (!(dev->priv_flags & IFF_EBRIDGE)) {
		/* Attempt to delete non bridge device! */
		ret = -EPERM;
	}

	else if (dev->flags & IFF_UP) {
		/* Not shutdown yet. */
		ret = -EBUSY;
	}

	else
		del_br(netdev_priv(dev));

	rtnl_unlock();
	return ret;
}

/* MTU of the bridge pseudo-device: ETH_DATA_LEN or the minimum of the ports */
int br_min_mtu(const struct net_bridge *br)
{
	const struct net_bridge_port *p;
	int mtu = 0;

	ASSERT_RTNL();

	if (list_empty(&br->port_list))
		mtu = ETH_DATA_LEN;
	else {
		list_for_each_entry(p, &br->port_list, list) {
			if (!mtu  || p->dev->mtu < mtu)
				mtu = p->dev->mtu;
		}
	}
	return mtu;
}

/*
 * Recomputes features using slave's features
 */
void br_features_recompute(struct net_bridge *br)
{
	struct net_bridge_port *p;
	unsigned long features, mask;

	features = mask = br->feature_mask;
	if (list_empty(&br->port_list))
		goto done;

	features &= ~NETIF_F_ONE_FOR_ALL;

	list_for_each_entry(p, &br->port_list, list) {
		features = netdev_increment_features(features,
						     p->dev->features, mask);
	}

done:
	br->dev->features = netdev_fix_features(features, NULL);
}

/* called with RTNL */
int br_add_if(struct net_bridge *br, struct net_device *dev)
{
	struct net_bridge_port *p;
	int err = 0;

	/* Don't allow bridging non-ethernet like devices */
	if ((dev->flags & IFF_LOOPBACK) ||
	    dev->type != ARPHRD_ETHER || dev->addr_len != ETH_ALEN)
		return -EINVAL;

	/* No bridging of bridges */
	if (dev->netdev_ops->ndo_start_xmit == br_dev_xmit)
		return -ELOOP;

	/* Device is already being bridged */
	if (dev->br_port != NULL)
		return -EBUSY;

	p = new_nbp(br, dev);
	if (IS_ERR(p))
		return PTR_ERR(p);

	err = dev_set_promiscuity(dev, 1);
	if (err)
		goto put_back;

	err = kobject_init_and_add(&p->kobj, &brport_ktype, &(dev->dev.kobj),
				   SYSFS_BRIDGE_PORT_ATTR);
	if (err)
		goto err0;

#ifdef BR_FDB_VLAN
	err = br_fdb_insert(br, p, BR_ALL_VLAN, dev->dev_addr, 1);
#else
	err = br_fdb_insert(br, p, dev->dev_addr, 1);
#endif
	if (err)
		goto err1;

	err = br_sysfs_addif(p);
	if (err)
		goto err2;

	rcu_assign_pointer(dev->br_port, p);
	dev_disable_lro(dev);

	list_add_rcu(&p->list, &br->port_list);

	spin_lock_bh(&br->lock);
	br_stp_recalculate_bridge_id(br);
	br_features_recompute(br);

	if ((dev->flags & IFF_UP) && netif_carrier_ok(dev) &&
	    (br->dev->flags & IFF_UP))
		br_stp_enable_port(p);
	spin_unlock_bh(&br->lock);

	br_ifinfo_notify(RTM_NEWLINK, p);

	dev_set_mtu(br->dev, br_min_mtu(br));

	kobject_uevent(&p->kobj, KOBJ_ADD);

	return 0;
err2:
	br_fdb_delete_by_port(br, p, 1);
err1:
	kobject_put(&p->kobj);
	p = NULL; /* kobject_put frees */
err0:
	dev_set_promiscuity(dev, -1);
put_back:
	dev_put(dev);
	kfree(p);
	return err;
}

/* called with RTNL */
int br_del_if(struct net_bridge *br, struct net_device *dev)
{
	struct net_bridge_port *p = dev->br_port;

	if (!p || p->br != br)
		return -EINVAL;

	del_nbp(p);

	spin_lock_bh(&br->lock);
	br_stp_recalculate_bridge_id(br);
	br_features_recompute(br);
	spin_unlock_bh(&br->lock);

	return 0;
}

void br_net_exit(struct net *net)
{
	struct net_device *dev;

	rtnl_lock();
restart:
	for_each_netdev(net, dev) {
		if (dev->priv_flags & IFF_EBRIDGE) {
			del_br(netdev_priv(dev));
			goto restart;
		}
	}
	rtnl_unlock();

}

#if defined(CONFIG_BRIDGE_BR_MODE) || defined(CONFIG_BRIDGE_LOOP_GUARD)
int br_set_br_mode(struct net_bridge *br, int param, int value)
{
	switch (param) {
#if defined(CONFIG_BRIDGE_BR_MODE)
	case BR_BRIDGE_MODE_NORMAL:
	case BR_BRIDGE_MODE_HOST_ONLY:
		br->br_mode = param;
		break;
#endif
#if defined(CONFIG_BRIDGE_LOOP_GUARD)
	case BR_BRIDGE_MODE_SET_LOOP_HOLD:
		BR_SET_LOOP_HOLD(value * HZ);
		break;

	case BR_BRIDGE_MODE_SET_FLAP_THRESHOLD:
		br->flap_threshold = value;
		break;
#endif
	default:
		return -EINVAL;
	}
	return 0;
}
#endif

#ifdef CONFIG_BRIDGE_PORT_MODE
int br_set_port_mode(struct net_bridge *br, int ifindex, int mode)
{
	struct net_device *dev;
	struct net_bridge_port *p;

	if (mode < 0 || mode > BR_PORT_MODE_MAX) {
		return -EINVAL;
	}

	dev = dev_get_by_index(dev_net(br->dev), ifindex);
	if (dev == NULL)
		return -EINVAL;

	p = dev->br_port;
	if (!p || p->br != br) {
		dev_put(dev);
		return -EINVAL;
	}

	if (p->port_mode != mode) {
		p->port_mode = mode;
	}

	dev_put(dev);
	return 0;
}

#if defined (CONFIG_SOURCE_MAC_GUARD) || \
	defined(CONFIG_CI_WHITELIST) || \
	defined(CONFIG_ANTI_SPOOF)
int br_set_port_ci_state(struct net_bridge * br,int ifidx ,int state)
{
	struct net_device * dev;
	struct net_bridge_port * p;

	if( state < BR_PORT_CI_INIT || state > BR_PORT_CI_ISOLATION)
		return -EINVAL;

	dev = dev_get_by_index(dev_net(br->dev), ifidx);
	if(dev == NULL)
	return -EINVAL;

	p = dev->br_port;
	if( !p || p->br != br ) {
		dev_put(dev);
		return -EINVAL;
	}

	if( p->port_ci_state != state)
		p->port_ci_state = state;

	dev_put(dev);
	return 0;
}
#endif
#endif

#ifdef CONFIG_BRIDGE_MESH_PKT_FORWARDING_FILTER
int br_clear_macs(struct net_bridge *br)
{
	br_fdb_clear(br);
	return 0;
}

int br_add_mac(struct net_bridge *br, int ifindex, unsigned char *mac)
{
	struct net_device *dev;
	struct net_bridge_port *p;
	int ret;

	dev = dev_get_by_index(dev_net(br->dev), ifindex);
	if (dev == NULL)
		return -EINVAL;

	p = dev->br_port;
	if (p == NULL) {
		dev_put(dev);
		return -EINVAL;
	}

	printk( FMT_MAC "\n", ARG_MAC(mac));

	ret = br_fdb_insert(br,p,0,mac,0);
	dev_put(dev);
	return ret;
}

int br_set_learning(struct net_bridge *br, int ifindex, int learning)
{
	struct net_device *dev;
	struct net_bridge_port *p;

	dev = dev_get_by_index(dev_net(br->dev), ifindex);
	if (dev == NULL)
		return -EINVAL;

	p = dev->br_port;
	if (p == NULL) {
		dev_put(dev);
		return -EINVAL;
	}

	spin_lock_bh(&br->lock);
	p->is_disable_learning = !learning;
	spin_unlock_bh(&br->lock);

	dev_put(dev);
	return 0;
}

int br_set_pff(struct net_bridge *br, int ifindex, int pff)
{
	struct net_device *dev;
	struct net_bridge_port *p;

	dev = dev_get_by_index(dev_net(br->dev), ifindex);
	if (dev == NULL)
		return -EINVAL;

	p = dev->br_port;
	if (p == NULL) {
		dev_put(dev);
		return -EINVAL;
	}

	spin_lock_bh(&br->lock);
	p->is_pff = pff;
	spin_unlock_bh(&br->lock);

	dev_put(dev);
	return 0;
}
#endif

#if 1 /* V54_BSP */
int br_set_port_stp_forward_drop(struct net_bridge *br, int ifindex, int drop)
{
	struct net_device *dev;
	struct net_bridge_port *p;

	dev = dev_get_by_index(dev_net(br->dev), ifindex);
	if (dev == NULL)
		return -EINVAL;

	p = dev->br_port;
	if (!p || p->br != br) {
		dev_put(dev);
		return -EINVAL;
	}

	p->stp_forward_drop = drop;
	dev_put(dev);
	return 0;
}
#endif
