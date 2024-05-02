/*
 * ndisc.c - ICMPv6 neighbour discovery command line tool
 * $Id: ndisc.c 657 2010-10-31 20:51:32Z remi $
 */

/*************************************************************************
 *  Copyright © 2004-2007 Rémi Denis-Courmont.                           *
 *  This program is free software: you can redistribute and/or modify    *
 *  it under the terms of the GNU General Public License as published by *
 *  the Free Software Foundation, versions 2 or 3 of the license.        *
 *                                                                       *
 *  This program is distributed in the hope that it will be useful,      *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 *  GNU General Public License for more details.                         *
 *                                                                       *
 *  You should have received a copy of the GNU General Public License    *
 *  along with this program. If not, see <http://www.gnu.org/licenses/>. *
 *************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <gettext.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h> /* div() */
#include <inttypes.h> /* uint8_t */
#include <limits.h> /* UINT_MAX */
#include <locale.h>
#include <stdbool.h>

#include <errno.h> /* EMFILE */
#include <sys/types.h>
#include <unistd.h> /* close() */
#include <time.h> /* clock_gettime() */
#include <poll.h> /* poll() */
#include <sys/socket.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <signal.h>

#include "gettime.h"

#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif

#include <netdb.h> /* getaddrinfo() */
#include <arpa/inet.h> /* inet_ntop() */
#include <net/if.h> /* if_nametoindex() */

#include <netinet/in.h>
#include <netinet/icmp6.h>
#ifdef RKS_PATCHES
#include <netinet/ip6.h>
#include <netinet/if_ether.h>
#include <netpacket/packet.h>
#endif


#ifndef IPV6_RECVHOPLIMIT
/* Using obsolete RFC 2292 instead of RFC 3542 */ 
# define IPV6_RECVHOPLIMIT IPV6_HOPLIMIT
#endif

#ifndef AI_IDN
# define AI_IDN 0
#endif

#ifdef RKS_PATCHES
static int ndisc_m = 0;
static int exitsignal_pid = 0 ;
static int exitsignal_success = SIGUSR1;
static int exitsignal_val = SIGUSR2; // default on failure
static char *outgoing_iface = NULL;
static char *latency_filename = NULL;
static unsigned int last = 0;
#ifdef SO_INIFINDEX
#define _IN_DEV_SUPPORT 1
#else
#undef _IN_DEV_SUPPORT
#endif
#endif

enum ndisc_flags
{
	NDISC_VERBOSE1=0x1,
	NDISC_VERBOSE2=0x2,
	NDISC_VERBOSE3=0x3,
	NDISC_VERBOSE =0x3,
	NDISC_NUMERIC =0x4,
	NDISC_SINGLE  =0x8,
};


static int
getipv6byname (const char *name, const char *ifname, int numeric,
               struct sockaddr_in6 *addr)
{
	struct addrinfo hints, *res;
	memset (&hints, 0, sizeof (hints));
	hints.ai_family = PF_INET6;
	hints.ai_socktype = SOCK_DGRAM; /* dummy */
	hints.ai_flags = numeric ? AI_NUMERICHOST : 0;

	int val = getaddrinfo (name, NULL, &hints, &res);
	if (val)
	{
		fprintf (stderr, _("%s: %s\n"), name, gai_strerror (val));
		return -1;
	}

	memcpy (addr, res->ai_addr, sizeof (struct sockaddr_in6));
	freeaddrinfo (res);

	val = if_nametoindex (ifname);
	if (val == 0)
	{
		perror (ifname);
		return -1;
	}
	addr->sin6_scope_id = val;

	return 0;
}


static inline int
sethoplimit (int fd, int value)
{
	return (setsockopt (fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
	                    &value, sizeof (value))
	     || setsockopt (fd, IPPROTO_IPV6, IPV6_UNICAST_HOPS,
	                    &value, sizeof (value))) ? -1 : 0;
}


static void
printmacaddress (const uint8_t *ptr, size_t len)
{
	while (len > 1)
	{
		printf ("%02X:", *ptr);
		ptr++;
		len--;
	}

	if (len == 1)
		printf ("%02X\n", *ptr);
}

#define NDISC6_CACHE_PATH "/tmp/ndisc6_cache_%s"
#define NDISC6_MAX_AGE 300

static int lookup_ndisc6_cache(struct in6_addr addr, struct ether_addr *mac, unsigned int* age) 
{
    char addr6[INET6_ADDRSTRLEN];
    char lookup_fn[100];
    char hwa[100];
    char timestamp_str[100];
    time_t timestamp = 0;
    time_t now = 0;
    FILE *fp;
    int rc = 1;

    inet_ntop(AF_INET6, &addr, addr6, INET6_ADDRSTRLEN);
    snprintf(lookup_fn,sizeof(lookup_fn),NDISC6_CACHE_PATH,addr6);

    fp = fopen(lookup_fn, "rt");

    if (!fp)
        return 0;

    if( fgets(hwa,sizeof(hwa),fp) == NULL || ether_aton_r(hwa, mac) == NULL ||
        fgets(timestamp_str,sizeof(timestamp_str),fp) == NULL || (timestamp=atol(timestamp_str)) == 0 )
    {
        rc = 0;
        timestamp = 0;
    }

    fclose(fp);

    now = time(NULL);
    *age = (now - timestamp);
    // ndisc6 cache aging is NDISC_MAX_AGE seconds
    if( *age > NDISC6_MAX_AGE ) {
        remove(lookup_fn);
        rc = 0;
    }

    return rc;
}

static int update_ndisc6_cache(struct in6_addr addr, unsigned char *mac) 
{
    char addr6[INET6_ADDRSTRLEN];
    char lookup_fn[100];
    char hwa[100];
    FILE *fp;

    inet_ntop(AF_INET6, &addr, addr6, INET6_ADDRSTRLEN);
    snprintf(lookup_fn,sizeof(lookup_fn),NDISC6_CACHE_PATH,addr6);

    remove(lookup_fn);
    fp = fopen(lookup_fn, "wt");

    if( !fp )
        return 0;

    ether_ntoa_r((struct ether_addr *)mac,hwa);

    fputs(hwa,fp);
    fprintf(fp,"\n%ld\n", time(NULL));

    fclose(fp);
    return 1;
}

