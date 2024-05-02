/*
 * Ruckus Wireless driver debug module
 *
 * Copyright (c) 2013 Ruckus Wireless, Inc.
 * All rights reserved.
 * Author    : Manjunath Goudar
 */

#ifndef _RKS_KERNEL_DEBUG_H
#define _RKS_KERNEL_DEBUG_H

#include <linux/kernel.h>


#define RKS_KERN_EMERG      0    /* system is unusable */
#define RKS_KERN_ALERT      1    /* action must be taken immediately */
#define RKS_KERN_CRIT       2    /* critical conditions */
#define RKS_KERN_ERR        3    /* error conditions */
#define RKS_KERN_WARNING    4    /* warning conditions */
#define RKS_KERN_NOTICE     5    /* normal but significant condition */
#define RKS_KERN_INFO       6    /* informational */
#define RKS_KERN_DEBUG      7    /* debug-level messages */
#define DBG_SEVERITY_MAX    8


#define DEFAULT_SEVERITY    RKS_KERN_WARNING







#define RKS_DEBUG_PIF 9
#define DEFAULT_SEVERITY    RKS_KERN_WARNING


#define RKSIOCTL_RKSDBG_GET 0x1
#define RKSIOCTL_RKSDBG_SET 0x2

#if defined(CONFIG_RKSDBG)
#define rks_dbg(mod, sev, fmt, args...) do{ if(sev > DEFAULT_SEVERITY) printk(fmt , ##args); }while(0);
#else
#define rks_dbg(mod, sev, fmt, args...) do{}while(0);
#endif



#endif //_RKS_KERNEL_DBG_H end
