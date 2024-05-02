/*
 *	Linux NET3:	GRE over IP protocol decoder.
 *
 *	Authors: Alexey Kuznetsov (kuznet@ms2.inr.ac.ru)
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 */

#include <linux/capability.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/in.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/if_arp.h>
#include <linux/mroute.h>
#include <linux/init.h>
#include <linux/in6.h>
#include <linux/inetdevice.h>
#include <linux/igmp.h>
#include <linux/netfilter_ipv4.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#if 1 // V54_TUNNELMGR
#include <linux/if_bridge.h>
#include <linux/scatterlist.h>
#include <linux/crypto.h>
#include <linux/version.h>
#include <linux/highmem.h>
#include <ruckus/l2_frag.h>
#include <linux/if_vlan.h>
#include <linux/time.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <ruckus/ieee80211_linux.h>
#endif // V54_TUNNELMGR

#include <net/sock.h>
#include <net/ip.h>
#include <net/icmp.h>
#include <net/protocol.h>
#include <net/ipip.h>
#include <net/arp.h>
#include <net/checksum.h>
#include <net/dsfield.h>
#include <net/inet_ecn.h>
#include <net/xfrm.h>
#include <net/net_namespace.h>
#include <net/netns/generic.h>
#include <net/rtnetlink.h>

#ifdef CONFIG_IPV6
#include <net/ipv6.h>
#include <net/ip6_fib.h>
#include <net/ip6_route.h>
#endif

/*
   Problems & solutions
   --------------------

   1. The most important issue is detecting local dead loops.
   They would cause complete host lockup in transmit, which
   would be "resolved" by stack overflow or, if queueing is enabled,
   with infinite looping in net_bh.

   We cannot track such dead loops during route installation,
   it is infeasible task. The most general solutions would be
   to keep skb->encapsulation counter (sort of local ttl),
   and silently drop packet when it expires. It is the best
   solution, but it supposes maintaing new variable in ALL
   skb, even if no tunneling is used.

   Current solution: HARD_TX_LOCK lock breaks dead loops.



   2. Networking dead loops would not kill routers, but would really
   kill network. IP hop limit plays role of "t->recursion" in this case,
   if we copy it from packet being encapsulated to upper header.
   It is very good solution, but it introduces two problems:

   - Routing protocols, using packets with ttl=1 (OSPF, RIP2),
     do not work over tunnels.
   - traceroute does not work. I planned to relay ICMP from tunnel,
     so that this problem would be solved and traceroute output
     would even more informative. This idea appeared to be wrong:
     only Linux complies to rfc1812 now (yes, guys, Linux is the only
     true router now :-)), all routers (at least, in neighbourhood of mine)
     return only 8 bytes of payload. It is the end.

   Hence, if we want that OSPF worked or traceroute said something reasonable,
   we should search for another solution.

   One of them is to parse packet trying to detect inner encapsulation
   made by our node. It is difficult or even impossible, especially,
   taking into account fragmentation. TO be short, tt is not solution at all.

   Current solution: The solution was UNEXPECTEDLY SIMPLE.
   We force DF flag on tunnels with preconfigured hop limit,
   that is ALL. :-) Well, it does not remove the problem completely,
   but exponential growth of network traffic is changed to linear
   (branches, that exceed pmtu are pruned) and tunnel mtu
   fastly degrades to value <68, where looping stops.
   Yes, it is not good if there exists a router in the loop,
   which does not force DF, even when encapsulating packets have DF set.
   But it is not our problem! Nobody could accuse us, we made
   all that we could make. Even if it is your gated who injected
   fatal route to network, even if it were you who configured
   fatal static route: you are innocent. :-)



   3. Really, ipv4/ipip.c, ipv4/ip_gre.c and ipv6/sit.c contain
   practically identical code. It would be good to glue them
   together, but it is not very evident, how to make them modular.
   sit is integral part of IPv6, ipip and gre are naturally modular.
   We could extract common parts (hash table, ioctl etc)
   to a separate module (ip_tunnel.c).

   Alexey Kuznetsov.
 */

static struct rtnl_link_ops ipgre_link_ops __read_mostly;
static int ipgre_tunnel_init(struct net_device *dev);
static void ipgre_tunnel_setup(struct net_device *dev);
static int ipgre_tunnel_bind_dev(struct net_device *dev);
#if 1 // V54_TUNNELMGR
static int ipgre_rcv(struct sk_buff *skb);
static int ipgre_tunnel_get_wlanid(struct net_device *dev);
void rks_dump_pkt(struct sk_buff *skb, char *s);
// for GRE over UDP. Register/deregister function pointer in UDP stack for redirecting GREoUDP packet here.
extern int _udp_reg_ipgre_func(int (*func1)(u16 *, u16 *), int (*func2)(struct sk_buff *));
extern int _udp_unreg_ipgre_func(void);
#endif // V54_TUNNELMGR

/* Fallback tunnel: no source, no destination, no key, no options */

#define HASH_SIZE  16

static int ipgre_net_id;
struct ipgre_net {
	struct ip_tunnel *tunnels[4][HASH_SIZE];

	struct net_device *fb_tunnel_dev;
};

/* Tunnel hash table */

/*
   4 hash tables:

   3: (remote,local)
   2: (remote,*)
   1: (*,local)
   0: (*,*)

   We require exact key match i.e. if a key is present in packet
   it will match only tunnel with the same key; if it is not present,
   it will match only keyless tunnel.

   All keysless packets, if not matched configured keyless tunnels
   will match fallback tunnel.
 */

#define HASH(addr) (((__force u32)addr^((__force u32)addr>>4))&0xF)

#define tunnels_r_l	tunnels[3]
#define tunnels_r	tunnels[2]
#define tunnels_l	tunnels[1]
#define tunnels_wc	tunnels[0]

static DEFINE_RWLOCK(ipgre_lock);

#if 1 // V54_TUNNELMGR
/* TODO: The following definition is the same as defined in tunnelmgr.h
 * Need to have a separate .h file to be shared
 */

/* Ruckus-GRE always enables GRE_SEQ flag */
#define IS_RKS_GRE_TUNNEL(t)    ((t)->parms.o_flags & GRE_SEQ)

/* Netlink sock */
#define WT_APPL_INIT           0x5454           /* semi-unique message identifier */
#define WT_APPL_SENDMSG        0x5455           /* semi-unique message identifier */
#define WT_APPL_UPDATE_TIMEOUT 0x5456           /* semi-unique message identifier */
#define WT_APPL_INIT_MSG       "RaisingRuckus"  /* semi-unique appl name          */

static struct sock *appl_sock = NULL;
static DECLARE_MUTEX(appl_semaphore);

static void ipgre_cntl_pkt_recv( struct sk_buff *skb );
static void ipgre_cntl_pkt_wakeup(struct sk_buff *skb);

// change need to match cavium/common/include/rks-ip-gre.h
#define WT_GRE_KEEPALIVE         __cpu_to_be32(0x80000000)
#define WT_GRE_ENCRYPTION        __cpu_to_be32(0x40000000)
#define WT_GRE_PADDING           __cpu_to_be32(0x20000000)
#define WT_GRE_L2FRAG            __cpu_to_be32(0x10000000)
#define WT_GRE_POLICY_MASK       __cpu_to_be32(0x000F0000)
#define WT_GRE_POLICY_BIT_SHIFT 16
#define SET_WT_GRE_POLICY(p)    ((p) << WT_GRE_POLICY_BIT_SHIFT)
// mask on csum/route fields
#define WT_GRE_WLANID            __cpu_to_be32(0x0000FFFF)
#define WT_GRE_TUNNELID          __cpu_to_be32(0x0000FFFF)


typedef enum {
    WSG_ERROR = -1,
    WSG_OK = 0,
    WSG_DISCARD,
    WSG_TIMEOUT,
    WSG_EVENT,
    WSG_SKIP_ACCT,
} WSG_STATUS;

typedef enum {
    WSG_APPL_TUNNEL = 0,
    WSG_APPL_MAX,       /* this entry must be last */
} WSG_APPL_ID;

typedef struct {
    unsigned int   appl_pid[WSG_APPL_MAX]; /* applications for DTLS handshaking   */
    int            appl_tid[WSG_APPL_MAX]; /* applications for tunnel id          */
    unsigned char  psk[32];                /* cipher pre-shared key */
    unsigned short psk_bits;               /* length of pre-shared key */
    unsigned char  iv[16];                 /* Inital Vector */
    unsigned short iv_len;                 /* length of Inital Vector  */
    unsigned short s_port;                 /* The source UDP port # for GRE over UDP. */
    unsigned short d_port;                 /* The destination UDP port # for GRE over UDP. 0 means no GRE over UDP */
    __u32          saddr;                  /* AP IP (src) address of GRE */
    __u32          daddr;                  /* WSG-D (dst) address of GRE */
    int            stat_cp_inatomic;       /* # of skb_copy calls in atomic mode  */
    int            stat_cp_not_inatomic;   /* # of skb_copy calls not in atomic   */
    int            stat_cl_inatomic;       /* # of clone calls in atomic mode     */
    int            stat_cl_not_inatomic;   /* # of clone calls not in atomic mode */
} WSG_GLOBAL;

WSG_GLOBAL *_wsgGlobal = NULL;

/* Crypto */
#define ENCRYPT 1
#define DECRYPT 0
#define MODE_ECB 1
#define MODE_CBC 0

#include <ruckus/rks_policy.h>
// set FWD_policy in skb
static inline void
SKB_SET_FWD_POLICY(struct sk_buff *skb, u32 * key)
{
	_l2_cb_t * skb_cb ;
	_rks_policy_t  my_policy;
#define MY_FWD_CLASS	my_policy.p.fwd_class

	skb_cb = (_l2_cb_t *)&skb->cb[0];

	if ( skb_cb->fwd_class != RKS_FWD_Default ) {
		MY_FWD_CLASS = skb_cb->fwd_class ;
	} else {
		// only set to match setting in net_device  if not already set.
		if ( _dev_get_policy(skb->input_dev, &(my_policy._word32)) < 0 ) {
			// get policy failed
			printk(KERN_NOTICE "_dev_get_policy(%p,) failed\n", skb->input_dev);
			return;
		}
	}
	if ( MY_FWD_CLASS != 0 ) {
		*key |= (SET_WT_GRE_POLICY(MY_FWD_CLASS) &  WT_GRE_POLICY_MASK) ;
#if 0
		printk(KERN_NOTICE "%s: set policy %01X , key=%08x\n", __FUNCTION__,
			(unsigned) (my_policy.p.fwd_class), (unsigned) key );
#endif
	}
	return ;
}

#if defined(CONFIG_CRYPTO_DEV_TALITOS) /* SEC with Talitos driver */
struct ipgre_ablkcipher_data
{
	struct sk_buff *skb;
	uint8_t is_padding;
	uint8_t is_udp;
	uint16_t gre_hdr_len;
};
#endif
#endif // V54_TUNNELMGR

/* Given src, dst and key, find appropriate for input tunnel. */

static struct ip_tunnel * ipgre_tunnel_lookup(struct net_device *dev,
					      __be32 remote, __be32 local,
					      __be32 key, __be16 gre_proto)
{
	struct net *net = dev_net(dev);
	int link = dev->ifindex;
	unsigned h0 = HASH(remote);
	unsigned h1 = HASH(key);
	struct ip_tunnel *t, *cand = NULL;
	struct ipgre_net *ign = net_generic(net, ipgre_net_id);
	int dev_type = (gre_proto == htons(ETH_P_TEB)) ?
		       ARPHRD_ETHER : ARPHRD_IPGRE;
	int score, cand_score = 4;

	for (t = ign->tunnels_r_l[h0^h1]; t; t = t->next) {
		if (local != t->parms.iph.saddr ||
		    remote != t->parms.iph.daddr ||
		    key != t->parms.i_key ||
		    !(t->dev->flags & IFF_UP))
			continue;

		if (t->dev->type != ARPHRD_IPGRE &&
		    t->dev->type != dev_type)
			continue;

		score = 0;
		if (t->parms.link != link)
			score |= 1;
		if (t->dev->type != dev_type)
			score |= 2;
		if (score == 0)
			return t;

		if (score < cand_score) {
			cand = t;
			cand_score = score;
		}
	}

	for (t = ign->tunnels_r[h0^h1]; t; t = t->next) {
		if (remote != t->parms.iph.daddr ||
		    key != t->parms.i_key ||
		    !(t->dev->flags & IFF_UP))
			continue;

		if (t->dev->type != ARPHRD_IPGRE &&
		    t->dev->type != dev_type)
			continue;

		score = 0;
		if (t->parms.link != link)
			score |= 1;
		if (t->dev->type != dev_type)
			score |= 2;
		if (score == 0)
			return t;

		if (score < cand_score) {
			cand = t;
			cand_score = score;
		}
	}

	for (t = ign->tunnels_l[h1]; t; t = t->next) {
		if ((local != t->parms.iph.saddr &&
		     (local != t->parms.iph.daddr ||
		      !ipv4_is_multicast(local))) ||
		    key != t->parms.i_key ||
		    !(t->dev->flags & IFF_UP))
			continue;

		if (t->dev->type != ARPHRD_IPGRE &&
		    t->dev->type != dev_type)
			continue;

		score = 0;
		if (t->parms.link != link)
			score |= 1;
		if (t->dev->type != dev_type)
			score |= 2;
		if (score == 0)
			return t;

		if (score < cand_score) {
			cand = t;
			cand_score = score;
		}
	}

	for (t = ign->tunnels_wc[h1]; t; t = t->next) {
		if (t->parms.i_key != key ||
		    !(t->dev->flags & IFF_UP))
			continue;

		if (t->dev->type != ARPHRD_IPGRE &&
		    t->dev->type != dev_type)
			continue;

		score = 0;
		if (t->parms.link != link)
			score |= 1;
		if (t->dev->type != dev_type)
			score |= 2;
		if (score == 0)
			return t;

		if (score < cand_score) {
			cand = t;
			cand_score = score;
		}
	}

	if (cand != NULL)
		return cand;

	if (ign->fb_tunnel_dev->flags & IFF_UP)
		return netdev_priv(ign->fb_tunnel_dev);

	return NULL;
}

