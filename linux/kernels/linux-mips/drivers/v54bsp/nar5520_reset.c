/*
 * Copyright (c) 2004-2010 Ruckus Wireless, Inc.
 * All rights reserved.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/reboot.h>
#include <linux/nmi.h>
#include <linux/version.h>
#include <asm/nmi.h>

#include "nar5520_ich7_gpio.h"
#include "nar5520_superio_gpio.h"
#include "nar5520_hwck.h"
#include "v54_himem.h"


#if defined(__KERNEL__)
#include <linux/kernel.h>
#define V54_PRINTF printk
#define DBG_LEVEL
#else
#define V54_PRINTF(...)
#define DBG_LEVEL
#endif

#ifdef __DEBUF__
#define RFD_TRACK(fmt,args...) V54_PRINTF(DBG_LEVEL "#### [%s] "fmt" ####\n", __FUNCTION__, ## args)
#define RFD_PRINTF RFD_TRACK
#else
#define RFD_TRACK(fmt, args...) V54_PRINTF(DBG_LEVEL "%s: "fmt"\n", __FUNCTION__, ## args)
#define RFD_PRINTF(...)
#endif


#define HOLD_TIME          (HZ*4)
#define RESET_SAMPLE_TIME  (HZ/4)
#define EXCEED_HOLD_TIME() ((jiffies - first_reset_time) >= HOLD_TIME)




static struct completion reset_timer_done;
int nar5520_killer_thread(void *data)
{
   do {
      wait_for_completion_interruptible(&reset_timer_done);
   } while (!reset_timer_done.done);
   schedule_timeout(HZ/2);
   V54_HIMEM_SET_FACTORY(1);
   printk(KERN_EMERG "Restarting system.\n");
   machine_restart(NULL);
   return 0;
}
static void cob7402_check_button(unchar* button)
{
    unchar val = 0x00;
    W627_OPEN_PORT();

    outb(CR07, W627_CONFIG);
    outb(LDN9, W627_DATA);

    W627_READ_PORT(CR30, val);
    W627_WRITE_PORT(CR30, (val | 0x02));

    W627_READ_PORT(CRF1, val);
    if(val & 0x02)
    {

        *button = 1;
    }
    else
    {

        *button = 0;
    }

    W627_CLOSE_PORT();
}

static void cob7402_reset_button()
{
    unchar val = 0x00;
    W627_OPEN_PORT();

    outb(CR07, W627_CONFIG);
    outb(LDN9, W627_DATA);

    W627_READ_PORT(CR30, val);
    W627_WRITE_PORT(CR30, (val | 0x02));

    W627_READ_PORT(CRF1, val);
    W627_WRITE_PORT(CRF1, (val & 0xfe));

    W627_CLOSE_PORT();
    return;
}



static struct timer_list hold_timer;
static unsigned long first_reset_time;
static void check_reset_time(unsigned long init_time)
{
   switch(get_platform_info()) {
      case NAR5520_HW: {

         unchar holding_button = 0x0;
         nar5520_ich7_gpio_read_GPI6(holding_button);

         if(holding_button){
            if(EXCEED_HOLD_TIME()){
               RFD_TRACK("Reset to factory default (F/D)");
               del_timer(&hold_timer);
               complete_all(&reset_timer_done);
               return;
            }
         }else{
            first_reset_time = jiffies;
         }


         nar5520_ich7_gpio_reset_GPI6();
         mod_timer(&hold_timer, first_reset_time + RESET_SAMPLE_TIME);

         break;
      }
      case T5520UR_HW: {

         break;
      }
      case COB7402_HW: {
         unchar holding_button = 0x0;
         cob7402_check_button(&holding_button );
         if(holding_button){
            if(EXCEED_HOLD_TIME()){
               RFD_TRACK("Reset to factory default (F/D)");
               del_timer(&hold_timer);
               complete_all(&reset_timer_done);
               return;
            }
            else {

               mod_timer(&hold_timer, jiffies + RESET_SAMPLE_TIME);
            }
         }else{


            cob7402_reset_button();
            first_reset_time = jiffies;
            mod_timer(&hold_timer, first_reset_time + RESET_SAMPLE_TIME);
         }
         break;
      }
      default:
         break;
   }

   return;
}




static void cob7402_reset_init()
{
    unchar val = 0x00;
    W627_OPEN_PORT();

    outb(CR07, W627_CONFIG);
    outb(LDNA, W627_DATA);

    W627_READ_PORT(CR30, val);
    W627_WRITE_PORT(CR30, (val | 0x01));

    W627_READ_PORT(CRFE, val);
    W627_WRITE_PORT(CRFE, (val & 0xcc));
    W627_CLOSE_PORT();

    W627_OPEN_PORT();

    outb(CR07, W627_CONFIG);
    outb(LDN9, W627_DATA);

    W627_READ_PORT(CR30, val);
    W627_WRITE_PORT(CR30, (val | 0x02));

    W627_READ_PORT(CRFE, val);
    W627_WRITE_PORT(CRFE, ((val & 0xcf) & 0xfc) | 0x03);

    W627_READ_PORT(CRF2, val);
    W627_WRITE_PORT(CRF2, (val & 0xfc));

    W627_READ_PORT(CRF0, val);
    W627_WRITE_PORT(CRF0, ((val & 0xfe) & 0xfd) | 0x02);

    W627_READ_PORT(CRF1, val);
    W627_WRITE_PORT(CRF1, (val & 0xfe));
    W627_CLOSE_PORT();
}
static int8_t nar5520_reset_init(void)
{
   int8_t cpu;
   struct task_struct *pFDreset;
   hw_type_t hw_platform = get_platform_info();

   if (NAR5520_HW == hw_platform) {
      unchar test = 0x0;
      nar5520_ich7_gpio_init();
      nar5520_ich7_gpio_setpindest();
      nar5520_ich7_gpio_read_GPI6(test);
   }
   if (COB7402_HW == hw_platform) {
      cob7402_reset_init();
   }

   init_completion(&reset_timer_done);
   for_each_online_cpu(cpu){
      pFDreset = kthread_create(nar5520_killer_thread, (void*)&cpu, "V54_FDreset/%d", cpu);
      if(!IS_ERR(pFDreset)){
         kthread_bind(pFDreset, cpu);
         wake_up_process(pFDreset);
      }
   }

   switch(hw_platform) {
      case NAR5520_HW: {
         first_reset_time = jiffies;
         init_timer(&hold_timer);
         hold_timer.expires = jiffies + RESET_SAMPLE_TIME;
         hold_timer.data = first_reset_time;
         hold_timer.function = check_reset_time;
         add_timer(&hold_timer);
         break;
      }
      case T5520UR_HW: {

         break;
      }
      case COB7402_HW: {
         first_reset_time = jiffies;
         init_timer(&hold_timer);
         hold_timer.expires = jiffies + RESET_SAMPLE_TIME;
         hold_timer.data = first_reset_time;
         hold_timer.function = check_reset_time;
         add_timer(&hold_timer);
         break;
      }
      default:
         break;
   }
   return 0;
}

static void nar5520_reset_exit(void)
{
   switch(get_platform_info()) {
      case NAR5520_HW: {
         del_timer(&hold_timer);
         break;
      }
      case T5520UR_HW: {
         break;
      }
      case COB7402_HW: {
         del_timer(&hold_timer);
         break;
      }
      default:
         break;
   }

   return;
}




int8_t __init
bsp_reset_init(void)
{
   return nar5520_reset_init();
}

void __exit
bsp_reset_exit(void)
{
   return nar5520_reset_exit();
}

void rks_sw2_reset(void)
{
}

void rks_sw2_cb_reg(void)
{
}
