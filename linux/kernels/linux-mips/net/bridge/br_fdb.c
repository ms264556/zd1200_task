/*
 *	Forwarding database
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
#include <linux/init.h>
#include <linux/rculist.h>
#include <linux/spinlock.h>
#include <linux/times.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/jhash.h>
#include <linux/random.h>
#include <asm/atomic.h>
#include <asm/unaligned.h>
#include "br_private.h"

#ifdef V54_BSP
#include <linux/if_bridge.h>
static BrDbUpdate_CB_type br_db_update_hook = NULL;
void
register_br_db_update_hook(BrDbUpdate_CB_type cb_func)
{
	br_db_update_hook = cb_func;
}
#endif

static struct kmem_cache *br_fdb_cache __read_mostly;

static int fdb_insert(struct net_bridge *br, struct net_bridge_port *source,
#ifdef BR_FDB_VLAN
		      const unsigned int vlan_id,
#endif
		      const unsigned char *addr,
		      int is_local);
static u32 fdb_salt __read_mostly;


#ifdef CONFIG_ANTI_SPOOF
int (*aspoof_filter_hook)(const struct sk_buff *) = 0;
EXPORT_SYMBOL(aspoof_filter_hook);
#endif /* CONFIG_ANTI_SPOOF */


int __init br_fdb_init(void)
{
	br_fdb_cache = kmem_cache_create("bridge_fdb_cache",
					 sizeof(struct net_bridge_fdb_entry),
					 0,
					 SLAB_HWCACHE_ALIGN, NULL);
	if (!br_fdb_cache)
		return -ENOMEM;

	get_random_bytes(&fdb_salt, sizeof(fdb_salt));
	return 0;
}

void br_fdb_fini(void)
{
	kmem_cache_destroy(br_fdb_cache);
}


/* if topology_changing then use forward_delay (default 15 sec)
 * otherwise keep longer (default 5 minutes)
 */
static inline unsigned long hold_time(const struct net_bridge *br)
{
	return br->topology_change ? br->forward_delay : br->ageing_time;
}

static inline int has_expired(const struct net_bridge *br,
			      const struct net_bridge_fdb_entry *fdb)
{
	return !fdb->is_static &&
	       time_before_eq(fdb->ageing_timer + hold_time(br), jiffies);
}

static inline int br_mac_hash(const unsigned char *mac)
{
	/* use 1 byte of OUI cnd 3 bytes of NIC */
	u32 key = get_unaligned((u32 *)(mac + 2));
	return jhash_1word(key, fdb_salt) & (BR_HASH_SIZE - 1);
}

static void fdb_rcu_free(struct rcu_head *head)
{
	struct net_bridge_fdb_entry *ent
		= container_of(head, struct net_bridge_fdb_entry, rcu);
	kmem_cache_free(br_fdb_cache, ent);
}

static inline void fdb_delete(struct net_bridge_fdb_entry *f)
{
	hlist_del_rcu(&f->hlist);
	call_rcu(&f->rcu, fdb_rcu_free);
}

void br_fdb_changeaddr(struct net_bridge_port *p,
#ifdef BR_FDB_VLAN
		       const unsigned int vlan_id,
#endif
		       const unsigned char *newaddr)
{
	struct net_bridge *br = p->br;
	int i;

	spin_lock_bh(&br->hash_lock);

	/* Search all chains since old address/hash is unknown */
	for (i = 0; i < BR_HASH_SIZE; i++) {
		struct hlist_node *h;
		hlist_for_each(h, &br->hash[i]) {
			struct net_bridge_fdb_entry *f;

			f = hlist_entry(h, struct net_bridge_fdb_entry, hlist);
			if (f->dst == p && f->is_local) {
				/* maybe another port has same hw addr? */
				struct net_bridge_port *op;
				list_for_each_entry(op, &br->port_list, list) {
					if (op != p &&
#ifdef BR_FDB_VLAN
					    (f->vlan_id == vlan_id) &&
#endif
					    !compare_ether_addr(op->dev->dev_addr,
								f->addr.addr)) {
						f->dst = op;
						goto insert;
					}
				}

				/* delete old one */
				fdb_delete(f);
				goto insert;
			}
		}
	}
 insert:
	/* insert new address,  may fail if invalid address or dup. */
#ifdef BR_FDB_VLAN
	fdb_insert(br, p, vlan_id, newaddr, 1);
#else
	fdb_insert(br, p, newaddr, 1);
#endif

	spin_unlock_bh(&br->hash_lock);
}

