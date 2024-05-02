/*
 * Copyright (c), 2005-2011, Ruckus Wireless, Inc.
 */

#ifndef __RKS_IOCTL_H__
#define __RKS_IOCTL_H__

#include <linux/compiler.h>
#include <linux/sockios.h>
//#include <ruckus/rks_ioctl_builtin.h>



/*
 * The macroes and structures definition for each module should be added to
 * rks_ioctl_builtin.h or its own header files.
 *
 * Note:
 * When new header files introduced,
 * please also update //depot/atheros/linux/driver/rks_include/Kbuild to make sure
 * toolchain header files can be updated.
 */

struct rks_ioctl_req
{
	unsigned int module;
	unsigned int cmd;
	unsigned int length;
	void __user *data;
};

#ifdef __KERNEL__

int rks_ioctl_register(unsigned int module, int (*handler)(unsigned int, unsigned int, void *));
int rks_ioctl_deregister(unsigned int module);
int rks_ioctl(void __user *arg);
extern void rks_ioctl_set(int (*hook)(void __user *));

#endif

#endif /* __RKS_IOCTL_H__ */
