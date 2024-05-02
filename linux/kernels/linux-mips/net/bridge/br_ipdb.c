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

//#define IPDB_DEBUG 1


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
static kmem_cache_t *br_ipdb_cache __read_mostly;
#else
static struct kmem_cache *br_ipdb_cache __read_mostly;
#endif

void __init br_ipdb_init(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
    br_ipdb_cache = kmem_cache_create("bridge_ipdb_cache",
                                      sizeof(struct net_bridge_ipdb_entry),
                                      0,
                                      SLAB_HWCACHE_ALIGN, NULL, NULL);

#else
    br_ipdb_cache = kmem_cache_create("bridge_ipdb_cache",
                                      sizeof(struct net_bridge_ipdb_entry),
                                      0,
                                      SLAB_HWCACHE_ALIGN, NULL);
#endif
}

void __exit br_ipdb_fini(void)
{
    kmem_cache_destroy(br_ipdb_cache);
}

static __inline__ int ipdb_has_expired(const struct net_bridge *br,
                                       const struct net_bridge_ipdb_entry *ipdb)
{
    if (br->ip_ageing_time) {
        return time_before_eq(ipdb->ageing_timer + br->ip_ageing_time, jiffies);
    } else {
        /* ignore expired when ip_ageing_time is 0 */
        return 0;
    }
}

static __inline__ int br_ip_hash(const unsigned int ipaddr)
{
    return jhash((unsigned char *)&ipaddr, sizeof(unsigned int), 0) & (BR_HASH_SIZE - 1);
}

static inline struct net_bridge_ipdb_entry *ipdb_find(struct hlist_head *head,
                                                      const unsigned int ipaddr, const unsigned int vlan)
{
    struct hlist_node *h;
    struct net_bridge_ipdb_entry *ipdb;

    hlist_for_each_entry_rcu(ipdb, h, head, hlist) {
#ifdef IPDB_DEBUG
        printk(KERN_NOTICE "%s: ipsearch " IPADDR_FMT ", head %p, "
               "ip %d.%d.%d.%d mac " MAC_FMT "\n",
               __func__, NIPQUAD(ipaddr), head, NIPQUAD(ipdb->ipaddr),
               MAC_ARG(ipdb->mac_addr.addr) );
#endif
        if ( ipdb->ipaddr == ipaddr && ipdb->vlan == vlan )
            return ipdb;
    }
    return NULL;
}

static inline struct net_bridge_ipdb_entry *ipdb_match(struct hlist_head *head, const unsigned char *macaddr,
                                                       const unsigned int ipaddr, const unsigned int vlan)
{
    struct hlist_node *h;
    struct net_bridge_ipdb_entry *ipdb;

    hlist_for_each_entry_rcu(ipdb, h, head, hlist) {
#ifdef IPDB_DEBUG
        printk(KERN_NOTICE "%s: ipsearch " IPADDR_FMT ", head %p, "
               "ip %d.%d.%d.%d mac " MAC_FMT "\n",
               __func__, NIPQUAD(ipaddr), head, NIPQUAD(ipdb->ipaddr),
               MAC_ARG(ipdb->mac_addr.addr) );
#endif
        if (ipdb->ipaddr == ipaddr &&
            ipdb->vlan == vlan &&
            !compare_ether_addr(ipdb->mac_addr.addr, macaddr))
            return ipdb;
    }
    return NULL;
}

static void ipdb_rcu_free(struct rcu_head *head)
{
    struct net_bridge_ipdb_entry *ent
        = container_of(head, struct net_bridge_ipdb_entry, rcu);
    kmem_cache_free(br_ipdb_cache, ent);
}


static void br_ipdb_put(struct net_bridge_ipdb_entry *ent)
{
    if (atomic_dec_and_test(&ent->use_count))
        call_rcu(&ent->rcu, ipdb_rcu_free);
}


static __inline__ void ipdb_delete(struct net_bridge_ipdb_entry *f)
{
    hlist_del_rcu(&f->hlist);
    br_ipdb_put(f);
}


void br_ipdb_clear(struct net_bridge *br)
{
    int i;

    spin_lock_bh(&br->ip_hash_lock);
    for (i = 0; i < BR_HASH_SIZE; i++) {
        struct net_bridge_ipdb_entry *f;
        struct hlist_node *h, *n;

        hlist_for_each_entry_safe(f, h, n, &br->ip_hash[i], hlist) {
            ipdb_delete(f);
        }
    }
    spin_unlock_bh(&br->ip_hash_lock);
}
EXPORT_SYMBOL(br_ipdb_clear);


