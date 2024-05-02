#include <linux/genetlink.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/version.h>
#include <net/genetlink.h>

#include "rks_nl_drv.h"



struct rks_genl_op_map {
    int (*rks_drv_get)(struct nlattr *tb[]);
};

#define RKS_CHECK_POINTER(p) \
    do { \
        if(NULL == p) \
            return RKS_GENL_ERRNO_INVALID_PARAM; \
    } while(0)





static struct genl_family rks_genl_family = {
    .id      = GENL_ID_GENERATE,
    .hdrsize = 0,
    .name    = RKS_GENL_FAMILY_NAME,
    .version = 1,
    .maxattr = RKS_ATTR_MAX ,
};

struct rks_genl_op_map op_map[RKS_CMD_MAX + 1];


void rks_genl_notify_send(int cmd,int grpid,int attr,
                          const void *data,int len)
{
    void *hdr;
    struct sk_buff *genl_skb;
    int ret = RKS_GENL_NOERR;

    genl_skb = nlmsg_new(NLMSG_GOODSIZE, GFP_ATOMIC);
    if (!genl_skb){
        printk("%s: new netlink message error\n", __FUNCTION__);
        return;
    }


    hdr = genlmsg_put(genl_skb, /* skb */
                      //op_map[cmd].genl_snd_pid, /* pid */
                      0,
                      //op_map[cmd].genl_snd_seq++, /* seq */
                      0,
                      &rks_genl_family, /* type */
                      0, /* flags */
                      cmd /* cmd */
                      );

    if (!hdr){
        printk("%s: put netlink head error\n", __FUNCTION__);
        kfree_skb(genl_skb);
        return ;
    }

    nla_put(genl_skb,attr,len,data);

    genlmsg_end(genl_skb, hdr);

     ret = genlmsg_multicast(genl_skb, 0, grpid, GFP_KERNEL);

    return ;
}
EXPORT_SYMBOL(rks_genl_notify_send);


int __init rks_genl_init(void)
{
    int rc = 0;
    rc = genl_register_family(&rks_genl_family);
    if (!rc) {
        printk(KERN_INFO "register rks_genl_family succeeded\n");
    } else {
        printk(KERN_ERR "register rks_genl_family failed\n");
        return rc;
    }

   return rc;
}

void __exit rks_genl_exit(void)
{
    int rc = 0;

    rc = genl_unregister_family(&rks_genl_family);
    if (!rc) {
        printk(KERN_INFO "unregister rks_genl_family succeeded\n");
    } else {
        printk(KERN_ERR "unregister rks_genl_family failed\n");
    }

    return;
}


int rks_genl_drv_register_get(int cmd,
                             int (*drv_get_data)(struct nlattr *tb[]))
{
    if(cmd > RKS_CMD_MAX)
        return RKS_GENL_ERRNO_INVALID_PARAM;
    op_map[cmd].rks_drv_get = drv_get_data;
    return RKS_GENL_NOERR;
}
EXPORT_SYMBOL(rks_genl_drv_register_get);

int rks_genl_drv_unregister_get(int cmd)
{
   return rks_genl_drv_register_get(cmd,NULL);
}
EXPORT_SYMBOL(rks_genl_drv_unregister_get);



int rks_genl_drv_get_data(int attr,
                          struct nlattr *tb[],
                          void **data,int *len)
{
    if(attr >= RKS_ATTR_MAX)
        return RKS_GENL_ERRNO_CMD_OUTOFRANGE;
    RKS_CHECK_POINTER(data);
    RKS_CHECK_POINTER(tb[attr]);
    *data = (void*)nla_data(tb[attr]);
    if(len != NULL)
        *len = nla_len(tb[attr]);
    return RKS_GENL_NOERR;
}
EXPORT_SYMBOL(rks_genl_drv_get_data);


int rks_genl_drv_get_group_id(const char *group_name)
{
    int group_id = -1;

    if(strcmp(group_name, "ClientDiag") == 0) {
        group_id = RKS_GENL_CLIENT_DIAGNOSTICS_GRP_ID;
    }

    return group_id;
}
EXPORT_SYMBOL(rks_genl_drv_get_group_id);