void br_fdb_cleanup(unsigned long _data)
{
	struct net_bridge *br = (struct net_bridge *)_data;
	unsigned long delay = hold_time(br);
	unsigned long next_timer = jiffies + br->forward_delay;
	int i;

	spin_lock_bh(&br->hash_lock);
	for (i = 0; i < BR_HASH_SIZE; i++) {
		struct net_bridge_fdb_entry *f;
		struct hlist_node *h, *n;

		hlist_for_each_entry_safe(f, h, n, &br->hash[i], hlist) {
			unsigned long this_timer;
			if (f->is_static)
				continue;
			this_timer = f->ageing_timer + delay;
			if (time_before_eq(this_timer, jiffies)) {
				fdb_delete(f);
			}
			else if (time_before(this_timer, next_timer))
				next_timer = this_timer;
		}
	}
	spin_unlock_bh(&br->hash_lock);

	/* Add HZ/4 to ensure we round the jiffies upwards to be after the next
	 * timer, otherwise we might round down and will have no-op run. */
	mod_timer(&br->gc_timer, round_jiffies(next_timer + HZ/4));
}

/* Completely flush all dynamic entries in forwarding database.*/
void br_fdb_flush(struct net_bridge *br)
{
	int i;

	spin_lock_bh(&br->hash_lock);
	for (i = 0; i < BR_HASH_SIZE; i++) {
		struct net_bridge_fdb_entry *f;
		struct hlist_node *h, *n;
		hlist_for_each_entry_safe(f, h, n, &br->hash[i], hlist) {
			if (!f->is_static)
				fdb_delete(f);
		}
	}
	spin_unlock_bh(&br->hash_lock);
}


#ifdef CONFIG_BRIDGE_MESH_PKT_FORWARDING_FILTER
void br_fdb_clear(struct net_bridge *br)
{
	int i;

	spin_lock_bh(&br->hash_lock);
	for (i = 0; i < BR_HASH_SIZE; i++) {
		struct net_bridge_fdb_entry *f;
		struct hlist_node *h, *n;

		hlist_for_each_entry_safe(f, h, n, &br->hash[i], hlist) {
			if (!f->is_local) {
				fdb_delete(f);
			}
		}
	}
	spin_unlock_bh(&br->hash_lock);
}
#endif

/* Flush all entries refering to a specific port.
 * if do_all is set also flush static entries
 */
void br_fdb_delete_by_port(struct net_bridge *br,
			   const struct net_bridge_port *p,
			   int do_all)
{
	int i;

	spin_lock_bh(&br->hash_lock);
	for (i = 0; i < BR_HASH_SIZE; i++) {
		struct hlist_node *h, *g;

		hlist_for_each_safe(h, g, &br->hash[i]) {
			struct net_bridge_fdb_entry *f
				= hlist_entry(h, struct net_bridge_fdb_entry, hlist);
			if (f->dst != p)
				continue;

			if (f->is_static && !do_all)
				continue;
			/*
			 * if multiple ports all have the same device address
			 * then when one port is deleted, assign
			 * the local entry to other port
			 */
			if (f->is_local) {
				struct net_bridge_port *op;
				list_for_each_entry(op, &br->port_list, list) {
					if (op != p &&
					    !compare_ether_addr(op->dev->dev_addr,
								f->addr.addr)) {
						f->dst = op;
						goto skip_delete;
					}
				}
			}

			fdb_delete(f);

		skip_delete: ;
		}
	}
	spin_unlock_bh(&br->hash_lock);
}

