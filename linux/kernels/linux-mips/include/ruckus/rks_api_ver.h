#ifndef _RKS_API_VER_H_
#define _RKS_API_VER_H_

#ifdef __KERNEL__
#include <linux/version.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>


static inline int do_hard_header(struct sk_buff *skb,
                   struct net_device *dev,
                   unsigned short type,
                   void *daddr,
                   void *saddr,
                   unsigned len)
{
  return dev_hard_header(skb, dev, type, daddr, saddr, len);
}


#define RKSV___DEV_GET_BY_NAME(index)        __dev_get_by_name(&init_net, (index))
#define RKSV_DEV_GET_BY_NAME(index)          dev_get_by_name(&init_net, (index))
#define RKSV_DEV_GET_BY_INDEX(index)         dev_get_by_index(&init_net, (index))
#define RKSV_DEV_PRIV(dev)                   netdev_priv((dev))
#define RKSV_RESET_MAC_HEADER(skb)           skb_reset_mac_header((skb))
#define RKSV_RESET_NETWORK_HEADER(skb)       skb_reset_network_header((skb))
#define RKSV_RESET_UDP_HEADER(skb)           skb_reset_transport_header(skb)
#define RKSV_RESET_TCP_HEADER(skb)           skb_reset_transport_header(skb)
#define RKSV_RESET_TRANSPORT_HEADER(skb)     skb_reset_transport_header(skb)
#define RKSV_GET_MAC_HEADER(skb)             skb_mac_header((skb))
#define RKSV_GET_UDP_HEADER(skb)             ((struct udphdr*)skb_transport_header(skb))
#define RKSV_GET_TCP_HEADER(skb)             ((struct tcphdr*)skb_transport_header(skb))
#define RKSV_GET_TRANSPORT_HEADER(skb)       skb_transport_header(skb)
#define RKSV_GET_NETWORK_HEADER(skb)			skb_network_header(skb)
#define RKSV_GET_IPV6_HEADER(skb)            ((struct ipv6hdr*)skb_network_header(skb))
#define RKSV_GET_IPV4_HEADER(skb)            ((struct iphdr*)skb_network_header(skb))
#define RKSV_SKB_SET_MAC_HEADER(skb, ph)     skb_set_mac_header((skb), (unsigned char *)(ph) - (skb)->data)
#define RKSV_SKB_SET_NETWORK_HEADER(skb, ph) skb_set_network_header((skb), (unsigned char *)(ph) - (skb)->data)
#define RKSV_SKB_SET_TRANSPORT_HEADER(skb, ph) skb_set_transport_header((skb), (unsigned char *)(ph) - (skb)->data)
#define RKSV_SKB_SET_UDP_HEADER(skb, ph)     RKSV_SKB_SET_TRANSPORT_HEADER(skb, ph)
#define RKSV_SKB_SET_TCP_HEADER(skb, ph)     RKSV_SKB_SET_TRANSPORT_HEADER(skb, ph)
#define RKSV_SET_OWNER(dir, val)			do{}while(0)
#define RKSV_SET_INPUT_DEV(skb, dev)         (skb)->input_dev = (dev)

#define RKSV_GET_SKB_DST(skb)                skb_dst((skb))
#define RKSV_SET_SKB_DST(skb, val)           skb_dst_set((skb), (val))
#define RKSV_DEV_NETDEV_OPS(dev, ops)		 (dev)->netdev_ops = (ops)
#define RKSV_DO_HARD_START_XMIT(dev)         (dev)->netdev_ops->ndo_start_xmit
#define RKSV_DO_IOCTL(dev)                   (dev)->netdev_ops->ndo_do_ioctl
#define RKSV_DO_INIT(dev)                    (dev)->netdev_ops->ndo_init

#define RKSV_DO_HARD_HEADER                  do_hard_header
#define RKSV_TCP_V4_CHECK(h, l, s, d, b)     tcp_v4_check((l), (s), (d), (b))
#define RKSV_SKB_LINEARIZE(skb)              skb_linearize(skb)

#define RKSV_IP_ROUTE_OUTPUT_KEY(rp, flp)    ip_route_output_key(&init_net, (rp), (flp))

#define RKSV_IP6_ROUTE_OUTPUT(sk, fl)        ip6_route_output(&init_net, (sk), (fl))
#define RKSV_IP_INC_STATS(field)         IP_INC_STATS(&init_net, (field))
#define RKSV_IP6_INC_STATS(field)        IP6_INC_STATS(&init_net, NULL, (field))
#define RKSV_IP6_FMT                         "%pI6"
#define RKSV_IP6_IP(pip6)                    (pip6)
#define RKSV_PPPOE_SES_HLEN				PPPOE_SES_HLEN
#define RKSV_KMEM_CACHE_CREATE(name, size, align, flags, ctor) \
			kmem_cache_create(name, size, align, flags, ctor)
#define PROC_NET                             init_net.proc_net


#define RKSV_SKB_IIF_IF(skb, idx)			do{}while(0)


#define RKSV_SKB_NETWORK_OFFSET(skb)			(RKSV_GET_NETWORK_HEADER(skb)	- (skb)->data)
#define RKSV_PPPOE_HDR(skb)						((struct pppoe_hdr *)RKSV_GET_NETWORK_HEADER(skb))


#define RKSV_GET_RAW_MAC_HEADER(skb)			skb_mac_header(skb)
#define RKSV_GET_RAW_NETWORK_HEADER(skb)		skb_network_header(skb)
#define RKSV_GET_RAW_TRANSPORT_HEADER(skb)		skb_transport_header(skb)
#define RKSV_RESET_RAW_MAC_HEADER(skb)			skb_reset_mac_header(skb)
#define RKSV_SET_RAW_NETWORK_HEADER(skb, offset)	skb_set_network_header(skb, (offset))
#define RKSV_SET_RAW_MAC_HEADER(skb, offset) skb_set_mac_header(skb, (offset))
#define RKSV_SET_TRANSPORT_HEADER(skb, offset) skb_set_transport_header(skb, (offset))

#define RKSV_RESET_RAW_TRANSPORT_HEADER(skb)		skb_reset_transport_header(skb)

#define RKSV_RESET_MAC_LEN(skb)					do{(skb)->mac_len = RKSV_GET_NETWORK_HEADER((skb)) - RKSV_GET_MAC_HEADER((skb));}while(0)



#define RKSV_PDE_DATA(inode)					(PDE((inode))->data)
#define RKS_MASTER_DEV(dev) ((dev)->master)
#define RKSV_PDE_SET_DATA(de, ptr)				((de)->data = (ptr))
#define RKSV_NETDEV_NOTIFIER_INFO_TO_DEV(ptr)	(ptr)


#endif /* __KERNEL__ */
#endif /* _RKS_API_VER_H_ */
