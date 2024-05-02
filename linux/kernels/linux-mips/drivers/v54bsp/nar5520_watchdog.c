/*
 * Copyright (c) 2004-2010 Ruckus Wireless, Inc.
 * All rights reserved.
 *
 */
 //Tony Chen    2008-03-04   Initial

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include <linux/kthread.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/mm.h>
#include <linux/hardirq.h>

#include "nar5520_superio_gpio.h"
#include "t5520ur_ipmi.h"
#include "nar5520_cf.h"
#include "nar5520_hwck.h"
#include "compat.h"
#include "v54_himem.h"

#define TIMEOUTTRG (12)
#define IN_MS   0x01
#define IN_SEC  0x02
#define IN_MIN  0x03


#if defined(__KERNEL__)
#include <linux/kernel.h>
#define V54_PRINTF printk
#define DBG_LEVEL
#else
#define V54_PRINTF(...)
#define DBG_LEVEL KERN_DEBUG
#endif

#ifdef __DEBUG__
#define WDT_TRACK(fmt, args...) V54_PRINTF(DBG_LEVEL "#### [%s] "fmt" ####\n", __FUNCTION__, ## args )
#define WDT_PRINTF WDT_TRACK
#else
#define WDT_TRACK(fmt, args...) V54_PRINTF(DBG_LEVEL "%s: "fmt"\n", __FUNCTION__, ## args )
#define WDT_PRINTF(...)
#endif


#define GPIO6_DATA_REG		CRF5
#define GPIO6_INVERSION_REG	CRF6
#define GAME_PORT_PAD_CTRL_REG	CRF7

#define WDT_DEVICE		LDN8

void v54_on_reboot(u_int32_t reason);





static int8_t nar5520_wdt_refresh(uint8_t period)
{
   unchar tmp = period;

   W627_WRITE_PORT(GPIO6_INVERSION_REG, tmp);

   return 0;
}




static int8_t nar5520_wdt_set_grain(unchar grain)
{
   unchar val=0x00;

   switch(grain){
   case IN_MS:
      WDT_PRINTF("Watchdog set to msec");

      W627_READ_PORT(GPIO6_DATA_REG, val);
      W627_WRITE_PORT(GPIO6_DATA_REG, (val|0x10));
      W627_READ_PORT(GPIO6_DATA_REG, val);
      W627_WRITE_PORT(GPIO6_DATA_REG, (val&0xf7));
   break;
   case IN_SEC:
      WDT_PRINTF("Watchdog set to sec");

      W627_READ_PORT(GPIO6_DATA_REG, val);
      W627_WRITE_PORT(GPIO6_DATA_REG, (val&0xef));
      W627_READ_PORT(GPIO6_DATA_REG, val);
      W627_WRITE_PORT(GPIO6_DATA_REG, (val&0xf7));
   break;
   case IN_MIN:
      WDT_PRINTF("Watchdog set to min");

      W627_READ_PORT(GPIO6_DATA_REG, val);
      W627_WRITE_PORT(GPIO6_DATA_REG, (val&0xef));
      W627_READ_PORT(GPIO6_DATA_REG, val);
      W627_WRITE_PORT(GPIO6_DATA_REG, (val|0x08));
   break;
   default:
      WDT_PRINTF("Watchdog set to default");

      W627_READ_PORT(GPIO6_DATA_REG, val);
      W627_WRITE_PORT(GPIO6_DATA_REG, (val&0x7f));
   break;
   }

   return 0;
}

static int8_t nar5520_wdt_disable(void)
{
   W627_OPEN_PORT();

   outb(GPIO6_INVERSION_REG, W627_CONFIG);
   outb(W627_END, W627_DATA);

   W627_CLOSE_PORT();
   return 0;
}

