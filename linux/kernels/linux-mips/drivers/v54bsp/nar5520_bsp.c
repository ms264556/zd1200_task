/*
 * Copyright (c) 2004-2010 Ruckus Wireless, Inc.
 * All rights reserved.
 *
 */
 //Tony Chen 03-07-2008 Initial



#include <linux/sched.h>
#include <linux/irqflags.h>
#include <linux/reboot.h>
#include <linux/kthread.h>
#include <linux/hardirq.h>
#include <linux/wait.h>
#include <linux/module.h>
#include "ar531x_bsp.h"
#include "nar5520_bsp.h"
#include "nar5520_cf.h"
#include "v54_himem.h"

extern void v54_on_reboot(u_int32_t reason);


#if defined(__KERNEL__)
#include <linux/kernel.h>
#define V54_PRINTF printk
#define DBG_LEVEL
#else
#define V54_PRINTF(...)
#define DBG_LEVEL KERN_DEBUG
#endif

#ifdef __DEBUG__
#define BSP_TRACK(fmt, args...) V54_PRINTF(DBG_LEVEL "#### [%s] "fmt" ####\n", __FUNCTION__, ## args )
#define BSP_PRINTF BSP_TRACK
#define BSP_DEFINE nar5520_show_flags
#else
#define BSP_TRACK(fmt, args...) V54_PRINTF(DBG_LEVEL "%s: "fmt"\n", __FUNCTION__, ## args )
#define BSP_PRINTF(...)
#define BSP_DEFINE(...)
#endif




#define BOARD_CONFIG_SIZE   (sizeof(struct ar531x_boarddata))
struct ar531x_boarddata *ar531x_boardConfig = NULL;
static struct ar531x_boarddata _ar531x_boardData;
struct ar531x_boarddata *ar531x_boardData = &_ar531x_boardData;

#define RADIO_CONFIG_SIZE 0x1000
static char _radio_config[RADIO_CONFIG_SIZE];
char *radioConfig = &_radio_config[0];


struct rks_boarddata *rks_boardConfig = NULL;
static struct rks_boarddata _rks_boardData;
struct rks_boarddata *rks_boardData = &_rks_boardData;

static char v54Btype[V54_BTYPE_MAX+1][10] = {
                                  {"Undef"},
                                  {"GD1"},
                                  {"GD2"},
                                  {"GD3"},
                                  {"GD4"},
                                  {"GD6-5"},
                                  {"GD6-1"},
                                  {"GD7"},
                                  {"HUSKY"},
                                  {"RETRIEVER"},
                                  {"Undef10"},
                                  {"Undef11"},
                                  {"Undef12"},
                                  {"Undef13"},
                                  {"Undef14"},
};

int oops_occurred = 0;
int to_write_himem = 0;
static wait_queue_head_t nar5520_oops_wait_queue_head;
static spinlock_t nar5520_oops_lock;


extern unsigned long v54_himem_buf_start(void);

char* v54BType(int btype)
{
   return v54Btype[btype];
}




#define REGION1_START          0x0


#if CONFIG_V54_ZD_PLATFORM == 0
#define REGION2_START          3981601
#define OOPS_START             3981900
#elif CONFIG_V54_ZD_PLATFORM == 1
#define REGION2_START          3920881
#define OOPS_START             3921180
#endif
int region2_start = REGION2_START;
int oops_start = OOPS_START;



#define OOPS_SECTORS_MAX_SIZE  (512 * 11)

#define RKS_BOARD_DATA_STARAT  (RKS_BD_OFFSET)
#define RKS_BC_OFFSET          (0)
#define RKS_BOARD_CONFIG_START (RKS_BC_OFFSET)
#define ZD3000_BD_FS_TYPE      "ext2"
#define ZD3000_MM_FS_TYPE      "ext2"