/* No locking or refcounting, assumes caller has no preempt (rcu_read_lock) */
struct net_bridge_ipdb_entry *__br_ipdb_get(struct net_bridge *br,
                                            const unsigned int ipaddr, const unsigned int vlan )
{
    struct hlist_node *h;
    struct net_bridge_ipdb_entry *ipdb;

    hlist_for_each_entry_rcu(ipdb, h, &br->ip_hash[br_ip_hash(ipaddr)], hlist) {
        if (ipdb->ipaddr == ipaddr && ipdb->vlan == vlan) {
            if (unlikely(ipdb_has_expired(br, ipdb)))
                break;
            return ipdb;
        }
    }

    return NULL;
}


/* Interface used by ATM hook that keeps a ref count */
struct net_bridge_ipdb_entry *br_ipdb_get(struct net_bridge *br,
                                          unsigned int ipaddr, const unsigned int vlan )
{
    struct net_bridge_ipdb_entry *ipdb;

    rcu_read_lock();
    ipdb = __br_ipdb_get(br, ipaddr, vlan);
    if (ipdb)
        atomic_inc(&ipdb->use_count);
    rcu_read_unlock();
    return ipdb;
}


static struct net_bridge_ipdb_entry *ipdb_create(struct hlist_head *head,
                                                 const unsigned int ipaddr,
                                                 const unsigned int vlan,
                                                 const unsigned char *macaddr,
                                                 struct net_bridge_port *br_port )
{
    struct net_bridge_ipdb_entry *ipdb;

#ifdef IPDB_DEBUG
    printk(KERN_NOTICE "%s: head %p, ip %d.%d.%d.%d, mac " MAC_FMT "\n",
           __func__, head, NIPQUAD(ipaddr), MAC_ARG(macaddr) );
#endif

    ipdb = kmem_cache_alloc(br_ipdb_cache, GFP_ATOMIC);
    if (ipdb) {
        ipdb->ipaddr = ipaddr;
        ipdb->vlan = vlan;
        memcpy(ipdb->mac_addr.addr, macaddr, ETH_ALEN);
        atomic_set(&ipdb->use_count, 1);
        hlist_add_head_rcu(&ipdb->hlist, head);
        ipdb->br_port = br_port;
        ipdb->ageing_timer = jiffies;
    }
    return ipdb;
}


static int ipdb_insert(struct net_bridge *br,
                       const unsigned int ipaddr,
                       const unsigned int vlan,
                       const unsigned char *macaddr,
                       struct net_bridge_port *br_port )
{
    struct hlist_head *head = &br->ip_hash[br_ip_hash(ipaddr)];
    struct net_bridge_ipdb_entry *ipdb;
    int rc = 0;

#ifdef IPDB_DEBUG
    printk(KERN_NOTICE "%s: hash %d, head %p, ip %d.%d.%d.%d, mac " MAC_FMT "\n",
           __func__, br_ip_hash(ipaddr), head, NIPQUAD(ipaddr), MAC_ARG(macaddr));
#endif

    if (!is_valid_ether_addr(macaddr))
        return -EINVAL;

    if (ipaddr == 0 || ipaddr == 0xFFFFFFFF)
        return -EINVAL;

    rcu_read_lock();
    ipdb = ipdb_find(head, ipaddr, vlan);
    if (ipdb) {
        /* it is okay to have multiple ports with same
         * address, just use the first one.
         */
        // refresh aging timer
        ipdb->ageing_timer = jiffies;
        // refresh ingress port
        if(br_port) ipdb->br_port = br_port;
    } else if (!ipdb_create(head, ipaddr, vlan, macaddr, br_port)) {
        rc = -ENOMEM;
    }

    rcu_read_unlock();
    return rc;
}


int br_ipdb_insert(struct net_bridge *br, const unsigned int ipaddr,
                   const unsigned int vlan,
                   const unsigned char *macaddr,
                   struct net_bridge_port *br_port )

{
    int ret;

    spin_lock_bh(&br->ip_hash_lock);
    ret = ipdb_insert(br, ipaddr, vlan, macaddr, br_port);
    spin_unlock_bh(&br->ip_hash_lock);
    return ret;
}
EXPORT_SYMBOL(br_ipdb_insert);