static int8_t nar5520_wdt_open(void)
{
   unchar val = 0x00;

   switch(get_platform_info()) {
      case NAR5520_HW: {
         W627_OPEN_PORT();


         W627_READ_PORT(CR2D, val);
         W627_WRITE_PORT(CR2D, (val&0xfe));


         outb(CR07, W627_CONFIG);
         outb(WDT_DEVICE, W627_DATA);

         W627_READ_PORT(CR30, val);
         W627_WRITE_PORT(CR30, (val|0x01));


         W627_READ_PORT(GPIO6_DATA_REG, val);
         W627_WRITE_PORT(GPIO6_DATA_REG, (val&0xfb));


         W627_READ_PORT(GAME_PORT_PAD_CTRL_REG, val);
         W627_WRITE_PORT(GAME_PORT_PAD_CTRL_REG, (val&0x3f));

         return 0;
      }
      case T5520UR_HW: {

         return 0;
      }
      case COB7402_HW: {
         W627_OPEN_PORT();

         W627_READ_PORT(CR2D, val);
         W627_WRITE_PORT(CR2D, (val&0xfe));


         outb(CR07, W627_CONFIG);
         outb(WDT_DEVICE, W627_DATA);

         W627_READ_PORT(CR30, val);
         W627_WRITE_PORT(CR30, (val|0x01));

         W627_READ_PORT(GPIO6_DATA_REG, val);
         W627_WRITE_PORT(GPIO6_DATA_REG, (val&0xfb));

         W627_READ_PORT(GAME_PORT_PAD_CTRL_REG, val);
         W627_WRITE_PORT(GAME_PORT_PAD_CTRL_REG, (val&0x3f));
         return 0;
      }
      default:
         break;
   }

   return -1;
}

static int8_t nar5520_wdt_close(void)
{
   switch(get_platform_info()) {
      case NAR5520_HW: {
         W627_CLOSE_PORT();
         return 0;
      }
      case T5520UR_HW: {

         return 0;
      }
      case COB7402_HW: {
         W627_CLOSE_PORT();
         return 0;
      }
      default:
         break;
   }
   return -1;
}




#define KFLAG_FORCE_RETRY     "1"
#define KFLAG_KERNEL_LOADED   "2"
#define KFLAG_KERNEL_WDT_INIT "3"
#define KFLAG_KERNEL_READY    "4"
#define KFLAG_SYSTEM_WDT_INIT "5"
#define KFLAG_SYSTEM_OOM      "6"

#define KFLAG_BOUNDARY        "7"

#define KFLAG_SYSTEM_READY    "8"
#define KFLAG_WDT_REBOOT      "9"

#define KFLAG_SECTOR  0x0
#define KFLAG_OFFSET  0x0
#define KFLAG_OFFSET1_NAR5520 20160
#define KFLAG_OFFSET2_NAR5520 509040
#define KFLAG_OFFSET1_T5520UR 53816
#define KFLAG_OFFSET2_T5520UR 3982384
#define KFLAG_FS_TYPE "ext2"

static int8_t write_kflag(const char* data, const char* device, int offset)
{
   struct nar5520_cf_head cfh;
   cfh.sector_start = KFLAG_SECTOR;
   cfh.bytes_offset = offset;
   cfh.rw_cmd = CF_WRITE;
   cfh.dev_path = device;
   cfh.fs_type = KFLAG_FS_TYPE;
   if(nar5520_cf_read_write(&cfh, (unchar*)data, strlen(data)) != strlen(data)){
      WDT_TRACK("*** Write to CF card failed, device %s not ready ***", device);
      return 1;
   }

   return 0;
}

#define KFLAG_DEV_R   "/dev/root"

char kflag_dev1_path[10] = {0};
char kflag_dev2_path[10] = {0};
static DEFINE_MUTEX(kflag_lock);
static int8_t write_kflags(const char* data)
{
   int8_t err = 0;

   WARN_ON(in_interrupt());

   mutex_lock(&kflag_lock);

   err |= write_kflag(data, kflag_dev1_path, KFLAG_OFFSET);
   err |= write_kflag(data, kflag_dev2_path, KFLAG_OFFSET);

   mutex_unlock(&kflag_lock);
   return 0;
}