#ifdef RKS_PATCHES
static inline unsigned short foldto16(unsigned long x)
{
	/* add up 16-bit for 17 bits */
	x = (x & 0xffff) + (x >> 16);
	/* add up carry.. */
	x = (x & 0xffff) + (x >> 16);
	return x;
}

static unsigned long do_csum(const unsigned char *buff, int len)
{
	int odd, count;
	unsigned long result = 0;

	if (len <= 0)
		goto out;

	odd = 1 & (unsigned long) buff;
	if (odd) {
		result = *buff << 8;
		len--;
		buff++;
	}
	count = len >> 1;	/* nr of 16-bit words.. */
	if (count) {
		if (2 & (unsigned long) buff) {
			result += *(unsigned short *) buff;
			count--;
			len -= 2;
			buff += 2;
		}
		count >>= 1;	/* nr of 32-bit words.. */
		if (count) {
			unsigned long carry = 0;
			do {
				unsigned long w = *(unsigned long *) buff;
				buff += 4;
				count--;
				result += carry;
				result += w;
				carry = (w > result);
			} while (count);
			result += carry;
			result = (result & 0xffff) + (result >> 16);
		}
		if (len & 2) {
			result += *(unsigned short *) buff;
			buff += 2;
		}
	}
	if (len & 1)
		result += *buff;
	result = foldto16(result);
	if (odd)
		result = ((result >> 8) & 0xff) | ((result & 0xff) << 8);


      out:
	return result;
}

/* computes the checksum of a memory block at buff, length len,
   and adds in "sum" (32-bit)  */
unsigned int csum_partial(const unsigned char *buff, int len, unsigned int sum)
{
	unsigned long long result = do_csum(buff, len);

	/* add in old sum, and carry.. */
	result += sum;
	/* 32+c bits -> 32 bits */
	result = (result & 0xffffffff) + (result >> 32);


	return result;
}

static unsigned int csum_fold( unsigned int sum )
{
 /* add the swapped two 16-bit halves of sum,
    a possible carry from adding the two 16-bit halves,
    will carry from the lower half into the upper half,
    giving us the correct sum in the upper half. */
 sum += (sum << 16) + (sum >> 16);
 return (~sum) >> 16;
}


static unsigned short int csum_ipv6_magic(struct in6_addr *saddr,
                                                     struct in6_addr *daddr,
                                                     __u16 len,
                                                     unsigned short proto,
                                                     unsigned int csum)
{

        int carry;
        __u32 ulen;
        __u32 uproto;

        csum += saddr->s6_addr32[0];
        carry = (csum < saddr->s6_addr32[0]);
        csum += carry;

        csum += saddr->s6_addr32[1];
        carry = (csum < saddr->s6_addr32[1]);
        csum += carry;

        csum += saddr->s6_addr32[2];
        carry = (csum < saddr->s6_addr32[2]);
        csum += carry;

        csum += saddr->s6_addr32[3];
        carry = (csum < saddr->s6_addr32[3]);
        csum += carry;

        csum += daddr->s6_addr32[0];
        carry = (csum < daddr->s6_addr32[0]);
        csum += carry;

        csum += daddr->s6_addr32[1];
        carry = (csum < daddr->s6_addr32[1]);
        csum += carry;

        csum += daddr->s6_addr32[2];
        carry = (csum < daddr->s6_addr32[2]);
        csum += carry;

        csum += daddr->s6_addr32[3];
        carry = (csum < daddr->s6_addr32[3]);
        csum += carry;

        ulen = htonl((__u32) len);
        csum += ulen;
        carry = (csum < ulen);
        csum += carry;

        uproto = htonl(proto);
        csum += uproto;
        carry = (csum < uproto);
        csum += carry;

        return csum_fold(csum);
}

static int getipv6address(char *ifname, struct in6_addr *addr)
{
	FILE  *fh;
	char  iface[IFNAMSIZ + 1];
	int scope, fcnt, ret = 0;
	struct in6_addr address;
	char pcmd[64];

	snprintf(pcmd, sizeof(pcmd), "cat /proc/net/if_inet6 | grep %s", ifname);
	if (!(fh = popen(pcmd, "r"))) {
		return ret;
	} else {
		while (!feof(fh)) {
			if ((fcnt = fscanf(fh, "%8x%8x%8x%8x %*8x %*02x %02x %*02x %20s\n",
							  &address.s6_addr32[0], &address.s6_addr32[1],
							  &address.s6_addr32[2], &address.s6_addr32[3],
							  &scope, iface)) != 6) {
				break;
			} else if (!strcmp(iface, ifname) && (scope == 0x20)) {
				*addr = address;
				ret = 1;
				break;
			}
		}
		pclose(fh);
	}
	return ret;
}

#endif

#ifndef RDISC
# ifdef __linux__
#  include <sys/ioctl.h>
# endif

static int
getmacaddress (const char *ifname, uint8_t *addr)
{
# ifdef SIOCGIFHWADDR
	struct ifreq req;
	memset (&req, 0, sizeof (req));

	if (((unsigned)strlen (ifname)) >= (unsigned)IFNAMSIZ)
		return -1; /* buffer overflow = local root */
	strcpy (req.ifr_name, ifname);

	int fd = socket (AF_INET6, SOCK_DGRAM, 0);
	if (fd == -1)
		return -1;

	if (ioctl (fd, SIOCGIFHWADDR, &req))
	{
		perror (ifname);
		close (fd);
		return -1;
	}
	close (fd);

	memcpy (addr, req.ifr_hwaddr.sa_data, 6);
	return 0;
# else
	(void)ifname;
	(void)addr;
	return -1;
# endif
}


