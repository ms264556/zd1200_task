#ifndef ___RKS_API_H__
#define ___RKS_API_H__

#include <linux/kernel.h>
#include <linux/version.h>

#include <linux/crypto.h>
#include <linux/netdevice.h>
#include <linux/percpu.h>
#include <linux/rcupdate.h>
#include <linux/timer.h>
#include <linux/types.h>


#include <net/ipv6.h>
#include <net/sock.h>

void rks_call_rcu(struct rcu_head *head, void (*func)(struct rcu_head *));
void rks_rcu_barrier(void);
void rks_synchronize_rcu(void);

unsigned long rks_round_jiffies(unsigned long j);

struct ndisc_options *rks_ndisc_parse_options(u8 *opt, int opt_len, struct ndisc_options *ndopts);
struct nd_opt_hdr *rks_ndisc_next_option(struct nd_opt_hdr *cur, struct nd_opt_hdr *end);


int rks_ip6_dst_lookup(struct sock *sk, struct dst_entry **dst, struct flowi *fl);



struct dst_entry *rks_ip6_dst_lookup_flow(struct sock *sk, struct flowi *fl, const struct in6_addr *final_dst);


int rks_ip_local_out(struct sk_buff *skb);
int rks_ip6_local_out(struct sk_buff *skb);

void rks_sock_recv_timestamp(struct msghdr *msg, struct sock *sk, struct sk_buff *skb);

extern void  *__rks_alloc_percpu(size_t size, size_t align);
#define rks_alloc_percpu(type)	\
	(typeof(type) __percpu *)__rks_alloc_percpu(sizeof(type), __alignof__(type))
extern void rks_free_percpu(void  *__pdata);

struct crypto_blkcipher *rks_crypto_alloc_blkcipher(const char *alg_name, u32 type, u32 mask);
struct crypto_ablkcipher *rks_crypto_alloc_ablkcipher(const char *alg_name, u32 type, u32 mask);

void rks_crypto_free_blkcipher(struct crypto_blkcipher *tfm);
void rks_crypto_free_ablkcipher(struct crypto_ablkcipher *tfm);


#endif /* ___RKS_API_H__ */
