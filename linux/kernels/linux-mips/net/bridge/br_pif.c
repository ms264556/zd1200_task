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
#include <linux/version.h>
#include <linux/module.h>
#include <linux/etherdevice.h>
#include <linux/if_arp.h>
#include <linux/if_vlan.h>
#include <linux/in.h>
#include <linux/ipv6.h>
#include <linux/jhash.h>
#include <linux/jiffies.h>
#include <linux/netfilter_bridge.h>
#include <linux/netdevice.h>
#include <linux/udp.h>
#include <linux/version.h>
#include <net/addrconf.h>
#include <net/ip6_checksum.h>
#include <net/ipv6.h>
#include <net/ndisc.h>
#include <ruckus/rks_pif.h>

#include <ruckus/rks_api_ver.h>
#include <ruckus/rks_kernel_debug.h>
#include "br_private.h"
#include <ruckus/rks_parse.h>
#include <ruckus/rks_api.h>

#define UDP_PORT_DHCP_SRV            67
#define UDP_PORT_DHCP_CLNT           68


#define ICMPV6_TYPE_RS              133
#define ICMPV6_TYPE_RA              134
#define ICMPV6_TYPE_NS              135
#define ICMPV6_TYPE_NA              136

#define PIF_PKT_TYPE_L2_MCAST   (1 << 0)
#define PIF_PKT_TYPE_ARP_REQ    (1 << 1)
#define PIF_PKT_TYPE_ARP_RSP    (1 << 2)
#define PIF_PKT_TYPE_DHCP_RSP   (1 << 3)
#define PIF_PKT_TYPE_ICMP6_NS   (1 << 4)
#define PIF_PKT_TYPE_ICMP6_NA   (1 << 5)
#define PIF_PKT_TYPE_ICMP6_RA   (1 << 6)
#define PIF_PKT_TYPE_DHCP_REQ   (1 << 7)
#define PIF_PKT_TYPE_DHCP_RSP_NAK   (1 << 8)

struct dhcp_pkt {
    unsigned char   op;
    unsigned char   htype[1];
    unsigned char   hlen[1];
    unsigned char   hops[1];
    unsigned int    xid;
    unsigned char   secs[2];
    unsigned char   flags[2];
    unsigned int    ciaddr;
    unsigned int    yiaddr;
    unsigned int    siaddr;
    unsigned int    giaddr;
    unsigned char   chaddr[16];
    unsigned char   sname[64];
    unsigned char   file[128];
    unsigned char   magic[4];
    unsigned char   options[0];
};

struct icmpv6_pkt {
    struct icmp6hdr hdr;
    unsigned int tiaddr[4];
    unsigned char sll_type;
    unsigned char sll_len;
    unsigned char sll_addr[6];
};

struct icmpv6_pkt_ra {
    unsigned char  icmp6_type;
    unsigned char  icmp6_code;
    unsigned short icmp6_cksum;
    unsigned char buf[0];
};
struct pkt_info {
    struct sk_buff *skb;
    unsigned int pkt_type;
    struct ethhdr   *eth_h;
    struct arp_pkt  *arp_h;
    struct iphdr    *ip_h;
    struct ipv6hdr  *ipv6_h;
    struct tcphdr   *tcp_h;
    struct udphdr   *udp_h;
    struct icmpv6_pkt *icmpv6_h;
    struct dhcp_pkt *dhcp_h;
    unsigned int    vlan;
    struct net_bridge *br;
    struct net_bridge_port *in_p;
    unsigned char   *ip;
};

struct __ndp_counters {
    unsigned long long ndp;
    unsigned long long nss;
    unsigned long long rap;
    unsigned long long rsg;
    unsigned long long rag;
    unsigned long long rat;
    unsigned long long raa;
    unsigned long long rat_timestamp;
};

extern void br_ipdb_cleanup(unsigned long _data);
extern void br_ipdb_clear(struct net_bridge *br);
extern int  br_ipdb_delete_by_mac(struct net_bridge *br, int ifidx, char *macaddr);
extern int  br_ipdb_delete_by_port(struct net_bridge *br, int port);
extern int  br_ipdb_delete_by_ifmode(struct net_bridge *br, int ifmode);

extern int  br_ipdb_get_mac(struct net_bridge *br, unsigned int ipaddr,
				unsigned int vlan, unsigned char **macaddr,
				unsigned int *iptype);
extern int  br_ipdb_fillbuf(struct net_bridge *br, void *buf,
				unsigned long count, unsigned long off);

extern void br_ipv6db_fini(void);
extern void br_ipv6db_cleanup(unsigned long _data);
extern void br_ipv6db_clear(struct net_bridge *br);
extern int  br_ipv6db_delete_by_mac(struct net_bridge *br, int ifidx, char *macaddr);
extern int  br_ipdb_delete_by_mac_n_xid(struct net_bridge *br, int ifidx, char *macaddr, unsigned int xid);
extern int  br_ipdb_lookup(struct net_bridge *br, const unsigned int ipaddr, const unsigned int vlan,
                                const unsigned char *macaddr, struct net_bridge_port *br_port, const unsigned int xid);
extern int  br_ipv6db_delete_by_port(struct net_bridge *br, int port);

extern int  br_ipv6db_fillbuf(struct net_bridge *br, void *buf,
				unsigned long count, unsigned long off);
extern int br_ipv6db_find_router_by_vlan(struct net_bridge *br, int vlan, unsigned char num_routers, struct ra_cache_info *rinfo);

const char *const wlan_ifnames[] = {
    "wlan0", "wlan1", "wlan2", "wlan3", "wlan4", "wlan5", "wlan6", "wlan7",
    "wlan8", "wlan9", "wlan10", "wlan11", "wlan12", "wlan13", "wlan14", "wlan15",
    "wlan16", "wlan17", "wlan18", "wlan19", "wlan20", "wlan21", "wlan22", "wlan23",
    "wlan24", "wlan25", "wlan26", "wlan27", "wlan28", "wlan29", "wlan30", "wlan31",
    "wlan32", "wlan33", "wlan34", "wlan35", "wlan36", "wlan37", "wlan38", "wlan39",
    "wlan40", "wlan41", "wlan42", "wlan43", "wlan44", "wlan45", "wlan46", "wlan47",
    "wlan48", "wlan49", "wlan50", "wlan51", "wlan52", "wlan53", "wlan54", "wlan55",
    "wlan56", "wlan57", "wlan58", "wlan59", "wlan60", "wlan61", "wlan62", "wlan63",
};
#define wlan_if_names_count  (sizeof(wlan_ifnames) / sizeof(char*))
const char* const brnames[] = {
    "br0",
    "br1",
    "br2",
    "br3",
    "br4",
    "br5",
    "br6",
    "br7",
    "br8",
    "br9",
    "br10",
    "br11",
    "br12",
    "br13",
    "br14",
    "br15",
    "br16",
    "br17",
    "br18"
};
#define ipv6_br_names_count  (sizeof(brnames) / sizeof(char*))
struct __ndp_counters g_pif_ndp_counter[wlan_if_names_count] = {{0}};
struct br_pif_ipv6_filter_pvt ipv6_filter[(ipv6_br_names_count)] = {{0}};

static atomic_t ra_proxy_enable_cnt[ipv6_br_names_count] = ATOMIC_INIT(0);

int db_update;
struct ra_throttle_port_cfg
{
    atomic_t ra_throttle_allow_cnt;
    unsigned long last_refresh_timestamp;
};
static struct ra_throttle_port_cfg g_rat_cfg[wlan_if_names_count];
static int get_idx_from_wlanname(const char *name)
{
    int i;
    for(i = 0; i < wlan_if_names_count; i++) {
        if(strcmp(wlan_ifnames[i], name) == 0) {
            return i;
        }
    }
    return -1;
}
static int get_idx_from_brname(const char *name)
{
    int i;
    for(i = 0; i < ipv6_br_names_count; i++) {
        if(strcmp(brnames[i], name) == 0) {
            return i;
        }
    }
    return -1;
}
#define MAX_ROUTER_PER_VLAN  2
static unsigned int g_num_routers = MAX_ROUTER_PER_VLAN;


#define ALLOW_GW_LEARNING(p) \
    ((p)->in_p->pif_learning == BR_PIF_LEARNING_GW_ONLY)


static inline int __is_gw_of_client(unsigned char *macaddr, const unsigned int ipaddr, const struct net_device *dev)
{
    return 0;
}

