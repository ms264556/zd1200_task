/*
 * Copyright (c) 2004-2011 Ruckus Wireless, Inc.
 * All rights reserved.
 *
 */
 //Tony Chen    2011-02-23   Initial

#include <linux/ipmi.h>
#include <linux/ipmi_smi.h>
#include <linux/reboot.h>

#include "compat.h"
#include "t5520ur_ipmi.h"
#include "nar5520_proc.h"
#include <linux/slab.h>

#if defined(__KERNEL__)
#include <linux/kernel.h>
#define V54_PRINTF printk
#define DBG_LEVEL KERN_EMERG
#else
#define V54_PRINTF(...)
#define DBG_LEVEL KERN_DEBUG
#endif

#ifdef __DEBUG__
#define HWK_TRACK(fmt, args...) V54_PRINTF(DBG_LEVEL "#### [%s] "fmt" ####\n", __FUNCTION__, ## args )
#define HWK_PRINTF HWK_TRACK
#else
#define HWK_TRACK(fmt, args...) V54_PRINTF(DBG_LEVEL "%s: "fmt"\n", __FUNCTION__, ## args )
#define HWK_PRINTF(...)
#endif


#define ENTRY_STR(name,var) {name, NULL, proc_read_str, NULL, NULL, var}
#define ENTRY_INT(name,var) {name, NULL, proc_read_int, NULL, NULL, var}
#define PRODUCT_NAME "T5520UR"
#define UNKNOWN_STR "unknown"
#define UNKNOWN_INT 0
#define ESC_MIN_1970_1996 (13674180)

static char* chassis_type		= UNKNOWN_STR;
static char* chassis_part_num		= UNKNOWN_STR;
static char* chassis_serial_num		= UNKNOWN_STR;
static uint32_t* board_mfg_date		= UNKNOWN_INT;
static char* board_manufacturer		= UNKNOWN_STR;
static char* board_product_name 	= UNKNOWN_STR;
static char* board_serial_num		= UNKNOWN_STR;
static char* board_part_num		= UNKNOWN_STR;
static char* board_fru_file_id		= UNKNOWN_STR;
static char* product_mfg_name		= UNKNOWN_STR;
static char* product_name		= UNKNOWN_STR;
static char* product_part_num		= UNKNOWN_STR;
static char* product_version		= UNKNOWN_STR;
static char* product_serial_num		= UNKNOWN_STR;
static char* product_asset_tag		= UNKNOWN_STR;
static char* product_fru_file_id	= UNKNOWN_STR;

static int proc_read_str(char* page, char ** start,
                off_t off, int count, int *eof, void *data)
{
   return sprintf(page, "%s\n", (char*)data);
}

static int proc_read_int(char* page, char ** start,
                off_t off, int count, int *eof, void *data)
{
   return sprintf(page, "%u\n", (uint32_t)&data);
}

static struct entry_table proc_node[] = {
   ENTRY_STR("chassis_type", &chassis_type),
   ENTRY_STR("chassis_part_num", &chassis_part_num),
   ENTRY_STR("chassis_serial_num", &chassis_serial_num),
   ENTRY_INT("board_mfg_date", &board_mfg_date),
   ENTRY_STR("board_manufacturer", &board_manufacturer),
   ENTRY_STR("board_product_name", &board_product_name),
   ENTRY_STR("board_serial_num", &board_serial_num),
   ENTRY_STR("board_part_num", &board_part_num),
   ENTRY_STR("board_fru_file_id", &board_fru_file_id),
   ENTRY_STR("product_mfg_name", &product_mfg_name),
   ENTRY_STR("product_name", &product_name),
   ENTRY_STR("product_part_num", &product_part_num),
   ENTRY_STR("product_version", &product_version),
   ENTRY_STR("product_serial_num", &product_serial_num),
   ENTRY_STR("product_asset_tag", &product_asset_tag),
   ENTRY_STR("product_fru_file_id", &product_fru_file_id),
   {NULL, NULL, NULL, NULL, NULL, NULL}
};

const char* chassis_type_str[] = {
   "Error",
   "Other",
   "Unknown",
   "Desktop",
   "Low Profile Desktop",
   "Pizza Box",
   "Mini Tower",
   "Tower",
   "Portable",
   "LapTop",
   "Notebook",
   "Hand Held",
   "Docking Station",
   "All in One",
   "Sub Notebook",
   "Space-saving",
   "Lunch Box",
   "Main Server Chassis",
   "Expansion Chassis",
   "SubChassis",
   "Bus Expansion Chassis",
   "Peripheral Chassis",
   "RAID Chassis",
   "Rack Mount Chassis",
};