static struct ip_tunnel **__ipgre_bucket(struct ipgre_net *ign,
		struct ip_tunnel_parm *parms)
{
	__be32 remote = parms->iph.daddr;
	__be32 local = parms->iph.saddr;
	__be32 key = parms->i_key;
	unsigned h = HASH(key);
	int prio = 0;

	if (local)
		prio |= 1;
	if (remote && !ipv4_is_multicast(remote)) {
		prio |= 2;
		h ^= HASH(remote);
	}

	return &ign->tunnels[prio][h];
}

static inline struct ip_tunnel **ipgre_bucket(struct ipgre_net *ign,
		struct ip_tunnel *t)
{
	return __ipgre_bucket(ign, &t->parms);
}

static void ipgre_tunnel_link(struct ipgre_net *ign, struct ip_tunnel *t)
{
	struct ip_tunnel **tp = ipgre_bucket(ign, t);

	t->next = *tp;
	write_lock_bh(&ipgre_lock);
	*tp = t;
	write_unlock_bh(&ipgre_lock);
}

static void ipgre_tunnel_unlink(struct ipgre_net *ign, struct ip_tunnel *t)
{
	struct ip_tunnel **tp;

	for (tp = ipgre_bucket(ign, t); *tp; tp = &(*tp)->next) {
		if (t == *tp) {
			write_lock_bh(&ipgre_lock);
			*tp = t->next;
			write_unlock_bh(&ipgre_lock);
			break;
		}
	}
}

static struct ip_tunnel *ipgre_tunnel_find(struct net *net,
					   struct ip_tunnel_parm *parms,
					   int type)
{
	__be32 remote = parms->iph.daddr;
	__be32 local = parms->iph.saddr;
	__be32 key = parms->i_key;
	int link = parms->link;
	struct ip_tunnel *t, **tp;
	struct ipgre_net *ign = net_generic(net, ipgre_net_id);

	for (tp = __ipgre_bucket(ign, parms); (t = *tp) != NULL; tp = &t->next)
		if (local == t->parms.iph.saddr &&
		    remote == t->parms.iph.daddr &&
		    key == t->parms.i_key &&
		    link == t->parms.link &&
		    type == t->dev->type)
			break;

	return t;
}

static struct ip_tunnel * ipgre_tunnel_locate(struct net *net,
		struct ip_tunnel_parm *parms, int create)
{
	struct ip_tunnel *t, *nt;
	struct net_device *dev;
	char name[IFNAMSIZ];
	struct ipgre_net *ign = net_generic(net, ipgre_net_id);

	t = ipgre_tunnel_find(net, parms, ARPHRD_IPGRE);
	if (t || !create)
		return t;

	if (parms->name[0])
		strlcpy(name, parms->name, IFNAMSIZ);
	else
		sprintf(name, "gre%%d");

	dev = alloc_netdev(sizeof(*t), name, ipgre_tunnel_setup);
	if (!dev)
	  return NULL;

	dev_net_set(dev, net);

	if (strchr(name, '%')) {
		if (dev_alloc_name(dev, name) < 0)
			goto failed_free;
	}

	nt = netdev_priv(dev);
	nt->parms = *parms;
	dev->rtnl_link_ops = &ipgre_link_ops;

	dev->mtu = ipgre_tunnel_bind_dev(dev);

	if (register_netdevice(dev) < 0)
		goto failed_free;

	dev_hold(dev);
	ipgre_tunnel_link(ign, nt);
	return nt;

failed_free:
	free_netdev(dev);
	return NULL;
}

static void ipgre_tunnel_uninit(struct net_device *dev)
{
	struct net *net = dev_net(dev);
	struct ipgre_net *ign = net_generic(net, ipgre_net_id);

#if 1 // V54_TUNNELMGR
	/* Free created crypto tfm structure */
{
	struct ip_tunnel *tunnel = netdev_priv(dev);

#if defined(CONFIG_CRYPTO_DEV_TALITOS) /* SEC with Talitos driver */
	if(tunnel->parms.tfm)
		crypto_free_ablkcipher(tunnel->parms.tfm);
	tunnel->parms.tfm = NULL;
#else /* Kernel Crypto */
	if(tunnel->parms.tfm)
		crypto_free_blkcipher(tunnel->parms.tfm);
	tunnel->parms.tfm = NULL;
#endif
}
#endif // V54_TUNNELMGR
	ipgre_tunnel_unlink(ign, netdev_priv(dev));
	dev_put(dev);
}


static void ipgre_err(struct sk_buff *skb, u32 info)
{

/* All the routers (except for Linux) return only
   8 bytes of packet payload. It means, that precise relaying of
   ICMP in the real Internet is absolutely infeasible.

   Moreover, Cisco "wise men" put GRE key to the third word
   in GRE header. It makes impossible maintaining even soft state for keyed
   GRE tunnels with enabled checksum. Tell them "thank you".

   Well, I wonder, rfc1812 was written by Cisco employee,
   what the hell these idiots break standrads established
   by themself???
 */

	struct iphdr *iph = (struct iphdr *)skb->data;
	__be16	     *p = (__be16*)(skb->data+(iph->ihl<<2));
	int grehlen = (iph->ihl<<2) + 4;
	const int type = icmp_hdr(skb)->type;
	const int code = icmp_hdr(skb)->code;
	struct ip_tunnel *t;
	__be16 flags;

	flags = p[0];
	if (flags&(GRE_CSUM|GRE_KEY|GRE_SEQ|GRE_ROUTING|GRE_VERSION)) {
		if (flags&(GRE_VERSION|GRE_ROUTING))
			return;
		if (flags&GRE_KEY) {
			grehlen += 4;
			if (flags&GRE_CSUM)
				grehlen += 4;
		}
	}

	/* If only 8 bytes returned, keyed message will be dropped here */
	if (skb_headlen(skb) < grehlen)
		return;

	switch (type) {
	default:
	case ICMP_PARAMETERPROB:
		return;

	case ICMP_DEST_UNREACH:
		switch (code) {
		case ICMP_SR_FAILED:
		case ICMP_PORT_UNREACH:
			/* Impossible event. */
			return;
		case ICMP_FRAG_NEEDED:
			/* Soft state for pmtu is maintained by IP core. */
			return;
		default:
			/* All others are translated to HOST_UNREACH.
			   rfc2003 contains "deep thoughts" about NET_UNREACH,
			   I believe they are just ether pollution. --ANK
			 */
			break;
		}
		break;
	case ICMP_TIME_EXCEEDED:
		if (code != ICMP_EXC_TTL)
			return;
		break;
	}

	read_lock(&ipgre_lock);
	t = ipgre_tunnel_lookup(skb->dev, iph->daddr, iph->saddr,
				flags & GRE_KEY ?
				*(((__be32 *)p) + (grehlen / 4) - 1) : 0,
				p[1]);
	if (t == NULL || t->parms.iph.daddr == 0 ||
	    ipv4_is_multicast(t->parms.iph.daddr))
		goto out;

	if (t->parms.iph.ttl == 0 && type == ICMP_TIME_EXCEEDED)
		goto out;

	if (time_before(jiffies, t->err_time + IPTUNNEL_ERR_TIMEO))
		t->err_count++;
	else
		t->err_count = 1;
	t->err_time = jiffies;
out:
	read_unlock(&ipgre_lock);
	return;
}

static inline void ipgre_ecn_decapsulate(struct iphdr *iph, struct sk_buff *skb)
{
	if (INET_ECN_is_ce(iph->tos)) {
		if (skb->protocol == htons(ETH_P_IP)) {
			IP_ECN_set_ce(ip_hdr(skb));
		} else if (skb->protocol == htons(ETH_P_IPV6)) {
			IP6_ECN_set_ce(ipv6_hdr(skb));
		}
	}
}

static inline u8
ipgre_ecn_encapsulate(u8 tos, struct iphdr *old_iph, struct sk_buff *skb)
{
	u8 inner = 0;
	if (skb->protocol == htons(ETH_P_IP))
		inner = old_iph->tos;
	else if (skb->protocol == htons(ETH_P_IPV6))
		inner = ipv6_get_dsfield((struct ipv6hdr *)old_iph);
	return INET_ECN_encapsulate(tos, inner);
}

#if 1 // V54_TUNNELMGR
extern int ieee80211_get_wlanid(struct net_device *dev);
static int
ipgre_tunnel_get_wlanid(struct net_device *dev)
{
	return ieee80211_get_wlanid(dev);
}

static void
ipgre_cntl_pkt_recv( struct sk_buff *skb )
{
    struct sk_buff      *new_skb;
    struct nlmsghdr     *nlh;
    unsigned int         dst_pid;
    int                  appl_id, err;
    int                  offset_to_pkt;
    struct net_device   *dev;
    gfp_t                skb_priority;

    nlh = (struct nlmsghdr *)skb->data;
    dst_pid  = nlh->nlmsg_pid;
    appl_id  = nlh->nlmsg_type;

    switch( nlh->nlmsg_flags ) {
    case WT_APPL_INIT:
        _wsgGlobal->appl_pid[appl_id] = dst_pid;
        if( in_atomic() ) {
            skb_priority = GFP_ATOMIC;
            _wsgGlobal->stat_cl_inatomic++;
        } else {
            skb_priority = GFP_KERNEL;
            _wsgGlobal->stat_cl_not_inatomic++;
        }
        new_skb = skb_clone(skb, skb_priority);
        if( new_skb != NULL ) {
            NETLINK_CB(new_skb).pid     = 0;

            err = netlink_unicast( appl_sock, new_skb, dst_pid, MSG_DONTWAIT );
            if( err < 0 ) {
                if (net_ratelimit())
                    printk("IP_GRE: Appl %d echo initialize message failed, rc=%d\n",
		            dst_pid, err );
            }
        }
        if (net_ratelimit())
            printk("IP_GRE: Appl %d echo initialize message successfully\n", _wsgGlobal->appl_pid[appl_id]);
		break;

    case WT_APPL_SENDMSG:
        dev = dev_get_by_name( &init_net, "gre1" );
        if( dev ) {
            struct sk_buff *newskb;

            skb->dev = dev;
            offset_to_pkt = (unsigned char *)(NLMSG_DATA(nlh)) - skb->data;
            skb_pull( skb, offset_to_pkt );

            skb->len = sizeof(struct keep_alive_msg);

            //printk("offset_to_pkt %d, skb->mac.raw %p, skb->data %p, skb->h.raw %p, skb->len %d\n", offset_to_pkt, skb->mac.raw, skb->data, skb->h.raw, skb->len);
            //printk("skb->dev->name %s\n", skb->dev->name);

            newskb = skb_clone(skb, GFP_ATOMIC);
            err = dev_queue_xmit( newskb );
            dev_put(dev);
            if(err){
                if (net_ratelimit())
                    printk("IP_GRE: Appl %d send packet to dev %s failed\n",
                            dst_pid,  "gre1" );
            }
        }else {
        if (net_ratelimit())
            printk("IP_GRE: Appl %d send packet to dev %s failed\n",
                    dst_pid,  "gre1" );
        }
        break;

    default:
        break;
    }
}