/* No locking or refcounting, assumes caller has no preempt (rcu_read_lock) */
struct net_bridge_fdb_entry *__br_fdb_get(struct net_bridge *br,
#ifdef BR_FDB_VLAN
					  const unsigned int vlan_id,
#endif
					  const unsigned char *addr)
{
	struct hlist_node *h;
	struct net_bridge_fdb_entry *fdb;

	hlist_for_each_entry_rcu(fdb, h, &br->hash[br_mac_hash(addr)], hlist) {
#ifdef BR_FDB_VLAN
		if (vlan_id > 0 &&
		    (fdb->vlan_id == vlan_id || fdb->is_local || fdb->is_static))
#endif
		if (!compare_ether_addr(fdb->addr.addr, addr)) {
			if (unlikely(has_expired(br, fdb)))
				break;
			return fdb;
		}
	}

	return NULL;
}
#if defined(CONFIG_BRIDGE_DHCP_HOOK) || defined(CONFIG_BRIDGE_DHCP_HOOK_MODULE) || (CONFIG_BRIDGE_PACKET_INSPECTION_FILTER_MODULE)
EXPORT_SYMBOL(__br_fdb_get);
#endif


#if defined(CONFIG_ATM_LANE) || defined(CONFIG_ATM_LANE_MODULE)
/* Interface used by ATM LANE hook to test
 * if an addr is on some other bridge port */
int br_fdb_test_addr(struct net_device *dev, unsigned char *addr)
{
	struct net_bridge_fdb_entry *fdb;
	int ret;

	if (!dev->br_port)
		return 0;

	rcu_read_lock();
#ifdef BR_FDB_VLAN
	fdb = __br_fdb_get(dev->br_port->br, 1 /* right vlan id ?*/, addr);
#else
	fdb = __br_fdb_get(dev->br_port->br, addr);
#endif
	ret = fdb && fdb->dst->dev != dev &&
		fdb->dst->state == BR_STATE_FORWARDING;
	rcu_read_unlock();

	return ret;
}
#endif /* CONFIG_ATM_LANE */

/*
 * Fill buffer with forwarding table records in
 * the API format.
 */
int br_fdb_fillbuf(struct net_bridge *br, void *buf,
		   unsigned long maxnum, unsigned long skip)
{
	struct __fdb_entry *fe = buf;
	int i, num = 0;
	struct hlist_node *h;
	struct net_bridge_fdb_entry *f;

	memset(buf, 0, maxnum*sizeof(struct __fdb_entry));

	rcu_read_lock();
	for (i = 0; i < BR_HASH_SIZE; i++) {
		hlist_for_each_entry_rcu(f, h, &br->hash[i], hlist) {
			if (num >= maxnum)
				goto out;

			if (has_expired(br, f))
				continue;

			if (skip) {
				--skip;
				continue;
			}

			/* convert from internal format to API */
			memcpy(fe->mac_addr, f->addr.addr, ETH_ALEN);

			/* due to ABI compat need to split into hi/lo */
			/*
			 * yun.wu, 2010-12-03
			 * FDB entry for bridge mac address, whose dst field is NULL, probably exists.
			 * So comparing f->dst with NULL is absolutely necessary here.
			 */
			if (NULL != f->dst)
			{
				fe->port_no = f->dst->port_no;
				fe->port_hi = f->dst->port_no >> 8;
#if 1 /* V54_BSP */
				memcpy(fe->port_name, f->dst->dev->name, IFNAMSIZ);
#endif
			}
			else
			{
				/* FDB entry for bridge mac address */
				fe->port_no = 0;
				fe->port_hi = 0;
			}

			fe->is_local = f->is_local;
#ifdef BR_FDB_VLAN
			fe->vlan_id = f->vlan_id;
#endif
			if (!f->is_static)
				fe->ageing_timer_value = jiffies_to_clock_t(jiffies - f->ageing_timer);
			++fe;
			++num;
		}
	}

 out:
	rcu_read_unlock();

	return num;
}

static inline struct net_bridge_fdb_entry *fdb_find(struct hlist_head *head,
#ifdef BR_FDB_VLAN
						    const unsigned int vlan_id,
#endif
						    const unsigned char *addr)
{
	struct hlist_node *h;
	struct net_bridge_fdb_entry *fdb;

	hlist_for_each_entry_rcu(fdb, h, head, hlist) {
#ifdef BR_FDB_VLAN
		if ( fdb->vlan_id == vlan_id || fdb->is_local || fdb->is_static )
#endif
		if (!compare_ether_addr(fdb->addr.addr, addr))
			return fdb;
	}
	return NULL;
}