inline void nar5520_show_flags(void)
{
   BSP_PRINTF("DEFINES======================");

   BSP_PRINTF("Defined V54_RBD_BOOTREC");

   return;
}



int v54_get_main_img_id(void)
{
   if(rks_boardConfig == NULL || rks_boardConfig->rev<RKS_BD_REV_BOOTREC){
      return 0;
   }

   if(rks_boardConfig->bootrec.magic == RKS_BOOTREC_MAGIC){
      return rks_boardConfig->bootrec.main_img;
   }

   return 0;
}

int v54_set_main_img_id(int main_img)
{
   if(rks_boardConfig == NULL) {
      return -1;
   }

   if(main_img < 0 || main_img > 2){
      return -2;
   }

   if(main_img == 0){
      main_img = 1;
   }

   if(rks_boardConfig->rev < RKS_BD_REV_BOOTREC){
      rks_boardConfig->rev = RKS_BD_REV_BOOTREC;
   }
   else if(rks_boardConfig->bootrec.magic == RKS_BOOTREC_MAGIC && \
      rks_boardConfig->bootrec.main_img == main_img){
      return 0;
   }

   rks_boardConfig->bootrec.magic = RKS_BOOTREC_MAGIC;
   rks_boardConfig->bootrec.main_img = main_img;


   return 0;
}

ulong get_bd_offset(void)
{
   return RKS_BC_OFFSET;
}

ulong get_rbd_offset(void)
{
   return RKS_BD_OFFSET;
}

char boarddata_dev_path[10] = {0};
char himem_dev_path[10] = {0};

int
ar531x_get_board_config(void)
{
   int dataFound = 0;
   struct nar5520_cf_head cfh;


   BSP_DEFINE();
   cfh.sector_start = region2_start;
   cfh.bytes_offset = RKS_BOARD_DATA_STARAT;
   cfh.rw_cmd = CF_READ;
   cfh.dev_path = boarddata_dev_path;
   cfh.fs_type = ZD3000_BD_FS_TYPE;

   if(nar5520_cf_read_write(&cfh, (unchar*) rks_boardData, \
      sizeof(_rks_boardData)) != sizeof(_rks_boardData)){

      rks_boardConfig = (struct rks_boarddata *)0;
      BSP_PRINTF("Ruckus Board Data read error");
   }else{
      rks_boardConfig = rks_boardData;


      if(rks_boardConfig->magic != RKS_BD_MAGIC){
         BSP_PRINTF("Ruckus Board Data not found, bad magic [0x%X]!", rks_boardConfig->magic);
         rks_boardConfig = (struct rks_boarddata *)0;
      }


   }

   cfh.bytes_offset = RKS_BOARD_CONFIG_START;
   ar531x_boardConfig = ar531x_boardData;
   if(nar5520_cf_read_write(&cfh, (unchar*) ar531x_boardData, \
      BOARD_CONFIG_SIZE) != BOARD_CONFIG_SIZE){
         BSP_PRINTF("Ruckus Board Config read error");
         goto failed_read;
   }


   if(*(int *) ar531x_boardConfig == AR531X_BD_MAGIC){
      BSP_TRACK("Ruckus Board Data and Config found!!");
      dataFound = 1;
   }else{
      BSP_PRINTF("Ruckus Board Config not found, bad magic [0x%X]!", ar531x_boardConfig);
      goto failed_read;
   }

   return (dataFound);
failed_read:
   BSP_PRINTF("Board Data read failed");
   ar531x_boardConfig = (struct ar531x_boarddata *)0;
   return 0;
}

int
chk_valid_macaddr(unsigned char *mac)
{
    int i;

    for (i=0; i<6; i++) {
        if (*(mac+i) != 0xff)
            break;
    }
    if (i == 6)
        return(FALSE);

    for (i=0; i<6; i++) {
        if (*(mac+i) != 0)
            break;
    }
    if (i == 6)
        return(FALSE);

    return(TRUE);
}