static const uint8_t nd_type_advert = ND_NEIGHBOR_ADVERT;
static const unsigned nd_delay_ms = 1000;
static const unsigned ndisc_default = NDISC_VERBOSE1 | NDISC_SINGLE;
static const char ndisc_usage[] = N_(
	"Usage: %s [options] <IPv6 address> <interface>\n"
	"Looks up an on-link IPv6 node link-layer address (Neighbor Discovery)\n");
static const char ndisc_dataname[] = N_("link-layer address");

typedef struct
{
	struct nd_neighbor_solicit hdr;
	struct nd_opt_hdr opt;
	uint8_t hw_addr[6];
} solicit_packet;

typedef struct 
{
	unsigned short vlan_tci;
	unsigned short vlan_type;
} vlan_hdr;

static ssize_t
buildsol (solicit_packet *ns, struct sockaddr_in6 *tgt, const char *ifname)
{
	/* builds ICMPv6 Neighbor Solicitation packet */
	ns->hdr.nd_ns_type = ND_NEIGHBOR_SOLICIT;
	ns->hdr.nd_ns_code = 0;
	ns->hdr.nd_ns_cksum = 0; /* computed by the kernel */
	ns->hdr.nd_ns_reserved = 0;
	memcpy (&ns->hdr.nd_ns_target, &tgt->sin6_addr, 16);

	/* determines actual multicast destination address */
	memcpy (tgt->sin6_addr.s6_addr, "\xff\x02\x00\x00\x00\x00\x00\x00"
	                                "\x00\x00\x00\x01\xff", 13);

	/* gets our own interface's link-layer address (MAC) */
	if (getmacaddress (ifname, ns->hw_addr))
		return sizeof (ns->hdr);

	ns->opt.nd_opt_type = ND_OPT_SOURCE_LINKADDR;
	ns->opt.nd_opt_len = 1; /* 8 bytes */
	return sizeof (*ns);
}
#ifdef RKS_PATCHES
static unsigned int get_mono_us(struct timespec *ts)
{
    return ts->tv_sec * 1000000ULL + ts->tv_nsec/1000;
}

static inline void ipv6_eth_mc_map(struct in6_addr *addr, char *buf)
{
        /*
         *      +-------+-------+-------+-------+-------+-------+
         *      |   33  |   33  | DST13 | DST14 | DST15 | DST16 |
         *      +-------+-------+-------+-------+-------+-------+
         */

        buf[0]= 0x33;
        buf[1]= 0x33;

        memcpy(buf + 2, &addr->s6_addr32[3], sizeof(__u32));
}

static inline void ipv6_addr_set(struct in6_addr *addr,
                                     __be32 w1, __be32 w2,
                                     __be32 w3, __be32 w4)
{
        addr->s6_addr32[0] = w1;
        addr->s6_addr32[1] = w2;
        addr->s6_addr32[2] = w3;
        addr->s6_addr32[3] = w4;
}

static inline void addrconf_addr_solict_mult(const struct in6_addr *addr,
                                             struct in6_addr *solicited)
{
        ipv6_addr_set(solicited,
                      htonl(0xFF020000), 0,
                      htonl(0x1),
                      htonl(0xFF000000) | addr->s6_addr32[3]);
}

static ssize_t
buildrawsol (char *pktbuf, struct sockaddr_in6 *tgt_addr, char *ifname)
{
	uint8_t smac[ETH_ALEN];
	uint8_t dmac[ETH_ALEN];
	unsigned int age;
	struct in6_addr mcaddr;
	struct ether_header *eth_hdr;
	struct ip6_hdr *ipv6_hdr;
	vlan_hdr *vlan_hdr;
	solicit_packet *icmp6_hdr;
	int icmp_plen = sizeof(solicit_packet);
	char *p = 0;
	int vlan_id = 0;
	char *pvid = NULL;

	p = pktbuf;
	eth_hdr = (struct ether_header *)p;

	if (getmacaddress (ifname, smac))
		return -1;

	/* get vlan id, if use vlan interface */
	if ((pvid = strchr(ifname, '.')) != NULL)
		vlan_id = atoi(pvid+1);

	memcpy(eth_hdr->ether_shost, smac, ETH_ALEN);

	addrconf_addr_solict_mult(&(tgt_addr->sin6_addr), &mcaddr);

	if( lookup_ndisc6_cache(tgt_addr->sin6_addr, dmac, &age) )
	{
		printf("Target link-layer address found from cache : ");
	        printmacaddress ((uint8_t *)dmac, ETH_ALEN);
	        printf("Trying Unicast Neighbor Solicitation\n");
	    	memcpy(eth_hdr->ether_dhost, dmac, ETH_ALEN);
	}
	else
	{
	        printf("Trying Multicast Neighbor Solicitation\n");
		ipv6_eth_mc_map(&mcaddr, eth_hdr->ether_dhost);
	}

	eth_hdr->ether_type = htons(ETH_P_IPV6);
	p += sizeof(struct ether_header);

	if (vlan_id) {
		eth_hdr->ether_type = htons(ETH_P_8021Q);
		vlan_hdr = p;
		vlan_hdr->vlan_tci = htons(vlan_id & 0xfff);
		vlan_hdr->vlan_type = htons(ETH_P_IPV6);
		p += sizeof(vlan_hdr);
	}

	ipv6_hdr = (struct ip6_hdr *)p;
	ipv6_hdr->ip6_flow = 0x60000000; /* ip version:6 and no flow ctrl*/
	ipv6_hdr->ip6_plen = htons(icmp_plen); /* icmp header size */
	ipv6_hdr->ip6_nxt = 0x3a;
	ipv6_hdr->ip6_hlim = 0xff;

	if (!getipv6address(ifname, &ipv6_hdr->ip6_src)) {
		return -1;
	}

	ipv6_hdr->ip6_dst = mcaddr;

	p += sizeof(struct ip6_hdr);

	icmp6_hdr = (solicit_packet *)p;
	icmp6_hdr->hdr.nd_ns_type = ND_NEIGHBOR_SOLICIT;
	icmp6_hdr->hdr.nd_ns_code = 0;
	icmp6_hdr->hdr.nd_ns_cksum = 0;
	icmp6_hdr->hdr.nd_ns_reserved = 0;
	memcpy (&icmp6_hdr->hdr.nd_ns_target, &tgt_addr->sin6_addr, 16);
	memcpy(icmp6_hdr->hw_addr, smac, ETH_ALEN);
	icmp6_hdr->opt.nd_opt_type = ND_OPT_SOURCE_LINKADDR;
	icmp6_hdr->opt.nd_opt_len = 1; /* 8 bytes */
	icmp6_hdr->hdr.nd_ns_cksum = csum_ipv6_magic(&ipv6_hdr->ip6_src, &ipv6_hdr->ip6_dst, icmp_plen,
												 IPPROTO_ICMPV6, csum_partial(icmp6_hdr, icmp_plen, 0));
	p += icmp_plen;

	return (int)(p-pktbuf);
}
#endif

