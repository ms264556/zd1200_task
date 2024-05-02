/*
 *  Learned MAC IP address database
 *  Linux ethernet bridge
 *
 *  Authors:
 *  Ruckus Wireless
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/netfilter_bridge.h>
#include <linux/jhash.h>
#include <linux/if_arp.h>
#include <linux/jiffies.h>
#include <linux/version.h>
#include "br_private.h"

#ifdef CONFIG_BRIDGE_PACKET_INSPECTION_FILTER_MODULE

#ifndef BR_FDB_VLAN
#error Assumes BR VLAN support
#endif

//#define IPV6DB_DEBUG 1


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
static kmem_cache_t *br_ipv6db_cache __read_mostly;
#else
static struct kmem_cache *br_ipv6db_cache __read_mostly;
#endif

void __init br_ipv6db_init(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
    br_ipv6db_cache = kmem_cache_create("bridge_ipv6db_cache",
                                         sizeof(struct net_bridge_ipv6db_entry),
                                         0,
                                         SLAB_HWCACHE_ALIGN, NULL, NULL);
#else
    br_ipv6db_cache = kmem_cache_create("bridge_ipv6db_cache",
                                         sizeof(struct net_bridge_ipv6db_entry),
                                         0,
                                         SLAB_HWCACHE_ALIGN, NULL);
#endif
}

void __exit br_ipv6db_fini(void)
{
    kmem_cache_destroy(br_ipv6db_cache);
}

static __inline__ int ipv6db_has_expired(const struct net_bridge *br,
                                         const struct net_bridge_ipv6db_entry *ipv6db)
{
    if (br->ip_ageing_time) {
            return time_before_eq(ipv6db->ageing_timer + br->ip_ageing_time, jiffies);
    } else {
        /* ignore expired when ip_ageing_time is 0 */
        return 0;
    }
}

static __inline__ int br_ipv6_hash(const unsigned int *ipv6addr)
{
    return jhash((unsigned char *)&ipv6addr[3], sizeof(unsigned int), 0) & (BR_HASH_SIZE - 1);
}

static inline struct net_bridge_ipv6db_entry *ipv6db_find(struct hlist_head *head,
                                                          const unsigned int *ipv6addr,
                                                          const unsigned int vlan)
{
    struct hlist_node *h;
    struct net_bridge_ipv6db_entry *ipv6db;

    hlist_for_each_entry_rcu(ipv6db, h, head, hlist) {
#ifdef IPV6DB_DEBUG
        printk(KERN_NOTICE "%s: ipv6search " IPV6ADDR_FMT ", head %p, "
               " ipv6 " IPV6ADDR_FMT " mac " MAC_FMT "\n",
               __func__, IPV6_ARG(ipv6addr), head, IPV6_ARG(ipv6db->ipv6addr),
               MAC_ARG(ipv6db->mac_addr.addr) );
#endif
        if (IPV6ADDR_EQUAL(ipv6db->ipv6addr, ipv6addr) && ipv6db->vlan == vlan)
            return ipv6db;
    }
    return NULL;
}


static void ipv6db_rcu_free(struct rcu_head *head)
{
    struct net_bridge_ipv6db_entry *ent
        = container_of(head, struct net_bridge_ipv6db_entry, rcu);
    kmem_cache_free(br_ipv6db_cache, ent);
}


static void br_ipv6db_put(struct net_bridge_ipv6db_entry *ent)
{
    if (atomic_dec_and_test(&ent->use_count))
        call_rcu(&ent->rcu, ipv6db_rcu_free);
}


static __inline__ void ipv6db_delete(struct net_bridge_ipv6db_entry *f)
{
    hlist_del_rcu(&f->hlist);
    br_ipv6db_put(f);
}


void br_ipv6db_clear(struct net_bridge *br)
{
    int i;

    spin_lock_bh(&br->ipv6_hash_lock);
    for (i = 0; i < BR_HASH_SIZE; i++) {
        struct net_bridge_ipv6db_entry *f;
        struct hlist_node *h, *n;

        hlist_for_each_entry_safe(f, h, n, &br->ipv6_hash[i], hlist) {
            ipv6db_delete(f);
        }
    }
    spin_unlock_bh(&br->ipv6_hash_lock);
}
EXPORT_SYMBOL(br_ipv6db_clear);