static inline void __update_gw_of_client(unsigned char *macaddr, const unsigned int *ipv6addr, const struct net_device *dev)
{
    return;
}


static inline int __ipdb_should_upd(struct pkt_info *pktinfo, struct net_bridge *br, struct net_bridge_port  *port,
                                    unsigned int pkt_type, unsigned int iftype, unsigned int ifmode)
{

    if (pkt_type & (PIF_PKT_TYPE_ARP_REQ  |
                    PIF_PKT_TYPE_ARP_RSP  |
                    PIF_PKT_TYPE_DHCP_RSP |
                    PIF_PKT_TYPE_DHCP_REQ |
                    PIF_PKT_TYPE_DHCP_RSP_NAK |
                    PIF_PKT_TYPE_ICMP6_RA |
                    PIF_PKT_TYPE_ICMP6_NS |
                    PIF_PKT_TYPE_ICMP6_NA)) {
        if (port->pif_learning) {
            if (port->pif_learning == BR_PIF_LEARNING_ALL) {
                return 1;
            } else if (port->pif_learning == BR_PIF_LEARNING_GW_ONLY) {
                if (pkt_type & (PIF_PKT_TYPE_ARP_REQ|PIF_PKT_TYPE_ARP_RSP)) {
                    return __is_gw_of_client(pktinfo->eth_h->h_dest, pktinfo->arp_h->ar_sip, pktinfo->skb->input_dev);
                } else if (pkt_type & PIF_PKT_TYPE_ICMP6_RA) {
                    return 1;
                } else if (pkt_type & PIF_PKT_TYPE_ICMP6_NA) {
                    return pktinfo->icmpv6_h->hdr.icmp6_router;
                }
            }
        }
        if (iftype == BR_PIF_IFTYPE_PROXYARP) {
            return 1;
        } else if (br->pif_directed_arp) {
            if (ifmode == BR_PIF_IFMODE_DNLINK_PORT) {
                return 1;
            } else if (br->is_pif_proc_uplink) {
                return 1;
            }
        }
    }
    return 0;

}


static int br_pif_ipdb_upd(struct pkt_info *pktinfo)
{
    unsigned short is_rsp;
    if (pktinfo->in_p == NULL) {
        return 0;
    }

    if ((pktinfo->pkt_type & PIF_PKT_TYPE_ARP_REQ)
       || ((pktinfo->pkt_type & PIF_PKT_TYPE_ARP_RSP) && ALLOW_GW_LEARNING(pktinfo))
       ) {
        struct arp_pkt  *arp_hdr = pktinfo->arp_h;


        if (arp_hdr->ar_sip != 0 && arp_hdr->ar_sip != 0xFFFFFFFF &&
            !is_zero_ether_addr(arp_hdr->ar_sha) && !is_broadcast_ether_addr(arp_hdr->ar_sha)) {
            if (__ipdb_should_upd(pktinfo, pktinfo->br, pktinfo->in_p, pktinfo->pkt_type, pktinfo->in_p->pif_iftype, pktinfo->in_p->pif_ifmode)) {
                br_ipdb_insert(pktinfo->br, arp_hdr->ar_sip, pktinfo->vlan, arp_hdr->ar_sha, pktinfo->in_p);
                rks_dbg(RKS_DEBUG_PIF, RKS_KERN_INFO, "[%s]: Add IP: %pM->%pM, sip %pI4, sha %pM\n", __func__,
                       pktinfo->eth_h->h_source, pktinfo->eth_h->h_dest,
                       &arp_hdr->ar_sip, arp_hdr->ar_sha);

            }
        }
    }
    else if (pktinfo->pkt_type & (PIF_PKT_TYPE_DHCP_RSP_NAK)) {

        struct dhcp_pkt * dhcp_hdr = pktinfo->dhcp_h;
        if (dhcp_hdr && !is_zero_ether_addr(dhcp_hdr->chaddr) && !is_broadcast_ether_addr(dhcp_hdr->chaddr)) {
            struct net_bridge_fdb_entry *fdb = NULL;

            if (!ether_addr_equal(dhcp_hdr->chaddr, pktinfo->br->dev->dev_addr)) {
                fdb = __BR_FDB_GET(pktinfo->br, dhcp_hdr->chaddr, pktinfo->vlan);
            }
            if (fdb && __ipdb_should_upd(pktinfo, pktinfo->br, pktinfo->in_p, pktinfo->pkt_type, fdb->dst->pif_iftype, fdb->dst->pif_ifmode)) {
                if (dhcp_hdr->xid) {
                    br_ipdb_delete_by_mac_n_xid(pktinfo->br, fdb->dst->dev->ifindex, dhcp_hdr->chaddr, dhcp_hdr->xid);
                    rks_dbg(RKS_DEBUG_PIF, RKS_KERN_INFO, "[%s]: DEL IP NAK: %pM->%pM, cha %pM\n", __func__,
                           pktinfo->eth_h->h_source, pktinfo->eth_h->h_dest,
                           dhcp_hdr->chaddr);

                }
            }
        }
    }
    else if (pktinfo->pkt_type & (PIF_PKT_TYPE_DHCP_RSP)) {

        struct dhcp_pkt * dhcp_hdr = pktinfo->dhcp_h;

        if (dhcp_hdr && !is_zero_ether_addr(dhcp_hdr->chaddr) && !is_broadcast_ether_addr(dhcp_hdr->chaddr)) {
            struct net_bridge_fdb_entry *fdb = NULL;

            if (!ether_addr_equal(dhcp_hdr->chaddr, pktinfo->br->dev->dev_addr)) {
                fdb = __BR_FDB_GET(pktinfo->br, dhcp_hdr->chaddr, pktinfo->vlan);
            }

            if (fdb && __ipdb_should_upd(pktinfo, pktinfo->br, pktinfo->in_p, pktinfo->pkt_type, fdb->dst->pif_iftype, fdb->dst->pif_ifmode)) {
                unsigned int *ipaddr = NULL;
                if (dhcp_hdr->ciaddr != 0 && dhcp_hdr->ciaddr != 0xFFFFFFFF) {
                    ipaddr = &dhcp_hdr->ciaddr;
                } else if (dhcp_hdr->yiaddr != 0 && dhcp_hdr->yiaddr != 0xFFFFFFFF) {
                    ipaddr = &dhcp_hdr->yiaddr;
                }
                if (ipaddr != NULL) {
                    if (!br_ipdb_lookup(pktinfo->br, *ipaddr, pktinfo->vlan, dhcp_hdr->chaddr, fdb->dst, dhcp_hdr->xid)) {
                        br_ipdb_delete_by_mac(pktinfo->br, fdb->dst->dev->ifindex, dhcp_hdr->chaddr);
                        br_ipdb_insert(pktinfo->br, *ipaddr, pktinfo->vlan, dhcp_hdr->chaddr, fdb->dst);
                        rks_dbg(RKS_DEBUG_PIF, RKS_KERN_INFO, "[%s]: Add IP: %pM->%pM, ciaddr %pI4, chaddr %pM\n", __func__,
                            pktinfo->eth_h->h_source, pktinfo->eth_h->h_dest, ipaddr, dhcp_hdr->chaddr);

                    }
                }
            }
        }
    }
    else if (pktinfo->pkt_type & (PIF_PKT_TYPE_DHCP_REQ)) {

        struct dhcp_pkt * dhcp_hdr = pktinfo->dhcp_h;

        if (dhcp_hdr && !is_zero_ether_addr(dhcp_hdr->chaddr) && !is_broadcast_ether_addr(dhcp_hdr->chaddr)) {
            if (__ipdb_should_upd(pktinfo, pktinfo->br, pktinfo->in_p, pktinfo->pkt_type, pktinfo->in_p->pif_iftype, pktinfo->in_p->pif_ifmode)) {
                unsigned int *ipaddr = (unsigned int *)pktinfo->ip;
                if (ipaddr != NULL) {
                    br_ipdb_insert(pktinfo->br, *ipaddr, pktinfo->vlan, dhcp_hdr->chaddr, pktinfo->in_p);
                    rks_dbg(RKS_DEBUG_PIF, RKS_KERN_INFO, "[%s]: Add entry : %pM->%pM, ciaddr %pI4, chaddr %pM, xid %d\n", __func__,
                        pktinfo->eth_h->h_source, pktinfo->eth_h->h_dest, ipaddr, dhcp_hdr->chaddr, dhcp_hdr->xid);

                }
            }
        }
    }

    return 0;
}