int
bd_get_lan_macaddr(int lanid, unsigned char *macaddr)
{
    unsigned char* mac;

    if (ar531x_boardConfig == NULL ||
        rks_boardConfig == NULL ||
        macaddr == NULL ||
        lanid  > 4) {
        return(FALSE);
    }

    switch(lanid) {
    case 0:
        mac = ar531x_boardConfig->enet0Mac;
        break;
    case 1:
        mac = ar531x_boardConfig->enet1Mac;
        break;

    case 2:
        mac = rks_boardConfig->enetxMac[0];
        break;

    case 3:
        mac = rks_boardConfig->enetxMac[1];
        break;

    case 4:
        mac = rks_boardConfig->enetxMac[2];
        break;

    default:
        return(FALSE);
    }

    if (chk_valid_macaddr(mac) == FALSE)
        return(FALSE);

    memcpy(macaddr, mac, 6);
    return(TRUE);
}


int
ar531xSetupFlash(void)
{

   if(ar531x_get_board_config()){
      return (1);
   }
   return (0);
}



void nar5520_load_himem(void)
{
   struct v54_high_mem himem_cf;
   struct v54_high_mem *himem;
   struct nar5520_cf_head cfh;


   himem = v54_himem_data();
   if (!himem) {
      printk(KERN_ERR "%s:v54 himem is null!\n", __FUNCTION__);
      return;
   }

   cfh.sector_start = REGION1_START;
   cfh.bytes_offset = 0x0;
   cfh.rw_cmd = CF_READ;
   cfh.dev_path = himem_dev_path;
   cfh.fs_type = ZD3000_MM_FS_TYPE;
   if(nar5520_cf_read_write(&cfh, (unchar*)&himem_cf, sizeof(himem_cf)) == sizeof(himem_cf)){
      if (HIMEM_CHECK_MAGIC(&himem_cf))
         memcpy(himem, &himem_cf, sizeof(himem_cf));
   } else {
      printk(KERN_ERR "%s:failed to read himem from cf card, himem_dev_path:%s, boarddata_dev_path:%s\n", __FUNCTION__, himem_dev_path, boarddata_dev_path);
   }
}

void nar5520_save_himem(void)
{
   struct v54_high_mem *himem;
   struct nar5520_cf_head cfh;


   himem = v54_himem_data();
   if (!himem) {
      printk(KERN_ERR "%s:v54 himem is null!\n", __FUNCTION__);
      return;
   }

   if (in_interrupt()) {
      to_write_himem = 1;
      wake_oops_guard_thread();
      return;
   }

   cfh.sector_start = REGION1_START;
   cfh.bytes_offset = 0x0;
   cfh.rw_cmd = CF_WRITE;
   cfh.dev_path = himem_dev_path;
   cfh.fs_type = ZD3000_MM_FS_TYPE;
   if(nar5520_cf_read_write(&cfh, (unchar*)himem, sizeof(struct v54_high_mem)) != sizeof(struct v54_high_mem)){
      printk(KERN_ERR "%s:failed to save himem to cf card!\n", __FUNCTION__);
   }
}




void nar5520_load_oops(void)
{
   struct _himem_oops * oops_hdr;
   struct nar5520_cf_head cfh;
   int max_len;

   oops_hdr = (struct _himem_oops *)v54_himem_buf_start();
   if (!oops_hdr) {
      printk(KERN_ERR "%s:v54 oops_hdr is null!\n", __FUNCTION__);
      return;
   }

   cfh.sector_start = oops_start;
   cfh.bytes_offset = 0x0;
   cfh.rw_cmd = CF_READ;
   cfh.dev_path = boarddata_dev_path;
   cfh.fs_type = ZD3000_MM_FS_TYPE;
   max_len = sizeof(struct _himem_oops) + MAX_OOPS_SIZE;
   if (max_len > OOPS_SECTORS_MAX_SIZE)
       max_len = OOPS_SECTORS_MAX_SIZE;
   if(nar5520_cf_read_write(&cfh, (unchar*)oops_hdr, max_len) == max_len){
   } else {
      printk(KERN_ERR "%s:failed to read oops from cf card!\n", __FUNCTION__);
   }
}