static struct net_bridge_fdb_entry *fdb_create(struct hlist_head *head,
					       struct net_bridge_port *source,
#ifdef BR_FDB_VLAN
					       const unsigned int vlan_id,
#endif
					       const unsigned char *addr,
					       int is_local,
					       int is_static)
{
	struct net_bridge_fdb_entry *fdb;

	fdb = kmem_cache_alloc(br_fdb_cache, GFP_ATOMIC);
	if (fdb) {
		memcpy(fdb->addr.addr, addr, ETH_ALEN);
		hlist_add_head_rcu(&fdb->hlist, head);

		fdb->dst = source;
		fdb->is_local = is_local;
		fdb->is_static = is_static;
		fdb->ageing_timer = jiffies;
#ifdef BR_FDB_VLAN
		fdb->vlan_id = vlan_id;
#endif
#ifdef CONFIG_BRIDGE_LOOP_GUARD
		fdb->mac_track.track_window = 0;
		fdb->mac_track.flap_cnt = 0;
#endif
	}
	return fdb;
}

static int fdb_insert(struct net_bridge *br, struct net_bridge_port *source,
#ifdef BR_FDB_VLAN
		      const unsigned int vlan_id,
#endif
		      const unsigned char *addr,
		      int is_local )
{
	struct hlist_head *head = &br->hash[br_mac_hash(addr)];
	struct net_bridge_fdb_entry *fdb;

	if (!is_valid_ether_addr(addr))
		return -EINVAL;

#ifdef BR_FDB_VLAN
	fdb = fdb_find(head, vlan_id, addr);
#else
	fdb = fdb_find(head, addr);
#endif
	if (fdb) {
		/* it is okay to have multiple ports with same
		 * address, just use the first one.
		 */
		if (fdb->is_local)
			return 0;
#if 1 /*to do.by gun.to temporary avoid kernel crashing*/
		if (!source)
			printk(KERN_WARNING "(source is null) adding interface with same address "
			       "as a received packet\n");
		else
#endif
			printk(KERN_WARNING "%s adding interface with same address "
			       "as a received packet\n", source->dev->name);
		fdb_delete(fdb);
	}

	if (!fdb_create(head, source,
#ifdef BR_FDB_VLAN
			vlan_id,
#endif
			addr, is_local, 1 ))
		return -ENOMEM;

	return 0;
}

int br_fdb_insert(struct net_bridge *br, struct net_bridge_port *source,
#ifdef BR_FDB_VLAN
		  const unsigned int vlan_id,
#endif
		  const unsigned char *addr,
		  int is_local )
{
	int ret;

	spin_lock_bh(&br->hash_lock);
#ifdef BR_FDB_VLAN
	ret = fdb_insert(br, source, vlan_id, addr, is_local);
#else
	ret = fdb_insert(br, source, addr, is_local );
#endif
	spin_unlock_bh(&br->hash_lock);
	return ret;
}

#ifdef CONFIG_BRIDGE_LOOP_GUARD
int _br_loop_hold = LOOP_HOLD_JIFFIES;

static inline void _br_loop_check(struct net_bridge *br,
				  struct net_bridge_mac_change *mac_track,
				  const unsigned char *addr)
{
	if ( mac_track->track_window == 0 ) {
		// start loop detect
		mac_track->track_window = jiffies + LOOP_DETECT_WINDOW;
		mac_track->flap_cnt = 1;
	} else if (time_after(jiffies, mac_track->track_window)) {
		// reset track window
		mac_track->track_window = 0;
		mac_track->flap_cnt = 0;
	} else {
		// check for flapping within LOOP_DETECT_WINDOW
		mac_track->flap_cnt++;
		if (mac_track->flap_cnt > br->flap_threshold) {
			// we concluded that it is a loop
			if (br->hostonly == 0) {
				br->hostonly = jiffies + _br_loop_hold;
				br->hostonly_cnt ++;
				if (net_ratelimit()) {
					printk(KERN_WARNING "%s: " FMT_MAC ": loop detected, hostonly\n",
					       br->dev->name, ARG_MAC(addr));
				}
			} else {
				if (net_ratelimit())
					printk(KERN_WARNING "%s: hostonly\n", br->dev->name);
			}
		}
	}
}
#endif

