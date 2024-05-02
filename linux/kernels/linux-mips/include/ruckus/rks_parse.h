/*
 * Copyright (c), 2005-2013, Ruckus Wireless, Inc.
 */

#ifndef __RKS_PARSE_H__
#define __RKS_PARSE_H__

#include <linux/types.h>
#include <linux/if_ether.h>
#include <linux/in6.h>
#include <net/addrconf.h>
#include <linux/skbuff.h>

#define MAX_IPV6_PREFIXES 16

#define PARSE_MAC_ADDR        0x00000001
#define PARSE_L3_ADDR         0x00000002
#define PARSE_L4_L5_APPTYPE   0x00000004
#define STRICT_CHECK_HDR_L3   0x00000010
#define STRICT_CHECK_HDR_L4   0x00000020
#define CHECK_CHECKSUM_L3     0x00000040
#define CHECK_CHECKSUM_L4     0x00000080
#define PARSE_ADDR       (PARSE_MAC_ADDR | PARSE_L3_ADDR)
#define STRICT_CHECK_HDR (STRICT_CHECK_HDR_L3 | STRICT_CHECK_HDR_L4)
#define CHECK_CHECKSUM   (CHECK_CHECKSUM_L3 | CHECK_CHECKSUM_L4)


#define L2_MAC_ADDR_PARSED   0x00000001
#define L2_NO_L2             0x00000002

#define L2_HAS_VLAN          0x00000010
#define L2_HAS_PPPOES        0x00000020

#define L2_ADDR_SRC_BCAST    0x00000100
#define L2_ADDR_SRC_MCAST    0x00000200
#define L2_ADDR_SRC_ZERO     0x00000400
#define L2_ADDR_DST_BCAST    0x00001000
#define L2_ADDR_DST_MCAST    0x00002000
#define L2_ADDR_DST_ZERO     0x00004000

#define L2_ADDR_DST_IPMCAST  0x00010000
#define L2_ADDR_DST_STP      0x00020000

#define L2_ERR_PKT_LEN       0x01000000
#define L2_ERR               (L2_ERR_PKT_LEN)


#define L3_HDR_CHECKED       0x00000001
#define L3_CHECKSUM_CHECKED  0x00000002
#define L3_ADDR_PARSED       0x00000004

#define L3_HAS_AH            0x00000008

#define L3_FRAG_1ST          0x00000010
#define L3_FRAG_MID          0x00000020
#define L3_FRAG_LAST         0x00000040
#define L3_FRAG              (L3_FRAG_1ST | L3_FRAG_MID | L3_FRAG_LAST)

#define L3_ADDR_DST_BCAST             0x00000100
#define L3_ADDR_DST_MCAST             0x00000200

#define L3_ADDR_DST_MCAST_224         0x00000400
#define L3_ADDR_DST_MCAST_BONJOUR     0x00001000
#define L3_ADDR_DST_MCAST_LLMNR       0x00002000
#define L3_ADDR_DST_MCAST_VOCERA      0x00004000
#define L3_ADDR_DST_MCAST_SPECTRALINK 0x00008000
#define L3_ADDR_DST_MCAST_UPNP        0x00010000
#define L3_ADDR_DST_MCAST_DHCPAGENT   0x00020000
#define L3_ADDR_DST_MCAST_KNOWN (L3_ADDR_DST_MCAST_224 | L3_ADDR_DST_MCAST_BONJOUR | L3_ADDR_DST_MCAST_LLMNR | \
                                 L3_ADDR_DST_MCAST_VOCERA | L3_ADDR_DST_MCAST_SPECTRALINK | L3_ADDR_DST_MCAST_UPNP | L3_ADDR_DST_MCAST_DHCPAGENT)

#define L3_ERR_PKT_LEN       0x01000000
#define L3_ERR_HDR_VER       0x02000000
#define L3_ERR_HDR_IHL       0x04000000
#define L3_ERR_HDR_TOT       0x08000000
#define L3_ERR_CHECKSUM      0x10000000
#define L3_ERR_HDR_ARP       0x20000000
#define L3_ERR               (L3_ERR_PKT_LEN | L3_ERR_HDR_VER | L3_ERR_HDR_IHL | L3_ERR_HDR_TOT | L3_ERR_CHECKSUM | L3_ERR_HDR_ARP)


#define L4_L5_APPTYPE_PARSED 0x00000001
#define L4_HDR_CHECKED       0x00000002
#define L4_CHECKSUM_CHECKED  0x00000004

#define L4_UNKNOWN_PROT      0x00000010
#define L4_NO_L4             0x00000020

#define L4_PKT_INCOMPLETE    0x00000100