static int
ipgre_cntl_pkt_send( WSG_APPL_ID appl_id, struct sk_buff *skb )
{
    WSG_STATUS	     ret;
    unsigned int     dst_pid;
    int              idx, len;
    unsigned char   *pkt;
    int              nl_rc;
    struct sk_buff  *out_skb = NULL;
    struct nlmsghdr *nlh;
    gfp_t            skb_priority;
    struct net_device   *dev;

    ret = WSG_OK;

    if( skb == NULL ||
	    appl_id > WSG_APPL_MAX ) {
        printk("ipgre_cntl_pkt_send: validation failure for Appl %d\n",
               appl_id );
        ret = WSG_ERROR;
        goto exit;
    }

    dst_pid = _wsgGlobal->appl_pid[appl_id];
    if( dst_pid == 0 ) {
        printk("ipgre_cntl_pkt_send: NULL pid for Appl %d\n",
               appl_id );
        goto exit;
    }

    dev = dev_get_by_name( &init_net, "gre1" );
    if(dev)
        skb->dev = dev;
    else
    {
        printk("ipgre_cntl_pkt_send: unable to get valid dev\n");
        ret = WSG_ERROR;
        goto exit;
    }

	pkt = skb_mac_header(skb);
	len = skb_tail_pointer(skb) - pkt;
	idx = skb->dev->ifindex;

    if( in_atomic() ) {
        skb_priority = GFP_ATOMIC;
        _wsgGlobal->stat_cl_inatomic++;
    } else {
        skb_priority = GFP_KERNEL;
        _wsgGlobal->stat_cl_not_inatomic++;
    }

    out_skb = alloc_skb( NLMSG_SPACE(len), skb_priority );
    if( out_skb == NULL ) {
        goto exit;
    }

    nlh = NLMSG_PUT( out_skb, 0, idx, appl_id, len );
    nlh->nlmsg_flags = WT_APPL_SENDMSG;
    memcpy( NLMSG_DATA(nlh), pkt, len );

    NETLINK_CB(out_skb).pid        = 0;

    nl_rc = netlink_unicast( appl_sock, out_skb, dst_pid, MSG_DONTWAIT );

    if( nl_rc < 0 ) {
        ret = WSG_ERROR;
        if( net_ratelimit() ) {
            printk( "IP_GRE: Appl %d send message failed, rc=%d\n",
                    dst_pid, nl_rc );
        }
    } else {
#if 0
        if (p->flgs & PKT_INFO_FLGS_IGMP_PARSED) {
            _v54Global->stat_msg_igmp++;
        } else if (p->flgs & PKT_INFO_FLGS_MLD_PARSED) {
            _v54Global->stat_msg_mld++;
        } else {
            _v54Global->stat_msg_queued++;
        }
#endif
        ret = WSG_OK;
    }

exit:
    dev_put(dev);
    return(ret);

nlmsg_failure:
    if( out_skb ) dev_kfree_skb( out_skb );

    return WSG_ERROR;
}

static void
ipgre_cntl_pkt_wakeup(struct sk_buff *skb)
{
	//if (down_trylock(&appl_semaphore))
	//	return;

	ipgre_cntl_pkt_recv(skb);

	//up(&appl_semaphore);

}

#if defined(CONFIG_CRYPTO_DEV_TALITOS) /* SEC with Talitos driver */
static void ipgre_talitos_encrypt_complete(struct crypto_async_request *req, int err)
{
	struct ipgre_ablkcipher_data *ablkcipher_data = (struct ipgre_ablkcipher_data *)req->data;
	struct sk_buff *skb = (struct sk_buff *)ablkcipher_data->skb;
    struct iphdr  *iph;
    struct rtable *rt;
	struct ip_tunnel *tunnel;
	struct net_device_stats *stats;
	struct ablkcipher_request *ablk_req = ablkcipher_request_cast(req);
	int rc = 0;

	if(skb->dev)
	{
	    tunnel = netdev_priv(skb->dev);
	    stats = &tunnel->dev->stats;

	    iph = ip_hdr(skb);
	    rt = skb_rtable(skb);

		IPTUNNEL_XMIT();
	}
	else
	{
		rc = -1;
		goto out;
	}

out:
	kfree(ablkcipher_data);
	ablkcipher_request_free(ablk_req);
	if(rc < 0)
		dev_kfree_skb(skb);
	return;
}

static void ipgre_talitos_decrypt_complete(struct crypto_async_request *req, int err)
{
	struct ipgre_ablkcipher_data *ablkcipher_data = (struct ipgre_ablkcipher_data *)req->data;
	struct sk_buff *skb = (struct sk_buff *)ablkcipher_data->skb;
	int is_padding = ablkcipher_data->is_padding;
	int hdr_len = ablkcipher_data->gre_hdr_len;
	struct ablkcipher_request *ablk_req = ablkcipher_request_cast(req);
	int rc = 0;

	if (is_padding) {
		int i = 0;
		uint8_t *data_ptr = skb->data + hdr_len;
		int data_len = skb->len - hdr_len;

		if ((data_len % 16) != 0) {
			if (net_ratelimit())
				printk("encrypted payload has wrong padding size\n");
			rc = -1;
			goto out;
		}
		data_ptr += data_len; // point to the end of payload
		for (i=1; i<16; i++) { // backtrace to check the padding pattern
			if (*(--data_ptr) == 0x80) {
				break;
			}
		}
		skb_trim(skb, skb->len - i); // remove the padding
	}

	ipgre_rcv(skb);

out:
	kfree(ablkcipher_data);
	ablkcipher_request_free(ablk_req);
	if(rc < 0)
		dev_kfree_skb(skb);
	return;
}

static int ipgre_tunnel_cipher(struct crypto_ablkcipher *tfm, u8 *key, int key_len, u8 *iv, int iv_len, int enc, struct sk_buff *skb, int hdr_len, int is_padding, int is_udp)
{
    struct scatterlist sg[8];
	int   ret = 0;
	struct ablkcipher_request *req;
	struct ipgre_ablkcipher_data *ablkcipher_data;
	uint8_t *input;
	int ilen;
	input = skb->data + hdr_len;
	ilen = skb->len - hdr_len;

	req = ablkcipher_request_alloc(tfm, GFP_ATOMIC);
	if (!req) {
		if (net_ratelimit())
			printk("alg: skcipher: Failed to allocate request "
			   "for %s\n", "cbc(aes)-talitos");
		return -1;
	}

	ablkcipher_data = kmalloc(sizeof(struct ipgre_ablkcipher_data), GFP_ATOMIC);
	if(!ablkcipher_data)
	{
        if (net_ratelimit())
            printk("ipgre_tunnel_cipher() alloc ablkcipher_data failed size %d\n", sizeof(struct ipgre_ablkcipher_data));
		ret = -1;
		goto err_data;
	}
	ablkcipher_data->skb = skb;
	ablkcipher_data->is_padding = is_padding;
	ablkcipher_data->is_udp = is_udp;
	ablkcipher_data->gre_hdr_len = hdr_len;

	if (enc)
		ablkcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
						ipgre_talitos_encrypt_complete, ablkcipher_data);
	else
		ablkcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
						ipgre_talitos_decrypt_complete, ablkcipher_data);

	crypto_ablkcipher_clear_flags(tfm, ~0);

	ret = crypto_ablkcipher_setkey(tfm, key, key_len/8);

	if (ret) {
        if (net_ratelimit())
            printk("crypto_ablkcipher_setkey() failed ret=%d flags=%x\n", ret, tfm->base.crt_flags);
		ret = -1;
		goto err;
	}

	sg_init_one(&sg[0], input, ilen);

	ablkcipher_request_set_crypt(req, sg, sg,
					 ilen, iv);

	if (enc)
		ret = crypto_ablkcipher_encrypt(req);
	else
		ret = crypto_ablkcipher_decrypt(req);

	switch (ret) {
	case 0:
	case -EINPROGRESS:
	case -EBUSY:
		break;
	case -EAGAIN:
		if (net_ratelimit())
			printk("ipgre_tunnel_cipher send reqest failed ret %d, drop it!\n", ret);
			goto err;
		break;
		/* fall through */
	default:
		if (net_ratelimit())
			printk("ipgre_tunnel_cipher send reqest failed ret %d, drop it!\n", ret);
		goto err;
	}

	return 0;

err:
	kfree(ablkcipher_data);
err_data:
	ablkcipher_request_free(req);
	return ret;
}
#else /* Kernel Crypto */
static int ipgre_tunnel_cipher(struct crypto_blkcipher *tfm, u8 *key, int key_len, u8 *iv, int iv_len, int enc, char *input, unsigned short ilen)
{
    /* Always use AES CBC */
    struct scatterlist sg[8];
    int   ret = 0;
    const char *e;
    struct blkcipher_desc desc;

    if (enc == ENCRYPT)
        e = "encryption";
    else
        e = "decryption";

    memset( &desc, 0, sizeof(struct blkcipher_desc));

    desc.tfm = tfm;
    desc.flags = 0;

    ret = crypto_blkcipher_setkey(tfm, key, key_len/8);

	if (ret) {
        if (net_ratelimit())
            printk("setkey() failed flags=%x\n", tfm->base.crt_flags);
		ret = -1;
		goto out;
	}

	sg_init_table(sg, 8);
	sg_set_buf(&sg[0], input, ilen);

    crypto_blkcipher_set_iv(tfm, iv, crypto_blkcipher_ivsize(tfm));

	if (enc)
		ret = crypto_blkcipher_encrypt(&desc, sg, sg, ilen);
	else
		ret = crypto_blkcipher_decrypt(&desc, sg, sg, ilen);

	if (ret) {
        if (net_ratelimit())
            printk("%s () failed flags=%x\n", e, tfm->base.crt_flags);
		goto out;
	}

out:
	return ret;

}
#endif

void
rks_dump_pkt(struct sk_buff *skb, char *s)
{
	int i = 0;

	printk("\n raw data: %s ", s);
	for (i = 0; i < 100; i++) {
		if ((i != 0) && ((i % 4) == 0))
			printk(" ");
		printk("%02x", skb->data[i]);
		if ((i % 16) == 15)
			printk("\n");
	}
	printk("\n mac raw: ");
	for (i = 0; i < 30; i++) {
                printk("%02x", skb->mac_header[i]);
	}
	printk("\n nh raw: ");
	for (i = 0; i < 40; i++) {
                printk("%02x", skb->network_header[i]);
	}
	printk("\n data");
}

#define TCP_SYN 0x02

static inline int rks_is_pkt_tcp_syn (struct tcphdr *m)
{
	if (m->syn)
		return 1;

	return 0;
}

static u_int16_t
rks_fcheck (u_int32_t oldvalinv, u_int32_t newval, u_int16_t oldcheck)
{
    u_int32_t diffs[] = { oldvalinv, newval };
	return csum_fold(csum_partial((char *)diffs, sizeof(diffs),
						oldcheck^0xFFFF));
}

static inline int
IS_PROTO_IP(struct sk_buff *skb)
{
	if (skb->protocol == __constant_htons(ETH_P_IP))  {
		return 1;
	} else if ( skb->protocol == __constant_htons(ETH_P_8021Q) ) {
		struct vlan_ethhdr vlanehdr ;
		memcpy(&vlanehdr, vlan_eth_hdr(skb), sizeof(struct vlan_ethhdr));
		if ( vlanehdr.h_vlan_encapsulated_proto == __constant_htons(ETH_P_IP)) {
			return 1;
		}
	}
	return 0;
}

static inline void
rks_modify_tcp_mss (struct sk_buff *skb, struct iphdr *old_iph, int mtu)
{
	uint16_t oldmss, newmss;

	if (old_iph == NULL)
		return;

	if (IS_PROTO_IP(skb) && old_iph && (old_iph->protocol == IPPROTO_TCP)) {
		int ihl = old_iph->ihl*4;
		struct tcphdr *tcph = (void *)old_iph + ihl;
		uint8_t *tcp_opt = ((uint8_t *) old_iph) + ihl + sizeof(struct tcphdr);

		if ((tcph == NULL) || (tcp_opt == NULL))
			return;
		if (rks_is_pkt_tcp_syn(tcph)) { // check mss and adjust it if required
			oldmss = (tcp_opt[2] << 8) | tcp_opt[3];
			// 40 bytes is the largest size of TCP options. include in MSS calculation per RFC
			newmss = mtu - ihl - sizeof(struct tcphdr) - ETH_HLEN - VLAN_HLEN - 40;
			if (oldmss > newmss) {
				tcp_opt[2] = (newmss & 0xff00) >> 8;
				tcp_opt[3] = (newmss & 0x00ff);
				tcph->check = rks_fcheck(htons(oldmss)^0xFFFF, htons(newmss), tcph->check);
			}
		}
	}
}

#endif // V54_TUNNELMGR

