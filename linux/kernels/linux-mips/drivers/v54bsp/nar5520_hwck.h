/*
 * Copyright (c) 2004-2010 Ruckus Wireless, Inc.
 * All rights reserved.
 *
 */
 //Tony Chen 12-07-2010 Initial

#ifndef __NAR5520_HWCK_H__
#define __NAR5520_HWCK_H__

#include <linux/proc_fs.h>

typedef enum {
    UNKNOWN_HW = 0,
    NAR5520_HW,
    T5520UR_HW,
    COB7402_HW,
    MAX_HW_TYPE,
} hw_type_t;

hw_type_t get_platform_info(void);
int t5520ur_proc_create(struct proc_dir_entry* proc_base);
int t5520ur_proc_remove(struct proc_dir_entry* proc_base);

#endif