static int
parseadv (const uint8_t *buf, size_t len, const struct sockaddr_in6 *tgt,
          bool verbose)
{
	const struct nd_neighbor_advert *na =
		(const struct nd_neighbor_advert *)buf;
	const uint8_t *ptr;

#ifdef RKS_PATCHES
	if (len < 24)
		return -1;
#endif
	/* checks if the packet is a Neighbor Advertisement, and
	 * if the target IPv6 address is the right one */
	if ((len < sizeof (struct nd_neighbor_advert))
	 || (na->nd_na_type != ND_NEIGHBOR_ADVERT)
	 || (na->nd_na_code != 0)
	 || memcmp (&na->nd_na_target, &tgt->sin6_addr, 16))
		return -1;

	len -= sizeof (struct nd_neighbor_advert);

	/* looks for Target Link-layer address option */
	ptr = buf + sizeof (struct nd_neighbor_advert);

#ifdef RKS_PATCHES
	/* accept NA with no link-layer address */
	if (len == 0)
	{
		if (ndisc_m)
			return 0;
	}
	else 
#endif
	while (len >= 8)
	{
		uint16_t optlen;

		optlen = ((uint16_t)(ptr[1])) << 3;
		if (optlen == 0)
			break; /* invalid length */

		if (len < optlen) /* length > remaining bytes */
			break;
		len -= optlen;


		/* skips unrecognized option */
		if (ptr[0] != ND_OPT_TARGET_LINKADDR)
		{
			ptr += optlen;
			continue;
		}

		/* Found! displays link-layer address */
		ptr += 2;
		optlen -= 2;
		if (verbose)
		{
			char addr6[INET6_ADDRSTRLEN];
			inet_ntop(AF_INET6, &na->nd_na_target, addr6, INET6_ADDRSTRLEN);
			fputs (_("Target address: "), stdout);
			printf("%s\n", addr6);
			fputs (_("Target link-layer address: "), stdout);
		}

		printmacaddress (ptr, optlen);

		update_ndisc6_cache(tgt->sin6_addr, ptr);
		return 0;
	}

	return -1;
}
#else
static const uint8_t nd_type_advert = ND_ROUTER_ADVERT;
static const unsigned nd_delay_ms = 4000;
static const unsigned ndisc_default = NDISC_VERBOSE1;
static const char ndisc_usage[] = N_(
	"Usage: %s [options] [IPv6 address] <interface>\n"
	"Solicits on-link IPv6 routers (Router Discovery)\n");
static const char ndisc_dataname[] = N_("advertised prefixes");

typedef struct nd_router_solicit solicit_packet;

static ssize_t
buildsol (solicit_packet *rs, struct sockaddr_in6 *tgt, const char *ifname)
{
	(void)tgt;
	(void)ifname;

	/* builds ICMPv6 Router Solicitation packet */
	rs->nd_rs_type = ND_ROUTER_SOLICIT;
	rs->nd_rs_code = 0;
	rs->nd_rs_cksum = 0; /* computed by the kernel */
	rs->nd_rs_reserved = 0;
	return sizeof (*rs);
}


static void
print32time (uint32_t v)
{
	v = ntohl (v);

	if (v == 0xffffffff)
		fputs (_("    infinite (0xffffffff)\n"), stdout);
	else
		printf (_("%12u (0x%08x) %s\n"),
		        v, v, ngettext ("second", "seconds", v));
}


static int
parseprefix (const struct nd_opt_prefix_info *pi, size_t optlen, bool verbose)
{
	char str[INET6_ADDRSTRLEN];

	if (optlen < sizeof (*pi))
		return -1;

	/* displays prefix informations */
	if (inet_ntop (AF_INET6, &pi->nd_opt_pi_prefix, str,
	               sizeof (str)) == NULL)
		return -1;

	if (verbose)
		fputs (_(" Prefix                   : "), stdout);
	printf ("%s/%u\n", str, pi->nd_opt_pi_prefix_len);

	if (verbose)
	{
		fputs (_("  Valid time              : "), stdout);
		print32time (pi->nd_opt_pi_valid_time);
		fputs (_("  Pref. time              : "), stdout);
		print32time (pi->nd_opt_pi_preferred_time);
	}
	return 0;
}


static void
parsemtu (const struct nd_opt_mtu *m)
{
	unsigned mtu = ntohl (m->nd_opt_mtu_mtu);

	fputs (_(" MTU                      : "), stdout);
	printf ("       %5u %s (%s)\n", mtu,
	        ngettext ("byte", "bytes", mtu),
			gettext((mtu >= 1280) ? N_("valid") : N_("invalid")));
}


static const char *
pref_i2n (unsigned val)
{
	static const char *values[] =
		{ N_("medium"), N_("high"), N_("medium (invalid)"), N_("low") };
	return gettext (values[(val >> 3) & 3]);
}