static inline struct net_bridge_ipdb_entry *ipdb_find_entry(struct hlist_head *head, const unsigned char *macaddr,
                                                            const unsigned int ipaddr, const unsigned int vlan)
{
    struct hlist_node *h;
    struct net_bridge_ipdb_entry *ipdb;

    hlist_for_each_entry_rcu(ipdb, h, head, hlist) {
#ifdef IPDB_DEBUG
        printk(KERN_NOTICE "%s: ipsearch " IPADDR_FMT ", head %p, "
               "ip %d.%d.%d.%d mac " MAC_FMT "\n",
               __func__, NIPQUAD(ipaddr), head, NIPQUAD(ipdb->ipaddr),
               MAC_ARG(ipdb->mac_addr.addr) );
#endif
        if ( ipdb->ipaddr == ipaddr && ipdb->vlan == vlan &&
             !compare_ether_addr(ipdb->mac_addr.addr, macaddr))
            return ipdb;
    }
    return NULL;
}


static int __ipdb_insert(struct net_bridge *br,
                         const unsigned int ipaddr,
                         const unsigned int vlan,
                         const unsigned char *macaddr,
                         struct net_bridge_port *br_port )
{
    struct hlist_head *head = &br->ip_hash[br_ip_hash(ipaddr)];
    struct net_bridge_ipdb_entry *ipdb;

#ifdef IPDB_DEBUG
    printk(KERN_NOTICE "%s: hash %d, head %p, ip %d.%d.%d.%d, mac " MAC_FMT "\n",
           __func__, br_ip_hash(ipaddr), head, NIPQUAD(ipaddr), MAC_ARG(macaddr));
#endif

    if (!is_valid_ether_addr(macaddr))
        return -EINVAL;

    if (ipaddr == 0 || ipaddr == 0xFFFFFFFF)
        return -EINVAL;

    ipdb = ipdb_match(head, macaddr,ipaddr, vlan);
    if (ipdb) {
        ipdb->br_port = br_port;
        ipdb->ageing_timer = jiffies;
        return 0;
    }

    if (!ipdb_create(head, ipaddr, vlan, macaddr, br_port))
        return -ENOMEM;

    return 0;
}


/*
 * Fill buffer with forwarding table records in
 * the API format.
 */
