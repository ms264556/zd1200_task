/*
 * Copyright (c) 2004-2008 Ruckus Wireless, Inc.
 * All rights reserved.
 *
 */

#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <asm/io.h>

#include "compat.h"
#include "nar5520_cf.h"
#include "nar5520_hwck.h"
#include "t5520ur_ipmi.h"


#if defined(__KERNEL__)
#include <linux/kernel.h>
#define V54_PRINTF printk
#define DBG_LEVEL
#else
#define V54_PRINTF(...)
#define DBG_LEVEL KERN_DEBUG
#endif

#ifdef __DEBUG__
#define HWK_TRACK(fmt, args...) V54_PRINTF(DBG_LEVEL "#### [%s] "fmt" ####\n", __FUNCTION__, ## args )
#define HWK_PRINTF HWK_TRACK
#else
#define HWK_WARN(fmt, args...)  V54_PRINTF(KERN_EMERG fmt"\n", ## args )
#define HWK_TRACK(fmt, args...) V54_PRINTF(DBG_LEVEL "%s: "fmt"\n", __FUNCTION__, ## args )
#define HWK_PRINTF(...)
#endif

#define BODYGARD_TIMEOUT (HZ*600)
static struct completion scan_done_signal;

struct mem_footprint {
   uint32_t start_addr[3];
   uint32_t length;
   unsigned char footprint[32];
};

static int footprint_match(struct mem_footprint* fp)
{
   if(NULL == fp) {
      return 0;
   }

   while(fp->length){
      uint32_t *addr = &(fp->start_addr[0]);
      while(*addr) {
         int i;
         unsigned char* data = ioremap(*addr, fp->length);
         if(NULL == data) {
            return 0;
         }

         for(i=0, ++addr; i<fp->length; i++){
            if((fp->footprint[i]) != data[i]){
               if (!*addr) {
                  iounmap(data);
                  return 1;
               }
               break;
            }
         }
         iounmap(data);
         break;
      }
      fp++;
   }
   return 0;
}

static atomic_t rks_hardware_type = ATOMIC_INIT(UNKNOWN_HW);
static int nar5520_scan_mem(struct mem_footprint* fp, int size_footprint)
{
   int hw_idx, i;

   if((hw_idx = atomic_read(&rks_hardware_type)))

      return footprint_match(fp + (hw_idx - 1) * size_footprint);


   for(i=1; hw_idx < MAX_HW_TYPE-1 && i; hw_idx++) {
      if(T5520UR_HW == hw_idx+1) {
         i = t5520ur_hwck_scan();
      } else {
         i = footprint_match(fp + hw_idx * size_footprint);
      }
   }


   if(!i) {
      const char* root_dev;
      atomic_set(&rks_hardware_type, hw_idx);
      root_dev = strstr(saved_command_line, "root=");
      if (root_dev && strncmp(root_dev, "root=/dev/", 10) == 0) {
          strncpy(boarddata_dev_path, root_dev+5, 8);
      } else {
          switch(hw_idx) {
             case NAR5520_HW: {
                sprintf(boarddata_dev_path, "/dev/hda");
                break;
             }
             case T5520UR_HW: {
                sprintf(boarddata_dev_path, "/dev/sdb");
                break;
             }
             case COB7402_HW: {
                sprintf(boarddata_dev_path, "/dev/sda");
                break;
             }
          }
      }

      sprintf(himem_dev_path, "%s1", boarddata_dev_path);
      sprintf(kflag_dev1_path, "%s2", boarddata_dev_path);
      sprintf(kflag_dev2_path, "%s3", boarddata_dev_path);


      HWK_TRACK("\t Boarddata info:: %s\n", boarddata_dev_path);
      HWK_TRACK("\tHimem info: %s\n", himem_dev_path);
      HWK_TRACK("\tBoot flag: %s, %s\n", kflag_dev1_path, kflag_dev2_path);

      HWK_PRINTF("\tBoarddata info: %s\n", boarddata_dev_path);
      HWK_PRINTF("\tHimem info: %s\n", himem_dev_path);
      HWK_PRINTF("\tBoot flag: %s, %s\n", kflag_dev1_path, kflag_dev2_path);
   }

   HWK_TRACK("\tPlatform: %s\n", hw_idx==NAR5520_HW?"NAR5520":hw_idx==T5520UR_HW?"T5520UR":hw_idx==COB7402_HW?"COB7402":"UNKNOWN");
   HWK_PRINTF("\tPlatform: %s\n", hw_idx==NAR5520_HW?"NAR5520":hw_idx==T5520UR_HW?"T5520UR":hw_idx==COB7402_HW?"COB7402":"UNKNOWN");

   return i;
}