static int
parseroute (const uint8_t *opt)
{
	uint8_t optlen = opt[1], plen = opt[2];
	if ((optlen > 3) || (plen > 128) || (optlen < ((plen + 127) >> 6)))
		return -1;

	char str[INET6_ADDRSTRLEN];
	struct in6_addr dst = in6addr_any;
	memcpy (dst.s6_addr, opt + 8, (optlen - 1) << 3);
	if (inet_ntop (AF_INET6, &dst, str, sizeof (str)) == NULL)
		return -1;

	printf (_(" Route                    : %s/%"PRIu8"\n"), str, plen);
	printf (_("  Route preference        :       %6s\n"), pref_i2n (opt[3]));
	fputs (_("  Route lifetime          : "), stdout);
	print32time (((const uint32_t *)opt)[1]);
	return 0;
}


static int
parserdnss (const uint8_t *opt)
{
	uint8_t optlen = opt[1];
	if (((optlen & 1) == 0) || (optlen < 3))
		return -1;

	optlen /= 2;
	for (unsigned i = 0; i < optlen; i++)
	{
		char str[INET6_ADDRSTRLEN];

		if (inet_ntop (AF_INET6, opt + (16 * i + 8), str,
		               sizeof (str)) == NULL)
			return -1;

		printf (_(" Recursive DNS server     : %s\n"), str);
	}

	fputs (ngettext ("  DNS server lifetime     : ",
	                 "  DNS servers lifetime    : ", optlen), stdout);
	print32time (((const uint32_t *)opt)[1]);
	return 0;
}


static int
parseadv (const uint8_t *buf, size_t len, const struct sockaddr_in6 *tgt,
          bool verbose)
{
	const struct nd_router_advert *ra =
		(const struct nd_router_advert *)buf;
	const uint8_t *ptr;

	(void)tgt;

	/* checks if the packet is a Router Advertisement */
	if ((len < sizeof (struct nd_router_advert))
	 || (ra->nd_ra_type != ND_ROUTER_ADVERT)
	 || (ra->nd_ra_code != 0))
		return -1;

	if (verbose)
	{
		unsigned v;

		/* Hop limit */
		puts ("");
		fputs (_("Hop limit                 :    "), stdout);
		v = ra->nd_ra_curhoplimit;
		if (v != 0)
			printf (_("      %3u"), v);
		else
			fputs (_("undefined"), stdout);
		printf (_(" (      0x%02x)\n"), v);

		v = ra->nd_ra_flags_reserved;
		printf (_("Stateful address conf.    :          %3s\n"),
		        gettext ((v & ND_RA_FLAG_MANAGED) ? N_ ("Yes") : N_("No")));
		printf (_("Stateful other conf.      :          %3s\n"),
		        gettext ((v & ND_RA_FLAG_OTHER) ? N_ ("Yes") : N_("No")));
		printf (_("Router preference         :       %6s\n"), pref_i2n (v));

		/* Router lifetime */
		fputs (_("Router lifetime           : "), stdout);
		v = ntohs (ra->nd_ra_router_lifetime);
		printf (_("%12u (0x%08x) %s\n"), v, v,
		        ngettext ("second", "seconds", v));

		/* ND Reachable time */
		fputs (_("Reachable time            : "), stdout);
		v = ntohl (ra->nd_ra_reachable);
		if (v != 0)
			printf (_("%12u (0x%08x) %s\n"), v, v,
			        ngettext ("millisecond", "milliseconds", v));
		else
			fputs (_(" unspecified (0x00000000)\n"), stdout);

		/* ND Retransmit time */
		fputs (_("Retransmit time           : "), stdout);
		v = ntohl (ra->nd_ra_retransmit);
		if (v != 0)
			printf (_("%12u (0x%08x) %s\n"), v, v,
			        ngettext ("millisecond", "milliseconds", v));
		else
			fputs (_(" unspecified (0x00000000)\n"), stdout);
	}
	len -= sizeof (struct nd_router_advert);

	/* parses options */
	ptr = buf + sizeof (struct nd_router_advert);

	while (len >= 8)
	{
		uint16_t optlen;

		optlen = ((uint16_t)(ptr[1])) << 3;
		if ((optlen == 0) /* invalid length */
		 || (len < optlen) /* length > remaining bytes */)
			break;

		len -= optlen;

		/* only prefix are shown if not verbose */
		switch (ptr[0] * (verbose ? 1
		                          : (ptr[0] == ND_OPT_PREFIX_INFORMATION)))
		{
			case ND_OPT_SOURCE_LINKADDR:
				fputs (_(" Source link-layer address: "), stdout);
				printmacaddress (ptr + 2, optlen - 2);
				break;

			case ND_OPT_TARGET_LINKADDR:
				break; /* ignore */

			case ND_OPT_PREFIX_INFORMATION:
				if (parseprefix ((const struct nd_opt_prefix_info *)ptr,
				                 optlen, verbose))
					return -1;

			case ND_OPT_REDIRECTED_HEADER:
				break; /* ignore */

			case ND_OPT_MTU:
				parsemtu ((const struct nd_opt_mtu *)ptr);
				break;

			case 24: // RFC4191
				parseroute (ptr);
				break;

			case 25: // RFC5006
				parserdnss (ptr);
				break;
		}
		/* skips unrecognized option */

		ptr += optlen;
	}

	return 0;
}
#endif


static ssize_t
recvfromLL (int fd, void *buf, size_t len, int flags,
            struct sockaddr_in6 *addr)
{
	char cbuf[CMSG_SPACE (sizeof (int))];
	struct iovec iov =
	{
		.iov_base = buf,
		.iov_len = len
	};
	struct msghdr hdr =
	{
		.msg_name = addr,
		.msg_namelen = sizeof (*addr),
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = cbuf,
		.msg_controllen = sizeof (cbuf)
	};

