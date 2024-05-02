/*
 * Copyright (c), 2005-2012, Ruckus Wireless, Inc.
 */

/*------------------------ Includes ------------------------------------------*/
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <ruckus/rks_ioctl.h>
#include "rks_ioctl_builtin.h"



int tos_conf_ioctl(unsigned int cmd, unsigned int length, void *data)
{
	return 0;
}

static int fwdpolicy_ioctl_get(struct net_device *dev, struct req_fwdpolicy *req)
{
    if(copy_to_user(req->data, &(dev->rks_policy), sizeof(int)))
        return -EFAULT;
    return 0;
}

static int fwdpolicy_ioctl_set(struct net_device *dev, struct req_fwdpolicy *req)
{
    int value = 0;
    if(copy_from_user((void *)&value, req->data, sizeof(int)))
        return -EFAULT;

    dev->rks_policy = value;
    return 0;
}

int fwdpolicy_ioctl(unsigned int cmd, unsigned int length, void *data)
{
	int err = 0;
	struct net_device *dev = NULL;
	struct req_fwdpolicy *req = (struct req_fwdpolicy *)data;

	dev = dev_get_by_name(&init_net, req->ifname);

	if(dev == NULL)
		return -ENODEV;

	switch (cmd) {
        case FWDPOLICY_IOCTL_SET:
            err = fwdpolicy_ioctl_set(dev, req);
            break;
        case FWDPOLICY_IOCTL_GET:
            err = fwdpolicy_ioctl_get(dev, req);
            break;
        default:
            err = -EFAULT;
	}
    dev_put(dev);
    return err;
}

static int wlanid_ioctl_get(struct net_device *dev, struct req_wlanid *req)
{
    if(copy_to_user(req->data, &(dev->wlan_id), sizeof(short)))
            return -EFAULT;

    return 0;
}

static int wlanid_ioctl_set(struct net_device *dev, struct req_wlanid *req)
{
    short value = 0;
    if(copy_from_user((void *)&value, req->data, sizeof(short)))
        return -EFAULT;

    dev->wlan_id = value;

    return 0;
}

int wlanid_ioctl(unsigned int cmd, unsigned int length, void *data)
{
	int err = 0;
        struct net_device *dev = NULL;
        struct req_wlanid *req = (struct req_wlanid *)data;

        dev = dev_get_by_name(&init_net, req->ifname);

        if(dev == NULL)
                return -ENODEV;

        switch (cmd) {
        case WLANID_IOCTL_SET:
            err = wlanid_ioctl_set(dev, req);
            break;
        case WLANID_IOCTL_GET:
            err = wlanid_ioctl_get(dev, req);
            break;
        default:
            err = -EFAULT;
        }
    dev_put(dev);
    return err;
}
