/*
 *	Ioctl handler
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

#include <linux/capability.h>
#include <linux/kernel.h>
#include <linux/if_bridge.h>
#include <linux/netdevice.h>
#include <linux/times.h>
#include <net/net_namespace.h>
#include <asm/uaccess.h>
#include "br_private.h"

/* called with RTNL */
static int get_bridge_ifindices(struct net *net, int *indices, int num)
{
	struct net_device *dev;
	int i = 0;

	for_each_netdev(net, dev) {
		if (i >= num)
			break;
		if (dev->priv_flags & IFF_EBRIDGE)
			indices[i++] = dev->ifindex;
	}

	return i;
}

/* called with RTNL */
static void get_port_ifindices(struct net_bridge *br, int *ifindices, int num)
{
	struct net_bridge_port *p;

	list_for_each_entry(p, &br->port_list, list) {
		if (p->port_no < num)
			ifindices[p->port_no] = p->dev->ifindex;
	}
}

/*
 * Format up to a page worth of forwarding table entries
 * userbuf -- where to copy result
 * maxnum  -- maximum number of entries desired
 *            (limited to a page for sanity)
 * offset  -- number of records to skip
 */
static int get_fdb_entries(struct net_bridge *br, void __user *userbuf,
			   unsigned long maxnum, unsigned long offset)
{
	int num;
	void *buf;
	size_t size;

	/* Clamp size to PAGE_SIZE, test maxnum to avoid overflow */
	if (maxnum > PAGE_SIZE/sizeof(struct __fdb_entry))
		maxnum = PAGE_SIZE/sizeof(struct __fdb_entry);

	size = maxnum * sizeof(struct __fdb_entry);

	buf = kmalloc(size, GFP_USER);
	if (!buf)
		return -ENOMEM;

	num = br_fdb_fillbuf(br, buf, maxnum, offset);
	if (num > 0) {
		if (copy_to_user(userbuf, buf, num*sizeof(struct __fdb_entry)))
			num = -EFAULT;
	}
	kfree(buf);

	return num;
}

#if defined(CONFIG_CI_WHITELIST) || defined (CONFIG_SOURCE_MAC_GUARD)
int (*br_ciwl_ioctl)(int cmd, void *pData, int *) = NULL;
EXPORT_SYMBOL(br_ciwl_ioctl);
#endif /* CONFIG_CI_WHITELIST || CONFIG_SOURCE_MAC_GUARD */

#ifdef CONFIG_BRIDGE_PACKET_INSPECTION_FILTER_MODULE
/*
 * Format up to a page worth of bridge ip table entries
 * userbuf -- where to copy result
 * maxnum  -- maximum number of entries desired
 *            (limited to a page for sanity)
 * offset  -- number of records to skip
 */
static int get_ipdb_entries(struct net_bridge *br, void __user *userbuf,
			   unsigned long maxnum, unsigned long offset)
{
	int num;
	void *buf;
	size_t size = maxnum * sizeof(struct __ipdb_entry);

	if (size > PAGE_SIZE) {
		size = PAGE_SIZE;
		maxnum = PAGE_SIZE/sizeof(struct __ipdb_entry);
	}

	buf = kmalloc(size, GFP_USER);
	if (!buf)
		return -ENOMEM;

	num = br_ipdb_fillbuf(br, buf, maxnum, offset);
	if (num > 0) {
		if (copy_to_user(userbuf, buf, num*sizeof(struct __ipdb_entry)))
			num = -EFAULT;
	}
	kfree(buf);

	return num;
}