/* No locking or refcounting, assumes caller has no preempt (rcu_read_lock) */
struct net_bridge_ipv6db_entry *__br_ipv6db_get(struct net_bridge *br,
                                                const unsigned int *ipv6addr, const unsigned int vlan )
{
    struct hlist_node *h;
    struct net_bridge_ipv6db_entry *ipv6db;

    hlist_for_each_entry_rcu(ipv6db, h, &br->ipv6_hash[br_ipv6_hash(ipv6addr)], hlist) {
        if (IPV6ADDR_EQUAL(ipv6db->ipv6addr, ipv6addr) && ipv6db->vlan == vlan) {
            if (unlikely(ipv6db_has_expired(br, ipv6db)))
                break;
            return ipv6db;
        }
    }

    return NULL;
}


/* Interface used by ATM hook that keeps a ref count */
struct net_bridge_ipv6db_entry *br_ipv6db_get(struct net_bridge *br,
                                              unsigned int *ipv6addr, const unsigned int vlan )
{
    struct net_bridge_ipv6db_entry *ipv6db;

    rcu_read_lock();
    ipv6db = __br_ipv6db_get(br, ipv6addr, vlan);
    if (ipv6db)
        atomic_inc(&ipv6db->use_count);
    rcu_read_unlock();
    return ipv6db;
}


static struct net_bridge_ipv6db_entry *ipv6db_create(struct hlist_head *head,
                                                     const unsigned int *ipv6addr,
                                                     const unsigned int vlan,
                                                     const unsigned char *macaddr,
                                                     struct net_bridge_port *br_port )
{
    struct net_bridge_ipv6db_entry *ipv6db;

#ifdef IPV6DB_DEBUG
    printk(KERN_NOTICE "%s: head %p, ipv6 " IPV6ADDR_FMT ", mac " MAC_FMT "\n",
           __func__, head, IPV6_ARG(ipv6addr), MAC_ARG(macaddr) );
#endif

    ipv6db = kmem_cache_alloc(br_ipv6db_cache, GFP_ATOMIC);
    if (ipv6db) {
        memcpy(ipv6db->ipv6addr, ipv6addr, 16);
        ipv6db->vlan = vlan;
        memcpy(ipv6db->mac_addr.addr, macaddr, ETH_ALEN);
        atomic_set(&ipv6db->use_count, 1);
        hlist_add_head_rcu(&ipv6db->hlist, head);
        ipv6db->br_port = br_port;
        ipv6db->ageing_timer = jiffies;
    }
    return ipv6db;
}


static int ipv6db_insert(struct net_bridge *br,
                         const unsigned int *ipv6addr,
                         const unsigned int vlan,
                         const unsigned char *macaddr,
                         struct net_bridge_port *br_port )
{
    struct hlist_head *head = &br->ipv6_hash[br_ipv6_hash(ipv6addr)];
    struct net_bridge_ipv6db_entry *ipv6db;
    int rc = 0;

#ifdef IPV6DB_DEBUG
    printk(KERN_NOTICE "%s: hash %d, head %p, ip " IPV6ADDR_FMT ", mac " MAC_FMT "\n",
           __func__, br_ipv6_hash(ipv6addr), head, IPV6_ARG(ipv6addr), MAC_ARG(macaddr));
#endif

    if (!is_valid_ether_addr(macaddr))
        return -EINVAL;

    if (ipv6addr[0] == 0 &&
        ipv6addr[1] == 0 &&
        ipv6addr[2] == 0 &&
        ipv6addr[3] == 0)
        return -EINVAL;

    rcu_read_lock();
    ipv6db = ipv6db_find(head, ipv6addr, vlan);
    if (ipv6db) {
        /* it is okay to have multiple ports with same
         * address, just use the first one.
         */
        // Just refresh aging timer
#ifdef IPV6DB_DEBUG
        printk(KERN_NOTICE "%s: ipv6db_insert found " IPV6ADDR_FMT ", head %p, "
               " ipv6 " IPV6ADDR_FMT " mac " MAC_FMT "\n",
               __func__, IPV6_ARG(ipv6addr), head, IPV6_ARG(ipv6db->ipv6addr),
               MAC_ARG(ipv6db->mac_addr.addr) );
#endif
        // refresh ageing timer
        ipv6db->ageing_timer = jiffies;
        // refresh ingress port
        ipv6db->br_port = br_port;
    } else if (!ipv6db_create(head, ipv6addr, vlan, macaddr, br_port)) {
        rc = -ENOMEM;
    }

