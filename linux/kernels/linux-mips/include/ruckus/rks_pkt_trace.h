#ifndef _RKS_PKT_TRACE_H_
#define _RKS_PKT_TRACE_H_

extern int pkt_trace_enable;
void rks_pkt_trace_log(struct sk_buff *skb, const char *fmt, ...);
void rks_pkt_trace_copy(struct sk_buff *new, const struct sk_buff *old);
void rks_pkt_trace(struct sk_buff *skb);

#define RKS_PKT_TRACE


#define PKT_TRACE_LOG(skb, fmt, ...) \
    do {								\
        if (unlikely(pkt_trace_enable)) \
            rks_pkt_trace_log(skb, "%s:%d " fmt "\r", __func__, __LINE__, ##__VA_ARGS__); \
    } while(0)

#define __PKT_TRACE_LOG(skb, fmt, ...) \
            rks_pkt_trace_log(skb, "%s:%d " fmt "\r", __func__, __LINE__, ##__VA_ARGS__);

#define PKT_TRACE_COPY(new, old) \
    do {								\
        if (unlikely((old)->pkt_trace_log)) \
            rks_pkt_trace_copy(new, old); \
    } while(0)

#define PKT_TRACE(skb) \
    do {								\
        if (likely(skb) && unlikely((skb)->pkt_trace_log)) \
            rks_pkt_trace(skb); \
    } while(0)

#endif /* _RKS_PKT_TRACE_H_ */