static int ipgre_rcv(struct sk_buff *skb)
{
	struct iphdr *iph;
	u8     *h;
	__be16    flags;
	__sum16   csum = 0;
	__be32 key = 0;
	u32    seqno = 0;
	struct ip_tunnel *tunnel;
	int    offset = 4;
	__be16 gre_proto;
	unsigned int len;
#if 1 // V54_TUNNELMGR
	int    crypto_enabled = 0, is_padding = 0, is_keepalive = 0;
#endif // V54_TUNNELMGR
	struct vlan_ethhdr *vhdr;
	int vlen = 0;

	if (!pskb_may_pull(skb, 16))
		goto drop_nolock;

	iph = ip_hdr(skb);
	h = skb->data;
	flags = *(__be16*)h;

	if (flags&(GRE_CSUM|GRE_KEY|GRE_ROUTING|GRE_SEQ|GRE_VERSION)) {
		/* - Version must be 0.
		   - We do not support routing headers.
		 */
		if (flags&(GRE_VERSION|GRE_ROUTING))
			goto drop_nolock;

		if (flags&GRE_CSUM) {
			// We do not perform checksum calculation but
			// just use field for carrying wlan id inforamtion
#if 0 // V54_TUNNELMGR
			switch (skb->ip_summed) {
			case CHECKSUM_COMPLETE:
				csum = csum_fold(skb->csum);
				if (!csum)
					break;
				/* fall through */
			case CHECKSUM_NONE:
				skb->csum = 0;
				csum = __skb_checksum_complete(skb);
				skb->ip_summed = CHECKSUM_COMPLETE;
			}
#endif
			offset += 4;
		}
		if (flags&GRE_KEY) {
			key = *(__be32*)(h + offset);
			#if 1 //V54_TUNNELMGR
			/* remove crypto/padding flag in GRE key field from being processed again next time */
			*(__be32*)(h + offset) = *(__be32*)(h + offset) & WT_GRE_TUNNELID;
			#endif
			offset += 4;
		}
		if (flags&GRE_SEQ) {
			seqno = ntohl(*(__be32*)(h + offset));
			offset += 4;
		}
	}

#if 1 // V54_TUNNELMGR
	is_keepalive = key & WT_GRE_KEEPALIVE;
	/* Send keep alive up to tunnelmgr */
	if (is_keepalive)
		ipgre_cntl_pkt_send(WSG_APPL_TUNNEL, skb);

	if (key & WT_GRE_ENCRYPTION) {
		crypto_enabled = 1;
		if (key & WT_GRE_PADDING) {
			is_padding = 1;
		}
	}

	key = key & WT_GRE_TUNNELID; // first 4 bytes are not used in standard GRE and should be marked as zero
#endif // V54_TUNNELMGR

	gre_proto = *(__be16 *)(h + 2);

	read_lock(&ipgre_lock);
	if ((tunnel = ipgre_tunnel_lookup(skb->dev,
					  iph->saddr, iph->daddr, key,
					  gre_proto))) {
		struct net_device_stats *stats = &tunnel->dev->stats;
#if 1 // V54_TUNNELMGR
		struct iptun_stats *tun_stats = &tunnel->parms.tun_stats;
#endif

		secpath_reset(skb);

#if 1 // V54_TUNNELMGR
		if (is_keepalive) {
			tunnel->dev->stats.rx_packets++;
			tunnel->dev->stats.rx_bytes += skb->len;
			tunnel->parms.tun_stats.tun_rx_packets++;
			tunnel->parms.tun_stats.tun_rx_bytes += skb->len;
			read_unlock(&ipgre_lock);
			dev_kfree_skb(skb);
			return (0);
		}

#if defined(CONFIG_CRYPTO_DEV_TALITOS) /* SEC with Talitos driver */
		if (crypto_enabled) {
			int rc = 0;

			rc = ipgre_tunnel_cipher(tunnel->parms.tfm, tunnel->parms.psk, tunnel->parms.psk_bits,
			tunnel->parms.iv, tunnel->parms.iv_len, DECRYPT, skb, offset, is_padding, 0);
			if(rc < 0)
			{
				/* increase drop counter */
				stats->tx_dropped++;
				tun_stats->tun_tx_dropped++;
				kfree_skb(skb);
			}
			read_unlock(&ipgre_lock);
			return 0;
		}
#endif
#endif // V54_TUNNELMGR

		skb->protocol = gre_proto;
		/* WCCP version 1 and 2 protocol decoding.
		 * - Change protocol to IP
		 * - When dealing with WCCPv2, Skip extra 4 bytes in GRE header
		 */
		if (flags == 0 && gre_proto == htons(ETH_P_WCCP)) {
			skb->protocol = htons(ETH_P_IP);
			if ((*(h + offset) & 0xF0) != 0x40)
				offset += 4;
		}

#if 1 // V54_TUNNELMGR
		if (IS_RKS_GRE_TUNNEL(tunnel) && seqno) {
			// This is a layer 2 fragmented packet. Send it to layer 2 re-assembly
#ifdef DEBUG_FRAG
			rks_dump_pkt(skb,"fragmented packet");
#endif
			skb = l2_local_deliver(skb, seqno, offset);
			if (skb == NULL) {
#ifdef DEBUG_FRAG
				printk("\n %s: waiting for remaining fragments to arrive", __FUNCTION__);
#endif
				return 0; // Waiting for remaining fragments to arrive
			}
		}
#endif // V54_TUNNELMGR

        skb->mac_header = skb->network_header;
        __pskb_pull(skb, offset);

#if 1 // V54_TUNNELMGR
#if !(defined(CONFIG_CRYPTO_DEV_TALITOS)) /* Kernel Crypto */
        if (crypto_enabled) {
            unsigned char *ptr = skb->data;
            int i;

            ipgre_tunnel_cipher(tunnel->parms.tfm, tunnel->parms.psk, tunnel->parms.psk_bits,
            tunnel->parms.iv, tunnel->parms.iv_len, DECRYPT, skb->data, skb->len);

            if (is_padding) {
                if ((skb->len % 16) != 0) {
                    if (net_ratelimit())
                        printk("encrypted payload has wrong padding size\n");
                    goto drop;
                }
                ptr += skb->len; // point to the end of payload
                for (i=1; i<16; i++) { // backtrace to check the padding pattern
                    if (*(--ptr) == 0x80) {
                        break;
                    }
                }
                skb_trim(skb, skb->len - i); // remove the padding
            }
		}
#endif
#endif // V54_TUNNELMGR

		vhdr = (struct vlan_ethhdr *)skb->data;
		if (vhdr->h_vlan_proto == __constant_htons(ETH_P_8021Q)) {
			vlen = VLAN_HLEN;
		}

		skb_postpull_rcsum(skb, skb_transport_header(skb), offset);
		skb->pkt_type = PACKET_HOST;
#ifdef CONFIG_NET_IPGRE_BROADCAST
		if (ipv4_is_multicast(iph->daddr)) {
			/* Looped back packet, drop it! */
			if (skb_rtable(skb)->fl.iif == 0)
				goto drop;
			stats->multicast++;
			skb->pkt_type = PACKET_BROADCAST;
		}
#endif

#if 1 // V54_TUNNELMGR
#ifdef DEBUG_FRAG
		if (seqno != 0) {
			rks_dump_pkt(skb, "reassembled packet");
		}
#endif
#endif // V54_TUNNELMGR
		if (((flags&GRE_CSUM) && csum) ||
		    (!(flags&GRE_CSUM) && tunnel->parms.i_flags&GRE_CSUM)) {
			stats->rx_crc_errors++;
			stats->rx_errors++;
#if 1 // V54_TUNNELMGR
			tun_stats->tun_rx_errors++;
#endif // V54_TUNNELMGR
			goto drop;
		}
		if (tunnel->parms.i_flags&GRE_SEQ) {
			if (!(flags&GRE_SEQ) ||
			    (tunnel->i_seqno && (s32)(seqno - tunnel->i_seqno) < 0)) {
				stats->rx_fifo_errors++;
				stats->rx_errors++;
#if 1 // V54_TUNNELMGR
				tun_stats->tun_rx_errors++;
#endif // V54_TUNNELMGR
				goto drop;
			}
			tunnel->i_seqno = seqno + 1;
		}

		len = skb->len;

		/* Warning: All skb pointers will be invalidated! */
		if (tunnel->dev->type == ARPHRD_ETHER) {
			if (!pskb_may_pull(skb, ETH_HLEN)) {
				stats->rx_length_errors++;
				stats->rx_errors++;
#if 1 // V54_TUNNELMGR
				tun_stats->tun_rx_errors++;
#endif // V54_TUNNELMGR
				goto drop;
			}

			iph = ip_hdr(skb);
			skb->protocol = eth_type_trans(skb, tunnel->dev);
			skb_postpull_rcsum(skb, eth_hdr(skb), ETH_HLEN);
		}

		/* Update the last RX time */
		tunnel->dev->last_rx = jiffies;

		stats->rx_packets++;
		stats->rx_bytes += len;
#if 1 // V54_TUNNELMGR
		tun_stats->tun_rx_packets++;
		tun_stats->tun_rx_bytes += len;
#endif // V54_TUNNELMGR
		skb->dev = tunnel->dev;
		skb_dst_drop(skb);
		nf_reset(skb);

		skb_reset_network_header(skb);
		ipgre_ecn_decapsulate(iph, skb);

#if 1 // V54_TUNNELMGR
		iph = (struct iphdr *) ((uint8_t *) skb->data + vlen);
		rks_modify_tcp_mss(skb, iph, tunnel->dev->mtu);
#endif // V54_TUNNELMGR
#if 1 // V54_TUNNELMGR
                /* clear previous classification result and let inner payload can be classified again. */
                skb->priority &= ~0x0f;
                M_FLAG_CLR(skb, M_PRIORITY);
#endif // V54_TUNNELMGR
		netif_rx(skb);
		read_unlock(&ipgre_lock);
		return(0);
	}

#if 1 // V54_TUNNELMGR
    if (net_ratelimit())
	    printk("packet is dropped because configuration of gre1 mis-matches the received gre pkts (or gre1 down)\n");
#endif // V54_TUNNELMGR

	icmp_send(skb, ICMP_DEST_UNREACH, ICMP_PORT_UNREACH, 0);

drop:
	read_unlock(&ipgre_lock);
drop_nolock:
	kfree_skb(skb);
	return(0);
}

#if 1 //V54_TUNNELMGR
int ipgre_get_udp_port(u16 *s_port, u16 *d_port)
{
	struct net_device *dev;
	struct ip_tunnel *tunnel;
	dev = dev_get_by_name( &init_net, "gre1");
	if(dev)
	{
		tunnel = (struct ip_tunnel *)netdev_priv(dev);

		*s_port = tunnel->parms.s_port;
		*d_port = tunnel->parms.d_port;
		dev_put(dev);
	}
	else
	{
		*s_port = 0;
		*d_port = 0;
		return -1;
	}
	return 0;
}
int ipgre_rcv_from_udp(struct sk_buff *skb)
{
	ipgre_rcv(skb);
	return 0;
}

EXPORT_SYMBOL(ipgre_get_udp_port);
EXPORT_SYMBOL(ipgre_rcv_from_udp);
#endif //V54_TUNNELMGR
static netdev_tx_t ipgre_tunnel_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct ip_tunnel *tunnel = netdev_priv(dev);
	struct net_device_stats *stats = &tunnel->dev->stats;
#if 1 // V54_TUNNELMGR
	struct iptun_stats *tun_stats = &tunnel->parms.tun_stats;
#endif //V54_TUNNELMGR
	struct iphdr  *old_iph = ip_hdr(skb);
	struct iphdr  *tiph;
	u8     tos;
	__be16 df;
	struct rtable *rt;                  /* Route to the other host */
	struct net_device *tdev;			/* Device to other host */
	struct iphdr  *iph;			/* Our new IP header */
	unsigned int max_headroom;		/* The extra header space needed */
	int    gre_hlen;
#if 1 // V54_TUNNELMGR
	//int    push_hlen;
	int    is_keepalive = 0, is_padding = 0, is_udp = 0;
	int    crypto_enabled = 0, clen = 0, plen = 0;
	struct keep_alive_msg *msg;
	u32 *ptr;
	rks_gre_frag_dbg_t *rdbg = NULL;
	struct udphdr *udph;
	char *greh;
	int pure_gre_hlen = 0;
#endif // V54_TUNNELMGR
	__be32 dst;
	int    mtu;

#if 1 // V54_TUNNELMGR
	rks_modify_tcp_mss(skb, old_iph, dev->mtu);

	/* Run rks_gre_tunnel_frag() only for Ruckus-GRE */
	if (IS_RKS_GRE_TUNNEL(tunnel) && skb->len > dev->mtu) {
		// RKS_UPD_TSTAMP(rdbg);
		/* If IP and DF bit is set, sends back ICMP type 3, code 4. Otherwise frag it. */
		if ((skb->protocol == ETH_P_IP) && old_iph && (old_iph->frag_off & htons(IP_DF))) {
			rks_icmp_send(skb, ICMP_DEST_UNREACH, ICMP_FRAG_NEEDED,
			              dev->mtu - sizeof(struct vlan_ethhdr), tunnel->parms.iph.saddr);
			rdbg = rks_dbg_get_ent(old_iph->saddr, old_iph->daddr);
			RKS_UPD_PKT_ICMP_SENT(rdbg);
		} else {
			rks_gre_tunnel_frag(skb, dev, old_iph, rdbg);
		}
		return 0;
	}
#endif // V54_TUNNELMGR

#if 1 // V54_TUNNELMGR
	/* GRE Keep Alive ?? */
	msg = (struct keep_alive_msg *)skb->data;
	if( msg->magic == 0x54545454 ) {
		is_keepalive = 1;
	} else {
		is_keepalive = 0;
	}

	if (tunnel->parms.d_port) {
		is_udp = 1;
	}

	if (tunnel->parms.psk_bits != 0 ) { // psk 0 mean no encryption applied
		crypto_enabled = 1;
		clen = skb->len;

		plen = 16 - (clen % 16); // valid padding len should be 1 <= plen <= 15
		if (plen != 16) {
			u8 *tmp;

			//skb = skb_pad(skb, plen);
			//if (skb != NULL) {
			if(skb_pad(skb, plen) == 0) {
				tmp  = skb_put(skb, plen);
				*tmp = 0x80;
			} else {
				stats->tx_errors++;
				tun_stats->tun_tx_errors++;
				goto tx_error;
			}

			clen += plen;
			is_padding = 1;
		}
	}
