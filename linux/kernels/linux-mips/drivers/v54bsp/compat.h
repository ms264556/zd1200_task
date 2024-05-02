/*
 * Copyright (c) 2004-2011 Ruckus Wireless, Inc.
 * All rights reserved.
 *
 */
 //Tony Chen 03-24-2011 Initial
#ifndef __COMPAT_H__
#define __COMPAT_H__
#include <linux/version.h>

#define SET_OWNER(dir, val)
#define PROC_ROOT_MK(name)               proc_mkdir((name), NULL)
#define PROC_ROOT_RM(name)               remove_proc_entry(name, NULL)
#define OPEN_BDEV_EXCL                   open_bdev_exclusive
#define BDEV_HARDSECT_SIZE               bdev_logical_block_size
#define CLOSE_BDEV_EXCL(dev, mode)       close_bdev_exclusive(dev, mode)
#define NOTIFIER_CHAIN_REGISTER          atomic_notifier_chain_register

#endif