int t5520ur_proc_create(struct proc_dir_entry* proc_base)
{
   struct entry_table *p = proc_node;

   while(p->entry_name) {
      if(!(p->proc_entry = create_proc_entry(p->entry_name, 0644, proc_base))) {
         return -1;
      }
      SET_OWNER(p->proc_entry, THIS_MODULE);
      if(p->fop_proc) {
         p->proc_entry->proc_fops = p->fop_proc;
      } else {
         p->proc_entry->read_proc = p->read_proc;
         p->proc_entry->data = *(char**)p->data;
      }
      p++;
   }

   return 0;
}

int t5520ur_proc_remove(struct proc_dir_entry* proc_base)
{
   struct entry_table *p = proc_node;
   while(p->proc_entry) {
      remove_proc_entry(p->entry_name, proc_base);
      p++;
   }
   return 0;
}


static ipmi_user_t t5520ur_hwck_user = NULL;
static struct completion hardware_scan_done;
static fru_buff_t start_buff;
static struct semaphore scan_mutex;

static const char* get_cmd_str(int8_t cmd)
{
   switch(cmd) {
      case IPMI_HWCK_GET_IVNT_AREA:	return "get inventroy area info";
      case IPMI_HWCK_GET_FRU_DATA:	return "get FRU";
      case IPMI_HWCK_SET_FRU_DATA:	return "set FRU";
      default:				break;
   }

   return "Unknow FRU command";
}

static int t5520ur_hwck_sendmsg(unsigned char* data, unsigned short len, unsigned char cmd)
{
   int rv;
   struct ipmi_system_interface_addr addr;
   struct kernel_ipmi_msg msg;

   addr.addr_type = IPMI_SYSTEM_INTERFACE_ADDR_TYPE;
   addr.channel = IPMI_BMC_CHANNEL;
   addr.lun = 0;

   msg.netfn = IPMI_NETFUNC_STORAGE;
   msg.cmd = cmd;
   msg.data = data;
   msg.data_len = len;

   rv = ipmi_request_settime(t5520ur_hwck_user, (struct ipmi_addr*)&addr, 0, (struct kernel_ipmi_msg*)&msg, NULL, 1, -1, 0);
   if(rv) {
      printk("warning: send request for %s fail, watchdog broken\n", get_cmd_str(msg.cmd));
   } else {
      HWK_PRINTF("send request %s ok", get_cmd_str(msg.cmd));
   }

   return rv;
}

static void t5520ur_hwck_msg_handler(struct ipmi_recv_msg *msg,
                                   void* handler_data)
{
   fru_buff_t* fru_buff= (fru_buff_t*) handler_data;
   if(msg->msg.data[0]) {
      printk("warning: recieved %s fail [CC=0x%x], hardware scan failure\n", get_cmd_str(msg->msg.cmd), msg->msg.data[0]);
      goto err;
   }else{
      HWK_PRINTF("recieved %s response ok", get_cmd_str(msg->msg.cmd));
      switch(msg->msg.cmd) {
         case IPMI_HWCK_GET_IVNT_AREA: {

            if(!fru_buff->buff) {
               fru_buff->len = IPMI_FRU_BLOCK_SIZE * 8;
               fru_buff->buff = kmalloc(fru_buff->len, GFP_ATOMIC);
            }
            break;
         }
         case IPMI_HWCK_GET_FRU_DATA: {
            if(msg->msg.data[1]) {
               fru_buff->len = msg->msg.data[1];
               memcpy(fru_buff->buff, &msg->msg.data[2], msg->msg.data[1]);
            } else {
               goto err;
            }
            break;
         }
         case IPMI_HWCK_SET_FRU_DATA:
         default:
            goto err;
            break;
      }
   }

err:
   complete(&hardware_scan_done);
   ipmi_free_recv_msg(msg);

   return;
}

static struct ipmi_user_hndl t5520ur_hwck_ipmi_hndlrs =
{
   .ipmi_recv_hndl = t5520ur_hwck_msg_handler,
};

static int t5520ur_type_len_parser(uint8_t* data, int count, char** area[])
{
   int i = 0;
   while(i != count) {
      if(IPMI_FRU_LAN_ASCII8 == (*data>>IPMI_FRU_LAN_SHIFT)) {
         int len = *data&IPMI_FRU_LEN_MASK;
         *data=0;
         *area[i] = (data+=1);
         data += len;
         i++;
      } else {
         return -1;
      }
   }
   *data = 0;

   return 0;
}

