#ifndef _IF_TUNNEL_H_
#define _IF_TUNNEL_H_

#include <linux/types.h>
#include <asm/byteorder.h>

#ifdef __KERNEL__
#include <linux/ip.h>
#endif

#define SIOCGETTUNNEL   (SIOCDEVPRIVATE + 0)
#define SIOCADDTUNNEL   (SIOCDEVPRIVATE + 1)
#define SIOCDELTUNNEL   (SIOCDEVPRIVATE + 2)
#define SIOCCHGTUNNEL   (SIOCDEVPRIVATE + 3)
#define SIOCGETPRL      (SIOCDEVPRIVATE + 4)
#define SIOCADDPRL      (SIOCDEVPRIVATE + 5)
#define SIOCDELPRL      (SIOCDEVPRIVATE + 6)
#define SIOCCHGPRL      (SIOCDEVPRIVATE + 7)
#if 1 // V54_TUNNELMGR
#define SIOCGETTUNNRXIDLE (SIOCDEVPRIVATE + 10)
#define SIOCSETTUNNID     (SIOCDEVPRIVATE + 11)
#define SIOCSETTUNNCRYPTO (SIOCDEVPRIVATE + 12)
#define SIOCSETTUNNSK     (SIOCDEVPRIVATE + 13)
#define SIOCDELTUNNSK     (SIOCDEVPRIVATE + 14)
#define SIOCSETTUNNSKOPT  (SIOCDEVPRIVATE + 15)
#endif // V54_TUNNELMGR

#define GRE_CSUM	__cpu_to_be16(0x8000)
#define GRE_ROUTING	__cpu_to_be16(0x4000)
#define GRE_KEY		__cpu_to_be16(0x2000)
#define GRE_SEQ		__cpu_to_be16(0x1000)
#define GRE_STRICT	__cpu_to_be16(0x0800)
#define GRE_REC		__cpu_to_be16(0x0700)
#define GRE_FLAGS	__cpu_to_be16(0x00F8)
#define GRE_VERSION	__cpu_to_be16(0x0007)

#if 1 // V54_TUNNELMGR
struct iptun_stats {
	__u64	tun_rx_packets;		/* total packets received	*/
	__u64	tun_tx_packets;		/* total packets transmitted	*/
	__u64	tun_rx_bytes;		    /* total bytes received		*/
	__u64	tun_tx_bytes;		    /* total bytes transmitted	*/
	__u64	tun_rx_errors;		/* bad packets received		*/
	__u64	tun_tx_errors;		/* packet transmit problems	*/
	__u64	tun_rx_dropped;		/* no space in linux buffers	*/
	__u64	tun_tx_dropped;		/* no space available in linux	*/
	__u64	tun_tx_fragmented;	/* total fragmented tx packets	*/
};
#endif // V54_TUNNELMGR

struct ip_tunnel_parm
{
	char			name[IFNAMSIZ];
	int			link;
	__be16			i_flags;
	__be16			o_flags;
	__be32			i_key;
	__be32			o_key;
#if 1 // V54_TUNNELMGR
	__u16			flags;	  // interface flags
	__u16			tid;	  // Tunnel id
	__u8			psk[32];  // AES pre-shared key
	__u16			psk_bits; // AES key length - 0 means no encryption/decryption applied
	__u8			iv[16];   // Intial Vector used for crypto
	__u16			iv_len;   // IV length
	__u16			d_port;   // Remote (Destination) UDP port number - 0 means no GRE over UDP
	__u16			s_port;   // Local (Source) UDP port number
	int			wlanid;	//for wlanid(checksum) field in GRE header
	struct iptun_stats tun_stats;	// 64-bits counters
#if defined(CONFIG_CRYPTO_DEV_TALITOS) /* SEC with Talitos driver */
	struct crypto_ablkcipher *tfm;
#else /* Kernel Crypto */
	struct crypto_blkcipher *tfm;
#endif
#endif // V54_TUNNELMGR
	struct iphdr		iph;
};

#if 1 // V54_TUNNELMGR
struct ipgre_sockopt {
    int             level;
    int             optname;
    char           *optval;
    int             optlen;
    __u16           port;
};

struct keep_alive_msg {
    unsigned int       magic;
    unsigned int       seq_no;
    unsigned char      digest[20];
    unsigned short     pmtu;
} __attribute__ ((packed));
#endif // V54_TUNNELMGR

/* SIT-mode i_flags */
#define	SIT_ISATAP	0x0001

struct ip_tunnel_prl {
	__be32			addr;
	__u16			flags;
	__u16			__reserved;
	__u32			datalen;
	__u32			__reserved2;
	/* data follows */
};

/* PRL flags */
#define	PRL_DEFAULT		0x0001

enum
{
	IFLA_GRE_UNSPEC,
	IFLA_GRE_LINK,
	IFLA_GRE_IFLAGS,
	IFLA_GRE_OFLAGS,
	IFLA_GRE_IKEY,
	IFLA_GRE_OKEY,
	IFLA_GRE_LOCAL,
	IFLA_GRE_REMOTE,
	IFLA_GRE_TTL,
	IFLA_GRE_TOS,
	IFLA_GRE_PMTUDISC,
	__IFLA_GRE_MAX,
};

#define IFLA_GRE_MAX	(__IFLA_GRE_MAX - 1)

#endif /* _IF_TUNNEL_H_ */