#define WATCHDOG_USERSPACE_TIMEOUT (61)
#define NUM_GCPUS                  (2)
static atomic_t userspace_timeout = ATOMIC_INIT(WATCHDOG_USERSPACE_TIMEOUT*NUM_GCPUS);
static atomic_t userspace_wdt = ATOMIC_INIT(1);
static int8_t cpu_count = 0;

int nar5520_wdt_thread(void *data)
{



   WDT_PRINTF("cpu:%d in slot:%d, shift: %u\n",
      *(int8_t*)data, (*(int8_t*)data/NUM_GCPUS+1), TIMEOUTTRG*(*(int8_t*)data/NUM_GCPUS+1));
   set_current_state(TASK_INTERRUPTIBLE);
   schedule_timeout(HZ*TIMEOUTTRG*(*(int8_t*)data/NUM_GCPUS+1));

   while(!kthread_should_stop()){

      if(atomic_read(&userspace_timeout) > 0){
         if(atomic_read(&userspace_wdt)){
            atomic_sub(TIMEOUTTRG, &userspace_timeout);
            WDT_PRINTF("user space watchdog timeout in %d sec.", atomic_read(&userspace_timeout));
         }

         switch(get_platform_info()) {
            case NAR5520_HW: {
               nar5520_wdt_open();


               nar5520_wdt_set_grain(IN_SEC);
               nar5520_wdt_refresh(TIMEOUTTRG*3);
               nar5520_wdt_close();
               WDT_PRINTF("refresh NAR5520 watchdog, timeout %d sec, cpu %d", TIMEOUTTRG*3, *(int8_t*)data);

               break;
            }
            case T5520UR_HW: {
               t5520ur_timerop_t ops = {
                  .time    = TIMEOUTTRG*3,
                  .op      = IPMI_WD_RESET_TIMER,
                  .use     = IPMI_WD_USE_SMS_RUN,
                  .act     = IPMI_WD_ACTION_RESET,
                  .cpu_id  = *(int8_t*)data,
                  .retries = -1,
                  .timeout = 0
               };
               t5520ur_wd_settimer(&ops);
               WDT_PRINTF("refresh T5520UR watchdog, timeout %d sec, cpu %d", ops.time, ops->cpu_id);
               break;
            }
            case COB7402_HW: {
               nar5520_wdt_open();

               nar5520_wdt_set_grain(IN_SEC);
               nar5520_wdt_refresh(TIMEOUTTRG*3);
               nar5520_wdt_close();
               WDT_PRINTF("refresh COB7402 watchdog, timeout %d sec, cpu %d", TIMEOUTTRG*3, *(int8_t*)data);
               break;
            }
            default:
               break;
         }
      }else{
         printk("warning: u-watchdog timeout, system may reboot\n");
         v54_on_reboot(REBOOT_WATCHDOG);
         write_kflags(KFLAG_WDT_REBOOT);
      }



      WDT_PRINTF("schedule next watchdog tick in %u sec, cpu %d\n", TIMEOUTTRG*(cpu_count/NUM_GCPUS), *(int8_t*)data);
      set_current_state(TASK_INTERRUPTIBLE);
      schedule_timeout(HZ*TIMEOUTTRG*(cpu_count/NUM_GCPUS));
   }

   WDT_TRACK("NAR5520 watchdog stopped");
   return 0;
}




#define WATCHDOG_MINOR 130
static ssize_t
wd_feeder_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
   if(count){
      int i;
      for(i=0;i<count;i++){
         char c;
         if (get_user(c, buf + i)) return 0;
         switch(c){
         case 'Y': {
            if(T5520UR_HW == get_platform_info()) {
               t5520ur_timerop_t ops = {
                  .time    = TIMEOUTTRG*3,
                  .op      = IPMI_WD_SET_TIMER,
                  .use     = IPMI_WD_USE_SMS_RUN,
                  .act     = IPMI_WD_ACTION_RESET,
                  .cpu_id  = 0,
                  .retries = -1,
                  .timeout = 0
               };
               t5520ur_wd_settimer(&ops);
            }
            write_kflags(KFLAG_SYSTEM_READY);
         }
         break;
         case 'N':
            write_kflags(KFLAG_FORCE_RETRY);
         break;
         case 'D':
            atomic_set(&userspace_wdt, 0);
         break;
         case 'E':
            atomic_set(&userspace_wdt, 1);
         break;
         default:
            if(atomic_read(&userspace_wdt)){
               atomic_set(&userspace_timeout, WATCHDOG_USERSPACE_TIMEOUT);
            }
         break;

         }
      }
   }

   return 1;
}