#endif // V54_TUNNELMGR

	if (dev->type == ARPHRD_ETHER)
		IPCB(skb)->flags = 0;

	if (dev->header_ops && dev->type == ARPHRD_IPGRE) {
		gre_hlen = 0;
		tiph = (struct iphdr *)skb->data;
	} else {
		gre_hlen = tunnel->hlen;
		pure_gre_hlen = gre_hlen - sizeof(struct iphdr);
		if(is_udp)
			gre_hlen = gre_hlen + sizeof(struct udphdr);
		tiph = &tunnel->parms.iph;
	}

	if ((dst = tiph->daddr) == 0) {
		/* NBMA tunnel */

		if (skb_dst(skb) == NULL) {
			stats->tx_fifo_errors++;
			goto tx_error;
		}

		if (skb->protocol == htons(ETH_P_IP)) {
			rt = skb_rtable(skb);
			if ((dst = rt->rt_gateway) == 0)
				goto tx_error_icmp;
		}
#ifdef CONFIG_IPV6
		else if (skb->protocol == htons(ETH_P_IPV6)) {
			struct in6_addr *addr6;
			int addr_type;
			struct neighbour *neigh = skb_dst(skb)->neighbour;

			if (neigh == NULL)
				goto tx_error;

			addr6 = (struct in6_addr *)&neigh->primary_key;
			addr_type = ipv6_addr_type(addr6);

			if (addr_type == IPV6_ADDR_ANY) {
				addr6 = &ipv6_hdr(skb)->daddr;
				addr_type = ipv6_addr_type(addr6);
			}

			if ((addr_type & IPV6_ADDR_COMPATv4) == 0)
				goto tx_error_icmp;

			dst = addr6->s6_addr32[3];
		}
#endif
		else
			goto tx_error;
	}

	tos = tiph->tos;
	if (tos == 1) {
		tos = 0;
		if (skb->protocol == htons(ETH_P_IP))
			tos = old_iph->tos;
	}

	{
		struct flowi fl = { .oif = tunnel->parms.link,
				    .nl_u = { .ip4_u =
					      { .daddr = dst,
						.saddr = tiph->saddr,
						.tos = RT_TOS(tos) } },
				    .proto = IPPROTO_GRE };
		if (ip_route_output_key(dev_net(dev), &rt, &fl)) {
			stats->tx_carrier_errors++;
			goto tx_error;
		}
	}
	tdev = rt->u.dst.dev;

	if (tdev == dev) {
		ip_rt_put(rt);
		stats->collisions++;
		goto tx_error;
	}

	df = tiph->frag_off;
	if (df)
		mtu = dst_mtu(&rt->u.dst) - dev->hard_header_len - tunnel->hlen;
	else
		mtu = skb_dst(skb) ? dst_mtu(skb_dst(skb)) : dev->mtu;

	if (skb_dst(skb))
		skb_dst(skb)->ops->update_pmtu(skb_dst(skb), mtu);

#if 1 // V54_TUNNELMGR
    if (!IS_RKS_GRE_TUNNEL(tunnel) && dev->type == ARPHRD_ETHER) {
        /* Allow outer IP layer to do fragmentation if exceeds the size of mtu */
        if (skb->len > dev->mtu) {
            df &= ~htons(IP_DF);
            tun_stats->tun_tx_fragmented++;
        }
    } else {
#endif // V54_TUNNELMGR
	if (skb->protocol == htons(ETH_P_IP)) {
		df |= (old_iph->frag_off&htons(IP_DF));

		if ((old_iph->frag_off&htons(IP_DF)) &&
		    mtu < ntohs(old_iph->tot_len)) {
			icmp_send(skb, ICMP_DEST_UNREACH, ICMP_FRAG_NEEDED, htonl(mtu));
			ip_rt_put(rt);
			goto tx_error;
		}
	}
#ifdef CONFIG_IPV6
	else if (skb->protocol == htons(ETH_P_IPV6)) {
		struct rt6_info *rt6 = (struct rt6_info *)skb_dst(skb);

		if (rt6 && mtu < dst_mtu(skb_dst(skb)) && mtu >= IPV6_MIN_MTU) {
			if ((tunnel->parms.iph.daddr &&
			     !ipv4_is_multicast(tunnel->parms.iph.daddr)) ||
			    rt6->rt6i_dst.plen == 128) {
				rt6->rt6i_flags |= RTF_MODIFIED;
				skb_dst(skb)->metrics[RTAX_MTU-1] = mtu;
			}
		}

		if (mtu >= IPV6_MIN_MTU && mtu < skb->len - tunnel->hlen + gre_hlen) {
			icmpv6_send(skb, ICMPV6_PKT_TOOBIG, 0, mtu, dev);
			ip_rt_put(rt);
			goto tx_error;
		}
	}
#endif
#if 1 // V54_TUNNELMGR
    }
#endif // V54_TUNNELMGR

	if (tunnel->err_count > 0) {
		if (time_before(jiffies,
				tunnel->err_time + IPTUNNEL_ERR_TIMEO)) {
			tunnel->err_count--;

			dst_link_failure(skb);
		} else
			tunnel->err_count = 0;
	}

	max_headroom = LL_RESERVED_SPACE(tdev) + gre_hlen;

	if (skb_headroom(skb) < max_headroom || skb_shared(skb)||
	    (skb_cloned(skb) && !skb_clone_writable(skb, 0))) {
		struct sk_buff *new_skb = skb_realloc_headroom(skb, max_headroom);
		if (!new_skb) {
			ip_rt_put(rt);
			stats->tx_dropped++;
#if 1 // V54_TUNNELMGR
			tun_stats->tun_tx_dropped++;
#endif // V54_TUNNELMGR
			dev_kfree_skb(skb);
			return NETDEV_TX_OK;
		}
		if (skb->sk)
			skb_set_owner_w(new_skb, skb->sk);
		dev_kfree_skb(skb);
		skb = new_skb;
		old_iph = ip_hdr(skb);
	}

	skb_reset_transport_header(skb);
	skb_push(skb, gre_hlen);
	skb_reset_network_header(skb);
#if 1 // V54_TUNNELMGR
	if ((skb->cb[0] != PACKET_GRE_L2FRAG) && (skb->cb[0] != PACKET_GRE_L3FRAG)) {
	memset(&(IPCB(skb)->opt), 0, sizeof(IPCB(skb)->opt));
	}
#else
	memset(&(IPCB(skb)->opt), 0, sizeof(IPCB(skb)->opt));
#endif // V54_TUNNELMGR
	IPCB(skb)->flags &= ~(IPSKB_XFRM_TUNNEL_SIZE | IPSKB_XFRM_TRANSFORMED |
			      IPSKB_REROUTED);
	skb_dst_drop(skb);
	skb_dst_set(skb, &rt->u.dst);

#if 1 // V54_TUNNELMGR
	skb->tag_vid = 0;	/* zero out tag_vid in case packet comes from bridge */
#endif // V54_TUNNELMGR
	/*
	 *	Push down and install the IPIP header.
	 */

	iph             =	ip_hdr(skb);
	iph->version    =	4;
	iph->ihl		=	sizeof(struct iphdr) >> 2;
#if 1 // V54_TUNNELMGR
	iph->frag_off		=	(IS_RKS_GRE_TUNNEL(tunnel) ? htons(IP_DF) : df);
	iph->id = 0;
#else // V54_TUNNELMGR
	iph->frag_off		=	df;
#endif // V54_TUNNELMGR
#if 1 // V54_TUNNELMGR
	if(is_udp)
		iph->protocol		=	IPPROTO_UDP;
	else
		iph->protocol		=	IPPROTO_GRE;
#else // V54_TUNNELMGR
	iph->protocol		=	IPPROTO_GRE;
#endif // V54_TUNNELMGR
	iph->tos		=	ipgre_ecn_encapsulate(tos, old_iph, skb);
	iph->daddr		=	rt->rt_dst;
	iph->saddr		=	rt->rt_src;

	if ((iph->ttl = tiph->ttl) == 0) {
		if (skb->protocol == htons(ETH_P_IP))
			iph->ttl = old_iph->ttl;
#ifdef CONFIG_IPV6
		else if (skb->protocol == htons(ETH_P_IPV6))
			iph->ttl = ((struct ipv6hdr *)old_iph)->hop_limit;
#endif
		else
			iph->ttl = dst_metric(&rt->u.dst, RTAX_HOPLIMIT);
	}

#if 1 //V54_TUNNELMGR
	if(is_udp)
	{
		udph = (struct udphdr *)(iph+1);
		udph->source = htons(tunnel->parms.s_port);
		udph->dest = htons(tunnel->parms.d_port);
		udph->check = 0;
		udph->len = htons(skb->len - sizeof(struct iphdr));
		greh = (char *)(udph+1);
	}
	else
	{
		greh = (char *)(iph+1);
	}
#endif
#if 1 // V54_TUNNELMGR
	((__be16 *)(greh))[0] = tunnel->parms.o_flags;
#else // V54_TUNNELMGR
	((__be16 *)(iph + 1))[0] = tunnel->parms.o_flags;
#endif // V54_TUNNELMGR
	((__be16 *)(greh))[1] = (dev->type == ARPHRD_ETHER) ?
				   htons(ETH_P_TEB) : skb->protocol;
#if 1 // V54_TUNNELMGR
	ptr = (u32*)(((u8*)greh) + pure_gre_hlen - 4);

#ifdef DEBUG_FRAG
	printk("\n %s: skb->cb[0] %d PACKET_GRE_L2FRAG %d", __FUNCTION__, skb->cb[0], PACKET_GRE_L2FRAG);
#endif
	if (skb->cb[0] == PACKET_GRE_L2FRAG) {
		u32 seq_num = (((skb->cb[4] << 24) & 0xFF000000) |
					   ((skb->cb[3] << 16) & 0x00FF0000) |
					   ((skb->cb[2] <<	8) & 0x0000FF00) |
					   (skb->cb[1]		   & 0x000000FF));
		*ptr = htonl(seq_num);
#ifdef DEBUG_FRAG
		printk("\n seq_num in pkt %d", *ptr);
#endif
		ptr--;
	}
#endif // V54_TUNNELMGR

	if (tunnel->parms.o_flags&(GRE_KEY|GRE_CSUM|GRE_SEQ)) {
#if 1 // V54_TUNNELMGR
		if (skb->cb[0] != PACKET_GRE_L2FRAG) {
#endif // V54_TUNNELMGR
		if (tunnel->parms.o_flags&GRE_SEQ) {
			// ++tunnel->o_seqno;
			tunnel->o_seqno = 0;
			*ptr = htonl(tunnel->o_seqno);
			ptr--;
		}
#if 1 // V54_TUNNELMGR
		}
#endif // V54_TUNNELMGR
		if (tunnel->parms.o_flags&GRE_KEY) {
#if 1 // V54_TUNNELMGR
			u32 key = tunnel->parms.o_key;

			if( is_keepalive ) key |= WT_GRE_KEEPALIVE;
			if( crypto_enabled ) key |= WT_GRE_ENCRYPTION;
			if( is_padding ) key |= WT_GRE_PADDING;
			if (skb->cb[0] == PACKET_GRE_L2FRAG)
				key |= WT_GRE_L2FRAG;

			SKB_SET_FWD_POLICY(skb, &key);

			*ptr = key;
#else // V54_TUNNELMGR
			*ptr = tunnel->parms.o_key;
#endif // V54_TUNNELMGR
			ptr--;
		}
		if (tunnel->parms.o_flags&GRE_CSUM) {
#if 1 // V54_TUNNELMGR
			// We do not perform checksum calculation but
			// just use field for carrying wlan id inforamtion
			int wlanid = 0;

			if (skb->input_dev != NULL) {
				if (skb->input_dev->wireless_handlers != NULL) { // determine if this pkt is coming from wireless interface
					wlanid = ipgre_tunnel_get_wlanid(skb->input_dev);
				}
			}
#if 0
            else {
				if (net_ratelimit())
					printk("%s: skb->input_dev is NULL and wrong wlanid is embedded\n", __func__);
			}
#endif
			*ptr = htonl(wlanid) & WT_GRE_WLANID;
#else // V54_TUNNELMGR
			*ptr = 0;
			*(__sum16*)ptr = ip_compute_csum((void*)(iph+1), skb->len - sizeof(struct iphdr));
#endif // V54_TUNNELMGR
		}
	}

	nf_reset(skb);

#if 1 // V54_TUNNELMGR
    if( crypto_enabled ) {
#if defined(CONFIG_CRYPTO_DEV_TALITOS) /* SEC with Talitos driver */
		int rc = 0;
        rc = ipgre_tunnel_cipher(tunnel->parms.tfm, tunnel->parms.psk, tunnel->parms.psk_bits,
        tunnel->parms.iv, tunnel->parms.iv_len, ENCRYPT, skb, gre_hlen, is_padding, is_udp);
		if(rc < 0)
		{
			/* increase drop counter */
			stats->tx_dropped++;
			tun_stats->tun_tx_dropped++;
			dev_kfree_skb(skb);
		}
		return NETDEV_TX_OK;
#else /* Kernel Crypto */
        ipgre_tunnel_cipher(tunnel->parms.tfm, tunnel->parms.psk, tunnel->parms.psk_bits,
        tunnel->parms.iv, tunnel->parms.iv_len, ENCRYPT, skb->data + gre_hlen, clen);
#endif
    }
#endif // V54_TUNNELMGR

#if 1 // V54_TUNNELMGR
	IPTUNNEL_GRE_XMIT();
#else
	IPTUNNEL_XMIT();
#endif // V54_TUNNELMGR
	return NETDEV_TX_OK;

tx_error_icmp:
	dst_link_failure(skb);