#define NDP_ENTRY_PROBE_TIMEOUT (1*HZ) //1 second
static __inline__ int is_timedout(unsigned long entry_time, unsigned long timeout)
{
    return entry_time && time_before_eq(timeout + entry_time, jiffies);
}


static int br_pif_ipv6db_upd(struct pkt_info *pktinfo)
{
    struct ethhdr *eth = pktinfo->eth_h;

    if (pktinfo->in_p == NULL) {
        return 0;
    }

    if (pktinfo->pkt_type & PIF_PKT_TYPE_ICMP6_NS) {
        struct in6_addr *ipv6addr;
        unsigned char *macaddr;


        if (ipv6_addr_is_zero(&pktinfo->ipv6_h->saddr)) {
            ipv6addr = (struct in6_addr *)pktinfo->icmpv6_h->tiaddr; // Target IP address
            macaddr  = pktinfo->eth_h->h_source;
        } else {
            ipv6addr = &pktinfo->ipv6_h->saddr;
            macaddr  = pktinfo->icmpv6_h->sll_addr;
        }

        if (__ipdb_should_upd(pktinfo, pktinfo->br, pktinfo->in_p, pktinfo->pkt_type, pktinfo->in_p->pif_iftype, pktinfo->in_p->pif_ifmode)) {
            br_ipv6db_insert(pktinfo->br, ipv6addr, pktinfo->vlan, eth->h_source, pktinfo->in_p);

        }
    }
    else if ((pktinfo->pkt_type & (PIF_PKT_TYPE_ICMP6_RA|PIF_PKT_TYPE_ICMP6_NA)) &&
             ALLOW_GW_LEARNING(pktinfo)) {
        if (__ipdb_should_upd(pktinfo, pktinfo->br, pktinfo->in_p, pktinfo->pkt_type, pktinfo->in_p->pif_iftype, pktinfo->in_p->pif_ifmode)) {
            br_ipv6db_insert(pktinfo->br, &pktinfo->ipv6_h->saddr, pktinfo->vlan, eth->h_source, pktinfo->in_p);
            if (0 == ipv6_addr_is_ll_all_nodes(&pktinfo->ipv6_h->daddr)) {
                __update_gw_of_client(pktinfo->eth_h->h_dest, &pktinfo->ipv6_h->saddr, pktinfo->skb->input_dev);
            }

        }
    }

    return 0;
}

static int br_pif_rate_limit_check(struct pkt_info *pktinfo)
{
    struct net_bridge *br = pktinfo->br;
    unsigned long now;
    int drop = 0;
    char *action = "";

    if (pktinfo->in_p->pif_ifmode == BR_PIF_IFMODE_DNLINK_PORT) {
        return 0;
    }

    if (br->pif_token_max == PIF_TOKEN_MAX_IGNORE) {
        action = "IGNORE";
        drop = 0;
    } else if (br->pif_token_max == PIF_TOKEN_MAX_DROPALL) {
        action = "DROPALL";
        drop = 1;
    } else {
        now = jiffies;
        if (time_after(now, br->pif_token_timestamp + HZ)) {
            br->pif_token_timestamp = now;
            action = "pass";
            drop = 0;
            br->pif_token_count = 1;
        } else {

            if (br->pif_token_count * HZ > br->pif_token_max * (now - br->pif_token_timestamp)) {
                action = "drop";
                drop = 1;
            } else {
                action = "pass";
                drop = 0;
                br->pif_token_count++;
            }
        }
    }

    rks_dbg(RKS_DEBUG_PIF, RKS_KERN_INFO, "[%s]: PIF - %s: %pM->%pM, count %u, max %u", __func__, action,
           pktinfo->eth_h->h_source, pktinfo->eth_h->h_dest,
           br->pif_token_count, br->pif_token_max);

    return drop;
}

// xx
int br_pif_gen_garp_req(unsigned short idx, unsigned short vlan_id, unsigned int ip_addr)
{
    struct net_device *dev = NULL;
    struct sk_buff *skb = NULL;
    struct arp_pkt *arp = NULL;
    unsigned char macaddr[ETH_ALEN] = {0};
    unsigned char dmac[ETH_ALEN]= {0};
    unsigned char iface[16] = {0};
    unsigned char tmpIntf[16] = {0};

    strcpy(iface, "br1");
    if (vlan_id>1) {
        snprintf(tmpIntf, 16, ".%d", vlan_id);
        strcat(iface, tmpIntf);
    }

    if (idx) {
        snprintf(tmpIntf, 16, ":%d", idx);
        strcat(iface, tmpIntf);
    }

    dev = dev_get_by_name(&init_net, iface);
    if (dev == NULL) {
        return 0;
    }

    memcpy(macaddr, dev->dev_addr, ETH_ALEN);

    skb = alloc_skb(LL_RESERVED_SPACE(dev)+sizeof(struct arp_pkt), GFP_ATOMIC);

    if (skb == NULL)
        return 0;

    skb_reserve(skb, LL_RESERVED_SPACE(dev));

     skb_reset_network_header(skb);
    skb->protocol = __constant_htons(ETH_P_ARP);
    skb->dev = dev;
    skb->tag_vid = ((vlan_id == 1) ? 0 : vlan_id);

    memset(dmac, 0xFF, ETH_ALEN);

    if (

        dev_hard_header(skb, dev, ETH_P_ARP, dmac, macaddr, skb->len) < 0

    ) {
        kfree_skb(skb);
        return 0;
    }

    arp = (struct arp_pkt *)skb_put(skb, sizeof(struct arp_pkt));
    arp->ar_hrd = htons(dev->type);
    arp->ar_hln = 6;
    arp->ar_pro = __constant_htons(ETH_P_IP);
    arp->ar_pln = 4;
    arp->ar_op  = __constant_htons(ARPOP_REQUEST);

    memcpy(arp->ar_sha, macaddr, ETH_ALEN);
    arp->ar_sip = ip_addr;
    memcpy(arp->ar_tha, dmac, ETH_ALEN);
    arp->ar_tip = ip_addr;


    dev_queue_xmit(skb);


    return 0;
}


static int br_pif_gen_arp_rsp(struct pkt_info *pktinfo, unsigned char *macaddr)
{
    struct sk_buff *skb = NULL;
    struct net_device *in_dev = pktinfo->in_p->dev;
    struct net_device *dev = pktinfo->br->dev;
    struct arp_pkt *arp = NULL;

    if (dev == NULL || in_dev == NULL)
        return 0;

    skb = alloc_skb(LL_RESERVED_SPACE(dev)+sizeof(struct arp_pkt), GFP_ATOMIC);

    if (skb == NULL)
        return 0;

    skb_reserve(skb, LL_RESERVED_SPACE(dev));

    skb_reset_network_header(skb);

    skb->protocol = __constant_htons(ETH_P_ARP);
    skb->dev = in_dev;
    skb->tag_vid = ((pktinfo->vlan == 1) ? 0 : pktinfo->vlan);

    if (
        dev_hard_header(skb, dev, ETH_P_ARP, pktinfo->arp_h->ar_sha, macaddr, skb->len) < 0
    ) {
        kfree_skb(skb);
        return 0;
    }

    arp = (struct arp_pkt *)skb_put(skb, sizeof(struct arp_pkt));
    arp->ar_hrd = htons(dev->type);

    arp->ar_hln = 6;
    arp->ar_pro = __constant_htons(ETH_P_IP);
    arp->ar_pln = 4;
    arp->ar_op  = __constant_htons(ARPOP_REPLY);

    memcpy(arp->ar_sha, macaddr, ETH_ALEN);
    arp->ar_sip = pktinfo->arp_h->ar_tip;
    memcpy(arp->ar_tha, pktinfo->arp_h->ar_sha, ETH_ALEN);
    arp->ar_tip = pktinfo->arp_h->ar_sip;

    rks_dbg(RKS_DEBUG_PIF, RKS_KERN_INFO, "[%s]: Gen ARP Rsp: %pM->%pM, sip %pI4, sha %pM\n", __func__,
        dev->dev_addr, pktinfo->arp_h->ar_sha, &arp->ar_sip, arp->ar_sha);

    if (!br_is_pvid_set(pktinfo->in_p) && !skb->tag_vid) {
        skb->dev = dev;
        dev_queue_xmit(skb);
    } else {
        br_vlan_dev_queue_xmit(skb, in_dev);
    }

    return 0;
}

