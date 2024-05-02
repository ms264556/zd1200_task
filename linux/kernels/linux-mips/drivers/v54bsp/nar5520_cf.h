/*
 * Copyright (c) 2004-2010 Ruckus Wireless, Inc.
 * All rights reserved.
 *
 */
 //Tony Chen 03-20-2008 Initial


#ifndef __NAR5520_CF_IO_H__
#define __NAR5520_CF_IO_H__

#include <linux/unistd.h>


#define READ_FAIL 1
#define OPEN_FAIL 2

#define CF_READ   0
#define CF_WRITE  1

struct nar5520_cf_head{
   uint32_t sector_start;
   uint32_t bytes_offset;
   uint32_t rw_cmd;
   const unchar* dev_path;
   const unchar* fs_type;
};

int32_t nar5520_cf_read_write(struct nar5520_cf_head* cfh, unchar* buf, int32_t len);
extern char himem_dev_path[];
extern char boarddata_dev_path[];
extern char kflag_dev1_path[];
extern char kflag_dev2_path[];
#endif