	ssize_t val = recvmsg (fd, &hdr, flags);
	if (val == -1)
		return val;

	/* ensures the hop limit is 255 */
	for (struct cmsghdr *cmsg = CMSG_FIRSTHDR (&hdr);
	     cmsg != NULL;
	     cmsg = CMSG_NXTHDR (&hdr, cmsg))
	{
		if ((cmsg->cmsg_level == IPPROTO_IPV6)
		 && (cmsg->cmsg_type == IPV6_HOPLIMIT))
		{
			if (255 != *(int *)CMSG_DATA (cmsg))
			{
				// pretend to be a spurious wake-up
				errno = EAGAIN;
				return -1;
			}
		}
	}

	return val;
}

static ssize_t
recvadv (int fd, const struct sockaddr_in6 *tgt, unsigned wait_ms,
         unsigned flags)
{
	struct timespec now, end;
	unsigned responses = 0;
#if defined(RKS_PATCHES) && !defined(RDISC)
	struct timespec last, new;
#endif
	/* computes deadline time */
	mono_gettime (&now);
	{
		div_t d;

		d = div (wait_ms, 1000);
		end.tv_sec = now.tv_sec + d.quot;
		end.tv_nsec = now.tv_nsec + (d.rem * 1000000);
	}

#if defined(RKS_PATCHES) && !defined(RDISC)
    last = now;
#endif

	/* receive loop */
	for (;;)
	{
		/* waits for reply until deadline */
		ssize_t val = 0;
		if (end.tv_sec >= now.tv_sec)
		{
			val = (end.tv_sec - now.tv_sec) * 1000
				+ (int)((end.tv_nsec - now.tv_nsec) / 1000000);
			if (val < 0)
				val = 0;
		}

		val = poll (&(struct pollfd){ .fd = fd, .events = POLLIN }, 1, val);
		if (val < 0)
			break;

		if (val == 0)
			return responses;
#if defined(RKS_PATCHES) && !defined(RDISC)
#ifdef _IN_DEV_SUPPORT
		if (ndisc_m)
		{
			struct sockaddr_ll from;
			socklen_t alen = sizeof(from);
			unsigned char packet[1460];
			val = recvfrom(fd, packet, sizeof(packet), 0, (struct sockaddr *) &from, &alen);
			if (val == -1)
			{
				perror (_("Receiving ICMPv6 packet"));
				continue;
			}

			if (from.sll_ifindex != if_nametoindex(outgoing_iface))
			{
				continue;
			}

			char *p = 0;
			p = packet;
			p += sizeof(struct ip6_hdr);
			val -= sizeof(struct ip6_hdr);

			if (parseadv (p, val, tgt, (flags & NDISC_VERBOSE) != 0) == 0)
			{
				/* change value of signal */
				exitsignal_val = exitsignal_success;
				
				if (responses < INT_MAX)
					responses++;

				if (flags & NDISC_SINGLE)
					return 1 /* = responses */;
			}
		} else {
#endif
#endif
		/* receives an ICMPv6 packet */
		// TODO: use interface MTU as buffer size
		union
		{
			uint8_t  b[1460];
			uint64_t align;
		} buf;
		struct sockaddr_in6 addr;

		val = recvfromLL (fd, &buf, sizeof (buf), MSG_DONTWAIT, &addr);
		if (val == -1)
		{
			if (errno != EAGAIN)
				perror (_("Receiving ICMPv6 packet"));
			continue;
		}

		/* ensures the response came through the right interface */
		if (addr.sin6_scope_id
		 && (addr.sin6_scope_id != tgt->sin6_scope_id))
			continue;

		if (parseadv (buf.b, val, tgt, (flags & NDISC_VERBOSE) != 0) == 0)
		{
			if (flags & NDISC_VERBOSE)
			{
				char str[INET6_ADDRSTRLEN];

				if (inet_ntop (AF_INET6, &addr.sin6_addr, str,
						sizeof (str)) != NULL)
					printf (_(" from %s\n"), str);
			}

			if (responses < INT_MAX)
				responses++;

			if (flags & NDISC_SINGLE)
#if defined(RKS_PATCHES) && !defined(RDISC)
            {
                mono_gettime (&now);
                unsigned diff = get_mono_us(&now) - get_mono_us(&last);
                printf(" %u.%03ums\n", diff / 1000, diff % 1000);
                if( latency_filename ) {
                    FILE *fp;
                    remove(latency_filename);
                    fp = fopen(latency_filename,"wt");
                    if( fp ) {
                        fprintf(fp,"%u.%03u",diff / 1000, diff % 1000);
                        fclose(fp);
                    }
                 }
                exitsignal_val = exitsignal_success;
                return 1 /* = responses */;
            }
#else
				return 1 /* = responses */;
#endif
		}
#if defined(RKS_PATCHES) && !defined(RDISC) 
#ifdef _IN_DEV_SUPPORT
        }
#endif
        mono_gettime (&now);
        unsigned diff = get_mono_us(&now) - get_mono_us(&last);
        printf(" %u.%03ums\n", diff / 1000, diff % 1000);
        if( latency_filename ) {
            FILE *fp;
            remove(latency_filename);
            fp = fopen(latency_filename,"wt");
            if( fp ) {
                fprintf(fp,"%u.%03u",diff / 1000, diff % 1000);
                fclose(fp);
            }
        }
#else
		mono_gettime (&now);
#endif
	}

	return -1; /* error */
}


static int fd;
#ifdef RKS_PATCHES
#ifdef _IN_DEV_SUPPORT
static int sfd;
static int rfd;
#endif
#endif