void nar5520_save_oops(void)
{
   struct _himem_oops * oops_hdr;
   struct nar5520_cf_head cfh;
   int max_len;

   oops_hdr = (struct _himem_oops *)v54_himem_buf_start();
   if (!oops_hdr) {
      printk(KERN_ERR "%s:v54 oops_hdr is null!\n", __FUNCTION__);
      return;
   }

   if (in_interrupt()) {
      oops_occurred = 1;
      wake_oops_guard_thread();
      return;
   }

   cfh.sector_start = oops_start;
   cfh.bytes_offset = 0x0;
   cfh.rw_cmd = CF_WRITE;
   cfh.dev_path = boarddata_dev_path;
   cfh.fs_type = ZD3000_MM_FS_TYPE;
   max_len = sizeof(struct _himem_oops) + oops_hdr->len;
   if (max_len > OOPS_SECTORS_MAX_SIZE)
       max_len = OOPS_SECTORS_MAX_SIZE;
   if(nar5520_cf_read_write(&cfh, (unchar*)oops_hdr, max_len) == max_len){
   } else {
      printk(KERN_ERR "%s:failed to save oops info to cf card!\n", __FUNCTION__);
   }

}

static int nar5520_oops_guard_thread(void* data)
{
    int to_write_himem_tmp;
    int oops_occurred_tmp;
    int cpu = (int) data;

    printk(KERN_INFO "%s(cpu%d)...\n", __FUNCTION__, cpu);

    while(1) {
        if (wait_event_timeout(nar5520_oops_wait_queue_head,
             oops_occurred == 1 || to_write_himem == 1, 60*HZ) == 0)
            continue;
        spin_lock(&nar5520_oops_lock);
        to_write_himem_tmp = to_write_himem;
        oops_occurred_tmp = oops_occurred;
        if (to_write_himem)
            to_write_himem = 0;
        if (oops_occurred_tmp == 1)
            oops_occurred = 2;
        spin_unlock(&nar5520_oops_lock);

        if (oops_occurred_tmp == 1)
            break;
        if (to_write_himem_tmp)
            nar5520_save_himem();
        printk("%s:%d(cpu%d)\n", __FUNCTION__, __LINE__, cpu);
    }

    printk(KERN_INFO "%s(cpu%d) awake ...\n", __FUNCTION__, cpu);
    v54_reboot_reason(REBOOT_PANIC);
    nar5520_save_himem();
    nar5520_save_oops();

    printk(KERN_INFO "%s(cpu%d) exit\n", __FUNCTION__, cpu);
	printk(KERN_EMERG "Rebooting now(cpu%d) ...\n\r", cpu);
	emergency_restart();

    return 0;
}

int nar5520_oops_guard_init(void)
{
   int8_t cpu;
   struct task_struct *ptask;

   spin_lock_init(&nar5520_oops_lock);
   init_waitqueue_head(&nar5520_oops_wait_queue_head);
   for_each_online_cpu(cpu){
      ptask = kthread_create(nar5520_oops_guard_thread, (void *)cpu, "oops_guard/%d", cpu);
      if(!IS_ERR(ptask)){
         kthread_bind(ptask, cpu);
         wake_up_process(ptask);
      }else{
         printk(KERN_ERR "Error while binding oops_guard to cpu %d", cpu);
      }
   }

    return 0;
}

void wake_oops_guard_thread(void)
{
    wake_up_all(&nar5520_oops_wait_queue_head);
}

void nar5520_load_himem_init(void)
{
    nar5520_load_himem();
    nar5520_load_oops();
    nar5520_oops_guard_init();

    return;
}