static int get_ipv6db_entries(struct net_bridge *br, void __user *userbuf,
			   unsigned long maxnum, unsigned long offset)
{
	int num;
	void *buf;
	size_t size = maxnum * sizeof(struct __ipv6db_entry);

	if (size > PAGE_SIZE) {
		size = PAGE_SIZE;
		maxnum = PAGE_SIZE/sizeof(struct __ipv6db_entry);
	}

	buf = kmalloc(size, GFP_USER);
	if (!buf)
		return -ENOMEM;

	num = br_ipv6db_fillbuf(br, buf, maxnum, offset);
	if (num > 0) {
		if (copy_to_user(userbuf, buf, num*sizeof(struct __ipv6db_entry)))
			num = -EFAULT;
	}
	kfree(buf);

	return num;
}

static int br_set_ip_ageing(struct net_bridge *br, int ageing)
{
    spin_lock_bh(&br->lock);
    br->ip_ageing_time = ageing * 60 *HZ;
    mod_timer(&br->ipdb_gc_timer, jiffies + HZ/10);
    mod_timer(&br->ipv6db_gc_timer, jiffies + HZ/10);
    spin_unlock_bh(&br->lock);
    return 0;
}

int (*br_set_pif_ioctl)(struct net_bridge *, int, int, int) = NULL;
void (*br_pif_token_replenisher_cb)(struct net_bridge *) = NULL;
EXPORT_SYMBOL(br_set_pif_ioctl);
EXPORT_SYMBOL(br_pif_token_replenisher_cb);
#endif

static int add_del_if(struct net_bridge *br, int ifindex, int isadd)
{
	struct net_device *dev;
	int ret;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	dev = dev_get_by_index(dev_net(br->dev), ifindex);
	if (dev == NULL)
		return -EINVAL;

	if (isadd)
		ret = br_add_if(br, dev);
	else
		ret = br_del_if(br, dev);

	dev_put(dev);
	return ret;
}

/*
 * Legacy ioctl's through SIOCDEVPRIVATE
 * This interface is deprecated because it was too difficult to
 * to do the translation for 32/64bit ioctl compatability.
 */