tx_error:
	stats->tx_errors++;
#if 1 // V54_TUNNELMGR
	tun_stats->tun_tx_errors++;
#endif // V54_TUNNELMGR
	dev_kfree_skb(skb);
	return NETDEV_TX_OK;
}

#if 1 // V54_TUNNELMGR
int cavium_init_fraginfo_proc(void);

#define PPP_HDR_SIZE		8
#define IP_HDR_SIZE			20
#define UDP_HDR_SIZE		8
#define GRE_HDR_SIZE		16

#define GRE_TUN_EXTRA_BYTES	(PPP_HDR_SIZE + IP_HDR_SIZE + UDP_HDR_SIZE + GRE_HDR_SIZE)
#define FIX_BLOCK_PAD(m)	((m / 16) * 16) // 16-byte boundary
#endif // V54_TUNNELMGR

static int ipgre_tunnel_bind_dev(struct net_device *dev)
{
	struct net_device *tdev = NULL;
	struct ip_tunnel *tunnel;
	struct iphdr *iph;
	int hlen = LL_MAX_HEADER;
	int mtu;
	int addend = sizeof(struct iphdr) + 4;
	struct net_device *wan_dev = NULL;
	uint32_t wan_mtu;

	wan_dev = dev_get_by_name(&init_net, "br0");
	if (wan_dev) {
		wan_mtu = wan_dev->mtu;
		dev_put(wan_dev);
	} else {
		wan_mtu = ETH_DATA_LEN;
	}

	tunnel = netdev_priv(dev);
	iph = &tunnel->parms.iph;

	/* Guess output device to choose reasonable mtu and needed_headroom */

	if (iph->daddr) {
		struct flowi fl = { .oif = tunnel->parms.link,
				    .nl_u = { .ip4_u =
					      { .daddr = iph->daddr,
						.saddr = iph->saddr,
						.tos = RT_TOS(iph->tos) } },
				    .proto = IPPROTO_GRE };
		struct rtable *rt;
		if (!ip_route_output_key(dev_net(dev), &rt, &fl)) {
			tdev = rt->u.dst.dev;
			ip_rt_put(rt);
		}

		if (dev->type != ARPHRD_ETHER)
			dev->flags |= IFF_POINTOPOINT;
	}

	if (!tdev && tunnel->parms.link)
		tdev = __dev_get_by_index(dev_net(dev), tunnel->parms.link);

	if (tdev) {
		hlen = tdev->hard_header_len + tdev->needed_headroom;
		// mtu = tdev->mtu;
	}
	dev->iflink = tunnel->parms.link;

	/* Precalculate GRE options length */
	if (tunnel->parms.o_flags&(GRE_CSUM|GRE_KEY|GRE_SEQ)) {
		if (tunnel->parms.o_flags&GRE_CSUM)
			addend += 4;
		if (tunnel->parms.o_flags&GRE_KEY)
			addend += 4;
		if (tunnel->parms.o_flags&GRE_SEQ)
			addend += 4;
	}
	dev->needed_headroom = addend + hlen;
	mtu = wan_mtu - (dev->hard_header_len + addend);

#if 1 // V54_TUNNELMGR
	/* This parameter decides how to fragment a packet; Be liberal and
	 * clear: Ethernet header 14 + VLAN header 4 + IP header 20 + GRE header
	 * basic/key/seq 12, round up to nearest 16 bytes alignment is 64.
	 */
	dev->hard_header_len = 64;
	dev->needed_headroom = 64;

	/*
	 * MTU decides the threshold when to fragment a packet; Be conservative:
	 * Ruckus GRE interface supporting Transparent Ethernet Bridge (TEB) MTU
	 * is Ethernet data length, minus IP header length, UDP header length,
	 * GRE basic, key, rounded down to 16B alignment for optional encryption.
	 */
	mtu = FIX_BLOCK_PAD((wan_mtu - GRE_TUN_EXTRA_BYTES));

#if 0 // V54_TUNNELMGR
    //cavium_init_fraginfo_proc may trigger kernel exception
    cavium_init_fraginfo_proc();
#endif // V54_TUNNELMGR

    /* Set device fragmentation inner payload flag */
    dev->features   |= NETIF_F_FRAGINNER;
#endif // V54_TUNNELMGR

	if (mtu < 68)
		mtu = 68;

	tunnel->hlen = addend;

	return mtu;
}

int rks_gre_tunnel_frag (struct sk_buff *skb, struct net_device *dev,
						 struct iphdr *iph, struct rks_gre_frag_dbg *rdbg)
{
#ifdef DEBUG_FRAG
	printk("\n %s: skb->len %d dev->mtu %d gre_ip_fragment skb->protocol %x",
			__FUNCTION__, skb->len, dev->mtu, skb->protocol);
#endif
	if (skb->protocol == ETH_P_IP) {
		rdbg = rks_dbg_get_ent(iph->saddr, iph->daddr);
		RKS_UPD_PKT_L3(rdbg);
		RKS_UPD_INGR_PKTLEN(rdbg, skb->len);
		gre_ip_fragment(skb, dev, rdbg);
	} else if (skb->protocol == ETH_P_8021Q &&
		ntohs(((struct vlan_ethhdr *)skb->mac_header)->h_vlan_encapsulated_proto) == ETH_P_IP) {
		rdbg = rks_dbg_get_ent(iph->saddr, iph->daddr);
		RKS_UPD_PKT_L3(rdbg);
		RKS_UPD_INGR_PKTLEN(rdbg, skb->len);
		gre_ip_fragment(skb, dev, rdbg);
	} else {
		struct ethhdr *l2h = (struct ethhdr *) skb->data;
		rks_gre_frag_dbg_t *rdbg;
		__u32 dmac = (l2h->h_dest[2] << 24 | l2h->h_dest[3] << 16 |
						l2h->h_dest[4] <<  8 | l2h->h_dest[5]);
		__u32 smac = (l2h->h_source[2] << 24 | l2h->h_source[3] << 16 |
						l2h->h_source[4] <<  8 | l2h->h_source[5]);
		rdbg = rks_dbg_get_ent(smac, dmac);
		RKS_UPD_PKT_L2(rdbg);
		RKS_UPD_INGR_PKTLEN(rdbg, skb->len);
		gre_l2_fragment(skb, dev, rdbg);
	}
	return 0;
}

static int
ipgre_tunnel_ioctl (struct net_device *dev, struct ifreq *ifr, int cmd)
{
	int err = 0;
	struct ip_tunnel_parm p;
	struct ip_tunnel *t;
	struct net *net = dev_net(dev);
	struct ipgre_net *ign = net_generic(net, ipgre_net_id);
#if 1 // V54_TUNNELMGR
	unsigned long last_rx;
	struct timeval rx_idle;
#endif // V54_TUNNELMGR

	switch (cmd) {
	case SIOCGETTUNNEL:
		t = NULL;
		if (dev == ign->fb_tunnel_dev) {
			if (copy_from_user(&p, ifr->ifr_ifru.ifru_data, sizeof(p))) {
				err = -EFAULT;
				break;
			}
			t = ipgre_tunnel_locate(net, &p, 0);
		}
		if (t == NULL)
			t = netdev_priv(dev);
		memcpy(&p, &t->parms, sizeof(p));
		p.flags = dev->flags; // used for tunnelmgr to access gre1 interface info
		if (copy_to_user(ifr->ifr_ifru.ifru_data, &p, sizeof(p)))
			err = -EFAULT;
		break;

	case SIOCADDTUNNEL:
	case SIOCCHGTUNNEL:
		err = -EPERM;
		if (!capable(CAP_NET_ADMIN))
			goto done;

		err = -EFAULT;
		if (copy_from_user(&p, ifr->ifr_ifru.ifru_data, sizeof(p)))
			goto done;

		err = -EINVAL;
		if (p.iph.version != 4 || p.iph.protocol != IPPROTO_GRE ||
		    p.iph.ihl != 5 || (p.iph.frag_off&htons(~IP_DF)) ||
		    ((p.i_flags|p.o_flags)&(GRE_VERSION|GRE_ROUTING)))
			goto done;
		if (p.iph.ttl)
			p.iph.frag_off |= htons(IP_DF);

		if (!(p.i_flags&GRE_KEY))
			p.i_key = 0;
		if (!(p.o_flags&GRE_KEY))
			p.o_key = 0;

		t = ipgre_tunnel_locate(net, &p, cmd == SIOCADDTUNNEL);

		if (dev != ign->fb_tunnel_dev && cmd == SIOCCHGTUNNEL) {
			if (t != NULL) {
				if (t->dev != dev) {
					err = -EEXIST;
					break;
				}
			} else {
				unsigned nflags = 0;

				t = netdev_priv(dev);

				if (ipv4_is_multicast(p.iph.daddr))
					nflags = IFF_BROADCAST;
				else if (p.iph.daddr)
					nflags = IFF_POINTOPOINT;

				if ((dev->flags^nflags)&(IFF_POINTOPOINT|IFF_BROADCAST)) {
					err = -EINVAL;
					break;
				}
				ipgre_tunnel_unlink(ign, t);
				t->parms.iph.saddr = p.iph.saddr;
				t->parms.iph.daddr = p.iph.daddr;
				t->parms.i_key = p.i_key;
				t->parms.o_key = p.o_key;
				memcpy(dev->dev_addr, &p.iph.saddr, 4);
				memcpy(dev->broadcast, &p.iph.daddr, 4);
#if 1 // V54_TUNNELMGR
				/* Have a copy of src/dst address in global area */
				memcpy(&_wsgGlobal->saddr, &p.iph.saddr, 4);
				memcpy(&_wsgGlobal->daddr, &p.iph.daddr, 4);
#endif
				ipgre_tunnel_link(ign, t);
				netdev_state_change(dev);
			}
		}

		if (t) {
			err = 0;
			if (cmd == SIOCCHGTUNNEL) {
				t->parms.iph.ttl = p.iph.ttl;
				t->parms.iph.tos = p.iph.tos;
				t->parms.iph.frag_off = p.iph.frag_off;
				if (t->parms.link != p.link) {
					t->parms.link = p.link;
					dev->mtu = ipgre_tunnel_bind_dev(dev);
					netdev_state_change(dev);
				}
			}
			if (copy_to_user(ifr->ifr_ifru.ifru_data, &t->parms, sizeof(p)))
				err = -EFAULT;
		} else
			err = (cmd == SIOCADDTUNNEL ? -ENOBUFS : -ENOENT);
		break;

	case SIOCDELTUNNEL:
		err = -EPERM;
		if (!capable(CAP_NET_ADMIN))
			goto done;

		if (dev == ign->fb_tunnel_dev) {
			err = -EFAULT;
			if (copy_from_user(&p, ifr->ifr_ifru.ifru_data, sizeof(p)))
				goto done;
			err = -ENOENT;
			if ((t = ipgre_tunnel_locate(net, &p, 0)) == NULL)
				goto done;
			err = -EPERM;
			if (t == netdev_priv(ign->fb_tunnel_dev))
				goto done;
			dev = t->dev;
		}
		unregister_netdevice(dev);
		err = 0;
		break;

#if 1 // V54_TUNNELMGR
		case SIOCGETTUNNRXIDLE:
			last_rx = dev->last_rx;
			jiffies_to_timeval(jiffies - last_rx, &rx_idle);
			if (copy_to_user(ifr->ifr_ifru.ifru_data, &rx_idle, sizeof(rx_idle)))
				err = -EFAULT;
			break;

		case SIOCSETTUNNID:
			err = -EPERM;
			if (!capable(CAP_NET_ADMIN))
				goto done;
			err = -EFAULT;
			if (copy_from_user(&p, ifr->ifr_ifru.ifru_data, sizeof(p)))
				goto done;

            t = (struct ip_tunnel*)netdev_priv(dev);
			t->parms.tid = _wsgGlobal->appl_tid[WSG_APPL_TUNNEL] = p.tid;
			err = 0;
			break;

		case SIOCSETTUNNCRYPTO:
			err = -EPERM;
			if (!capable(CAP_NET_ADMIN))
				goto done;
			err = -EFAULT;
			if (copy_from_user(&p, ifr->ifr_ifru.ifru_data, sizeof(p)))
				goto done;

			t = (struct ip_tunnel*)netdev_priv(dev);

			t->parms.psk_bits =_wsgGlobal->psk_bits = p.psk_bits;
			memcpy(_wsgGlobal->psk, p.psk, p.psk_bits/8);
			memcpy(t->parms.psk, p.psk, p.psk_bits/8);

			t->parms.iv_len = _wsgGlobal->iv_len = p.iv_len;
			memcpy(_wsgGlobal->iv, p.iv, p.iv_len);
			memcpy(t->parms.iv, p.iv, p.iv_len);
			err = 0;
			break;

		case SIOCSETTUNNSK:
			err = -EPERM;
			if (!capable(CAP_NET_ADMIN))
				goto done;
			err = -EFAULT;
			if (copy_from_user(&p, ifr->ifr_ifru.ifru_data, sizeof(p)))
				goto done;

			if (net_ratelimit())
				printk("IP_GRE: create udp sock with src port %u and dest port %u\n", p.s_port, p.d_port);
			_wsgGlobal->s_port = p.s_port;
			_wsgGlobal->d_port = p.d_port;

			t = (struct ip_tunnel*)netdev_priv(dev);
			t->parms.d_port = p.d_port;
			t->parms.s_port = p.s_port;

			err = 0;
			break;

		case SIOCDELTUNNSK:
			_wsgGlobal->d_port = 0;
			t = (struct ip_tunnel*)netdev_priv(dev);
			t->parms.d_port = 0;

			err = 0;
			break;

		case SIOCSETTUNNSKOPT:
			err = 0;
			break;
#endif

	default:
		err = -EINVAL;
	}

done:
	return err;
}