#define NUM_GCPUS      (2)
#define FOOTPRINT_SIZE (10)
static int8_t cpu_count = 0;
static int bodyguard_thread(void* data)
{

   struct mem_footprint bios_footprint[][FOOTPRINT_SIZE] = {
      {

          {{0xfe0b7, 0x0}, 16, {0x65,0x73,0x2c,0x20,0x4c,0x54,0x44,0x00,0x6f,0x80,0x52,0x75,0x63,0x6b,0x75,0x73}},
          {{0xfe0c7, 0x0}, 16, {0x20,0x57,0x69,0x72,0x65,0x6c,0x65,0x73,0x73,0x2c,0x20,0x49,0x6e,0x63,0x2e,0x20}},
          {{0xfe0d7, 0x0}, 16, {0x50,0x50,0x41,0x50,0x2d,0x33,0x37,0x36,0x31,0x56,0x4c,0x20,0x42,0x49,0x4f,0x53}},
          {{0x0}, 0x0, {0x0}}
      },
      {

          {{0x0}, 0x0, {0x0}}
      },
      {

          {{0x0}, 0x0, {0x0}}
      },
   };


   struct mem_footprint dmi_footprint[][FOOTPRINT_SIZE] = {
      {

          {{0xfe0c0,0x0}, 16, {0x80,0x52,0x75,0x63,0x6b,0x75,0x73,0x20,0x57,0x69,0x72,0x65,0x6c,0x65,0x73,0x73}},
          {{0xfe0d0,0x0}, 16, {0x2c,0x20,0x49,0x6e,0x63,0x2e,0x20,0x50,0x50,0x41,0x50,0x2d,0x33,0x37,0x36,0x31}},
          {{0xfe0e0,0x0}, 16, {0x56,0x4c,0x20,0x42,0x49,0x4f,0x53,0x20,0x52,0x65,0x76,0x2e,0x3a,0x52,0x31,0x2e}},
          {{0xfe0f0,0x0}, 16, {0x31,0x30,0x2e,0x57,0x33,0x20,0x28,0x20,0x30,0x34,0x30,0x31,0x32,0x30,0x30,0x38}},
          {{0xfe100,0x0}, 16, {0x20,0x29,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xde}},
          {{0x0}, 0x0, {0x0}}
      },
      {


          {{0x0}, 0x0, {0x0}}
      },
      {

          {{0x000E2BA9,0x000E2BA9-0x11,0x0}, 16, {0x52,0x75,0x63,0x6B,0x75,0x73,0x20,0x57,0x69,0x72,0x65,0x6c,0x65,0x73,0x73,0x20}},
          {{0x000E2BB9,0x000E2BB9-0x11,0x0}, 16, {0x5A,0x6F,0x6E,0x65,0x20,0x44,0x69,0x72,0x65,0x63,0x74,0x6f,0x72,0x20,0x74,0x79}},
          {{0x0}, 0x0, {0x0}}
      },
   };

   set_current_state(TASK_INTERRUPTIBLE);
   schedule_timeout(BODYGARD_TIMEOUT*(*(int8_t*)data/NUM_GCPUS));

   while(1){
      if(nar5520_scan_mem((struct mem_footprint*) bios_footprint, FOOTPRINT_SIZE) ||
         nar5520_scan_mem((struct mem_footprint*) dmi_footprint, FOOTPRINT_SIZE)){
         while (1) {
             HWK_WARN("================================================");
             HWK_WARN("* !!! WARNING WARNING WARNING!!!               *");
             HWK_WARN("* You are not authorized to copy or duplicate  *");
             HWK_WARN("* software/firmware in any format.             *");
             HWK_WARN("* If you think that you use it legally, please *");
             HWK_WARN("* contact customer services                    *");
             HWK_WARN("================================================");

             kernel_halt();
         }
      }


      complete_all(&scan_done_signal);
      set_current_state(TASK_INTERRUPTIBLE);
      schedule_timeout(BODYGARD_TIMEOUT*(cpu_count/NUM_GCPUS));
   }
}

