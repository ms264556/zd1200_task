/*
 * Copyright 2015 Ruckus Wireless, Inc. All rights reserved.
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
#include <linux/if_ether.h>
#include <net/arp.h>
#include <net/ndisc.h>
#include <net/flow.h>
#include <net/ip6_route.h>
#include <net/route.h>
#include <ruckus/rks_utility.h>
#include <ruckus/rks_api.h>




void rks_dump_memory(void *mem, size_t len)
{
	size_t i;
	size_t line = (len >> 4);
	unsigned char *p;
	static char c[] = {

		'.',  '.',  '.',  '.',  '.',  '.',  '.',  '.',
		'.',  '.',  '.',  '.',  '.',  '.',  '.',  '.',
		'.',  '.',  '.',  '.',  '.',  '.',  '.',  '.',
		'.',  '.',  '.',  '.',  '.',  '.',  '.',  '.',


		0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
		0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
		0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
		0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
		0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
		0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
		0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
		0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
		0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
		0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
		0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
		0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E,


		                                          '.',
		'.',  '.',  '.',  '.',  '.',  '.',  '.',  '.',
		'.',  '.',  '.',  '.',  '.',  '.',  '.',  '.',
		'.',  '.',  '.',  '.',  '.',  '.',  '.',  '.',
		'.',  '.',  '.',  '.',  '.',  '.',  '.',  '.',
		'.',  '.',  '.',  '.',  '.',  '.',  '.',  '.',
		'.',  '.',  '.',  '.',  '.',  '.',  '.',  '.',
		'.',  '.',  '.',  '.',  '.',  '.',  '.',  '.',
		'.',  '.',  '.',  '.',  '.',  '.',  '.',  '.',
		'.',  '.',  '.',  '.',  '.',  '.',  '.',  '.',
		'.',  '.',  '.',  '.',  '.',  '.',  '.',  '.',
		'.',  '.',  '.',  '.',  '.',  '.',  '.',  '.',
		'.',  '.',  '.',  '.',  '.',  '.',  '.',  '.',
		'.',  '.',  '.',  '.',  '.',  '.',  '.',  '.',
		'.',  '.',  '.',  '.',  '.',  '.',  '.',  '.',
		'.',  '.',  '.',  '.',  '.',  '.',  '.',  '.',
		'.',  '.',  '.',  '.',  '.',  '.',  '.',  '.',
	};

	for (i = 0, p = mem; i < line; i++, p += 16) {
		printk("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x"
			"     %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
			p[ 0], p[ 1], p[ 2], p[ 3], p[ 4], p[ 5], p[ 6], p[ 7],
			p[ 8], p[ 9], p[10], p[11], p[12], p[13], p[14], p[15],
			c[p[ 0]], c[p[ 1]], c[p[ 2]], c[p[ 3]], c[p[ 4]], c[p[ 5]], c[p[ 6]], c[p[ 7]],
			c[p[ 8]], c[p[ 9]], c[p[10]], c[p[11]], c[p[12]], c[p[13]], c[p[14]], c[p[15]]);
	}

	len -= (line << 4);
	if (len > 0) {
		int  length = 0;
		char buffer[128];
		for (i = 0; i < len; i ++) {
			length += snprintf(buffer + length, sizeof(buffer) - length, "%02x ", p[i]);
		}
		memset(buffer + length, ' ', (16 - i) * 3 + 4);
		length += (16 - i) * 3 + 4;
		for (i = 0; i < len; i ++) {
			length += snprintf(buffer + length, sizeof(buffer) - length, "%c", c[p[i]]);
		}
		printk("%s\n", buffer);
	}
}
EXPORT_SYMBOL(rks_dump_memory);


int rks_get_nexthop(unsigned short family, const union union_in_addr *p_daddr, int query,
    struct dst_entry **p_dst, struct net_device **p_xmitdev, unsigned int *p_pmtu,
    union union_in_addr *p_saddr, union union_in_addr *p_nexthopaddr,
    __u8 *p_smac, __u8 *p_nexthopmac)
{
    int    rv = 0;
    union  union_in_addr saddr;
    __u8   nexthopmac[ETH_ALEN]= {0};
    struct net_device *xmitdev = NULL;
    struct neighbour  *neigh;
    struct dst_entry  *dst = NULL;
    struct in6_addr   *nexthop6 = NULL;
    __be32 nexthop = 0;
    unsigned int pmtu;

    if (family == AF_INET) {
        struct rtable *rt;


        struct flowi fl = {
            .nl_u = {
                .ip4_u = {
                    .daddr = p_daddr->v4.s_addr,
                }
            },
            .proto = IPPROTO_RAW,
        };
        if (ip_route_output_key(&init_net, &rt, &fl)) {
            printk(KERN_DEBUG "ip_route_output_key failed");
            goto quit;
        }
        dst = dst_clone(&rt->u.dst);

        rv++;

        dev_hold(dst->dev);
        xmitdev = dst->dev;
        nexthop = rt_nexthop(rt, p_daddr->v4.s_addr);

        saddr.v4.s_addr = rt->rt_src;

        pmtu = dst_mtu(dst);
        printk(KERN_DEBUG "Route found: xmitdev=%s, nexthop=%pI4, saddr=%pI4, pmtu=%u", xmitdev->name, &nexthop, &saddr.v4, pmtu);

        neigh = __neigh_lookup(&arp_tbl, &nexthop, xmitdev, 1);
        if (neigh) {
            if (neigh->nud_state & NUD_VALID) {
                read_lock_bh(&neigh->lock);
                memcpy(nexthopmac, neigh->ha, sizeof(nexthopmac));
                read_unlock_bh(&neigh->lock);
                printk(KERN_DEBUG "Neighbour found: nexthopmac=%pM", nexthopmac);
                neigh_release(neigh);
                neigh = NULL;

                rv++;
            } else {

            }
        }

        ip_rt_put(rt);
    } else {

        struct flowi fl = {
            .nl_u = {
                .ip6_u = {
                    .daddr = p_daddr->v6
                }
            }
        };
       dst = rks_ip6_dst_lookup_flow(NULL, &fl,  NULL);

        if (dst == NULL) {
            printk(KERN_DEBUG "ip6_dst_lookup_flow failed");
            goto quit;
        }
        rv++;

        dev_hold(dst->dev);
        xmitdev = dst->dev;
        nexthop6 = rt6_nexthop((struct rt6_info *)dst);

        saddr.v6 = fl.nl_u.ip6_u.saddr;

        pmtu = dst_mtu(dst);
        printk(KERN_DEBUG "Route found: xmitdev=%s, nexthop=%pI6, saddr=%pI6, pmtu=%u", xmitdev->name, nexthop6, &saddr.v6, pmtu);

        neigh = __neigh_lookup(&nd_tbl, nexthop6, xmitdev, 1);
        if (neigh) {
            if (neigh->nud_state & NUD_VALID) {
                read_lock_bh(&neigh->lock);
                memcpy(nexthopmac, neigh->ha, sizeof(nexthopmac));
                read_unlock_bh(&neigh->lock);
                printk(KERN_DEBUG "Neighbour found: nexthopmac=%pM", nexthopmac);
                neigh_release(neigh);
                neigh = NULL;

                rv++;
            } else {

            }
        }
    }

    if (neigh) {
        if (query) {
            if (family == AF_INET) {
                printk(KERN_DEBUG "Neighbour not found, sending arp request for %pI4 ...", &nexthop);
                arp_send(ARPOP_REQUEST, ETH_P_ARP, nexthop, xmitdev, saddr.v4.s_addr, NULL, xmitdev->dev_addr, NULL);
            } else {
                printk(KERN_DEBUG "Neighbour not found, sending icmpv6 ns for %pI6 ...", nexthop6);
                ndisc_send_ns(xmitdev, neigh, nexthop6, nexthop6, &saddr.v6);
            }
        }
        neigh_release(neigh);
        neigh = NULL;
        goto quit;
    }

quit:
    if (rv >= 1) {
        if (NULL != p_saddr) {
            *p_saddr = saddr;
        }
        if (NULL != p_nexthopaddr) {
            if (family == AF_INET) {
                p_nexthopaddr->v4.s_addr = nexthop;
            } else {
                p_nexthopaddr->v6 = *nexthop6;
            }
        }
        if (NULL != p_smac) {
            memcpy(p_smac,  xmitdev->dev_addr, ETH_ALEN);
        }
        if (NULL != p_dst) {
            *p_dst = dst;
        } else {
            dst_release(dst);
        }
        if (NULL != p_xmitdev) {
            *p_xmitdev = xmitdev;
        } else {
            dev_put(xmitdev);
        }
        if (NULL != p_pmtu) {
            *p_pmtu = pmtu;
        }
    }
    if (rv >= 2) {
        if (NULL != p_nexthopmac) {
            memcpy(p_nexthopmac, nexthopmac, sizeof(nexthopmac));
        }
    }
    return rv;
}
EXPORT_SYMBOL(rks_get_nexthop);