static int ipgre_tunnel_change_mtu(struct net_device *dev, int new_mtu)
{
	struct ip_tunnel *tunnel = netdev_priv(dev);
	if (new_mtu < 68 ||
	    new_mtu > 0xFFF8 - dev->hard_header_len - tunnel->hlen)
		return -EINVAL;
	dev->mtu = new_mtu;
	return 0;
}

/* Nice toy. Unfortunately, useless in real life :-)
   It allows to construct virtual multiprotocol broadcast "LAN"
   over the Internet, provided multicast routing is tuned.


   I have no idea was this bicycle invented before me,
   so that I had to set ARPHRD_IPGRE to a random value.
   I have an impression, that Cisco could make something similar,
   but this feature is apparently missing in IOS<=11.2(8).

   I set up 10.66.66/24 and fec0:6666:6666::0/96 as virtual networks
   with broadcast 224.66.66.66. If you have access to mbone, play with me :-)

   ping -t 255 224.66.66.66

   If nobody answers, mbone does not work.

   ip tunnel add Universe mode gre remote 224.66.66.66 local <Your_real_addr> ttl 255
   ip addr add 10.66.66.<somewhat>/24 dev Universe
   ifconfig Universe up
   ifconfig Universe add fe80::<Your_real_addr>/10
   ifconfig Universe add fec0:6666:6666::<Your_real_addr>/96
   ftp 10.66.66.66
   ...
   ftp fec0:6666:6666::193.233.7.65
   ...

 */

static int ipgre_header(struct sk_buff *skb, struct net_device *dev,
			unsigned short type,
			const void *daddr, const void *saddr, unsigned len)
{
	struct ip_tunnel *t = netdev_priv(dev);
	struct iphdr *iph = (struct iphdr *)skb_push(skb, t->hlen);
	__be16 *p = (__be16*)(iph+1);

	memcpy(iph, &t->parms.iph, sizeof(struct iphdr));
	p[0]		= t->parms.o_flags;
	p[1]		= htons(type);

	/*
	 *	Set the source hardware address.
	 */

	if (saddr)
		memcpy(&iph->saddr, saddr, 4);

	if (daddr) {
		memcpy(&iph->daddr, daddr, 4);
		return t->hlen;
	}
	if (iph->daddr && !ipv4_is_multicast(iph->daddr))
		return t->hlen;

	return -t->hlen;
}

static int ipgre_header_parse(const struct sk_buff *skb, unsigned char *haddr)
{
	struct iphdr *iph = (struct iphdr *) skb_mac_header(skb);
	memcpy(haddr, &iph->saddr, 4);
	return 4;
}

static const struct header_ops ipgre_header_ops = {
	.create	= ipgre_header,
	.parse	= ipgre_header_parse,
};

#ifdef CONFIG_NET_IPGRE_BROADCAST
static int ipgre_open(struct net_device *dev)
{
	struct ip_tunnel *t = netdev_priv(dev);

	if (ipv4_is_multicast(t->parms.iph.daddr)) {
		struct flowi fl = { .oif = t->parms.link,
				    .nl_u = { .ip4_u =
					      { .daddr = t->parms.iph.daddr,
						.saddr = t->parms.iph.saddr,
						.tos = RT_TOS(t->parms.iph.tos) } },
				    .proto = IPPROTO_GRE };
		struct rtable *rt;
		if (ip_route_output_key(dev_net(dev), &rt, &fl))
			return -EADDRNOTAVAIL;
		dev = rt->u.dst.dev;
		ip_rt_put(rt);
		if (__in_dev_get_rtnl(dev) == NULL)
			return -EADDRNOTAVAIL;
		t->mlink = dev->ifindex;
		ip_mc_inc_group(__in_dev_get_rtnl(dev), t->parms.iph.daddr);
	}
	return 0;
}

static int ipgre_close(struct net_device *dev)
{
	struct ip_tunnel *t = netdev_priv(dev);

	if (ipv4_is_multicast(t->parms.iph.daddr) && t->mlink) {
		struct in_device *in_dev;
		in_dev = inetdev_by_index(dev_net(dev), t->mlink);
		if (in_dev) {
			ip_mc_dec_group(in_dev, t->parms.iph.daddr);
			in_dev_put(in_dev);
		}
	}
	return 0;
}

#endif

static const struct net_device_ops ipgre_netdev_ops = {
	.ndo_init		= ipgre_tunnel_init,
	.ndo_uninit		= ipgre_tunnel_uninit,
#ifdef CONFIG_NET_IPGRE_BROADCAST
	.ndo_open		= ipgre_open,
	.ndo_stop		= ipgre_close,
#endif
	.ndo_start_xmit		= ipgre_tunnel_xmit,
	.ndo_do_ioctl		= ipgre_tunnel_ioctl,
	.ndo_change_mtu		= ipgre_tunnel_change_mtu,
};

static void ipgre_tunnel_setup(struct net_device *dev)
{
    uint32_t wan_mtu;
    struct net_device *wan_dev = NULL;

	dev->netdev_ops		= &ipgre_netdev_ops;
	dev->destructor     = free_netdev;

	dev->type		= ARPHRD_IPGRE;
	dev->needed_headroom    = LL_MAX_HEADER + sizeof(struct iphdr) + 4;
	wan_dev = dev_get_by_name( &init_net, "br0" );
	if (wan_dev) {
		wan_mtu = wan_dev->mtu;
		dev_put(wan_dev);
	} else {
		wan_mtu = ETH_DATA_LEN;
	}

#if 1 // V54_TUNNELMGR
	/*
	 * GRE interface MTU is Ethernet data length, minus IP header length,
	 * GRE basic, csum, key, seq. In latest kerel 2.6.3x, this is made
	 * per GRE tunnel flags. We don't need that complicated logic for now.
	 */
	dev->mtu		= FIX_BLOCK_PAD((wan_mtu - GRE_TUN_EXTRA_BYTES));
#else
	dev->mtu		= wan_mtu - sizeof(struct iphdr) - 4;
#endif
	dev->flags		= IFF_NOARP;
	dev->iflink		= 0;
	dev->addr_len		= 4;
	dev->features		|= NETIF_F_NETNS_LOCAL;
	dev->priv_flags		&= ~IFF_XMIT_DST_RELEASE;
}

static int ipgre_tunnel_init(struct net_device *dev)
{
	struct ip_tunnel *tunnel;
	struct iphdr *iph;

	tunnel = netdev_priv(dev);
	iph = &tunnel->parms.iph;

	tunnel->dev = dev;
	strcpy(tunnel->parms.name, dev->name);

	memcpy(dev->dev_addr, &tunnel->parms.iph.saddr, 4);
	memcpy(dev->broadcast, &tunnel->parms.iph.daddr, 4);

	if (iph->daddr) {
#ifdef CONFIG_NET_IPGRE_BROADCAST
		if (ipv4_is_multicast(iph->daddr)) {
			if (!iph->saddr)
				return -EINVAL;
			dev->flags = IFF_BROADCAST;
			dev->header_ops = &ipgre_header_ops;
		}
#endif
	} else
		dev->header_ops = &ipgre_header_ops;

	return 0;
}

static void ipgre_fb_tunnel_init(struct net_device *dev)
{
	struct ip_tunnel *tunnel = netdev_priv(dev);
	struct iphdr *iph = &tunnel->parms.iph;
	struct ipgre_net *ign = net_generic(dev_net(dev), ipgre_net_id);

	tunnel->dev = dev;
	strcpy(tunnel->parms.name, dev->name);

	iph->version		= 4;
	iph->protocol		= IPPROTO_GRE;
	iph->ihl		= 5;
	tunnel->hlen		= sizeof(struct iphdr) + 4;

	dev_hold(dev);
	ign->tunnels_wc[0]	= tunnel;
}


static const struct net_protocol ipgre_protocol = {
	.handler	=	ipgre_rcv,
	.err_handler	=	ipgre_err,
	.netns_ok	=	1,
};

static void ipgre_destroy_tunnels(struct ipgre_net *ign)
{
	int prio;

	for (prio = 0; prio < 4; prio++) {
		int h;
		for (h = 0; h < HASH_SIZE; h++) {
			struct ip_tunnel *t;
			while ((t = ign->tunnels[prio][h]) != NULL)
				unregister_netdevice(t->dev);
		}
	}
}

static int ipgre_init_net(struct net *net)
{
	int err;
	struct ipgre_net *ign;

	err = -ENOMEM;
	ign = kzalloc(sizeof(struct ipgre_net), GFP_KERNEL);
	if (ign == NULL)
		goto err_alloc;

	err = net_assign_generic(net, ipgre_net_id, ign);
	if (err < 0)
		goto err_assign;

	ign->fb_tunnel_dev = alloc_netdev(sizeof(struct ip_tunnel), "gre0",
					   ipgre_tunnel_setup);
	if (!ign->fb_tunnel_dev) {
		err = -ENOMEM;
		goto err_alloc_dev;
	}
	dev_net_set(ign->fb_tunnel_dev, net);

	ipgre_fb_tunnel_init(ign->fb_tunnel_dev);
	ign->fb_tunnel_dev->rtnl_link_ops = &ipgre_link_ops;

	if ((err = register_netdev(ign->fb_tunnel_dev)))
		goto err_reg_dev;

	return 0;

err_reg_dev:
	free_netdev(ign->fb_tunnel_dev);
err_alloc_dev:
	/* nothing */
err_assign:
	kfree(ign);
err_alloc:
	return err;
}

static void ipgre_exit_net(struct net *net)
{
	struct ipgre_net *ign;

	ign = net_generic(net, ipgre_net_id);
	rtnl_lock();
	ipgre_destroy_tunnels(ign);
	rtnl_unlock();
	kfree(ign);
}

static struct pernet_operations ipgre_net_ops = {
	.init = ipgre_init_net,
	.exit = ipgre_exit_net,
};

static int ipgre_tunnel_validate(struct nlattr *tb[], struct nlattr *data[])
{
	__be16 flags;

	if (!data)
		return 0;

	flags = 0;
	if (data[IFLA_GRE_IFLAGS])
		flags |= nla_get_be16(data[IFLA_GRE_IFLAGS]);
	if (data[IFLA_GRE_OFLAGS])
		flags |= nla_get_be16(data[IFLA_GRE_OFLAGS]);
	if (flags & (GRE_VERSION|GRE_ROUTING))
		return -EINVAL;

	return 0;
}

static int ipgre_tap_validate(struct nlattr *tb[], struct nlattr *data[])
{
	__be32 daddr;

	if (tb[IFLA_ADDRESS]) {
		if (nla_len(tb[IFLA_ADDRESS]) != ETH_ALEN)
			return -EINVAL;
		if (!is_valid_ether_addr(nla_data(tb[IFLA_ADDRESS])))
			return -EADDRNOTAVAIL;
	}

	if (!data)
		goto out;

	if (data[IFLA_GRE_REMOTE]) {
		memcpy(&daddr, nla_data(data[IFLA_GRE_REMOTE]), 4);
		if (!daddr)
			return -EINVAL;
	}

out:
	return ipgre_tunnel_validate(tb, data);
}

static void ipgre_netlink_parms(struct nlattr *data[],
				struct ip_tunnel_parm *parms)
{
	memset(parms, 0, sizeof(*parms));

	parms->iph.protocol = IPPROTO_GRE;

	if (!data)
		return;

	if (data[IFLA_GRE_LINK])
		parms->link = nla_get_u32(data[IFLA_GRE_LINK]);

	if (data[IFLA_GRE_IFLAGS])
		parms->i_flags = nla_get_be16(data[IFLA_GRE_IFLAGS]);

	if (data[IFLA_GRE_OFLAGS])
		parms->o_flags = nla_get_be16(data[IFLA_GRE_OFLAGS]);

	if (data[IFLA_GRE_IKEY])
		parms->i_key = nla_get_be32(data[IFLA_GRE_IKEY]);

	if (data[IFLA_GRE_OKEY])
		parms->o_key = nla_get_be32(data[IFLA_GRE_OKEY]);

	if (data[IFLA_GRE_LOCAL])
		parms->iph.saddr = nla_get_be32(data[IFLA_GRE_LOCAL]);

	if (data[IFLA_GRE_REMOTE])
		parms->iph.daddr = nla_get_be32(data[IFLA_GRE_REMOTE]);

	if (data[IFLA_GRE_TTL])
		parms->iph.ttl = nla_get_u8(data[IFLA_GRE_TTL]);

	if (data[IFLA_GRE_TOS])
		parms->iph.tos = nla_get_u8(data[IFLA_GRE_TOS]);

	if (!data[IFLA_GRE_PMTUDISC] || nla_get_u8(data[IFLA_GRE_PMTUDISC]))
		parms->iph.frag_off = htons(IP_DF);
}