hw_type_t get_platform_info(void)
{
   return atomic_read(&rks_hardware_type);
}
EXPORT_SYMBOL(get_platform_info);


#include <linux/proc_fs.h>
struct entry_table
{
    const char *entry_name;
    struct proc_dir_entry *proc_entry;
    int (*read_proc)(char *, char**, off_t, int, int *, void*);
    int (*write_proc)(struct file *, const char *, unsigned long , void *);
    struct file_operations *fop_proc;
    void *data;
};

static int proc_read_info(char* page, char** start, off_t off, int count, int* eof, void* data)
{
    switch(atomic_read((atomic_t*)data)) {
        case NAR5520_HW:
            return sprintf(page, "NAR5520");
            break;
        case T5520UR_HW:
            return sprintf(page, "T5520UR");
            break;
        case COB7402_HW:
            return sprintf(page, "COB7402");
            break;
        default:
            break;
    }

    return sprintf(page, "UNKNOWN");
}

static int proc_read_rbddev(char* page, char** start, off_t off, int count, int* eof, void* data)
{
    return sprintf(page, boarddata_dev_path);
}

static int proc_write_rbddev(struct file *file, const char *buffer,
                           unsigned long count, void*data)
{
    int len=0;

    if ( copy_from_user(boarddata_dev_path+5, buffer, 3)) {
        len = -EFAULT;
        return len;
    }

    return count;
}

#define PROC_BASE     "rks_hw_info"
static struct proc_dir_entry *proc_base;
static struct entry_table proc_node[] = {
    {"platform", NULL, proc_read_info, NULL, NULL, &rks_hardware_type},
    {"rbd_dev", NULL, proc_read_rbddev, proc_write_rbddev, NULL, &rks_hardware_type},
    {NULL,       NULL, NULL,      NULL,       NULL, NULL}
};

static int nar5520_proc_remove(void)
{
    struct entry_table *p = proc_node;
    while(p->proc_entry) {
        remove_proc_entry(p->entry_name, proc_base);
        p++;
    }

    t5520ur_proc_remove(proc_base);

    PROC_ROOT_RM(PROC_BASE);
    return 0;
}


static int nar5520_proc_create(void)
{
    struct entry_table *p;

    if(!(proc_base = PROC_ROOT_MK(PROC_BASE))) {
        goto err;
    }

    p = proc_node;
    while(p->entry_name) {
        if(!(p->proc_entry = create_proc_entry(p->entry_name, 0644, proc_base))) {
            goto err;
        }


        SET_OWNER(p->proc_entry, THIS_MODULE);
        if(p->fop_proc) {
            p->proc_entry->proc_fops = p->fop_proc;
        } else {
            p->proc_entry->read_proc = p->read_proc;
            p->proc_entry->write_proc = p->write_proc;
            p->proc_entry->data = p->data;
        }
        p++;
    }

    if(t5520ur_proc_create(proc_base)) {
        goto err;
    }

    return 0;

err:
    nar5520_proc_remove();
    return -ENOMEM;
}

static int8_t cpu_id[NR_CPUS];
int8_t nar5520_bodyguard_init(void)
{
   int8_t cpu;
   struct task_struct *ptask;

   HWK_TRACK("NAR5520 Bodyguard init");

   t5520ur_hwck_init();
   init_completion(&scan_done_signal);
   for_each_online_cpu(cpu){
      cpu_id[cpu] = cpu;
      ptask = kthread_create(bodyguard_thread, (void*) &cpu_id[cpu], "V54_guard/%d", cpu);
      if(!IS_ERR(ptask)){
         cpu_count++;
         kthread_bind(ptask, cpu);
         wake_up_process(ptask);
      }else{
         HWK_TRACK("Error while binding bodyguard to cpu %d", cpu);
      }
   }

   wait_for_completion(&scan_done_signal);
   nar5520_proc_create();

   return 0;
}