/* return 1 if detect own address */
int br_fdb_update(struct net_bridge_port *source, const struct sk_buff *skb)
{
	struct net_bridge_fdb_entry *fdb;
	struct net_bridge	    *br   = source->br;
	const unsigned char	    *addr = eth_hdr(skb)->h_source;
	struct hlist_head	    *head = &br->hash[br_mac_hash(addr)];

#ifdef BR_FDB_VLAN
	const unsigned int	    vlan_id = skb->tag_vid;
#endif /* BR_FDB_VLAN */

#ifdef V54_BSP
	int do_update_cb = 0;
#endif
	int rc = BR_FDB_UPDATE_OK;

	/* some users want to always flood. */
	if (hold_time(br) == 0)
		return 0;

	/* ignore packets unless we are using this port */
	if (!(source->state == BR_STATE_LEARNING ||
	      source->state == BR_STATE_FORWARDING))
		return 0;

#ifdef CONFIG_ANTI_SPOOF
	/*
	 * Check if this is a spoofed packet
	 * If the packet is from a restricted interface heading to NNI &
	 * we find its source MAC in the whitelist attached to the
	 * source interface - its an attempted spoof, so drop before
	 * updating the fdb.
	 */
	if (aspoof_filter_hook && aspoof_filter_hook(skb) == 0) {
		//printk(KERN_CRIT "[%s - Anti-spoof] - Dropped\n", __func__);
		return 0;
	}
#endif /* CONFIG_ANTI_SPOOF */

	rcu_read_lock();
#ifdef BR_FDB_VLAN
	fdb = fdb_find(head, vlan_id, addr);
#else
	fdb = fdb_find(head, addr);
#endif
	if (likely(fdb)) {
		/* attempt to update an entry for a local interface */
		if (unlikely(fdb->is_local)) {
			if (net_ratelimit()
#ifdef CONFIG_BRIDGE_LOOP_GUARD
				&& !br->hostonly
#endif
			) {
				printk(KERN_WARNING "%s: received pkt with own address "
				       FMT_MAC " as source address - dropping\n",
				       source->dev->name, ARG_MAC(addr));
#ifdef CONFIG_BRIDGE_LOOP_GUARD
				_br_loop_check(br, &br->mac_track, addr);
#endif
			}
#ifdef CONFIG_BRIDGE_MESH_PKT_FORWARDING_FILTER
			br->drop_from_self++;
#endif
			rc = BR_FDB_UPDATE_DROP; // inform caller
#ifdef CONFIG_BRIDGE_MESH_PKT_FORWARDING_FILTER
		} else if( fdb->dst->is_pff && source != fdb->dst ) {
#ifdef BR_PFF_DEBUG
			printk(KERN_NOTICE "%s: received packet with source "
			       "MAC that appears on a mff destination port "
			       "- dropping (source ",
			       source->dev->name);
			ETH_MAC_PRINT("", addr, " ", ")\n");
#endif
			rc = BR_FDB_UPDATE_UP_ONLY; // inform caller
			br->pff_drop++;
		} else if( fdb->dst->is_disable_learning ) {
#ifdef BR_PFF_DEBUG
			printk(KERN_NOTICE "%s: received packet with source "
			       "MAC that appears on a non-learning destination "
			       "port - skip update (source ",
			       source->dev->name);
			ETH_MAC_PRINT("", addr, " ", ")\n");
#endif
			br->skip_learning++;
#endif
		} else {
			/* fastpath: update of existing entry */
			if ( fdb->dst != source ) {
#ifdef CONFIG_BRIDGE_LOOP_GUARD
				_br_loop_check(br, &fdb->mac_track, addr);
#endif
#ifdef V54_BSP
				do_update_cb = 1;
#endif
			}
			fdb->dst = source;
			fdb->ageing_timer = jiffies;
		}
	} else {
		spin_lock(&br->hash_lock);
#ifdef BR_FDB_VLAN
		if (!fdb_find(head, vlan_id, addr))
#else
		if (!fdb_find(head, addr))
#endif
		{
#ifdef CONFIG_BRIDGE_MESH_PKT_FORWARDING_FILTER
			if( source->is_pff ) {
				br->pff_drop++;
				rc = BR_FDB_UPDATE_UP_ONLY; // inform caller
			} else if( source->is_disable_learning ) {
				br->skip_learning++;
			} else
#endif
			{
#ifdef BR_FDB_VLAN
				fdb_create(head, source, vlan_id, addr, 0, 0);
#else
				fdb_create(head, source, addr, 0, 0);
#endif
#ifdef V54_BSP
				do_update_cb = 1;
#endif
			}
		}
		/* else  we lose race and someone else inserts
		 * it first, don't bother updating
		 */
		spin_unlock(&br->hash_lock);
	}
	rcu_read_unlock();
#ifdef V54_BSP
	if ( do_update_cb && br_db_update_hook )
		(*br_db_update_hook)(addr);
#endif
	return rc;
}