static int br_pif_proc_arp(struct pkt_info *pktinfo,
                         const struct sk_buff *skb)
{
    unsigned char *macaddr;
    unsigned int  iftype;
    struct arp_pkt  *arp_hdr = pktinfo->arp_h;
    struct ethhdr *eth = pktinfo->eth_h;
    int drop = 0, fallthr = 0;
    int is_gratuious = (!(arp_hdr->ar_sip) ||
                    (arp_hdr->ar_sip == arp_hdr->ar_tip))?1:0;


    if (pktinfo->in_p == NULL)
        return 0;

    if (pktinfo->pkt_type & PIF_PKT_TYPE_ARP_RSP) {

        br_pif_ipdb_upd(pktinfo);
        if (is_gratuious) {
           goto rate_limit;
        }
        return 0;
    }


    if (br_ipdb_get_mac(pktinfo->br, arp_hdr->ar_tip, pktinfo->vlan, &macaddr, &iftype) == 0) {

        if (iftype == BR_PIF_IFTYPE_PROXYARP) {
            if (is_gratuious) {
                if (!ether_addr_equal(macaddr, arp_hdr->ar_sha)) {
                    br_pif_gen_arp_rsp(pktinfo, macaddr);
                    pktinfo->br->pif_proxy_arp_count++;
                    drop = 1;
                }
            } else {
                 br_pif_gen_arp_rsp(pktinfo, macaddr);
                 pktinfo->br->pif_proxy_arp_count++;
                 drop = 1;
            }
        } else if (pktinfo->br->pif_directed_arp) {
            if ((pktinfo->in_p->pif_ifmode == BR_PIF_IFMODE_UPLINK_PORT) ||
                (pktinfo->br->is_pif_proc_uplink)) {
                 memcpy(arp_hdr->ar_tha, macaddr, ETH_ALEN);
                 memcpy(eth->h_dest, macaddr, ETH_ALEN);
                 pktinfo->br->pif_directed_arp_count++;
                 fallthr = 1;
            }
        }
    }

    if (drop && is_gratuious) {
    } else {
        br_pif_ipdb_upd(pktinfo);
    }

rate_limit:
    if (!fallthr && !drop) {
        drop = br_pif_rate_limit_check(pktinfo);
    }

    rks_dbg(RKS_DEBUG_PIF, RKS_KERN_INFO, "[%s]: drop %d\n", __func__, drop);
    return drop;
}


static int br_pif_gen_icmpv6_na(struct pkt_info *pktinfo, const struct in6_addr *ipv6addr, unsigned char *macaddr)
{
    struct sk_buff *skb = NULL;
    struct net_device *in_dev = pktinfo->in_p->dev;
    struct net_device *dev = pktinfo->br->dev;
    struct ipv6hdr *ipv6;
    struct icmpv6_pkt *icmpv6;
    unsigned char *p;

    if (dev == NULL || in_dev == NULL)
        return 0;

    skb = alloc_skb(LL_RESERVED_SPACE(dev) + sizeof(struct ipv6hdr) + sizeof(struct icmpv6_pkt), GFP_ATOMIC);
    if (skb == NULL)
        return 0;
    skb_reserve(skb, LL_RESERVED_SPACE(dev));
    skb_reset_mac_header(skb);
    skb->protocol = __constant_htons(ETH_P_IPV6);
    skb->dev = in_dev;
    skb->tag_vid = ((pktinfo->vlan == 1) ? 0 : pktinfo->vlan);

    if (
        dev_hard_header(skb, dev, ETH_P_IPV6, pktinfo->eth_h->h_source, macaddr, skb->len) < 0
    ) {
        kfree_skb(skb);
        return 0;
    }

    ipv6 = (struct ipv6hdr *)skb_put(skb, sizeof(struct ipv6hdr));
    skb_set_network_header(skb, ((unsigned char *)ipv6 - skb->data));
    memset(ipv6, 0, sizeof(struct ipv6hdr));
    ipv6->version = 6;
    ipv6->payload_len = __constant_htons(sizeof(struct icmpv6_pkt));
    ipv6->nexthdr = NEXTHDR_ICMP;
    ipv6->hop_limit = 255;

    memcpy((unsigned char *)&ipv6->daddr, (unsigned char *)&pktinfo->ipv6_h->saddr, sizeof(struct in6_addr));
    if (ipv6_addr_is_zero(&pktinfo->ipv6_h->saddr)) {
        ipv6->daddr.s6_addr[0]  = 0xFF;
        ipv6->daddr.s6_addr[1]  = 0x02;
        ipv6->daddr.s6_addr[15] = 0x01;
    }
    memcpy((unsigned char *)&ipv6->saddr, (unsigned char *)&pktinfo->icmpv6_h->tiaddr, sizeof(struct in6_addr));
    p = skb_put(skb, sizeof(struct icmpv6_pkt));
    icmpv6 = (struct icmpv6_pkt *)p;
    memset(icmpv6, 0, sizeof(struct icmpv6_pkt));

    skb_set_transport_header(skb, ((unsigned char *)icmpv6 - skb->data));

    icmpv6->hdr.icmp6_type = ICMPV6_TYPE_NA;
    icmpv6->hdr.icmp6_code = 0;
    icmpv6->hdr.icmp6_cksum = 0;
    icmpv6->hdr.icmp6_dataun.u_nd_advt.solicited = 1;
    icmpv6->hdr.icmp6_dataun.u_nd_advt.override = 1;

    memcpy(icmpv6->tiaddr, ipv6addr, sizeof(icmpv6->tiaddr));
    icmpv6->sll_type = 2;
    icmpv6->sll_len = 1;
    memcpy(icmpv6->sll_addr, macaddr, ETH_ALEN);
    icmpv6->hdr.icmp6_cksum = csum_ipv6_magic(&ipv6->saddr, &ipv6->daddr, sizeof(struct icmpv6_pkt), NEXTHDR_ICMP,
                                              csum_partial((char *)icmpv6, sizeof(struct icmpv6_pkt), 0));

    rks_dbg(RKS_DEBUG_PIF, RKS_KERN_INFO, "[%s]: Gen ICMPv6 NA: %pI6->%pI6, tip %pI6, tmac %pM, len %d\n", __func__,
        &ipv6->saddr, &ipv6->daddr, ipv6addr, macaddr, skb->len);

    if (!br_is_pvid_set(pktinfo->in_p) && !skb->tag_vid) {

        skb->dev = dev;
        dev_queue_xmit(skb);
    } else {
        br_vlan_dev_queue_xmit(skb, in_dev);
    }

    return 0;
}


static int br_pif_proc_icmpv6_ns(struct pkt_info *pktinfo)
{
    unsigned char *macaddr;
    unsigned int  iftype;
    struct in6_addr *ipv6addr = (struct in6_addr *)pktinfo->icmpv6_h->tiaddr;
    struct ethhdr *eth = pktinfo->eth_h;
    int drop = 0, fallthr = 0;

    if (pktinfo->in_p == NULL)
        return 0;

    if (pktinfo->pkt_type & PIF_PKT_TYPE_ICMP6_NA) {
        br_pif_ipv6db_upd(pktinfo);
        return 0;
    }


    if (br_ipv6db_get_mac(pktinfo->br, ipv6addr, pktinfo->vlan, &macaddr, &iftype) == 0) {
        if (iftype == BR_PIF_IFTYPE_PROXYARP) {
            if (ipv6_addr_is_zero(&pktinfo->ipv6_h->saddr)) {
                if (!ether_addr_equal(macaddr, eth->h_source)) {
                    br_pif_gen_icmpv6_na(pktinfo, ipv6addr, macaddr);
                    pktinfo->br->pif_proxy_arp_count++;
                    drop = 1;
                }
            } else {
                br_pif_gen_icmpv6_na(pktinfo, ipv6addr, macaddr);
                pktinfo->br->pif_proxy_arp_count++;
                drop = 1;
            }
        } else if (pktinfo->br->pif_directed_arp) {

            memcpy(eth->h_dest, macaddr, ETH_ALEN);
            pktinfo->br->pif_directed_arp_count++;
            fallthr = 1;
        }
    }

    ipv6addr = &pktinfo->ipv6_h->saddr;
    if (drop && ipv6_addr_is_zero(ipv6addr)) {
    } else {
        br_pif_ipv6db_upd(pktinfo);
    }

    if (!fallthr && !drop) {
        drop = br_pif_rate_limit_check(pktinfo);
    }
    return drop;
}



