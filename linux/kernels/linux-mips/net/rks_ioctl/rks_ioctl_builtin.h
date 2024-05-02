/*
 * Copyright (c), 2005-2012, Ruckus Wireless, Inc.
 */

#ifndef __RKS_IOCTL_BUILTIN_H__
#define __RKS_IOCTL_BUILTIN_H__

/*------------------------ Includes ------------------------------------------*/
#include <linux/compiler.h>


#ifndef IFNAMSIZ
#define	IFNAMSIZ	16
#endif

struct req_fwdpolicy
{
    char ifname[IFNAMSIZ];
    int         *data;
};

#define FWDPOLICY_IOCTL_SET          0
#define FWDPOLICY_IOCTL_GET          1
int fwdpolicy_ioctl(unsigned int cmd, unsigned int length, void *data);

struct req_wlanid
{
    char ifname[IFNAMSIZ];
    short         *data;
};

#define WLANID_IOCTL_SET             0
#define WLANID_IOCTL_GET             1
int wlanid_ioctl(unsigned int cmd, unsigned int length, void *data);



#endif /* __RKS_IOCTL_BUILTIN_H__ */