static int
ndisc (const char *name, const char *ifname, unsigned flags, unsigned retry,
       unsigned wait_ms)
{
	struct sockaddr_in6 tgt;

	if (fd == -1)
	{
		perror (_("Raw IPv6 socket"));
		return -1;
	}

	fcntl (fd, F_SETFD, FD_CLOEXEC);

	/* set ICMPv6 filter */
	{
		struct icmp6_filter f;

		ICMP6_FILTER_SETBLOCKALL (&f);
		ICMP6_FILTER_SETPASS (nd_type_advert, &f);
		setsockopt (fd, IPPROTO_ICMPV6, ICMP6_FILTER, &f, sizeof (f));
	}

	setsockopt (fd, SOL_SOCKET, SO_DONTROUTE, &(int){ 1 }, sizeof (int));

	/* sets Hop-by-hop limit to 255 */
	sethoplimit (fd, 255);
	setsockopt (fd, IPPROTO_IPV6, IPV6_RECVHOPLIMIT,
	            &(int){ 1 }, sizeof (int));

	/* resolves target's IPv6 address */
	if (getipv6byname (name, ifname, (flags & NDISC_NUMERIC) ? 1 : 0, &tgt))
		goto error;
	else
	{
		char s[INET6_ADDRSTRLEN];

		inet_ntop (AF_INET6, &tgt.sin6_addr, s, sizeof (s));
		if (flags & NDISC_VERBOSE)
			printf (_("Soliciting %s (%s) on %s...\n"), name, s, ifname);
	}

	{
#if defined(RKS_PATCHES) && !defined(RDISC)
		char rawpktbuf[256];
		solicit_packet packet;
		struct sockaddr_in6 dst;
		struct sockaddr_ll addr;
		ssize_t plen;

#ifdef _IN_DEV_SUPPORT
		if (ndisc_m)
		{
			plen = buildrawsol(rawpktbuf, &tgt, ifname);
			if (plen == -1)
				goto error;

			memset(&addr, 0, sizeof(addr));
			addr.sll_family = AF_PACKET;
			addr.sll_ifindex = if_nametoindex(outgoing_iface);

			if (0 > (sfd = socket (AF_PACKET, SOCK_RAW, 0)))
			{
				perror (_("Sending ICMPv6 packet: socket"));
				goto error;
			} 
			else if (0 > bind(sfd, (struct sockaddr *)&addr, sizeof(addr)))
			{
				perror (_("Sending ICMPv6 packet: bind"));
				goto error;
			}
			addr.sll_ifindex = if_nametoindex(ifname);
			addr.sll_protocol = htons(ETH_P_IPV6);
			if (0 > (rfd = socket (AF_PACKET, SOCK_DGRAM, 0)))
			{
				perror (_("Receiving ICMPv6 packet: socket"));
				goto error;
			}
			else if (0 > bind(rfd, (struct sockaddr *)&addr, sizeof(addr)))
			{
				perror (_("Receiving ICMPv6 packet: bind"));
				goto error;
			}

			int val=1;
			if(setsockopt(rfd, SOL_SOCKET, SO_INIFINDEX, (const void*)&val, sizeof(val)))
			{
				perror (_("Receiving ICMPv6 packet: setsockopt"));
				goto error;
			}
		}
		else
#endif
		{
			memcpy (&dst, &tgt, sizeof (dst));
			plen = buildsol (&packet, &dst, ifname);
			if (plen == -1)
				goto error;

		}
#else
		solicit_packet packet;
		struct sockaddr_in6 dst;
		ssize_t plen;

		memcpy (&dst, &tgt, sizeof (dst));
		plen = buildsol (&packet, &dst, ifname);
		if (plen == -1)
			goto error;

#endif

		while (retry > 0)
		{
#if defined(RKS_PATCHES) && !defined(RDISC)
#ifdef _IN_DEV_SUPPORT
			if (ndisc_m)
			{
				if (send(sfd, &rawpktbuf, plen, 0) < 0)
				{
					perror (_("Sending ICMPv6 packet"));
					goto error;
				}
			} else
#endif
#endif
			/* sends a Solitication */
			if (sendto (fd, &packet, plen, 0,
			            (const struct sockaddr *)&dst,
			            sizeof (dst)) != plen)
			{
				perror (_("Sending ICMPv6 packet"));
				goto error;
			}
			retry--;

			/* receives an Advertisement */
#if defined(RKS_PATCHES) && !defined(RDISC)
			ssize_t val;
#ifdef _IN_DEV_SUPPORT
			if (ndisc_m)
				val = recvadv (rfd, &tgt, wait_ms, flags);
			else
#endif
				val = recvadv (fd, &tgt, wait_ms, flags);
#else
			ssize_t val = recvadv (fd, &tgt, wait_ms, flags);
#endif
			if (val > 0)
			{
#ifdef RKS_PATCHES
#ifdef _IN_DEV_SUPPORT
				if (sfd) close(sfd);
				if (rfd) close(rfd);
#endif
#endif
				close (fd);
				return 0;
			}
			else
			if (val == 0)
			{
				if (flags & NDISC_VERBOSE)
					puts (_("Timed out."));
			}
			else
				goto error;
		}
	}

	close (fd);
#ifdef RKS_PATCHES
#ifdef _IN_DEV_SUPPORT
	if (sfd) close (sfd);
	if (rfd) close (rfd);
#endif
#endif
	if (flags & NDISC_VERBOSE)
		puts (_("No response."));
	return -2;

error:
	close (fd);
#ifdef RKS_PATCHES
#ifdef _IN_DEV_SUPPORT
	if (sfd) close (sfd);
	if (rfd) close (rfd);
#endif
#endif
	return -1;
}


static int
quick_usage (const char *path)
{
	fprintf (stderr, _("Try \"%s -h\" for more information.\n"), path);
	return 2;
}