static int br_pif_parser(struct sk_buff *skb, struct pkt_info *pktinfo)
{
    struct ethhdr   *eth = eth_hdr(skb);
    unsigned short  ether_type;
    unsigned char   *p = NULL;
    unsigned int    len = skb->len;

    pktinfo->eth_h = eth;

    if (is_multicast_ether_addr(eth->h_dest)) {
        pktinfo->pkt_type |= PIF_PKT_TYPE_L2_MCAST;
    }

    if (eth->h_proto == __constant_htons(ETH_P_8021Q)) {
        ether_type = ((struct vlan_ethhdr *)eth_hdr(skb))->h_vlan_encapsulated_proto;
        p = (unsigned char *)(skb_mac_header(skb) + sizeof(struct vlan_ethhdr));
        len -= (skb_mac_header(skb) + sizeof(struct vlan_ethhdr)) - skb->data;
    } else {
        ether_type = eth->h_proto;
        p = (unsigned char *)(skb_mac_header(skb) + sizeof(struct ethhdr));
        len -= (skb_mac_header(skb) + sizeof(struct ethhdr)) - skb->data;
    }

    if (ether_type == __constant_htons(ETH_P_ARP)) {
        struct arp_pkt *arp_hdr = (struct arp_pkt *)p;

        if (len < sizeof(struct arp_pkt) ||
            arp_hdr->ar_hrd != __constant_htons(ETH_P_802_3) ||
            arp_hdr->ar_pro != __constant_htons(ETH_P_IP) ||
            arp_hdr->ar_hln != ETH_ALEN ||
            arp_hdr->ar_pln != 4 ||
            (arp_hdr->ar_op != __constant_htons(ARPOP_REQUEST) &&
             arp_hdr->ar_op != __constant_htons(ARPOP_REPLY))) {
            rks_dbg(RKS_DEBUG_PIF, RKS_KERN_ERR, "[%s]: Malformed ARP: %pM->%pM, len %d (exp %d), vlan %u, hrd %hu, pro %hu, hln %u, pln %u, op %hu\n",
                __func__, eth->h_source, eth->h_dest, len, sizeof(struct arp_pkt), pktinfo->vlan,
                ntohs(arp_hdr->ar_hrd), ntohs(arp_hdr->ar_pro), arp_hdr->ar_hln,
                arp_hdr->ar_pln, ntohs(arp_hdr->ar_op));

            return -1;
        }
        pktinfo->arp_h = arp_hdr;
        if (arp_hdr->ar_op == __constant_htons(ARPOP_REQUEST)) {
            pktinfo->pkt_type |= PIF_PKT_TYPE_ARP_REQ;
        } else {
            pktinfo->pkt_type |= PIF_PKT_TYPE_ARP_RSP;
        }
    } else if (ether_type == __constant_htons(ETH_P_IP)) {
        struct iphdr *ip_hdr = (struct iphdr *)p;
        pktinfo->ip_h = ip_hdr;
        p += sizeof(struct iphdr);
        if (ip_hdr->protocol == IPPROTO_UDP) {
            struct udphdr *udp_hdr = (struct udphdr *)p;
            int udp_len = __constant_ntohs(udp_hdr->len);
            if (udp_len > sizeof(struct udphdr)) {
                p += sizeof(struct udphdr);
                pktinfo->dhcp_h = (struct dhcp_pkt *)p;
                if (udp_hdr->source == __constant_htons(UDP_PORT_DHCP_SRV) ||
                    udp_hdr->source == __constant_htons(UDP_PORT_DHCP_CLNT)) {
                    unsigned char *opt = NULL;
                    int opt_len = udp_len - sizeof(struct udphdr) - sizeof(struct dhcp_pkt);
                    if (opt_len <= 0) {
                        rks_dbg(RKS_DEBUG_PIF, RKS_KERN_ERR, "[%s]:%d receiving DHCP without options \n", __func__, __LINE__);
                        return 0;
                    }
                    pktinfo->dhcp_h = (struct dhcp_pkt *)p;
                    opt = pktinfo->dhcp_h->options;
                    while(opt_len > 0) {
                        if (opt[0] == 0xff) {
                            break;
                        }
                        if ((opt_len < 2) || (opt[1] == 0)) {
                            rks_dbg(RKS_DEBUG_PIF, RKS_KERN_ERR, "[%s]:%d receiving DHCP with incomplete options \n", __func__, __LINE__);
                            break;
                        }
                        if (udp_hdr->source == __constant_htons(UDP_PORT_DHCP_SRV)) {
                            if (opt[0] == 0x35) {
                                if (opt[2] == 0x06) {
                                    pktinfo->pkt_type |= PIF_PKT_TYPE_DHCP_RSP_NAK;
                                    rks_dbg(RKS_DEBUG_PIF, RKS_KERN_INFO,"%s: Response DHCP NAK\n", __func__);
                                } else if (opt[2] == 0x05) {
                                    pktinfo->pkt_type |= PIF_PKT_TYPE_DHCP_RSP;
                                    rks_dbg(RKS_DEBUG_PIF, RKS_KERN_INFO,"%s: Response DHCP ACK\n", __func__);
                                }
                                break;
                            }
                        }
                        if (udp_hdr->source == __constant_htons(UDP_PORT_DHCP_CLNT)) {
                            if (opt[0] == 0x32) {
                                pktinfo->ip = &opt[2];
                                pktinfo->pkt_type |= PIF_PKT_TYPE_DHCP_REQ;
                                rks_dbg(RKS_DEBUG_PIF, RKS_KERN_INFO,"%s: Request DHCP NAK\n", __func__);
                                break;
                            }
                        }
                        opt += opt[1] + 2;
                        opt_len -= opt[1]+2;
                    }
                }
            }
        }
    } else if (ether_type == __constant_htons(ETH_P_IPV6)) {
        struct ipv6hdr *ipv6_hdr = (struct ipv6hdr *)p;
        pktinfo->ipv6_h = ipv6_hdr;
        p += sizeof(struct ipv6hdr);
        if (ipv6_hdr->nexthdr == NEXTHDR_ICMP) {
            struct icmpv6_pkt *icmpv6 = (struct icmpv6_pkt *)p;
            pktinfo->icmpv6_h = icmpv6;
            if (icmpv6->hdr.icmp6_type == ICMPV6_TYPE_NS) {
                pktinfo->pkt_type |= PIF_PKT_TYPE_ICMP6_NS;
            } else if (icmpv6->hdr.icmp6_type == ICMPV6_TYPE_NA) {
                pktinfo->pkt_type |= PIF_PKT_TYPE_ICMP6_NA;
            } else if (icmpv6->hdr.icmp6_type == ICMPV6_TYPE_RA) {
                pktinfo->pkt_type |= PIF_PKT_TYPE_ICMP6_RA;
            }
        }
    }

    return 0;
}


int br_ipv6_rcache_insert(struct net_bridge *br, struct net_bridge_port *br_port,
             unsigned short vlan, struct ra_cache_info *r_info) {
    rks_dbg(RKS_DEBUG_PIF, RKS_KERN_DEBUG, "IPv6 - RA received at [%s, %s] from %pM/%pI6c, Payload Len = %u bytes, vlan = %u, Pref = %u. ptr = %p\n", br->dev->name, br_port->dev->name,
       r_info->router_mac, r_info->router_ip6addr, r_info->len_ra_opt, vlan, r_info->pref, r_info->ra_opt);
    if((r_info->len_ra_opt > RA_OPT_MAX_BUF_SIZE)|| (r_info->len_ra_opt<=0)) {
       rks_dbg(RKS_DEBUG_PIF, RKS_KERN_ERR, "IPv6 - ERROR!!!!! Unsupported RA Opt len %d bytes\n", r_info->len_ra_opt);
       return -1;
    }

    br_ipv6db_insert(br, r_info->router_ip6addr, vlan, r_info->router_mac, br_port);
    return 0;
}