static int old_dev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct net_bridge *br = netdev_priv(dev);
	unsigned long args[4];

	if (copy_from_user(args, rq->ifr_data, sizeof(args)))
		return -EFAULT;

	switch (args[0]) {
	case BRCTL_ADD_IF:
	case BRCTL_DEL_IF:
		return add_del_if(br, args[1], args[0] == BRCTL_ADD_IF);

	case BRCTL_GET_BRIDGE_INFO:
	{
		struct __bridge_info b;

		memset(&b, 0, sizeof(struct __bridge_info));
		rcu_read_lock();
		memcpy(&b.designated_root, &br->designated_root, 8);
		memcpy(&b.bridge_id, &br->bridge_id, 8);
		b.root_path_cost = br->root_path_cost;
		b.max_age = jiffies_to_clock_t(br->max_age);
		b.hello_time = jiffies_to_clock_t(br->hello_time);
		b.forward_delay = br->forward_delay;
		b.bridge_max_age = br->bridge_max_age;
		b.bridge_hello_time = br->bridge_hello_time;
		b.bridge_forward_delay = jiffies_to_clock_t(br->bridge_forward_delay);
		b.topology_change = br->topology_change;
		b.topology_change_detected = br->topology_change_detected;
		b.root_port = br->root_port;

		b.stp_enabled = (br->stp_enabled != BR_NO_STP);
		b.eapol_filter = br->eapol_filter;
		b.des_mtu = br->des_mtu;

		b.ageing_time = jiffies_to_clock_t(br->ageing_time);
		b.hello_timer_value = br_timer_value(&br->hello_timer);
		b.tcn_timer_value = br_timer_value(&br->tcn_timer);
		b.topology_change_timer_value = br_timer_value(&br->topology_change_timer);
		b.gc_timer_value = br_timer_value(&br->gc_timer);
#ifdef CONFIG_BRIDGE_BR_MODE
		b.br_mode = br->br_mode;
#endif
#ifdef CONFIG_BRIDGE_LOOP_GUARD
		b.hostonly = br->hostonly;
		b.jiffies = jiffies;
		b.hostonly_cnt = br->hostonly_cnt;
		b.loop_hold = BR_GET_LOOP_HOLD()/HZ;
		b.flap_threshold = br->flap_threshold;
#endif

#ifdef CONFIG_BRIDGE_MESH_PKT_FORWARDING_FILTER
		b.pff_drop = br->pff_drop;
		b.skip_learning = br->skip_learning;
		b.drop_from_self = br->drop_from_self;
#endif

#ifdef CONFIG_BRIDGE_PACKET_INSPECTION_FILTER_MODULE
		b.pif = br->is_pif_enabled;
		b.pif_proc_uplink = br->is_pif_proc_uplink;
		b.pif_drop = br->pif_drop;
		b.pif_directed_arp = br->pif_directed_arp;
		b.ip_ageing_time = br->ip_ageing_time/(60*HZ); // present as min
		if (br_pif_token_replenisher_cb)
			br_pif_token_replenisher_cb(br); // ensure token availability is up to date
		b.pif_token_count = br->pif_token_count;
		b.pif_token_max = br->pif_token_max;
		b.pif_proxy_arp_count = br->pif_proxy_arp_count;
		b.pif_directed_arp_count = br->pif_directed_arp_count;
		b.ipdb_gc_timer_value = br_timer_value(&br->ipdb_gc_timer);
		b.ipv6db_gc_timer_value = br_timer_value(&br->ipv6db_gc_timer);
#endif

		rcu_read_unlock();

		if (copy_to_user((void __user *)args[1], &b, sizeof(b)))
			return -EFAULT;

		return 0;
	}

	case BRCTL_GET_PORT_LIST:
	{
		int num, *indices;

		num = args[2];
		if (num < 0)
			return -EINVAL;
		if (num == 0)
			num = 256;
		if (num > BR_MAX_PORTS)
			num = BR_MAX_PORTS;

		indices = kcalloc(num, sizeof(int), GFP_KERNEL);
		if (indices == NULL)
			return -ENOMEM;

		get_port_ifindices(br, indices, num);
		if (copy_to_user((void __user *)args[1], indices, num*sizeof(int)))
			num =  -EFAULT;
		kfree(indices);
		return num;
	}

	case BRCTL_SET_BRIDGE_FORWARD_DELAY:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		spin_lock_bh(&br->lock);
		br->bridge_forward_delay = clock_t_to_jiffies(args[1]);
		if (br_is_root_bridge(br))
			br->forward_delay = br->bridge_forward_delay;
		spin_unlock_bh(&br->lock);
		return 0;

	case BRCTL_SET_BRIDGE_HELLO_TIME:
	{
		unsigned long t = clock_t_to_jiffies(args[1]);
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if (t < HZ)
			return -EINVAL;

		spin_lock_bh(&br->lock);
		br->bridge_hello_time = t;
		if (br_is_root_bridge(br))
			br->hello_time = br->bridge_hello_time;
		spin_unlock_bh(&br->lock);
		return 0;
	}

	case BRCTL_SET_BRIDGE_MAX_AGE:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		spin_lock_bh(&br->lock);
		br->bridge_max_age = clock_t_to_jiffies(args[1]);
		if (br_is_root_bridge(br))
			br->max_age = br->bridge_max_age;
		spin_unlock_bh(&br->lock);
		return 0;

	case BRCTL_SET_AGEING_TIME:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		br->ageing_time = clock_t_to_jiffies(args[1]);
		return 0;

	case BRCTL_GET_PORT_INFO:
	{
		struct __port_info p;
		struct net_bridge_port *pt;

		rcu_read_lock();
		if ((pt = br_get_port(br, args[2])) == NULL) {
			rcu_read_unlock();
			return -EINVAL;
		}

		memset(&p, 0, sizeof(struct __port_info));
		memcpy(&p.designated_root, &pt->designated_root, 8);
		memcpy(&p.designated_bridge, &pt->designated_bridge, 8);
		p.port_id = pt->port_id;
		p.designated_port = pt->designated_port;
		p.path_cost = pt->path_cost;
		p.designated_cost = pt->designated_cost;
		p.state = pt->state;
		p.top_change_ack = pt->topology_change_ack;
		p.config_pending = pt->config_pending;
		p.message_age_timer_value = br_timer_value(&pt->message_age_timer);
		p.forward_delay_timer_value = br_timer_value(&pt->forward_delay_timer);
		p.hold_timer_value = br_timer_value(&pt->hold_timer);
#ifdef CONFIG_BRIDGE_PORT_MODE
		p.port_mode = pt->port_mode;
		p.port_ci_state = pt->port_ci_state;
#endif
#ifdef CONFIG_BRIDGE_MESH_PKT_FORWARDING_FILTER
		p.learning = !pt->is_disable_learning;
		p.pff = pt->is_pff;
#else
		p.learning = 1;
#endif
#ifdef CONFIG_BRIDGE_PACKET_INSPECTION_FILTER_MODULE
        p.pif_iftype = pt->pif_iftype;
        p.pif_ifmode = pt->pif_ifmode;
        p.pif_learning = pt->pif_learning;
#endif
#if 1 /* V54_BSP */
		p.stp_forward_drop = pt->stp_forward_drop;
#endif
		rcu_read_unlock();

		if (copy_to_user((void __user *)args[1], &p, sizeof(p)))
			return -EFAULT;

		return 0;
	}

#if defined(CONFIG_BRIDGE_BR_MODE) || defined(CONFIG_BRIDGE_LOOP_GUARD)
	case BRCTL_SET_BRIDGE_MODE:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		return br_set_br_mode(br, args[2], args[3]);
#endif

#ifdef CONFIG_BRIDGE_MESH_PKT_FORWARDING_FILTER
	case BRCTL_CLEAR_MACS:
		return br_clear_macs(br);

	case BRCTL_ADD_MAC:
	{
		unsigned char  mac_addr[ETH_ALEN];
		if (copy_from_user(mac_addr, (void __user *)args[2], ETH_ALEN))
			return -EFAULT;
		return br_add_mac(br, args[1], mac_addr);
	}

	case BRCTL_SET_LEARNING:
		return br_set_learning(br, args[1], args[2]);

	case BRCTL_SET_PFF:
		return br_set_pff(br, args[1], args[2]);
#endif

#if  defined(CONFIG_BRIDGE_PORT_MODE) && defined(V54_BSP)
	case BRCTL_SET_PORT_MODE:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		return br_set_port_mode(br, args[1], args[2]);

	case BRCTL_SET_PORT_CI_STATE:
		if(!capable(CAP_NET_ADMIN))
			return -EPERM;
	return br_set_port_ci_state(br,args[1],args[2]);
#endif
#if 1 /* V54_BSP */
	case BRCTL_SET_PORT_STP_FORWARD_DROP:
		return br_set_port_stp_forward_drop(br, args[1], args[2]);

	case BRCTL_SET_BRIDGE_EAPOL_FILTER:
		br->eapol_filter = args[1]?1:0;
		return 0;

	case BRCTL_SET_BRIDGE_DESIRE_MTU:
		br->des_mtu = args[1];
		dev_set_mtu(br->dev, br_min_mtu(br));
		return 0;
#endif
	case BRCTL_SET_BRIDGE_STP_STATE:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		br_stp_set_enabled(br, args[1]);
		return 0;

	case BRCTL_SET_BRIDGE_PRIORITY:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		spin_lock_bh(&br->lock);
		br_stp_set_bridge_priority(br, args[1]);
		spin_unlock_bh(&br->lock);
		return 0;

	case BRCTL_SET_PORT_PRIORITY:
	{
		struct net_bridge_port *p;
		int ret = 0;

		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if (args[2] >= (1<<(16-BR_PORT_BITS)))
			return -ERANGE;

		spin_lock_bh(&br->lock);
		if ((p = br_get_port(br, args[1])) == NULL)
			ret = -EINVAL;
		else
			br_stp_set_port_priority(p, args[2]);
		spin_unlock_bh(&br->lock);
		return ret;
	}

	case BRCTL_SET_PATH_COST:
	{
		struct net_bridge_port *p;
		int ret = 0;

		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if ((p = br_get_port(br, args[1])) == NULL)
			ret = -EINVAL;
		else
			br_stp_set_path_cost(p, args[2]);

		return ret;
	}

	case BRCTL_GET_FDB_ENTRIES:
		return get_fdb_entries(br, (void __user *)args[1],
				       args[2], args[3]);
#if 1 /* V54_BSP */
	/* Get the fwd port attributes (ifindex and port num) of a MAC addr */
	case BRCTL_GET_PORT_ATTRIB_FOR_MAC:
	{
		struct net_bridge_fdb_entry *fdb;
		int    rc = 0, port_num;
                unsigned char  mac_addr[ETH_ALEN];

		if (copy_from_user(mac_addr, (void __user *)args[1], ETH_ALEN))
			return -EFAULT;

#ifdef BR_FDB_VLAN
		// For backward compatibility and to avoid an interface change,
		// overload args[2] to use for passing in a VLAN_ID.
		// A 0 in that field will be interpreted as asking for
		// the default VLAN.
		{
		unsigned int vid = 0;
		if (copy_from_user(&vid, (void __user *)args[2], sizeof(unsigned int))) {
			return -EFAULT;
		}
		if ( vid  == 0 ) {
			vid = BR_DEFAULT_VLAN;
			printk("%s: vid=0->%u\n", __FUNCTION__,vid);
		}
		fdb = __br_fdb_get(br, vid, mac_addr);
		}
#else
		fdb = __br_fdb_get(br, mac_addr);
#endif

		if (!fdb || !fdb->dst || !fdb->dst->dev) {
			return -EFAULT;
		}
		  /* Pass ifindex back */
		else if (copy_to_user((void __user *)args[2],
				&(fdb->dst->dev->ifindex), sizeof(int))) {
			rc = -EFAULT;
		}
		  /* Pass port number as well */
		else {
			port_num = fdb->dst->port_no;
			if (copy_to_user((void __user *)args[3],
					&port_num, sizeof(int)))
				rc = -EFAULT;
		}


		return rc;
	}
#endif

#if defined(CONFIG_CI_WHITELIST) || defined (CONFIG_SOURCE_MAC_GUARD)

    case BRCTL_SET_CI_WL:
        if (br_ciwl_ioctl)
            return br_ciwl_ioctl(args[1], (void *)args[2], (int *)args[3]);
        else
            return -EOPNOTSUPP;

#endif /* CONFIG_CI_WHITELIST || CONFIG_SOURCE_MAC_GUARD */

#ifdef CONFIG_BRIDGE_PACKET_INSPECTION_FILTER_MODULE
    case BRCTL_SET_PIF:
        if (br_set_pif_ioctl)
            return br_set_pif_ioctl(br, args[1], args[2], args[3]);
        else
            return -EOPNOTSUPP;

    case BRCTL_SET_IP_AGEING:
        return br_set_ip_ageing(br, args[1]);

    case BRCTL_GET_IPDB_ENTRIES:
        return get_ipdb_entries(br, (void __user *)args[1],
                                args[2], args[3]);

    case BRCTL_GET_IPV6DB_ENTRIES:
        return get_ipv6db_entries(br, (void __user *)args[1],
                                  args[2], args[3]);

    case BRCTL_DEL_IPDB_ENTRY_BY_MAC:
    {
        unsigned char  mac_addr[ETH_ALEN];
        if (copy_from_user(mac_addr, (void __user *)args[2], ETH_ALEN))
            return -EFAULT;
        return br_ipdb_delete_by_mac(br, args[1], mac_addr);
    }

    case BRCTL_DEL_IPV6DB_ENTRY_BY_MAC:
    {
        unsigned char  mac_addr[ETH_ALEN];
        if (copy_from_user(mac_addr, (void __user *)args[2], ETH_ALEN))
            return -EFAULT;
        return br_ipv6db_delete_by_mac(br, args[1], mac_addr);
    }
#endif

	case BRCTL_SET_HOTSPOT_FLAG:
		spin_lock_bh(&br->lock);
		br->hotspot = args[1] ? 1 : 0;
		spin_unlock_bh(&br->lock);
		return 0;
	}

	return -EOPNOTSUPP;
}