    rcu_read_unlock();
    return rc;
}


int br_ipv6db_insert(struct net_bridge *br, const unsigned int *ipv6addr,
                     const unsigned int vlan,
                     const unsigned char *macaddr,
                     struct net_bridge_port *br_port )

{
    int ret;

    spin_lock_bh(&br->ipv6_hash_lock);
    ret = ipv6db_insert(br, ipv6addr, vlan, macaddr, br_port);
    spin_unlock_bh(&br->ipv6_hash_lock);
    return ret;
}
EXPORT_SYMBOL(br_ipv6db_insert);


/*
 * Fill buffer with forwarding table records in
 * the API format.
 */
int br_ipv6db_fillbuf(struct net_bridge *br, void *buf,
                      unsigned long maxnum, unsigned long skip)
{
    struct __ipv6db_entry *fe = buf;
    int i;
    unsigned long num = 0;
    struct hlist_node *h;
    struct net_bridge_ipv6db_entry *f;

    memset(buf, 0, maxnum*sizeof(struct __ipv6db_entry));

    rcu_read_lock();
    for (i = 0; i < BR_HASH_SIZE; i++) {
        hlist_for_each_entry_rcu(f, h, &br->ipv6_hash[i], hlist) {
#ifdef IPV6DB_DEBUG
            printk(KERN_NOTICE "%s: idx %d, num %lu, maxnum %lu, aging %lu %lu %lu, "
                   "expired %d, ip " IPV6ADDR_FMT ", mac " MAC_FMT "\n",
                   __func__, i, num, maxnum, f->ageing_timer, br->ip_ageing_time,
                   jiffies, ipv6db_has_expired(br, f),
                   IPV6_ARG(f->ipv6addr), MAC_ARG(f->mac_addr.addr) );
#endif
            if (num >= maxnum)
                goto out;

            if (skip) {
                --skip;
                continue;
            }

            if (ipv6db_has_expired(br, f))
                continue;

            if (!f->br_port)
                continue;

            /* convert from internal format to API */
            memcpy(fe->mac_addr, f->mac_addr.addr, ETH_ALEN);
            memcpy(fe->ipv6addr, f->ipv6addr, 16);

            if (f->br_port->dev) {
                memcpy(fe->port_name, f->br_port->dev->name, IFNAMSIZ);
            }
            fe->vlan = f->vlan;
            fe->ageing_timer_value = jiffies_to_clock_t(jiffies - f->ageing_timer);
            fe->iftype = f->br_port->pif_iftype;
            ++fe;
            ++num;
        }
    }

 out:
    rcu_read_unlock();

    return num;
}


int br_ipv6db_get_mac(struct net_bridge *br, unsigned int *ipv6addr,
                      unsigned int vlan, unsigned char **macaddr, unsigned int *iftype)
{
    struct hlist_head *head = &br->ipv6_hash[br_ipv6_hash(ipv6addr)];
    struct net_bridge_ipv6db_entry *ipv6db;
    int err = 0;

#ifdef IPV6DB_DEBUG
    printk(KERN_NOTICE "%s: hash %d, head %p, ip " IPV6ADDR_FMT,
           __func__, br_ipv6_hash(ipv6addr), head, IPV6_ARG(ipv6addr));
#endif

    spin_lock_bh(&br->ipv6_hash_lock);
    ipv6db = ipv6db_find(head, ipv6addr, vlan);
    if (ipv6db) {
        if (ipv6db_has_expired(br, ipv6db)) {
            err = -EINVAL;
        } else if (!ipv6db->br_port) {
            err = -EINVAL;
        } else {
            *macaddr = ipv6db->mac_addr.addr;
            *iftype = ipv6db->br_port->pif_iftype;
#ifdef IPV6DB_DEBUG
            printk(KERN_NOTICE "%s: found - hash %d, head %p, ip " IPV6ADDR_FMT", "
                   "vlan %d, mac " MAC_FMT ", iftype %d\n",
                   __func__, br_ipv6_hash(ipv6addr), head, IPV6_ARG(ipv6addr), vlan,
                   MAC_ARG(ipv6db->mac_addr.addr), *iftype);
#endif
        }
    } else {
        err = -EINVAL;
    }

    spin_unlock_bh(&br->ipv6_hash_lock);

    return err;
}
EXPORT_SYMBOL(br_ipv6db_get_mac);