static int
wd_feeder_open(struct inode *inode, struct file *file)
{
   atomic_set(&userspace_timeout, WATCHDOG_USERSPACE_TIMEOUT*NUM_GCPUS);

   write_kflags(KFLAG_SYSTEM_WDT_INIT);
   return 0;
}

static int
wd_feeder_close(struct inode *inode, struct file *file)
{
   atomic_set(&userspace_timeout, WATCHDOG_USERSPACE_TIMEOUT*NUM_GCPUS);
   return 0;
}

static struct file_operations wd_feeder_fops = {
   owner:  THIS_MODULE,
   llseek:  NULL,
   read:    NULL,
   write:   wd_feeder_write,
   ioctl:   NULL,
   open:    wd_feeder_open,
   release: wd_feeder_close,
};

static struct miscdevice wd_feeder_miscdev = {
   WATCHDOG_MINOR,
   "watchdog",
   &wd_feeder_fops
};



static struct task_struct *ptask[NR_CPUS];
static int8_t cpu_id[NR_CPUS];
int8_t nar5520_wdt_init(void)
{
   int8_t cpu;
   WDT_TRACK("NAR5520 Watchdog init");

   if(T5520UR_HW == get_platform_info()) {
      t5520ur_timerop_t ops = {
         .time    = TIMEOUTTRG*6,
         .op      = IPMI_WD_SET_TIMER,
         .use     = IPMI_WD_USE_OS_LOAD,
         .act     = IPMI_WD_ACTION_RESET,
         .cpu_id  = 0,
         .retries = -1,
         .timeout = 0
      };

      t5520ur_wd_init();
      t5520ur_wd_settimer(&ops);
   }

   for_each_online_cpu(cpu){
      cpu_id[cpu] = cpu;
      ptask[cpu] = kthread_create(nar5520_wdt_thread, &cpu_id[cpu], "V54_watchdog/%d", cpu);
      if(!IS_ERR(ptask[cpu])){
         cpu_count++;
         kthread_bind(ptask[cpu], cpu);
         wake_up_process(ptask[cpu]);
      }else{
         WDT_TRACK("Error while binding watchdog to cpu %d", cpu);
      }
   }


   misc_register(&wd_feeder_miscdev);



   return 0;
}

void nar5520_wdt_exit(void)
{
   int8_t cpu;
   WDT_TRACK("NAR5520 Watchdog exit");

   for_each_online_cpu(cpu){
      if(ptask[cpu]){
         kthread_stop(ptask[cpu]);
      }
   }

   switch(get_platform_info()) {
      case NAR5520_HW: {

         nar5520_wdt_disable();
         break;
      }
      case T5520UR_HW: {
         t5520ur_wd_exit();
         break;
      }
      case COB7402_HW: {
         nar5520_wdt_disable();
         break;
      }
      default:
         break;
   }


   misc_deregister(&wd_feeder_miscdev);

   return;
}


typedef void (*reboot_callback_t)(u_int32_t reason);
reboot_callback_t v54_reboot_callback = NULL;

void v54_register_on_reboot(reboot_callback_t reboot_callback)
{
   v54_reboot_callback = reboot_callback;
   return;
}

void v54_on_reboot(u_int32_t reason)
{
   if ( v54_reboot_callback != NULL ) {
      v54_reboot_callback(reason);
   }

   return;
}


void v54_oom_killer(void)
{
   printk("System out of memory!!\n");
   write_kflags(KFLAG_SYSTEM_OOM);
   printk("=======================================================\n");

   show_mem();

   printk("=======================================================\n");
}