int get_ra_prefix_len(int vlan, struct ra_cache_info *ra_info, unsigned short *prefix_len)
{
	struct prefix_info opt3[MAX_IPV6_PREFIXES] = {{0}};
	struct ndisc_options ndopts;
	struct nd_opt_hdr *p;
	int cnt = 0;
	uint8_t ra_size = 0, optlen = 0;

	ra_size = sizeof(struct ra_msg) - sizeof(struct icmpv6_pkt_ra);
	__u8 *opt = ra_info->ra_opt + ra_size;
	optlen = ra_info->len_ra_opt - ra_size;;
	rks_ndisc_parse_options(opt, optlen, &ndopts);

	if (ndopts.nd_opts_pi) {
		for (p = ndopts.nd_opts_pi; p; p = rks_ndisc_next_option(p, ndopts.nd_opts_pi_end), cnt++) {
			memcpy(&(opt3[cnt]), p, sizeof(struct prefix_info));

			*prefix_len = opt3[cnt].prefix_len;
		}
	} else {

	}
	return (*prefix_len) ? 1 : 0;
}

int br_pif_if_ingress_filter(struct sk_buff *skb, unsigned int is_vap_bridge)
{
    int index = -1;
    struct net_bridge_port *in_p = NULL;
    if(unlikely(!skb->input_dev)){
        skb_dump(skb);
        dump_stack();
    }

    in_p = br_port_get_rcu(skb->input_dev);
    if(in_p && (in_p->pif_ipv6type & (BR_PIF_IPV6_TYPE_ND_PROXY|BR_PIF_IPV6_TYPE_RSRA_GUARD))) {
        struct ethhdr   *eth = eth_hdr(skb);
        unsigned short  ether_type;
        unsigned char   *p = NULL;
        unsigned int    len = skb->len;

        if (eth->h_proto == __constant_htons(ETH_P_8021Q)) {
            ether_type = ((struct vlan_ethhdr *)eth_hdr(skb))->h_vlan_encapsulated_proto;
            p = (unsigned char *)(skb_mac_header(skb) + sizeof(struct vlan_ethhdr));
            len -= (skb_mac_header(skb) + sizeof(struct vlan_ethhdr)) - skb->data;
        } else {
            ether_type = eth->h_proto;
            p = (unsigned char *)(skb_mac_header(skb) + sizeof(struct ethhdr));
            len -= (skb_mac_header(skb) + sizeof(struct ethhdr)) - skb->data;
        }

        if (ether_type == __constant_htons(ETH_P_IPV6)) {
            struct ipv6hdr *ipv6_hdr = (struct ipv6hdr *)p;
            p += sizeof(struct ipv6hdr);
            if (ipv6_hdr->nexthdr == NEXTHDR_ICMP) {
                struct icmpv6_pkt *icmpv6 = (struct icmpv6_pkt *)p;
                if(is_vap_bridge) {
                    if(in_p->pif_ipv6type & BR_PIF_IPV6_TYPE_ND_PROXY) {
                        if(icmpv6->hdr.icmp6_type == ICMPV6_TYPE_NS || icmpv6->hdr.icmp6_type == ICMPV6_TYPE_NA) {
                            return 1;
                        }
                    }
                    if (in_p->pif_ipv6type & BR_PIF_IPV6_TYPE_RSRA_GUARD) {
                        if(icmpv6->hdr.icmp6_type == ICMPV6_TYPE_RS || icmpv6->hdr.icmp6_type == ICMPV6_TYPE_RA) {
                            return 1;
                        }
                    }
                } else {
                    if((in_p->pif_ipv6type & BR_PIF_IPV6_TYPE_RSRA_GUARD) && (icmpv6->hdr.icmp6_type == ICMPV6_TYPE_RA)) {
                        if((index = get_idx_from_wlanname(in_p->dev->name)) != -1) {
                            g_pif_ndp_counter[index].rag++;
                        }
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

int br_pif_inspection(struct sk_buff *skb, unsigned int vlan)
{
    int rc = 0;
    struct pkt_info pktinfo = { skb, 0, NULL, NULL, NULL,
                                NULL, NULL, NULL, NULL, NULL,
                                vlan, NULL, NULL};

    if(unlikely(!skb->input_dev)){

        skb_dump(skb);
        dump_stack();
        return 0;
    }

    if (unlikely(!skb->dev)) {
        return 0;
    }

    pktinfo.br = br_port_get_rcu(skb->dev)->br;
    pktinfo.in_p = br_port_get_rcu(skb->input_dev);

    if (br_pif_parser(skb, &pktinfo)) {
        return 0;
    }


    if (pktinfo.pkt_type & (PIF_PKT_TYPE_ARP_REQ|PIF_PKT_TYPE_ARP_RSP)) {
        rks_dbg(RKS_DEBUG_PIF, RKS_KERN_INFO,"[%s]: Processing ARP request or response ...\n", __func__);
        rc = br_pif_proc_arp(&pktinfo, skb);
    } else if (pktinfo.pkt_type & (PIF_PKT_TYPE_ICMP6_NS|PIF_PKT_TYPE_ICMP6_NA)) {
        rks_dbg(RKS_DEBUG_PIF, RKS_KERN_INFO,"[%s]: Processing ICMPv6 NS or NA ...\n", __func__);
        rc = br_pif_proc_icmpv6_ns(&pktinfo);
    } else if (pktinfo.pkt_type & PIF_PKT_TYPE_ICMP6_RA) {
        rks_dbg(RKS_DEBUG_PIF, RKS_KERN_INFO,"[%s]: Processing ICMPv6 RA ...\n", __func__);
        br_pif_ipv6db_upd(&pktinfo);
    } else if (pktinfo.pkt_type & (PIF_PKT_TYPE_DHCP_RSP|PIF_PKT_TYPE_DHCP_RSP_NAK|PIF_PKT_TYPE_DHCP_REQ)) {
        rks_dbg(RKS_DEBUG_PIF, RKS_KERN_INFO,"[%s]: Processing DHCP ...\n", __func__);
        br_pif_ipdb_upd(&pktinfo);
    }

    return rc;
}



int br_pif_filter(const struct sk_buff *skb, const struct net_bridge_port *port)
{

    return 0;
}



int br_set_pif (struct net_bridge *br, int pif)
{
    spin_lock_bh(&br->lock);
    br->is_pif_enabled = pif;

    if (pif) {
        mod_timer(&br->ipdb_gc_timer, jiffies + HZ/10);
        mod_timer(&br->ipv6db_gc_timer, jiffies + HZ/10);
    } else {
        del_timer_sync(&br->ipdb_gc_timer);
        del_timer_sync(&br->ipv6db_gc_timer);
        br_ipdb_clear(br);
        br_ipv6db_clear(br);
    }
    spin_unlock_bh(&br->lock);
    return 0;
}



int br_set_pif_if_type(struct net_bridge *br, int ifindex ,int if_type)
{
    struct net_device *dev;
    struct net_bridge_port *p;


    dev = dev_get_by_index(dev_net(br->dev), ifindex);

    if (dev == NULL)
        return -EINVAL;

    p = br_port_get_rcu(dev);
    if (p == NULL) {
        dev_put(dev);
        return -EINVAL;
    }

    if (p->pif_iftype != if_type) {
        spin_lock_bh(&br->lock);
        if (if_type == BR_PIF_IFTYPE_NONE) {
            br_ipdb_delete_by_port(br, ifindex);
            br_ipv6db_delete_by_port(br, ifindex);
        }
        p->pif_iftype = if_type;
        spin_unlock_bh(&br->lock);
    }

    dev_put(dev);
    return 0;

}



int br_set_pif_if_learning(struct net_bridge *br, int ifindex ,int learning)
{
    struct net_device *dev;
    struct net_bridge_port *p;


    dev = dev_get_by_index(dev_net(br->dev), ifindex);


    if (dev == NULL)
        return -EINVAL;

    p = br_port_get_rcu(dev);
    if (p == NULL) {
        dev_put(dev);
        return -EINVAL;
    }

    if (p->pif_learning != learning) {
        spin_lock_bh(&br->lock);
        p->pif_learning = learning;
        spin_unlock_bh(&br->lock);
    }

    dev_put(dev);
    return 0;

}



int br_set_pif_directed_arp(struct net_bridge *br, int directed_arp)
{
    spin_lock_bh(&br->lock);
    br->pif_directed_arp = directed_arp;
    spin_unlock_bh(&br->lock);
    return 0;
}



int br_set_pif_proc_uplink(struct net_bridge *br, int proc_uplink)
{
    if (br->is_pif_proc_uplink != proc_uplink) {
        spin_lock_bh(&br->lock);
        if (!proc_uplink) {
            br_ipdb_delete_by_ifmode(br, BR_PIF_IFMODE_UPLINK_PORT);
            br_ipv6db_delete_by_ifmode(br, BR_PIF_IFMODE_UPLINK_PORT);
        }
        br->is_pif_proc_uplink =proc_uplink;
        spin_unlock_bh(&br->lock);
    }
    return 0;
}

int ra_proxy_enable(struct net_bridge *br)
{
    int idx = get_idx_from_brname(br->dev->name);
    if(idx < 0) return 0;
    return atomic_read(&ra_proxy_enable_cnt[idx]);
}
EXPORT_SYMBOL(ra_proxy_enable);


void pif_ipv6_ctrl_feature_reset(struct net_bridge_port *p, int mode)
{
    int index = -1;

    if((index = get_idx_from_wlanname(p->dev->name)) != -1) {
        if( mode & BR_PIF_IPV6_TYPE_MAX )
            memset(&g_pif_ndp_counter[index], 0, sizeof(struct __ndp_counters));
        else if( mode & BR_PIF_IPV6_TYPE_ND_PROXY )
            g_pif_ndp_counter[index].ndp = 0;
        else if( mode & BR_PIF_IPV6_TYPE_NS_SUPPRESS )
            g_pif_ndp_counter[index].nss = 0;
        else if( mode & BR_PIF_IPV6_TYPE_RA_PROXY )
            g_pif_ndp_counter[index].rap = 0;
        else if( mode & BR_PIF_IPV6_TYPE_RSRA_GUARD ) {
            g_pif_ndp_counter[index].rsg = 0;
            g_pif_ndp_counter[index].rag = 0;
        } else if( mode & BR_PIF_IPV6_TYPE_RA_THROTTLE ) {
            g_pif_ndp_counter[index].rat = 0;
            g_pif_ndp_counter[index].raa = 0;
        }
    }
}


int pif_ipv6_set_ctrl_feature(struct net_bridge *br, u32 ifindex, u32 type, u32 mode)
{
    struct net_device *dev;
    struct net_bridge_port *p;
    int tmp = 0, idx = -1;
    int rc = 0;
    int portindex = -1;
    if(type <= BR_PIF_IPV6_TYPE_MIN || type > BR_PIF_IPV6_TYPE_MAX) {
        return -EINVAL;
    }

    dev = dev_get_by_index(dev_net(br->dev), ifindex);

    if (dev == NULL)
        return -EINVAL;


    p = br_port_get_rcu(dev);
    if (p == NULL) {
        dev_put(dev);
        return -EINVAL;
    }

    pif_ipv6_ctrl_feature_reset(p, type);
    if( type == BR_PIF_IPV6_TYPE_MAX || mode == 2 )
        return rc;

    if(type == BR_PIF_IPV6_TYPE_RA_PROXY) {
        idx = get_idx_from_brname(br->dev->name);
        if(idx < 0) {
            dev_put(dev);
            return -EINVAL;
        }
        tmp = (p->pif_ipv6type & BR_PIF_IPV6_TYPE_RA_PROXY)?1:0;
        if (mode != tmp) {
            if(mode)
                atomic_inc(&ra_proxy_enable_cnt[idx]);
            else
                atomic_dec(&ra_proxy_enable_cnt[idx]);
        }
    }
    portindex = get_idx_from_wlanname(p->dev->name);
    spin_lock_bh(&br->lock);
    if(mode)
        p->pif_ipv6type |= type;
    else
        p->pif_ipv6type &= ~type;


    if( !(p->pif_ipv6type & BR_PIF_IPV6_TYPE_ND_PROXY) && !(p->pif_ipv6type & BR_PIF_IPV6_TYPE_NS_SUPPRESS) ) {
        br_ipv6db_delete_by_port(br, ifindex);
    }

    if((type & BR_PIF_IPV6_TYPE_RA_THROTTLE_PARAM) && (portindex != -1)) {
        union throttle_params *params = &mode;
        p->pif_ra_throttle_count    = params->u16_params.num_ra_allowed;
        atomic_set(&g_rat_cfg[portindex].ra_throttle_allow_cnt, params->u16_params.num_ra_allowed);
        p->pif_ra_throttle_interval = params->u16_params.interval;
        g_rat_cfg[portindex].last_refresh_timestamp = jiffies;
    }

    spin_unlock_bh(&br->lock);

    if((type & BR_PIF_IPV6_TYPE_RA_THROTTLE_PARAM) && mode) {
        mod_timer(&br->ipv6_ra_throttle_timer, jiffies + HZ*60);
    }

    if((type & BR_PIF_IPV6_TYPE_RA_PROXY) && mode) {
        mod_timer(&br->ipv6db_gc_timer, jiffies + HZ/10);
    }

    dev_put(dev);

    return rc;
}


int pif_ipv6_get_filter(struct net_bridge *br, void __user *userdata)
{
    int ret = 0, index = -1;

    if( -1 == (index = get_idx_from_brname(br->dev->name)) ) {
        return -EINVAL;
    }

    if (copy_to_user(userdata, &(ipv6_filter[index].counter), sizeof(unsigned long long))) {
        ret = -EFAULT;
    }

    return ret;
}

int pif_ipv6_get_ra_prefix(struct net_bridge *br, u16 vlan, void __user *userdata)
{
	struct ra_cache_info ra_info[MAX_ROUTER_PER_VLAN]  = {{0}};
	int num_routers = g_num_routers;
	int ret = 0;
	unsigned short prefix_len = 0;

	if((ret = br_ipv6db_find_router_by_vlan(br, vlan, num_routers, &ra_info))>0) {

		if(get_ra_prefix_len(vlan, &ra_info[0], &prefix_len)) {

			copy_to_user(userdata, &prefix_len, sizeof(prefix_len));
		} else {

			ret = -EFAULT;
		}
	} else {
		ret = -EFAULT;
	}
	return ret;
}

int pif_ipv6_get_ctrl_feature(struct net_bridge *br, u32 ifindex, void __user *userdata)
{
    int ret = 0;
    struct __ndp_counters counters = {0};
    struct net_bridge_port *p;
    struct net_device *dev;
    int index = -1;


    dev = dev_get_by_index(dev_net(br->dev), ifindex);

    if (dev == NULL)
        return -EINVAL;

    p = br_port_get_rcu(dev);
    if (p == NULL) {
        dev_put(dev);
        return -EINVAL;
    }

    if((index = get_idx_from_wlanname(p->dev->name)) != -1) {
        g_pif_ndp_counter[index].rat_timestamp = (jiffies - g_rat_cfg[index].last_refresh_timestamp)/HZ;
        rcu_read_lock();
        memcpy(&counters, &g_pif_ndp_counter[index], sizeof(struct __ndp_counters));
        rcu_read_unlock();
    }
    pr_info("IPv6 - %s: Iface %s counters - ndp=%llu, nss=%llu, rap=%llu, rs=%llu, ra=%llu\n", __func__, p->dev->name,
        counters.ndp, counters.nss, counters.rap, counters.rsg, counters.rag );

    if (copy_to_user(userdata, &counters, sizeof(struct __ndp_counters))) {
        ret = -EFAULT;
    }

    dev_put(dev);

    return ret;
}

int pif_ipv6_set_filter(struct net_bridge *br, u16 filter_type)
{
    int rc = 0, index = 0;

    switch(filter_type) {

        case BR_PIF_IPV6_TYPE_MIN:
        case BR_PIF_IPV6_FILTER_ALL:
        case BR_PIF_IPV6_FILTER_MCAST:
            if( -1 == (index = get_idx_from_brname(br->dev->name)) ) {
                return -EINVAL;
            }

            spin_lock_bh(&ipv6_filter[index].lock);
            ipv6_filter[index].type = filter_type;
            ipv6_filter[index].counter = 0;
            spin_unlock_bh(&ipv6_filter[index].lock);
            break;
        default:
            rc = -EINVAL;
    }

    return rc;
}


int br_set_pif_if_mode(struct net_bridge *br, int ifindex ,int if_mode)
{

    struct net_device *dev;
    struct net_bridge_port *p;

    if (if_mode < BR_PIF_IFMODE_MIN || if_mode > BR_PIF_IFMODE_MAX)
        return -EINVAL;


    dev = dev_get_by_index(dev_net(br->dev), ifindex);

    if (dev == NULL)
        return -EINVAL;

    p = br_port_get_rcu(dev);
    if (p == NULL) {
        dev_put(dev);
        return -EINVAL;
    }

    spin_lock_bh(&br->lock);
    p->pif_ifmode = if_mode;
    spin_unlock_bh(&br->lock);

    dev_put(dev);
    return 0;

}


int br_set_pif_db(struct net_bridge *br, int action)
{
    if (action < 0 || action > 1) {
        return -EINVAL;
    }

    spin_lock_bh(&br->lock);
    db_update = action;
    spin_unlock_bh(&br->lock);
    return 0;
}


int br_set_pif_rate_limit(struct net_bridge *br, int limit)
{
    if (limit == PIF_TOKEN_MAX_DROPALL) {
    } else if (limit < 0 || limit > 3000) {
        return -EINVAL;
    }

    spin_lock_bh(&br->lock);
    br->pif_token_max = limit;
    br->pif_token_count = 0;
    spin_unlock_bh(&br->lock);
    return 0;
}



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

}

int add_ipv6db_entry(struct net_bridge *br, struct req_pif_update_ipdb *ipdb)
{
    struct net_device *dev;
    struct net_bridge_port *p;
    int ifindex = ipdb->ifindex;
    struct in6_addr *ipv6addr = &(ipdb->ipaddr.v6);
    unsigned short vlan_id = ipdb->vlanID;
    unsigned char *macaddr = ipdb->macaddr;


    dev = dev_get_by_index(dev_net(br->dev), ifindex);

    if (dev == NULL)
        return -EINVAL;

    p = br_port_get_rcu(dev);
    if (p == NULL) {
        return -EINVAL;
    }

    if (vlan_id == 0){
        return -EINVAL ;
    }

    if (p->pif_ipv6type & BR_PIF_IPV6_TYPE_ND_PROXY || p->pif_iftype & BR_PIF_IFTYPE_PROXYARP) {
        if (!ipv6_addr_is_zero(ipv6addr)) {
            br_ipv6db_insert(br, ipv6addr, vlan_id, macaddr, p);
        } else {
        }
    }
    return 0;
}


static int __br_pif_ioctl(struct net_bridge *br, unsigned int cmd, unsigned long p1, unsigned long p2, unsigned long p3)
{
    switch (cmd) {
        case PIF_IOCTL_SET_PIF:
            return br_set_pif(br, (int)p1);

        case PIF_IOCTL_SET_IF_TYPE:
            return br_set_pif_if_type(br, (int)p1, (int)p2);

        case PIF_IOCTL_SET_DIRECTED_ARP:
            return br_set_pif_directed_arp(br, (int)p1);

        case PIF_IOCTL_SET_IF_MODE:
            return br_set_pif_if_mode(br, (int)p1, (int)p2);

        case PIF_IOCTL_SET_RATE_LIMIT:
            return br_set_pif_rate_limit(br, (int)p1);

        case PIF_IOCTL_SET_PROC_UPLINK:
            return br_set_pif_proc_uplink(br, (int)p1);

        case PIF_IOCTL_SET_IF_LEARNING:
            return br_set_pif_if_learning(br, (int)p1, (int)p2);

        case PIF_IOCTL_SET_IP_AGEING:
            return br_set_ip_ageing(br, (int)p1);

        case PIF_IOCTL_GET_IPDB_ENTRIES:
            return get_ipdb_entries(br, (void __user *)p1, p2, p3);

        case PIF_IOCTL_GET_IPV6DB_ENTRIES:
            return get_ipv6db_entries(br, (void __user *)p1, p2, p3);

        case PIF_IOCTL_DEL_IPDB_ENTRY_BY_MAC: {
            unsigned char mac_addr[ETH_ALEN];
            if (copy_from_user(mac_addr, (void __user *)p2, ETH_ALEN))
                return -EFAULT;
            rks_dbg(RKS_DEBUG_PIF, RKS_KERN_ERR, "[%s]: IOCTL del %pM IPDB\n", __func__, mac_addr);
            return br_ipdb_delete_by_mac(br, (int)p1, mac_addr);
        }

        case PIF_IOCTL_DEL_IPV6DB_ENTRY_BY_MAC: {
            unsigned char mac_addr[ETH_ALEN];
            if (copy_from_user(mac_addr, (void __user *)p2, ETH_ALEN))
                return -EFAULT;
            return br_ipv6db_delete_by_mac(br, (int)p1, mac_addr);
        }

        default:
            return -EOPNOTSUPP;
    }
}


int br_pif_ioctl(unsigned int cmd, unsigned int length, void *data)
{
    struct req_pif_ioctl *req = (typeof(req))data;
    struct net_device *dev;
    struct net_bridge *br;
    int rv;

    if (NULL == (dev = dev_get_by_name(
                                       &init_net,
                                       req->brname))) {
        return -ENODEV;
    }

    if (!(dev->priv_flags & IFF_EBRIDGE)) {
	dev_put(dev);
	return -ENODEV;
    }

    br = netdev_priv(dev);
    rv = __br_pif_ioctl(br, cmd, req->args[0], req->args[1], req->args[2]);
    dev_put(dev);
    return rv;
}


void br_ipv6_throttle_reset(unsigned long _data)
{
    int i = 0, index = -1, br_timer_keep_running = 0;
    struct net_bridge *br = (struct net_bridge *)_data;
    struct net_bridge_port *p;
    if (br && !list_empty(&br->port_list)) {
        list_for_each_entry(p, &br->port_list, list) {
            i++;
            if(p->pif_ipv6type & BR_PIF_IPV6_TYPE_RA_THROTTLE) {
                br_timer_keep_running = 1;
                index = get_idx_from_wlanname(p->dev->name);
                if((index != -1) && is_timedout(g_rat_cfg[index].last_refresh_timestamp-HZ,
                                  p->pif_ra_throttle_interval*60*HZ )) {
                    rks_dbg(RKS_DEBUG_PIF, RKS_KERN_INFO, "IPv6 - RA Throttle - Port %s configurred interval %u minutes...reseting config\n",
                          p->dev->name, p->pif_ra_throttle_interval);
                    atomic_set(&g_rat_cfg[index].ra_throttle_allow_cnt, p->pif_ra_throttle_count);
                    g_rat_cfg[index].last_refresh_timestamp = jiffies;
                    pif_ipv6_ctrl_feature_reset(p, BR_PIF_IPV6_TYPE_RA_THROTTLE);   // Reset counters as well
                }
            }
        }
    }
    if(br_timer_keep_running)
         mod_timer(&br->ipv6_ra_throttle_timer, rks_round_jiffies(jiffies + HZ*60));
}


static int pif_netdev_unregister_handler(struct net_device *dev)
{
    struct net_bridge *br;

    if (!is_bridge_device(dev))
        return NOTIFY_DONE;

    br = netdev_priv(dev);
    br_ipdb_clear(br);
    del_timer_sync(&br->ipdb_gc_timer);
    br_ipv6db_clear(br);
    del_timer_sync(&br->ipv6db_gc_timer);
    return NOTIFY_OK;
}


static int pif_netdev_br_leave_handler(struct net_device *dev)
{
    struct net_bridge_port *p;
    struct net_bridge *br;

    if ((p = br_port_get_rcu(dev)) == NULL)
        return NOTIFY_DONE;

    br = p->br;
    br_ipdb_delete_by_port(br, dev->ifindex);
    br_ipv6db_delete_by_port(br, dev->ifindex);
    return NOTIFY_OK;
}


static int pif_dev_event(struct notifier_block *this, unsigned long event, void *ptr)
{
    struct net_device *dev = RKSV_NETDEV_NOTIFIER_INFO_TO_DEV(ptr);

    switch (event) {
    case NETDEV_UNREGISTER:
        return pif_netdev_unregister_handler(dev);
    case NETDEV_BR_LEAVE:
        return pif_netdev_br_leave_handler(dev);
    default:
        return NOTIFY_DONE;
    }
}


static struct notifier_block pif_notifier = {
    .notifier_call = pif_dev_event
};


int pif_parse_ra_rs_msg(struct sk_buff *skb, struct ra_rs_parsed *ra_rs_info)
{
    int rc = 0;


    return !rc;

}