#define L4_ERR_PKT_LEN       0x01000000
#define L4_ERR_UDP_LEN       0x02000000
#define L4_ERR_TCP_LEN       0x04000000
#define L4_ERR_CHECKSUM      0x08000000
#define L4_ERR               (L4_ERR_PKT_LEN | L4_ERR_UDP_LEN | L4_ERR_TCP_LEN | L4_ERR_CHECKSUM)
typedef enum rks_app_type {

    APP_UNINIT                 = 0,

    APP_ERROR                  = 1,


    APP_DHCP_REQ             = 10010,
    APP_DHCP_RSP             = 10020,
    APP_DNS_REQ              = 10030,
    APP_DNS_RSP              = 10040,

    APP_NETBIOS              = 10060,
    APP_MDNS                 = 10070,


    APP_ICMP_REPLY           = 20001,
    APP_ICMP_REQUEST         = 20002,

    APP_ICMPV6_REPLY         = 20003,
    APP_ICMPV6_REQUEST       = 20004,

    APP_MLD_QUERY            = 20010,
    APP_MLD_REPORT           = 20020,
    APP_MLD_DONE             = 20030,
    APP_MLDV2_REPORT         = 20040,
    APP_RS                   = 20060,
    APP_RA                   = 20070,
    APP_NS                   = 20080,
    APP_NA                   = 20090,

    APP_IGMP_MEMBERSHIP_REPORT = 30010,
    APP_IGMP_HOST_LEAVE        = 30020,


    APP_ARP_REQ                = 80010,
    APP_ARP_RSP                = 80020,

    APP_UNKNOWN                = -1,
} rks_app_t;

typedef struct rks_pkt_info_s {

    rks_app_t        app_type;
    u32              l2_flgs;
    u32              l3_flgs;
    u32              l4_flgs;
    union {
        u32          l3_prot_tos_l4_prot;
        struct {
        u16          l3_prot;
        u8           l3_tos;
        u8           l4_prot;
    };
    };
    union {
        u8           *l3_hdr;
        struct iphdr *iph;
        struct ipv6hdr *ip6h;
    };
    u8               *l4_hdr;


    union {
        u8           *l2_hdr;
        struct ethhdr *eth;
    };
    u32              l2_len;
    u32              l2_hdr_len;
    u32              l3_hdr_len;
    u32              l3_claim_len;
    u32              l4_hdr_len;
    u32              l4_claim_len;
    union {
        u32          l4_ports;
        struct {
        u16          l4_src_port;
        u16          l4_dst_port;
        };
    };



    u8               *l5_hdr;
} rks_pkt_info_t;

#define RKS_ADDR_BCAST             0xFFFFFFFF
#define RKS_ADDR_MCAST_START       0xE0000000
#define RKS_ADDR_MCAST_END         0xEFFFFFFF
#define RKS_ADDR_MCAST_224_START   0xE0000000
#define RKS_ADDR_MCAST_224_END     0xE00000FF


#define RKS_ADDR_MCAST_DHCPV6_AGENT {0xff, 0x02, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0, 0x2}



#define PORT_DNS_SVR        53
#define PORT_DHCP_SVR       67
#define PORT_DHCP_CLT       68
#define PORT_NETBIOS_NS     137
#define PORT_NETBIOS_DGM    138
#define PORT_BOOTPS_IPV6    547
#define PORT_BOOTPC_IPV6    546


#define PORT_MDNS            5353

#define DHCP_OPTION_DHCP_MSG_TYPE       53

#define DHCPv4_DISCOVER    1
#define DHCPv4_OFFER       2
#define DHCPv4_REQUEST     3
#define DHCPv4_ACK         5


#define DHCPv6_SOLICIT     1
#define DHCPv6_ADVERTISE   2
#define DHCPv6_REQUEST     3
#define DHCPv6_REPLY       7
static inline bool rks_is_zero_ether_addr(const u8 *addr)
 {

    return (*(const u16 *)(addr + 0) |
        *(const u16 *)(addr + 2) |
        *(const u16 *)(addr + 4)) == 0;

}

 static inline bool rks_is_broadcast_ether_addr(const u8 *addr)
 {
    return (*(const u16 *)(addr + 0) &
        *(const u16 *)(addr + 2) &
        *(const u16 *)(addr + 4)) == 0xffff;
 }

struct ra_rs_parsed {
    struct prefix_info opt3[MAX_IPV6_PREFIXES];
    u32 opt5_mtu;
    u8 opt1_source_lladdr[ETH_ALEN];
    u8 icmpv6_type;
    u8 opt3_len;
};

#endif
