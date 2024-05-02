/*
 * Copyright (c) 2004-2008 Ruckus Wireless, Inc.
 * All rights reserved.
 *
 */
 //Tony Chen 03-20-2008 Initial



#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/mount.h>
#include <linux/hardirq.h>

#include "compat.h"
#include "nar5520_cf.h"

#if defined(__KERNEL__)
#include <linux/kernel.h>
#define V54_PRINTF printk
#define DBG_LEVEL
#else
#define V54_PRINTF(...)
#define DBG_LEVEL KERN_DEBUG
#endif

#ifdef __DEBUG__
#define CF_TRACK(fmt, args...) V54_PRINTF(DBG_LEVEL "#### [%s] "fmt" ####\n", __FUNCTION__, ## args )
#define CF_PRINTF CF_TRACK
#define CF_DEFINE nar5520_show_flags
#else
#define CF_TRACK(fmt, args...) V54_PRINTF(DBG_LEVEL "%s: "fmt"\n", __FUNCTION__, ## args )
#define CF_PRINTF(...)
#define CF_DEFINE(...)
#endif

#if CONFIG_V54_ZD_PLATFORM == 0
#define ZD_PART_SECTOR 3982101
#elif CONFIG_V54_ZD_PLATFORM == 1
#define ZD_PART_SECTOR 3927001
#endif

#define ZD_PART_MAGIC1 0x55
#define ZD_PART_MAGIC2 0xAA

static DEFINE_MUTEX(rw_lock);
int32_t nar5520_cf_read_write(struct nar5520_cf_head* cfh, unchar* buf, int32_t len)
{
   Sector sect;
   int32_t ret;
   uint32_t close_flag;
   uint32_t sec_size;
   uint32_t sec_start;
   uint32_t sec_offset;
   unchar* data;

   struct block_device* nar5520_bdev;
   struct file_system_type* ftype;


   WARN_ON(in_interrupt());

   mutex_lock(&rw_lock);

   ret = len;
   close_flag = 1;
   ftype = get_fs_type(cfh->fs_type);

   CF_PRINTF("Try open device: %s", cfh->dev_path);

   nar5520_bdev = OPEN_BDEV_EXCL(cfh->dev_path, MS_SYNCHRONOUS, ftype);
   if(IS_ERR(nar5520_bdev)){
      close_flag = 0;
      CF_PRINTF("Lookup device: %s", cfh->dev_path);
      nar5520_bdev = lookup_bdev(cfh->dev_path);
      if(IS_ERR(nar5520_bdev)){
         dev_t devt = name_to_dev_t((char*)(cfh->dev_path));
         if(devt)


      nar5520_bdev = open_by_devnum(devt, FMODE_WRITE|FMODE_READ);


         if(IS_ERR(nar5520_bdev)){
             CF_PRINTF("Device open fail");
             goto _DEV_OPEN_FAIL;
         }
      }

      data = read_dev_sector(nar5520_bdev, ZD_PART_SECTOR, &sect);
      if (!data || !(ZD_PART_MAGIC1==data[510] && ZD_PART_MAGIC2==data[511])){
         CF_PRINTF("No partition table present");
         put_dev_sector(sect);
         goto _DEV_OPEN_FAIL;
      }

   }



   sec_size = BDEV_HARDSECT_SIZE(nar5520_bdev);
   sec_start = cfh->sector_start+(cfh->bytes_offset/sec_size);
   sec_offset = cfh->bytes_offset%sec_size;

   CF_PRINTF("secter size = %d, sector offset = %d", sec_size, sec_offset);
   CF_PRINTF("request read from sector:%d, length:%d", sec_start, len);

   while(len>0){
      int32_t w_len;
      unchar* tmp = read_dev_sector(nar5520_bdev, sec_start, &sect);
      if(!tmp){
         CF_PRINTF("Read from sector error");
         goto _DEV_READ_FAIL;
      }

      tmp += sec_offset;
      w_len = sec_size-sec_offset;
      w_len = len>w_len?w_len:len;
      switch(cfh->rw_cmd){
      case CF_READ:
         CF_PRINTF("CF do read.");
         memcpy(buf, tmp, w_len);
      break;
      case CF_WRITE:
          CF_PRINTF("CF do write.");
          lock_page(sect.v);
          memcpy(tmp, buf, w_len);
          set_page_dirty(sect.v);
          write_one_page(sect.v, 1);
      break;
      default:
      break;
      }

      put_dev_sector(sect);
      sect.v = NULL;
      buf += w_len;
      len -= w_len;
      sec_offset = 0;
      sec_start++;
   }

_DEV_READ_END:
   if(close_flag){
      CLOSE_BDEV_EXCL(nar5520_bdev, MS_SYNCHRONOUS);
   }

   mutex_unlock(&rw_lock);
   return ret;


_DEV_READ_FAIL:
   ret = -READ_FAIL;
   goto _DEV_READ_END;

_DEV_OPEN_FAIL:
   ret = -OPEN_FAIL;
   goto _DEV_READ_END;
}
