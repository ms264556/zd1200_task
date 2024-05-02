#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <ruckus/rks_api.h>
#include <net/ip6_route.h>
#include <linux/workqueue.h>


extern struct ndisc_options *ndisc_parse_options(u8 *opt, int opt_len, struct ndisc_options *ndopts);
extern struct nd_opt_hdr *ndisc_next_option(struct nd_opt_hdr *cur, struct nd_opt_hdr *end);

struct ndisc_options *rks_ndisc_parse_options(u8 *opt, int opt_len, struct ndisc_options *ndopts)
{
    return ndisc_parse_options(opt, opt_len, ndopts);
}
EXPORT_SYMBOL(rks_ndisc_parse_options);

struct nd_opt_hdr *rks_ndisc_next_option(struct nd_opt_hdr *cur, struct nd_opt_hdr *end)
{
    return ndisc_next_option(cur, end);
}
EXPORT_SYMBOL(rks_ndisc_next_option);

void rks_synchronize_rcu(void)
{
	synchronize_rcu();
}
EXPORT_SYMBOL(rks_synchronize_rcu);

void rks_call_rcu(struct rcu_head *head, void (*func)(struct rcu_head *))
{
	return call_rcu(head, func);
}
EXPORT_SYMBOL(rks_call_rcu);

void rks_rcu_barrier(void)
{
	rcu_barrier();
}
EXPORT_SYMBOL(rks_rcu_barrier);

unsigned long rks_round_jiffies(unsigned long j)
{
	return round_jiffies(j);
}
EXPORT_SYMBOL(rks_round_jiffies);




int rks_ip6_dst_lookup(struct sock *sk, struct dst_entry **dst, struct flowi *fl)
{
	return ip6_dst_lookup(sk, dst, fl);
}
EXPORT_SYMBOL(rks_ip6_dst_lookup);


struct dst_entry *rks_ip6_dst_lookup_flow(struct sock *sk, struct flowi *fl, const struct in6_addr *final_dst)
{
	int err;
	struct dst_entry *dst = NULL;

	err = ip6_dst_lookup(sk, &dst, fl);
	if (err)
		goto out;
	if (final_dst)
		ipv6_addr_copy(&fl->fl6_dst, final_dst);

	err = __xfrm_lookup(sock_net(sk), &dst, &fl, sk, 0);
	if (err < 0) {
		if (err == -EREMOTE)
			err = ip6_dst_blackhole(sk, &dst, fl);
		if (err < 0)
			goto out;
	}

out:
    return dst;
}

EXPORT_SYMBOL(rks_ip6_dst_lookup_flow);

int rks_ip_local_out(struct sk_buff *skb)
{
	return ip_local_out(skb);
}
EXPORT_SYMBOL(rks_ip_local_out);

int rks_ip6_local_out(struct sk_buff *skb)
{
	return ip6_local_out(skb);
}
EXPORT_SYMBOL(rks_ip6_local_out);

void rks_sock_recv_timestamp(struct msghdr *msg, struct sock *sk, struct sk_buff *skb)
{
	sock_recv_timestamp(msg, sk, skb);
}
EXPORT_SYMBOL(rks_sock_recv_timestamp);

void  *__rks_alloc_percpu(size_t size, size_t align)
{
	return __alloc_percpu(size, align);
}
EXPORT_SYMBOL(__rks_alloc_percpu);

void rks_free_percpu(void  *ptr)
{
	 free_percpu(ptr);
}
EXPORT_SYMBOL(rks_free_percpu);

struct crypto_blkcipher *rks_crypto_alloc_blkcipher(const char *alg_name, u32 type, u32 mask)
{
	return crypto_alloc_blkcipher(alg_name, type, mask);
}
EXPORT_SYMBOL(rks_crypto_alloc_blkcipher);

struct crypto_ablkcipher *rks_crypto_alloc_ablkcipher(const char *alg_name, u32 type, u32 mask)
{
	return crypto_alloc_ablkcipher(alg_name, type, mask);
}
EXPORT_SYMBOL(rks_crypto_alloc_ablkcipher);

void rks_crypto_free_blkcipher(struct crypto_blkcipher *tfm)
{
	crypto_free_blkcipher(tfm);
}
EXPORT_SYMBOL(rks_crypto_free_blkcipher);

void rks_crypto_free_ablkcipher(struct crypto_ablkcipher *tfm)
{
	crypto_free_ablkcipher(tfm);
}
EXPORT_SYMBOL(rks_crypto_free_ablkcipher);
struct workqueue_struct *rks_create_singlethread_wq(const char *wq_name)
{
    return create_singlethread_workqueue(wq_name);
}
EXPORT_SYMBOL(rks_create_singlethread_wq);

static int __init rks_api_init(void)
{
	printk(KERN_INFO "rks_api: initializing...\n");
	return 0;
}

static void __exit rks_api_exit(void)
{
	printk(KERN_INFO "rks_api: exiting...\n");
}

module_init(rks_api_init);
module_exit(rks_api_exit);

MODULE_AUTHOR("Ruckus Wireless");
MODULE_DESCRIPTION("Ruckus Wireless RKS API");
MODULE_LICENSE("GPL");