int br_ipv6db_delete_by_mac(struct net_bridge *br, int ifidx, char *macaddr)
{

    int i;

    rcu_read_lock();
    spin_lock_bh(&br->ipv6_hash_lock);
    for (i = 0; i < BR_HASH_SIZE; i++) {
        struct net_bridge_ipv6db_entry *f;
        struct hlist_node *h, *n;

        hlist_for_each_entry_safe(f, h, n, &br->ipv6_hash[i], hlist) {
            if (f->br_port &&
                f->br_port->dev &&
                f->br_port->dev->ifindex == ifidx &&
                !compare_ether_addr(f->mac_addr.addr, macaddr))
                ipv6db_delete(f);
        }
    }
    spin_unlock_bh(&br->ipv6_hash_lock);
    rcu_read_unlock();

    return 0;
}


int br_ipv6db_delete_by_port(struct net_bridge *br, int ifidx)
{

    int i;

    rcu_read_lock();
    spin_lock_bh(&br->ipv6_hash_lock);
    for (i = 0; i < BR_HASH_SIZE; i++) {
        struct net_bridge_ipv6db_entry *f;
        struct hlist_node *h, *n;

        hlist_for_each_entry_safe(f, h, n, &br->ipv6_hash[i], hlist) {
            if (f->br_port &&
                f->br_port->dev &&
                f->br_port->dev->ifindex == ifidx)
                ipv6db_delete(f);
        }
    }
    spin_unlock_bh(&br->ipv6_hash_lock);
    rcu_read_unlock();

    return 0;
}
EXPORT_SYMBOL(br_ipv6db_delete_by_port);


int br_ipv6db_delete_by_ifmode(struct net_bridge *br, int ifmode)
{

    int i;

    rcu_read_lock();
    spin_lock_bh(&br->ipv6_hash_lock);
    for (i = 0; i < BR_HASH_SIZE; i++) {
        struct net_bridge_ipv6db_entry *f;
        struct hlist_node *h, *n;

        hlist_for_each_entry_safe(f, h, n, &br->ipv6_hash[i], hlist) {
            if (f->br_port &&
                f->br_port->pif_ifmode == ifmode)
                ipv6db_delete(f);
        }
    }
    spin_unlock_bh(&br->ipv6_hash_lock);
    rcu_read_unlock();

    return 0;
}
EXPORT_SYMBOL(br_ipv6db_delete_by_ifmode);


void br_ipv6db_cleanup(unsigned long _data)
{
	struct net_bridge *br = (struct net_bridge *)_data;
	unsigned long delay = br->ip_ageing_time;
	unsigned long next_timer = jiffies + br->ip_ageing_time;
	int i;

	spin_lock_bh(&br->ipv6_hash_lock);
	for (i = 0; i < BR_HASH_SIZE; i++) {
		struct net_bridge_ipv6db_entry *f;
		struct hlist_node *h, *n;

		hlist_for_each_entry_safe(f, h, n, &br->ipv6_hash[i], hlist) {
			unsigned long this_timer;
			this_timer = f->ageing_timer + delay;
			if (time_before_eq(this_timer, jiffies))
				ipv6db_delete(f);
			else if (time_before(this_timer, next_timer))
				next_timer = this_timer;
		}
	}
	spin_unlock_bh(&br->ipv6_hash_lock);

    if (br->ip_ageing_time) {
	/* Add HZ/4 to ensure we round the jiffies upwards to be after the next
	 * timer, otherwise we might round down and will have no-op run. */
	mod_timer(&br->ipv6db_gc_timer, round_jiffies(next_timer + HZ/4));
    }
}

#endif // CONFIG_BRIDGE_PACKET_INSPECTION_FILTER_MODULE