static int old_deviceless(struct net *net, void __user *uarg)
{
	unsigned long args[3];

	if (copy_from_user(args, uarg, sizeof(args)))
		return -EFAULT;

	switch (args[0]) {
	case BRCTL_GET_VERSION:
		return BRCTL_VERSION;

	case BRCTL_GET_BRIDGES:
	{
		int *indices;
		int ret = 0;

		if (args[2] >= 2048)
			return -ENOMEM;
		indices = kcalloc(args[2], sizeof(int), GFP_KERNEL);
		if (indices == NULL)
			return -ENOMEM;

		args[2] = get_bridge_ifindices(net, indices, args[2]);

		ret = copy_to_user((void __user *)args[1], indices, args[2]*sizeof(int))
			? -EFAULT : args[2];

		kfree(indices);
		return ret;
	}

	case BRCTL_ADD_BRIDGE:
	case BRCTL_DEL_BRIDGE:
	{
		char buf[IFNAMSIZ];

		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if (copy_from_user(buf, (void __user *)args[1], IFNAMSIZ))
			return -EFAULT;

		buf[IFNAMSIZ-1] = 0;

		if (args[0] == BRCTL_ADD_BRIDGE)
			return br_add_bridge(net, buf);

		return br_del_bridge(net, buf);
	}
#if 1 /* V54_BSP */
	case BRCTL_GET_WHICH_BRIDGE:
	{
		struct net_device *dev;
		int ret = 0;

		dev = dev_get_by_index(net, args[1]);
		if (dev == NULL)
			return -EINVAL;

		if (dev->br_port)
			args[2] = dev->br_port->br->dev->ifindex;
		else
			args[2] = 0;

		ret = copy_to_user((void __user *)uarg, args, sizeof(args))
			? -EFAULT : 0;

		dev_put(dev);
		return ret;
	}
#endif
	}

	return -EOPNOTSUPP;
}

int br_ioctl_deviceless_stub(struct net *net, unsigned int cmd, void __user *uarg)
{
	switch (cmd) {
	case SIOCGIFBR:
	case SIOCSIFBR:
		return old_deviceless(net, uarg);

	case SIOCBRADDBR:
	case SIOCBRDELBR:
	{
		char buf[IFNAMSIZ];

		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if (copy_from_user(buf, uarg, IFNAMSIZ))
			return -EFAULT;

		buf[IFNAMSIZ-1] = 0;
		if (cmd == SIOCBRADDBR)
			return br_add_bridge(net, buf);

		return br_del_bridge(net, buf);
	}
	}
	return -EOPNOTSUPP;
}

int br_dev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct net_bridge *br = netdev_priv(dev);

	switch(cmd) {
	case SIOCDEVPRIVATE:
		return old_dev_ioctl(dev, rq, cmd);

	case SIOCBRADDIF:
	case SIOCBRDELIF:
		return add_del_if(br, rq->ifr_ifindex, cmd == SIOCBRADDIF);

	}

	pr_debug("Bridge does not support ioctl 0x%x\n", cmd);
	return -EOPNOTSUPP;
}