#if 1
/*
 * Function    : br_fdb_changeaddr_br
 * Description : Add bridge mac address to FDB, so that packets destined to this address can be handed up to the upper layer
 * Parameters  :
 * Return      : N/A
 */
void br_fdb_changeaddr_br(struct net_bridge *br, const unsigned char *newaddr)
{
	int ifmac = 0;
	struct net_bridge_port *nbp;
	struct net_bridge_fdb_entry *fdb;

	spin_lock_bh(&br->hash_lock);

	/* delete fdb for old mac if it is not same to interface mac */
	list_for_each_entry(nbp, &br->port_list, list) {
		if (0 == compare_ether_addr(nbp->dev->dev_addr, br->dev->dev_addr)) {
			ifmac = 1;
			break;
		}
	}

	if (0 == ifmac) {
#ifdef BR_FDB_VLAN
		fdb = fdb_find(&br->hash[br_mac_hash(br->dev->dev_addr)], BR_ALL_VLAN, br->dev->dev_addr);
#else
		fdb = fdb_find(&br->hash[br_mac_hash(br->dev->dev_addr)], br->dev->dev_addr);
#endif
		if (fdb)
			fdb_delete(fdb);
	}

	/* add fdb for new mac */
#ifdef BR_FDB_VLAN
	fdb_insert(br, NULL, BR_ALL_VLAN, newaddr, 1);
#else
	fdb_insert(br, NULL, newaddr, 1);
#endif

	spin_unlock_bh(&br->hash_lock);
}
#endif

#ifdef BR_FDB_VLAN
// Execute my_func() for all the matched vlan entries
int
br_fdb_vlan_exec(struct net_bridge_port *br_port,
		struct sk_buff *skb, _br_vlan_func my_func)
{
	struct net_bridge *br = br_port->br;
	int i, num = 0;
	struct hlist_node *h;
	struct net_bridge_fdb_entry *f;
	unsigned vlan_id;
	struct net_bridge_port_vlan done_vlans;
#define NOT_YET_DONE(vid) !(done_vlans.filter[vid/8] & (1 << (vid & 7)))
#define SET_DONE(vid) do { done_vlans.filter[vid/8] |= (1 << (vid & 7)); } while(0);

	if ( my_func == NULL ) return 0;

	memset(&done_vlans, 0, sizeof(done_vlans));

	rcu_read_lock();
	for (i = 0; i < BR_HASH_SIZE; i++) {
		hlist_for_each_entry_rcu(f, h, &br->hash[i], hlist) {

			if (has_expired(br, f)) {
				continue;
			}

			if ( f->is_local ) {
				continue;
			}

			if ( f->dst !=  br_port ) {
#if 0
				LIMIT_PRINTK("%s: dst=%p!= br_port=%p(%s)\n", __FUNCTION__,
						f->dst, br_port, br_port->dev->name);
#endif
				// not our device
				continue;
			}

			// ours
			vlan_id = f->vlan_id ;
			if ( NOT_YET_DONE(vlan_id) ) {
				SET_DONE(vlan_id);
				if ( my_func(f, skb, vlan_id) < 0 ) {
					// error
				} else {
					num++;
				}

			}
		}
	}

	rcu_read_unlock();

	return num;
#undef NOT_YET_DONE
#undef SET_DONE
}
#endif
