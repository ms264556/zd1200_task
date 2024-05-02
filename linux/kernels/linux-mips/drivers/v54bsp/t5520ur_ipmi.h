/*
 * Copyright (c) 2004-2010 Ruckus Wireless, Inc.
 * All rights reserved.
 *
 */
 //Tony Chen    2010-12-27   Initial

#ifndef __T5520UR_IPMI_H__
#define __T5520UR_IPMI_H__

#include <linux/ipmi.h>
#include <linux/types.h>

#define IPMI_MAX_INTF            (4)
#define IPMI_NETFUNC_APP         (0x06)

#define IPMI_WD_RESET_TIMER      (0x22)
#define IPMI_WD_SET_TIMER        (0x24)
#define IPMI_WD_GET_TIMER        (0x25)

#define IPMI_WD_USE_BIOS_FRB2    (0x1)
#define IPMI_WD_USE_BIOS_POST    (0x2)
#define IPMI_WD_USE_OS_LOAD      (0x3)
#define IPMI_WD_USE_SMS_RUN      (0x4)
#define IPMI_WD_USE_OEM          (0x5)

#define IPMI_WD_ACTION_NONE      (0x0)
#define IPMI_WD_ACTION_RESET     (0x1)
#define IPMI_WD_ACTION_POW_DOWN  (0x2)
#define IPMI_WD_ACTION_POW_RESET (0x3)

#define IPMI_NETFUNC_STORAGE    (0x0A)
#define IPMI_HWCK_GET_IVNT_AREA (0x10)
#define IPMI_HWCK_GET_FRU_DATA  (0x11)
#define IPMI_HWCK_SET_FRU_DATA  (0x12)

#define IPMI_FRU_SPEC_VER       (0x01)
#define IPMI_FRU_DEVICE_ID      (0x0)
#define IPMI_FRU_HEADER_SIZE	(8)
#define IPMI_FRU_INFO_AREA_SIZE	(2)
#define IPMI_FRU_BLOCK_SIZE	(0x40)
#define IPMI_FRU_LAN_ASCII8	(0x3)

#define IPMI_FRU_LAN_SHIFT	(6)
#define IPMI_FRU_LEN_MASK	(0x3f)
#define IPMI_FRU_MFG_DATE_LEN   (3)

#define FRU_CHASSIS_TYPE_OFFSET     (2)
#define FRU_BOARD_MFG_DATE_OFFSET   (3)
#define FRU_PRODUCT_MFG_NAME_OFFSET (3)

typedef struct {
  unsigned int timeout;
  int          retries;
  uint16_t     time;
  uint8_t      op;
  uint8_t      use;
  uint8_t      act;
  uint8_t      cpu_id;
} t5520ur_timerop_t;

typedef struct {
   uint8_t version;
   uint8_t internal_offset;
   uint8_t chassis_offset;
   uint8_t board_offset;
   uint8_t product_offset;
   uint8_t multirecord_offset;
   uint8_t pad;
   uint8_t checksum;
} fru_com_hdr_t;

typedef struct {
   uint8_t* buff;
   uint32_t len;
} fru_buff_t;

int t5520ur_wd_settimer(t5520ur_timerop_t* ops);
int t5520ur_wd_init(void);
void t5520ur_wd_exit(void);

int t5520ur_hwck_init(void);
int t5520ur_hwck_exit(void);
int t5520ur_hwck_scan(void);
#endif
