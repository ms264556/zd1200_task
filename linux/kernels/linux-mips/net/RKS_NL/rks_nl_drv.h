/*
 * rks_nl_drv.h
 * Copyright (C) 2012 Ruckus Wireless, Inc.
 * All rights reserved.
 *
 * Ruckus generic netlink facility
 * between driver and application
 */

#ifndef _RKS_NL_DRV_H_
#define _RKS_NL_DRV_H_



#include <linux/netlink.h>
#include <linux/if_ether.h>




#define RKS_GENL_NOERR                        (0)
#define RKS_GENL_ERRNO_INVALID_PARAM          (1)
#define RKS_GENL_ERRNO_NO_MEM                 (2)
#define RKS_GENL_ERRNO_NO_SOCKFD              (3)
#define RKS_GENL_ERRNO_CMD_OUTOFRANGE         (4)
#define RKS_GENL_ERRNO_NEWMSG_FAIL            (5)
#define RKS_GENL_ERRNO_PUTMSG_FAIL            (6)
#define RKS_GENL_ERRNO_DUPLICATE_OP           (7)
#define RKS_GENL_ERRNO_RECV_DATA              (8)
#define RKS_GENL_ERRNO_SUBSCRIBE_MEMBERSHIP   (9)
#define RKS_GENL_ERRNO_UNSUBSCRIBE_MEMBERSHIP (10)


enum  rks_genl_group_id {
    RKS_GENL_DEFAULT_GRP_ID =1,

    RKS_GENL_CLIENT_DIAGNOSTICS_GRP_ID,

};


#define RKS_ATTR_MAX                        (256)
#define RKS_CMD_MAX                         (256)




#define RKS_GENL_FAMILY_NAME    "DRVAPP_GENL"

#ifdef __KERNEL__

int rks_genl_drv_register_get(int cmd,int (*drv_get_data)(struct nlattr *tb[]));
int rks_genl_drv_unregister_get(int cmd);


void rks_genl_notify_send(int cmd,int grpid,int attr,
                          const void *data,int len);

int rks_genl_drv_get_data(int attr,
                          struct nlattr *tb[],
                          void **data,int *len);

int rks_genl_drv_get_group_id(const char *group_name);

#endif /* end #ifdef __KERNEL__ */

#endif