static int
usage (const char *path)
{
	printf (gettext (ndisc_usage), path);

	printf (_("\n"
"  -1, --single   display first response and exit\n"
"  -h, --help     display this help and exit\n"
"  -m, --multiple wait and display all responses\n"
"  -n, --numeric  don't resolve host names\n"
"  -q, --quiet    only print the %s (mainly for scripts)\n"
"  -r, --retry    maximum number of attempts (default: 3)\n"
"  -V, --version  display program version and exit\n"
"  -v, --verbose  verbose display (this is the default)\n"
#ifdef RKS_PATCHES
#ifndef RDISC
"  -M, --mesh     set usage for mesh\n"
"  -S, --mesh_sig set signal value for meshd\n"
"  -P, --mesh_pid set meshd pid\n"
"  -I, --iface    outgoing ether device\n"
"  -L, --latency_log  latency log for meshd\n"
#endif
#endif
"  -w, --wait     how long to wait for a response [ms] (default: 1000)\n"
	           "\n"), gettext (ndisc_dataname));

	return 0;
}


static int
version (void)
{
	printf (_(
"ndisc6: IPv6 Neighbor/Router Discovery userland tool %s (%s)\n"), VERSION, "$Rev: 657 $");
	printf (_(" built %s on %s\n"), __DATE__, PACKAGE_BUILD_HOSTNAME);

	printf (_("Configured with: %s\n"), PACKAGE_CONFIGURE_INVOCATION);
	puts (_("Written by Remi Denis-Courmont\n"));

	printf (_("Copyright (C) %u-%u Remi Denis-Courmont\n"), 2004, 2007);
	puts (_("This is free software; see the source for copying conditions.\n"
	        "There is NO warranty; not even for MERCHANTABILITY or\n"
	        "FITNESS FOR A PARTICULAR PURPOSE.\n"));
	return 0;
}


static const struct option opts[] =
{
	{ "single",   no_argument,       NULL, '1' },
	{ "help",     no_argument,       NULL, 'h' },
	{ "multiple", required_argument, NULL, 'm' },
	{ "numeric",  no_argument,       NULL, 'n' },
	{ "quiet",    no_argument,       NULL, 'q' },
	{ "retry",    required_argument, NULL, 'r' },
	{ "version",  no_argument,       NULL, 'V' },
	{ "verbose",  no_argument,       NULL, 'v' },
	{ "wait",     required_argument, NULL, 'w' },
	{ "mesh",     required_argument, NULL, 'M' },
	{ "mesh_sig", required_argument, NULL, 'S' },
	{ "mesh_pid", required_argument, NULL, 'P' },
	{ "iface",    required_argument, NULL, 'I' },
	{ "latency_log", required_argument, NULL, 'L' },
	{ NULL,       0,                 NULL, 0   }
};

#ifdef RKS_PATCHES
static void exitsignal(void)
{
    if( exitsignal_pid )
        kill(exitsignal_pid, exitsignal_val);
}
#endif

int
main (int argc, char *argv[])
{
	fd = socket (PF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	int errval = errno;

	/* Drops root privileges (if setuid not run by root).
	 * Also make sure the socket is not STDIN/STDOUT/STDERR. */
	if (setuid (getuid ()) || ((fd >= 0) && (fd <= 2)))
		return 1;

	setlocale (LC_ALL, "");
	bindtextdomain (PACKAGE, LOCALEDIR);
	textdomain (PACKAGE);

	int val;
	unsigned retry = 3, flags = ndisc_default, wait_ms = nd_delay_ms;
	const char *hostname, *ifname;
#ifdef RKS_PATCHES
	while ((val = getopt_long (argc, argv, "1hmnqr:Vvw:M:S:P:I:L:", opts, NULL)) != EOF)
#else
	while ((val = getopt_long (argc, argv, "1hmnqr:Vvw:", opts, NULL)) != EOF)
#endif
	{
		switch (val)
		{
			case '1':
				flags |= NDISC_SINGLE;
				break;

			case 'h':
				return usage (argv[0]);

			case 'm':
				flags &= ~NDISC_SINGLE;
				break;

			case 'n':
				flags |= NDISC_NUMERIC;
				break;

			case 'q':
				flags &= ~NDISC_VERBOSE;
				break;

			case 'r':
			{
				unsigned long l;
				char *end;

				l = strtoul (optarg, &end, 0);
				if (*end || l > UINT_MAX)
					return quick_usage (argv[0]);
				retry = l;
				break;
			}
#ifdef RKS_PATCHES
#ifndef RDISC
			case 'M':
				ndisc_m = 1;
				break;

			case 'I':
				outgoing_iface = optarg;
				break;

			case 'S':
			{
				unsigned long l;
				char *end;

				l = strtoul (optarg, &end, 0);
				if (*end || l > UINT_MAX)
					return quick_usage (argv[0]);
				exitsignal_success = l;
				break;
			}

			case 'P':
			{
				unsigned long l;
				char *end;

				l = strtoul (optarg, &end, 0);
				if (*end || l > UINT_MAX)
					return quick_usage (argv[0]);
				exitsignal_pid = l;
				atexit(exitsignal);
				break;
			}

            case 'L':
            {
                latency_filename = optarg;
                break;
            }
#endif
#endif
			case 'V':
				return version ();

			case 'v':
				/* NOTE: assume NDISC_VERBOSE occupies low-order bits */
				if ((flags & NDISC_VERBOSE) < NDISC_VERBOSE)
					flags++;
				break;

			case 'w':
			{
				unsigned long l;
				char *end;

				l = strtoul (optarg, &end, 0);
				if (*end || l > UINT_MAX)
					return quick_usage (argv[0]);
				wait_ms = l;
				break;
			}

			case '?':
			default:
				return quick_usage (argv[0]);
		}
	}

	if (optind < argc)
	{
		hostname = argv[optind++];

		if (optind < argc)
			ifname = argv[optind++];
		else
			ifname = NULL;
	}
	else
		return quick_usage (argv[0]);

#ifdef RDISC
	if (ifname == NULL)
	{
		ifname = hostname;
		hostname = "ff02::2";
	}
	else
#endif
	if ((optind != argc) || (ifname == NULL))
		return quick_usage (argv[0]);

	errno = errval; /* restore socket() error value */
	return -ndisc (hostname, ifname, flags, retry, wait_ms);
}
