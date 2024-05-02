/*
 * Copyright (c) 2004-2010 Ruckus Wireless, Inc.
 * All rights reserved.
 *
 */
 //Tony Chen    2010-12-27   Initial

#include <linux/ipmi.h>
#include <linux/ipmi_smi.h>

#include "t5520ur_ipmi.h"
#include "compat.h"


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


#define IPMI_WD_LOG              (0x1 << 7)
#define IPMI_WD_NOLOG            (0x1 << 6)
#define IPMI_WD_FLAG_LEAVE_ALONE (0x0)

#define IPMI_SET_LOG(byte, log) (byte |= (log & 0x40))
#define IPMI_SET_USE(byte, use) (byte |= (use & 0x07)), atomic_set(&current_use, use);
#define IPMI_SET_ACT(byte, act) (byte |= (act & 0x07))
#define IPMI_SET_TIME(lsbyte, msbyte, time) \
   (lsbyte) = (((time) * 10) & 0xff), (msbyte) = (((time) * 10) >> 8)

static ipmi_user_t t5520ur_wd_user = NULL;
static atomic_t current_use = ATOMIC_INIT(IPMI_WD_USE_BIOS_FRB2);

static const char* get_cmd_str(int8_t cmd)
{
   switch(cmd) {
      case IPMI_WD_SET_TIMER:   return "set T5520UR watchdog timer";
      case IPMI_WD_RESET_TIMER: return "reset T5520UR watchdog timer";
      case IPMI_WD_GET_TIMER:   return "get T5520UR watchdog timer";
      default:                  break;
   }

   return "Unknow T5520UR watchdog command";
}

static void t5520ur_wd_msg_handler(struct ipmi_recv_msg *msg,
                                   void* handler_data)
{
   if(msg->msg.data[0]) {
      printk("warning: recieved %s fail [CC=0x%x], watchdog broken\n", get_cmd_str(msg->msg.cmd), msg->msg.data[0]);
   }else{
      WDT_PRINTF("recieved %s response ok", get_cmd_str(msg->msg.cmd));
   }

   ipmi_free_recv_msg(msg);

   return;
}

static struct ipmi_user_hndl t5520ur_wd_ipmi_hndlrs =
{
   .ipmi_recv_hndl = t5520ur_wd_msg_handler,
};

int t5520ur_wd_settimer(t5520ur_timerop_t* ops)
{
   int rv = -1;
   uint8_t data[6] = {0};
   struct ipmi_system_interface_addr addr;
   struct kernel_ipmi_msg msg;

   if(!t5520ur_wd_user) {
      printk("warning: user not available, watchdog broken\n");
      goto out;
   }

   addr.addr_type = IPMI_SYSTEM_INTERFACE_ADDR_TYPE;
   addr.channel = IPMI_BMC_CHANNEL;
   addr.lun = 0;

   msg.netfn = IPMI_NETFUNC_APP;
   msg.cmd = ops->op;
   msg.data = data;

   switch(ops->op) {
      case IPMI_WD_SET_TIMER: {
         IPMI_SET_LOG(data[0], IPMI_WD_LOG);
         IPMI_SET_USE(data[0], ops->use);
         IPMI_SET_ACT(data[1], ops->act);
         IPMI_SET_TIME(data[4], data[5], ops->time);
         msg.data_len = sizeof(data);
         break;
      }
      case IPMI_WD_RESET_TIMER: {
         msg.data_len = 0;
         break;
      }
      case IPMI_WD_GET_TIMER:
      default:

         goto out;
         break;
   }

   rv = ipmi_request_settime(t5520ur_wd_user, (struct ipmi_addr*)&addr, 0, &msg,
                             NULL, 1, ops->retries, ops->timeout);
   if(rv) {
      printk("warning: send request for %s fail, watchdog broken\n", get_cmd_str(ops->op));
      goto out;
   } else {
      WDT_PRINTF("send request %s ok", get_cmd_str(ops->op));
   }

   return 0;

out:
   return rv;
}

int ipmi_force_power_off(void)
{
   t5520ur_timerop_t ops;

   ops.op	= IPMI_WD_SET_TIMER;
   ops.use	= IPMI_WD_USE_SMS_RUN;
   ops.act	= IPMI_WD_ACTION_POW_DOWN;
   ops.time	= 0;
   ops.retries	= -1;
   ops.timeout	= 0;

   return t5520ur_wd_settimer(&ops);
}
EXPORT_SYMBOL(ipmi_force_power_off);















static void dummy_free_smi(struct ipmi_smi_msg *msg)
{
}
static void dummy_free_recv(struct ipmi_recv_msg *msg)
{
}
static struct ipmi_smi_msg dummy_smi_msg =
{
   .done = dummy_free_smi
};
static struct ipmi_recv_msg dummy_recv_msg =
{
   .done = dummy_free_recv
};

static int t5520ur_panic_handler(struct notifier_block *this,
                                 unsigned long         event,
                                 void                  *unused)
{
   struct kernel_ipmi_msg            msg;
   struct ipmi_system_interface_addr addr;
   uint8_t data[6] = {0};

   addr.addr_type = IPMI_SYSTEM_INTERFACE_ADDR_TYPE;
   addr.channel = IPMI_BMC_CHANNEL;
   addr.lun = 0;

   msg.netfn = IPMI_NETFUNC_APP;
   msg.cmd = IPMI_WD_SET_TIMER;
   msg.data = data;
   msg.data_len = sizeof(data);

   IPMI_SET_LOG(data[0], IPMI_WD_LOG);
   IPMI_SET_USE(data[0], (uint8_t)atomic_read(&current_use));
   IPMI_SET_ACT(data[1], IPMI_WD_ACTION_RESET);
   IPMI_SET_TIME(data[4], data[5], 10);

   ipmi_request_supply_msgs(t5520ur_wd_user, (struct ipmi_addr *) &addr, 0,
                            &msg, NULL, &dummy_smi_msg, &dummy_recv_msg, 1);
   printk("warning: watchdog detects a kernel panic, reboots system in 10 seconds\n");

   return NOTIFY_OK;
}

static struct notifier_block t5520ur_panic_notifier = {
   .notifier_call  = t5520ur_panic_handler,
   .next           = NULL,
   .priority       = 150
};

int t5520ur_wd_init(void)
{
   int i, rv = -1;
   NOTIFIER_CHAIN_REGISTER(&panic_notifier_list, &t5520ur_panic_notifier);


   for(i=0; i<IPMI_MAX_INTF && rv; i++) {
      rv = ipmi_create_user(i, &t5520ur_wd_ipmi_hndlrs, NULL, &t5520ur_wd_user);
   }
   if(rv < 0) {
      printk("warning: create user fail, watchdog broken\n");
   }

   return rv;
}

void t5520ur_wd_exit(void)
{
   NOTIFIER_CHAIN_REGISTER(&panic_notifier_list, &t5520ur_panic_notifier);

   ipmi_destroy_user(t5520ur_wd_user);

   return;
}