int br_ipdb_fillbuf(struct net_bridge *br, void *buf,
                    unsigned long maxnum, unsigned long skip)
{
    struct __ipdb_entry *fe = buf;
    int i;
    unsigned long num = 0;
    struct hlist_node *h;
    struct net_bridge_ipdb_entry *f;

    memset(buf, 0, maxnum*sizeof(struct __ipdb_entry));

    rcu_read_lock();
    for (i = 0; i < BR_HASH_SIZE; i++) {
        hlist_for_each_entry_rcu(f, h, &br->ip_hash[i], hlist) {
#ifdef IPDB_DEBUG
            printk(KERN_NOTICE "%s: idx %d, num %lu, maxnum %lu, aging %lu %lu %lu, "
                   "expired %d, ip %d.%d.%d.%d, mac " MAC_FMT "\n",
                   __func__, i, num, maxnum, f->ageing_timer, br->ip_ageing_time,
                   jiffies, ipdb_has_expired(br, f),
                   NIPQUAD(f->ipaddr), MAC_ARG(f->mac_addr.addr) );
#endif
            if (num >= maxnum)
                goto out;

            if (skip) {
                --skip;
                continue;
            }

            if (ipdb_has_expired(br, f))
                continue;

            if (!f->br_port)
                continue;

            /* convert from internal format to API */
            memcpy(fe->mac_addr, f->mac_addr.addr, ETH_ALEN);
            fe->ipaddr = f->ipaddr;
            fe->vlan = f->vlan;

            if (f->br_port->dev) {
                memcpy(fe->port_name, f->br_port->dev->name, IFNAMSIZ);
            }
            if (ipdb_has_expired(br, f))
                fe->ageing_timer_value = 0;
            else
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


int br_ipdb_get_mac(struct net_bridge *br, unsigned int ipaddr,
                    unsigned int vlan, unsigned char **macaddr, unsigned int *iftype)
{
    struct hlist_head *head = &br->ip_hash[br_ip_hash(ipaddr)];
    struct net_bridge_ipdb_entry *ipdb;
    int err = 0;

#ifdef IPDB_DEBUG
    printk(KERN_NOTICE "%s: hash %d, head %p, ip %d.%d.%d.%d\n",
           __func__, br_ip_hash(ipaddr), head, NIPQUAD(ipaddr));
#endif

    spin_lock_bh(&br->ip_hash_lock);
    ipdb = ipdb_find(head, ipaddr, vlan);
    if (ipdb) {
        if (ipdb_has_expired(br, ipdb)) {
            err = -EINVAL;
        } else if (!ipdb->br_port) {
            err = -EINVAL;
        } else {
            *macaddr = ipdb->mac_addr.addr;
            *iftype = ipdb->br_port->pif_iftype;
#ifdef IPDB_DEBUG
            printk(KERN_NOTICE "%s: found - hash %d, head %p, ip %d.%d.%d.%d, "
                   "vlan %d, mac " MAC_FMT ", iftype %d\n",
                   __func__, br_ip_hash(ipaddr), head, NIPQUAD(ipaddr), vlan, MAC_ARG(ipdb->mac_addr.addr), *iftype);
#endif
        }
    } else {
        err = -EINVAL;
    }

    spin_unlock_bh(&br->ip_hash_lock);

    return err;
}
EXPORT_SYMBOL(br_ipdb_get_mac);


int br_ipdb_delete_by_mac(struct net_bridge *br, int ifidx, char *macaddr)
{

    int i;

    rcu_read_lock();
    spin_lock_bh(&br->ip_hash_lock);
    for (i = 0; i < BR_HASH_SIZE; i++) {
        struct net_bridge_ipdb_entry *f;
        struct hlist_node *h, *n;

        hlist_for_each_entry_safe(f, h, n, &br->ip_hash[i], hlist) {
            if (f->br_port &&
                f->br_port->dev &&
                f->br_port->dev->ifindex == ifidx && !memcmp(f->mac_addr.addr, macaddr, ETH_ALEN))
                ipdb_delete(f);
        }
    }
    spin_unlock_bh(&br->ip_hash_lock);
    rcu_read_unlock();

    return 0;
}

int br_ipdb_delete_by_port(struct net_bridge *br, int port)
{
    int i;

    rcu_read_lock();
    spin_lock_bh(&br->ip_hash_lock);
    for (i = 0; i < BR_HASH_SIZE; i++) {
        struct net_bridge_ipdb_entry *f;
        struct hlist_node *h, *n;

        hlist_for_each_entry_safe(f, h, n, &br->ip_hash[i], hlist) {
            if (f->br_port &&
                f->br_port->dev &&
                f->br_port->dev->ifindex == port)
                ipdb_delete(f);
        }
    }
    spin_unlock_bh(&br->ip_hash_lock);
    rcu_read_unlock();
    return 0;
}
EXPORT_SYMBOL(br_ipdb_delete_by_port);


int br_ipdb_delete_by_ifmode(struct net_bridge *br, int ifmode)
{
    int i;

    rcu_read_lock();
    spin_lock_bh(&br->ip_hash_lock);
    for (i = 0; i < BR_HASH_SIZE; i++) {
        struct net_bridge_ipdb_entry *f;
        struct hlist_node *h, *n;

        hlist_for_each_entry_safe(f, h, n, &br->ip_hash[i], hlist) {
            if (f->br_port &&
                f->br_port->pif_ifmode == ifmode)
                ipdb_delete(f);
        }
    }
    spin_unlock_bh(&br->ip_hash_lock);
    rcu_read_unlock();
    return 0;
}
EXPORT_SYMBOL(br_ipdb_delete_by_ifmode);


void br_ipdb_cleanup(unsigned long _data)
{
	struct net_bridge *br = (struct net_bridge *)_data;
	unsigned long delay = br->ip_ageing_time;
	unsigned long next_timer = jiffies + br->ip_ageing_time;
	int i;

	spin_lock_bh(&br->ip_hash_lock);
	for (i = 0; i < BR_HASH_SIZE; i++) {
		struct net_bridge_ipdb_entry *f;
		struct hlist_node *h, *n;

		hlist_for_each_entry_safe(f, h, n, &br->ip_hash[i], hlist) {
			unsigned long this_timer;
			this_timer = f->ageing_timer + delay;
			if (time_before_eq(this_timer, jiffies))
				ipdb_delete(f);
			else if (time_before(this_timer, next_timer))
				next_timer = this_timer;
		}
	}
	spin_unlock_bh(&br->ip_hash_lock);

    if (br->ip_ageing_time) {
	/* Add HZ/4 to ensure we round the jiffies upwards to be after the next
	 * timer, otherwise we might round down and will have no-op run. */
	mod_timer(&br->ipdb_gc_timer, round_jiffies(next_timer + HZ/4));
    }
}

#endif // CONFIG_BRIDGE_PACKET_INSPECTION_FILTER_MODULE