static int ipgre_tap_init(struct net_device *dev)
{
	struct ip_tunnel *tunnel;

	tunnel = netdev_priv(dev);

	tunnel->dev = dev;
	strcpy(tunnel->parms.name, dev->name);

	ipgre_tunnel_bind_dev(dev);

	return 0;
}

static const struct net_device_ops ipgre_tap_netdev_ops = {
	.ndo_init		= ipgre_tap_init,
	.ndo_uninit		= ipgre_tunnel_uninit,
	.ndo_start_xmit		= ipgre_tunnel_xmit,
	.ndo_set_mac_address    = eth_mac_addr,
	.ndo_validate_addr  = eth_validate_addr,
#if 1 // V54_TUNNELMGR
	.ndo_do_ioctl		= ipgre_tunnel_ioctl,
#endif // V54_TUNNELMGR
	.ndo_change_mtu		= ipgre_tunnel_change_mtu,
};

static void ipgre_tap_setup(struct net_device *dev)
{

	ether_setup(dev);

	dev->netdev_ops		= &ipgre_tap_netdev_ops;
	dev->destructor     = free_netdev;

	dev->iflink		= 0;
	dev->features		|= NETIF_F_NETNS_LOCAL;
}

static int ipgre_newlink(struct net_device *dev, struct nlattr *tb[],
			 struct nlattr *data[])
{
	struct ip_tunnel *nt;
	struct net *net = dev_net(dev);
	struct ipgre_net *ign = net_generic(net, ipgre_net_id);
	int mtu;
	int err;

	nt = netdev_priv(dev);
	ipgre_netlink_parms(data, &nt->parms);

#if 1 //V54_TUNNELMGR
	/* Have a copy of src/dst address in global area */
	memcpy(&_wsgGlobal->saddr, &nt->parms.iph.saddr, 4);
	memcpy(&_wsgGlobal->daddr, &nt->parms.iph.daddr, 4);
	printk("ipgre_newlink sa %08x da %08x\n", nt->parms.iph.saddr, nt->parms.iph.daddr);
#endif // V54_TUNNELMGR
	if (ipgre_tunnel_find(net, &nt->parms, dev->type))
		return -EEXIST;

	if (dev->type == ARPHRD_ETHER && !tb[IFLA_ADDRESS])
		random_ether_addr(dev->dev_addr);

	mtu = ipgre_tunnel_bind_dev(dev);
	if (!tb[IFLA_MTU])
		dev->mtu = mtu;

	err = register_netdevice(dev);
	if (err)
		goto out;

	dev_hold(dev);
	ipgre_tunnel_link(ign, nt);

#if 1 //V54_TUNNELMGR
	/* Create and store crypto tfm for each created GRE device */
#if defined(CONFIG_CRYPTO_DEV_TALITOS) /* SEC with Talitos driver */
	nt->parms.tfm = crypto_alloc_ablkcipher("cbc(aes)", 0, 0);

	if (IS_ERR(nt->parms.tfm)) {
		if (net_ratelimit())
			printk(KERN_ERR "alg: ablkcipher: Failed to load transform for "
				"%s: %ld\n", "cbc(aes)", PTR_ERR(nt->parms.tfm));
		return PTR_ERR(nt->parms.tfm);
	}
#else /* Kernel Crypto */
	nt->parms.tfm = crypto_alloc_blkcipher("cbc(aes)", 0, 0);

	if (IS_ERR(nt->parms.tfm)) {
        if (net_ratelimit())
            printk(KERN_ERR "alg: blkcipher: Failed to load transform for "
                "%s: %ld\n", "cbc(aes)", PTR_ERR(nt->parms.tfm));
		return PTR_ERR(nt->parms.tfm);
	}
#endif
#endif // V54_TUNNELMGR

out:
	return err;
}

static int ipgre_changelink(struct net_device *dev, struct nlattr *tb[],
			    struct nlattr *data[])
{
	struct ip_tunnel *t, *nt;
	struct net *net = dev_net(dev);
	struct ipgre_net *ign = net_generic(net, ipgre_net_id);
	struct ip_tunnel_parm p;
	int mtu;

	if (dev == ign->fb_tunnel_dev)
		return -EINVAL;

	nt = netdev_priv(dev);
	ipgre_netlink_parms(data, &p);

	t = ipgre_tunnel_locate(net, &p, 0);

	if (t) {
		if (t->dev != dev)
			return -EEXIST;
	} else {
		t = nt;

		if (dev->type != ARPHRD_ETHER) {
			unsigned nflags = 0;

			if (ipv4_is_multicast(p.iph.daddr))
				nflags = IFF_BROADCAST;
			else if (p.iph.daddr)
				nflags = IFF_POINTOPOINT;

			if ((dev->flags ^ nflags) &
			    (IFF_POINTOPOINT | IFF_BROADCAST))
				return -EINVAL;
		}

		ipgre_tunnel_unlink(ign, t);
		t->parms.iph.saddr = p.iph.saddr;
		t->parms.iph.daddr = p.iph.daddr;
		t->parms.i_key = p.i_key;
		if (dev->type != ARPHRD_ETHER) {
			memcpy(dev->dev_addr, &p.iph.saddr, 4);
			memcpy(dev->broadcast, &p.iph.daddr, 4);
		}
		ipgre_tunnel_link(ign, t);
		netdev_state_change(dev);
	}

	t->parms.o_key = p.o_key;
	t->parms.iph.ttl = p.iph.ttl;
	t->parms.iph.tos = p.iph.tos;
	t->parms.iph.frag_off = p.iph.frag_off;

	if (t->parms.link != p.link) {
		t->parms.link = p.link;
		mtu = ipgre_tunnel_bind_dev(dev);
		if (!tb[IFLA_MTU])
			dev->mtu = mtu;
		netdev_state_change(dev);
	}

	return 0;
}

static size_t ipgre_get_size(const struct net_device *dev)
{
	return
		/* IFLA_GRE_LINK */
		nla_total_size(4) +
		/* IFLA_GRE_IFLAGS */
		nla_total_size(2) +
		/* IFLA_GRE_OFLAGS */
		nla_total_size(2) +
		/* IFLA_GRE_IKEY */
		nla_total_size(4) +
		/* IFLA_GRE_OKEY */
		nla_total_size(4) +
		/* IFLA_GRE_LOCAL */
		nla_total_size(4) +
		/* IFLA_GRE_REMOTE */
		nla_total_size(4) +
		/* IFLA_GRE_TTL */
		nla_total_size(1) +
		/* IFLA_GRE_TOS */
		nla_total_size(1) +
		/* IFLA_GRE_PMTUDISC */
		nla_total_size(1) +
		0;
}

static int ipgre_fill_info(struct sk_buff *skb, const struct net_device *dev)
{
	struct ip_tunnel *t = netdev_priv(dev);
	struct ip_tunnel_parm *p = &t->parms;

	NLA_PUT_U32(skb, IFLA_GRE_LINK, p->link);
	NLA_PUT_BE16(skb, IFLA_GRE_IFLAGS, p->i_flags);
	NLA_PUT_BE16(skb, IFLA_GRE_OFLAGS, p->o_flags);
	NLA_PUT_BE32(skb, IFLA_GRE_IKEY, p->i_key);
	NLA_PUT_BE32(skb, IFLA_GRE_OKEY, p->o_key);
	NLA_PUT_BE32(skb, IFLA_GRE_LOCAL, p->iph.saddr);
	NLA_PUT_BE32(skb, IFLA_GRE_REMOTE, p->iph.daddr);
	NLA_PUT_U8(skb, IFLA_GRE_TTL, p->iph.ttl);
	NLA_PUT_U8(skb, IFLA_GRE_TOS, p->iph.tos);
	NLA_PUT_U8(skb, IFLA_GRE_PMTUDISC, !!(p->iph.frag_off & htons(IP_DF)));

	return 0;

nla_put_failure:
	return -EMSGSIZE;
}

static const struct nla_policy ipgre_policy[IFLA_GRE_MAX + 1] = {
	[IFLA_GRE_LINK]		= { .type = NLA_U32 },
	[IFLA_GRE_IFLAGS]	= { .type = NLA_U16 },
	[IFLA_GRE_OFLAGS]	= { .type = NLA_U16 },
	[IFLA_GRE_IKEY]		= { .type = NLA_U32 },
	[IFLA_GRE_OKEY]		= { .type = NLA_U32 },
	[IFLA_GRE_LOCAL]	= { .len = FIELD_SIZEOF(struct iphdr, saddr) },
	[IFLA_GRE_REMOTE]	= { .len = FIELD_SIZEOF(struct iphdr, daddr) },
	[IFLA_GRE_TTL]		= { .type = NLA_U8 },
	[IFLA_GRE_TOS]		= { .type = NLA_U8 },
	[IFLA_GRE_PMTUDISC]	= { .type = NLA_U8 },
};

static struct rtnl_link_ops ipgre_link_ops __read_mostly = {
	.kind		= "gre",
	.maxtype	= IFLA_GRE_MAX,
	.policy		= ipgre_policy,
	.priv_size	= sizeof(struct ip_tunnel),
	.setup		= ipgre_tunnel_setup,
	.validate	= ipgre_tunnel_validate,
	.newlink	= ipgre_newlink,
	.changelink	= ipgre_changelink,
	.get_size	= ipgre_get_size,
	.fill_info	= ipgre_fill_info,
};

static struct rtnl_link_ops ipgre_tap_ops __read_mostly = {
	.kind		= "gretap",
	.maxtype	= IFLA_GRE_MAX,
	.policy		= ipgre_policy,
	.priv_size	= sizeof(struct ip_tunnel),
	.setup		= ipgre_tap_setup,
	.validate	= ipgre_tap_validate,
	.newlink	= ipgre_newlink,
	.changelink	= ipgre_changelink,
	.get_size	= ipgre_get_size,
	.fill_info	= ipgre_fill_info,
};

int cavium_init_fraginfo_proc(void);
void rks_tunnel_mtu_proc_initialize(void);
void rks_tunnel_mtu_proc_shutdown(void);

/*
 *	And now the modules code and kernel interface.
 */

static int __init ipgre_init(void)
{
	int err;

	printk(KERN_INFO "GRE over IPv4 tunneling driver\n");

	if (inet_add_protocol(&ipgre_protocol, IPPROTO_GRE) < 0) {
		printk(KERN_INFO "ipgre init: can't add protocol\n");
		return -EAGAIN;
	}

	err = register_pernet_gen_device(&ipgre_net_id, &ipgre_net_ops);
	if (err < 0)
		goto gen_device_failed;

	err = rtnl_link_register(&ipgre_link_ops);
	if (err < 0)
		goto rtnl_link_failed;

	err = rtnl_link_register(&ipgre_tap_ops);
	if (err < 0)
		goto tap_ops_failed;

#if 1 //V54_TUNNELMGR
    /* Initialize netlink sock */
    appl_sock = netlink_kernel_create( &init_net, NETLINK_IPGRE, 0, ipgre_cntl_pkt_wakeup, NULL, THIS_MODULE);
    if (appl_sock == NULL) {
        printk(KERN_ERR "ip_gre: failed to create netlink socket\n");
    }

    /* Initialize Global data */
    _wsgGlobal = (WSG_GLOBAL *)kmalloc( sizeof(WSG_GLOBAL), GFP_KERNEL|__GFP_NOFAIL );
    if( _wsgGlobal == NULL ) {
		return WSG_ERROR;
    } else {
        memset( (void *)_wsgGlobal, 0, sizeof(WSG_GLOBAL) );
    }
    cavium_init_fraginfo_proc();
    rks_tunnel_mtu_proc_initialize();
    // Register function pointer in UDP stack for redirecting GRE over UDP packet into IPGRE module.
    _udp_reg_ipgre_func(&ipgre_get_udp_port, &ipgre_rcv_from_udp);
#endif // V54_TUNNELMGR

out:
	return err;

tap_ops_failed:
	rtnl_link_unregister(&ipgre_link_ops);
rtnl_link_failed:
	unregister_pernet_gen_device(ipgre_net_id, &ipgre_net_ops);
gen_device_failed:
	inet_del_protocol(&ipgre_protocol, IPPROTO_GRE);
	goto out;
}

static void __exit ipgre_fini(void)
{
#if 1 //V54_TUNNELMGR
	netlink_kernel_release(appl_sock);
#endif // V54_TUNNELMGR
	rtnl_link_unregister(&ipgre_tap_ops);
	rtnl_link_unregister(&ipgre_link_ops);
	unregister_pernet_gen_device(ipgre_net_id, &ipgre_net_ops);
	if (inet_del_protocol(&ipgre_protocol, IPPROTO_GRE) < 0)
		printk(KERN_INFO "ipgre close: can't remove protocol\n");
#if 1 //V54_TUNNELMGR
	//ipgre_delete_sock();
	kfree(_wsgGlobal);
    // remove registered func pointer from UDP stack
    _udp_unreg_ipgre_func();
#endif // V54_TUNNELMGR

    rks_tunnel_mtu_proc_shutdown();
}

module_init(ipgre_init);
module_exit(ipgre_fini);
MODULE_LICENSE("GPL");
MODULE_ALIAS_RTNL_LINK("gre");
MODULE_ALIAS_RTNL_LINK("gretap");