static int t5520ur_product_parser(uint8_t* data)
{
   int rv = -1;
   if(IPMI_FRU_SPEC_VER == (*data & 0xf)) {
      char** product_area[] = {&product_mfg_name, &product_name, &product_part_num,
                               &product_version, &product_serial_num, &product_asset_tag,
                               &product_fru_file_id};
      rv = t5520ur_type_len_parser(data+FRU_PRODUCT_MFG_NAME_OFFSET,
                                   sizeof(product_area)/sizeof(char**), product_area);
   }
   return rv;
}

static int t5520ur_board_parser(uint8_t* data)
{
   int rv = -1;
   if(IPMI_FRU_SPEC_VER == (*data & 0xf)) {
      char** board_area[] = {&board_manufacturer, &board_product_name,
                             &board_serial_num, &board_part_num,
                             &board_fru_file_id};
      data += FRU_BOARD_MFG_DATE_OFFSET;
      board_mfg_date = (uint32_t*)(ESC_MIN_1970_1996 + (data[3]<<16|data[2]<<8|data[0]));
      rv = t5520ur_type_len_parser(data+IPMI_FRU_MFG_DATE_LEN,
                                   sizeof(board_area)/sizeof(char**), board_area);
   }
   return rv;
}

static int t5520ur_chassis_parser(uint8_t* data)
{
   int rv = -1;
   if(IPMI_FRU_SPEC_VER == (*data & 0xf)) {
      char** chassis_area[] = {&chassis_part_num, &chassis_serial_num};
      data += FRU_CHASSIS_TYPE_OFFSET;
      chassis_type = (char*)chassis_type_str[*data];
      rv = t5520ur_type_len_parser(++data,
                                   sizeof(chassis_area)/sizeof(char**), chassis_area);
   }
   return rv;
}

static int t5520ur_fru_parser(uint8_t* data)
{
   int rv = 0;
   fru_com_hdr_t* hdr = (fru_com_hdr_t*)data;
   if(IPMI_FRU_SPEC_VER == (hdr->version & 0xf)) {
      rv = t5520ur_chassis_parser(data+(hdr->chassis_offset<<3))|
           t5520ur_board_parser(data+(hdr->board_offset<<3))|
           t5520ur_product_parser(data+(hdr->product_offset<<3));
   }
   return rv;
}

static int t5520ur_query_fru(void)
{
   int i, rv = 1;
   fru_buff_t tmp_buff = {NULL, 0};

   for(i=0; i<IPMI_MAX_INTF && rv; i++) {
      rv = ipmi_create_user(i, &t5520ur_hwck_ipmi_hndlrs, &tmp_buff, &t5520ur_hwck_user);
   }

   if(rv < 0) {
      printk("warning: create user fail, watchdog broken\n");
   } else {
      uint8_t data[4] = {IPMI_FRU_DEVICE_ID};
      init_completion(&hardware_scan_done);
      if(!(rv = t5520ur_hwck_sendmsg(data, 1, IPMI_HWCK_GET_IVNT_AREA))) {
         wait_for_completion_interruptible(&hardware_scan_done);
         if(tmp_buff.buff) {
            start_buff = tmp_buff;

            HWK_PRINTF("RFU inventroy area info size %d \n", start_buff.len);
            for(i=0; i<start_buff.len; i+=IPMI_FRU_BLOCK_SIZE, tmp_buff.buff+=tmp_buff.len) {
               data[1] = i & 0xff;
               data[2] = i >> 8 & 0xff;
               data[3] = IPMI_FRU_BLOCK_SIZE;
               if(!(rv = t5520ur_hwck_sendmsg(data, 4, IPMI_HWCK_GET_FRU_DATA))) {
                  wait_for_completion_interruptible(&hardware_scan_done);
               } else {
                  rv = -1;
                  break;
               }
            }

            if(!rv) {
               rv = t5520ur_fru_parser(start_buff.buff);
            } else {
               kfree(start_buff.buff);
               start_buff.buff = NULL;
            }
         }
      }

      ipmi_destroy_user(t5520ur_hwck_user);
   }

   return rv;
}

int t5520ur_hwck_scan(void)
{
   int rv;
   down(&scan_mutex);

   rv = strcmp(board_product_name, PRODUCT_NAME) | strcmp(product_name, PRODUCT_NAME);
   up(&scan_mutex);
   return rv;
}

int t5520ur_hwck_init(void)
{
   init_MUTEX(&scan_mutex);
   return t5520ur_query_fru();
}

int t5520ur_hwck_exit(void)
{
   if(start_buff.buff) {
      kfree(start_buff.buff);
   }

   return 0;
}
