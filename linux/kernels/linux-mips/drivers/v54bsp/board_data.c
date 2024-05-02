/*
 * Pseudo driver for Board info
 *
 * Copyright (c) 2004,2005 Video54 Technologies, Inc.
 * All rights reserved.
 *
 */


#include <linux/version.h>

#include <linux/workqueue.h>
#define MOD_INC_USE_COUNT
#define MOD_DEC_USE_COUNT



#define TRACK()   printk("%d: %s\n", __LINE__, __FUNCTION__)


#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/if_ether.h>


#include <ruckus/rks_api_ver.h>


#include <ruckus/rks_utility.h>

#include "ar531x_bsp.h"
#include "v54_himem.h"
#include "v54_linux_gpio.h"
#include "v54_timer.h"
#include "compat.h"



#define MOD_VER "1.1"
#define MODULE_NAME "v54_bsp"
#define BOARD_DEV "v54bsp"
#define BOARD_DATA_PROC_READ_BUF_SIZE	256


MODULE_VERSION( MOD_VER );

EXPORT_SYMBOL(rks_boardData);
EXPORT_SYMBOL(rks_boardConfig);


EXPORT_SYMBOL(v54_reboot_reason);


extern void v54_himem_oops_init(void);
EXPORT_SYMBOL(v54_himem_oops_init);



struct ar531x_boarddata *bd = NULL;

struct rks_boarddata *rbd =  NULL;

u32 flash_id = 0;

#define IS_RBD_VALID()  ( rbd != NULL && GET_RKS_MAGIC(rbd) == RKS_BD_MAGIC )

#define GET_BOARD_NAME(bd)  (bd->boardName)
#define GET_MAGIC(bd)  (bd->magic)
#define GET_CKSUM(bd)  (bd->cksum)
#define GET_REV(bd)    (bd->rev)
#define GET_MAJOR(bd)  (bd->major)
#define GET_MINOR(bd)  (bd->minor)
#define GET_PCIID(bd)  (bd->pciId)
#define GET_WLAN0(bd)  ((char *)bd->wlan0Mac)
#define GET_WLAN1(bd)  ((char *)bd->wlan1Mac)
#define GET_ENET0(bd)  ((char *)bd->enet0Mac)
#define GET_ENET1(bd)  ((char *)bd->enet1Mac)
#define GET_ENET2(rbd) ((char *)rbd->enetxMac[0])
#define GET_ENET3(rbd) ((char *)rbd->enetxMac[1])
#define GET_ENET4(rbd) ((char *)rbd->enetxMac[2])
#define GET_ENET5(rbd) ((char *)rbd->enetxMac[3])
#define GET_ENETX(i,rbd)    ((char *)&(rbd->enetxMac[i][0]))

#define HAS_WLAN0(bd)    (bd->config & BD_WLAN0)
#define HAS_WLAN1(bd)    (bd->config & BD_WLAN1)
#define HAS_ENET0(bd)    (bd->config & BD_ENET0)
#define HAS_ENET1(bd)    (bd->config & BD_ENET1)

#define GET_RKS_MAGIC(rbd)  (rbd->magic)

#define HAS_ENET2(rbd)    (1)
#define HAS_ENET3(rbd)    (1)
#define HAS_ENET4(rbd)    (1)
#define HAS_ENET5(rbd)    (1)


#define GET_BOARD_TYPE(rbd)                                             \
    ((rbd->boardType == V54_BTYPE_PROTO) ? v54BType(0) : v54BType(rbd->boardType))

#define GET_RANDOM(rbd)  (&(rbd->randomNumber[0]))

#define GET_SERIAL(rbd)  (&(rbd->serialNumber[0]))
#define GET_SERIAL32(rbd)   (&(rbd->serialNumber32[0]))
#define GET_CUSTOMER(rbd)  (&(rbd->customerID[0]))
#define GET_MODEL(rbd)  (&(rbd->model[0]))
#define GET_CTRY(rbd)  (rbd->v54Config & BD_FIXED_CTRY)
#define ANTINFO_INVALID(rbd)  ( rbd == NULL || (rbd->v54Config & BD_ANTCNTL) == 0)
#define GET_ANTINFO(rbd) (rbd->antCntl)

#if ( RKS_BD_REV > 2 )
#define GET_ETH_PORT        (rbd->eth_port)
#define GET_SYM_IMGS        (rbd->sym_imgs)
#else
#define GET_ETH_PORT        (0)
#define GET_SYM_IMGS        (0)
#endif

#define GET_HEATER(rbd)             (rbd->v54Config & BD_HEATER)
#define GET_LED_AIMING_MODE(rbd)    (rbd->v54Config & LED_AIMING_MODE)
#define GET_GE_PORT(rbd)            (rbd->v54Config & BD_GE_PORT)
#define GET_BROWNOUT12(rbd)         (rbd->v54Config & BD_BROWNOUT12)
#define GET_GPS(rbd)                (rbd->v54Config & BD_GPS)
#define GET_USB(rbd)                (rbd->v54Config & BD_USB)
#define GET_FIPS_SKU(rbd)           (rbd->v54Config & BD_FIPS_SKU)
#define GET_FIPS_MODE(rbd)          (rbd->v54Config & BD_FIPS_MODE)




#define MAX_SW_VERSION_LEN 127
static char sw_version_str[MAX_SW_VERSION_LEN + 1] = {0};
static const int max_version_len = MAX_SW_VERSION_LEN;

char *get_sw_version(void) {
    return sw_version_str;
}

int get_sw_version_len(void) {
	return strlen(sw_version_str);
}

EXPORT_SYMBOL(get_sw_version);
EXPORT_SYMBOL(get_sw_version_len);

#define RKS_TRAPDUMP "trapdump"

static char *
SPRINT_MAC(unsigned char* mac)
{
    static char my_mac[19];
    my_mac[18] = '\0';
    sprintf(my_mac, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1] , mac[2], mac[3] , mac[4] , mac[5]);
    return my_mac;
}



#define MACPOOL_MAX_FREEMAC   18
#define V54_MACLEN             6
#define GET_BASEMAC(rbd) ((char *)rbd->MACbase)
#define GET_NUMMAC(rbd)  (rbd->MACcnt)
#define GET_MACPOOL(rbd) (rbd->v54Config & BD_MACPOOL)

typedef struct rks_macpool_s {
    int      MAClimit;
    int      MACalloc;
    int      freeWlancnt;
    u_int8_t freeWmac[MACPOOL_MAX_FREEMAC][V54_MACLEN];
} rks_macpool_t;


static rks_macpool_t radio_macpool[2];

static u_int8_t allocMac[V54_MACLEN];


static void
init_wlan_macpool( int MAClimit )
{
    memset( &radio_macpool, 0, sizeof(radio_macpool) );
    radio_macpool[0].MACalloc =
        radio_macpool[1].MACalloc = 0;
    radio_macpool[0].MAClimit =
        radio_macpool[1].MAClimit = MAClimit;
}

static inline void
_init_radio_macpool(void)
{
    int MAClimit;

    if (rbd->MACcnt == 8) {
        MAClimit = 4;
    } else {
        MAClimit = 16;
    }
    init_wlan_macpool( MAClimit );
    return;
}

static int
assign_wlan_macaddr( rks_macpool_t *pool, u_int8_t *wlanMAC, u_int8_t **macaddr, int wlan )
{
    int temp = 0;

    if( pool->freeWlancnt > 0 ) {
        pool->freeWlancnt--;
        *macaddr = pool->freeWmac[pool->freeWlancnt];
        return( TRUE );
    }

    memcpy( allocMac, wlanMAC, V54_MACLEN );
    temp = (allocMac[V54_MACLEN-3]<<16) |
        (allocMac[V54_MACLEN-2]<<8)  |
        (allocMac[V54_MACLEN-1]);


    temp +=  wlan / 4;
    temp |= (wlan % 4) << 22;

    pool->MACalloc++;

    allocMac[V54_MACLEN-1] = 0xff & temp;
    temp >>= 8;
    allocMac[V54_MACLEN-2] = 0xff & temp;
    temp >>= 8;
    allocMac[V54_MACLEN-3] = 0xff & temp;
    *macaddr = allocMac;

    return( TRUE );
}

int
rks_assign_wlan_macaddr( int radio, int wlan, u_int8_t **macaddr )
{
    int rc;
    u_int8_t base_mac[V54_MACLEN];

    switch (radio) {
    case 0:
      if ( wlan < 16 )
	rc = assign_wlan_macaddr( &radio_macpool[0], GET_WLAN0(bd), macaddr, wlan );
      else
	rc = assign_wlan_macaddr( &radio_macpool[0], GET_BASEMAC(rbd), macaddr, wlan % 16 );
      break;
    case 1:
      if ( wlan < 16 ) {
	rc = assign_wlan_macaddr( &radio_macpool[1], GET_WLAN1(bd), macaddr, wlan );
      }
      else {
	memcpy(base_mac, GET_BASEMAC(rbd), V54_MACLEN);
	base_mac[5] += 4;
	rc = assign_wlan_macaddr( &radio_macpool[1], base_mac, macaddr, wlan % 16 );
      }
      break;
    default:
        rc = FALSE;
        break;
    }

    return rc;
}
EXPORT_SYMBOL(rks_assign_wlan_macaddr);

int
rks_release_wlan_macaddr( int radio, u_int8_t *macaddr )
{
    rks_macpool_t *pool;

    if( radio < 0 ||
        radio > 1 ) {
        return( FALSE );
    }

    pool = &radio_macpool[radio];

    if( pool->freeWlancnt >= MACPOOL_MAX_FREEMAC ) {
        return( FALSE );
    }
    memcpy( pool->freeWmac[pool->freeWlancnt], macaddr, V54_MACLEN );
    pool->freeWlancnt++;

    return( TRUE );
}
EXPORT_SYMBOL(rks_release_wlan_macaddr);


int
rks_get_wlan_macaddr(int radio, int wlanid, char *macaddr)
{
    char* mac;
    if (ar531x_boardConfig == NULL ||
        rks_boardConfig == NULL ||
        macaddr == NULL ) {
        return(FALSE);
    }


    if( rks_boardConfig->v54Config & BD_MACPOOL ) {
        u_int8_t *lmac;

        if( rks_assign_wlan_macaddr( radio, wlanid, &lmac ) ) {
            memcpy(macaddr, lmac, 6);
            return(TRUE);
        } else {
            return(FALSE);
        }
    }

    if( wlanid > 3 ) {
        return(FALSE);
    }

    switch(wlanid) {
    case 0:
        mac = (char *)(ar531x_boardConfig->wlan0Mac);
        break;
    case 1:
        mac = (char *)(ar531x_boardConfig->wlan1Mac);
        break;

    case 2:
        mac = (char *)(rks_boardConfig->enetxMac[3]);
        break;

    case 3:
        mac = (char *)(rks_boardConfig->enetxMac[2]);
        break;

    default:
        return(FALSE);

    }
    memcpy(macaddr, mac, 6);
    return(TRUE);
}
EXPORT_SYMBOL(rks_get_wlan_macaddr);



#ifdef CONFIG_PROC_FS



    #define BDATA_FILE          "bdata"
    #define RBDATA_FILE         "rbdata"
    #define RESCAN_FILE         "Rescan"
    #define BD_OFFSET_FILE      "bd_offset"
    #define RBD_OFFSET_FILE     "rbd_offset"
    #define NAME_FILE           "name"
    #define CUSTOMER_FILE       "customer"
    #define SERIAL_FILE         "serial"
    #define MODEL_FILE          "model"
    #define TYPE_FILE           "type"
    #define BOARDTYPE_FILE      "board_type"
    #define BOARDCLASS_FILE     "board_class"
    #define RANDOM_FILE         "random"
    #define ANT_INFO_FILE       "antinfo"
    #define HIMEM_FILE          "himem"
    #define HIMEM_DATA_FILE     "himem_data"
    #define BOOTLINE_FILE       "bootline"
    #define MAIN_IMG_FILE       "main_img"
    #define RUNNING_IMG_FILE    "running_img"
    #define OOPS_FILE           "oops"
    #define FACTORY_FILE        "factory"
    #define SW2_FILE            "sw2"
    #define MAJOR_FILE          "major"
    #define MINOR_FILE          "minor"
    #define REASON_FILE         "reason"
    #define TOTAL_BOOT_FILE     "total_boot"
    #define BOOT_VERSION_FILE   "boot_version"
    #define FLASH_ID_FILE	    "flash_id"
    #define LED0_FILE           "sysLed0"
    #define LED1_FILE           "sysLed1"
    #define LED2_FILE           "sysLed2"
    #define LED3_FILE           "sysLed3"
    #define LED4_FILE           "sysLed4"
    #define LED5_FILE           "sysLed5"
    #define CTRY_FILE      "fixed_ctry_code"
    #define MAC_POOL      	"MACpool"
    #define LIMIT_MAC      	"MAClimit"
    #define NUM_MACS      	"MACcnt"
    #define BASE_MAC      	"MACbase"
    #define HIWATER_MAC    	"MAChiwater"
    #define FREE_WMAC    	"MACfreeWlan"
    #define RKS_SW_VER      "sw_version"


    #define ETH_PORT_FILE   "ethPort"
    #define SYM_IMGS_FILE   "symImgs"


#define RESET_BIB_TIME  (2 * HZ)
struct timer_list bib_timer;
int bib_in_reset=0;
int tpm_rst_en;




static char *bd_err_msg = "board_data: board config data unavailable\n";

static ssize_t proc_read_bdata (struct file *filp, char __user *buffer,
			size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;
    if ( bd == NULL ) {
        printk(KERN_ERR "%s", bd_err_msg);
        goto done;
    }

    if ( count < sizeof(struct ar531x_boarddata)) {

        printk(KERN_ERR "sizeof(board_data)=%d > count=%d\n",
               sizeof(struct ar531x_boarddata), count);
        return len;
    } else {
        len = simple_read_from_buffer(buffer, count, pos, ar531x_boardData, sizeof(struct ar531x_boarddata));
    }

 done:
    MOD_DEC_USE_COUNT;
    return len;
}


static ssize_t
proc_write_bdata (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;

    MOD_INC_USE_COUNT;

    if ( count != sizeof(struct ar531x_boarddata) ) {
        printk(KERN_ERR "count(%lu) != sizeof(board_data)(%d)\n",
               (unsigned long)count, sizeof(struct ar531x_boarddata));
        len = 0;
        goto finish;
    } else {
        len = count;
    }


    if ( copy_from_user((char *)ar531x_boardData, buffer, len) ) {
        len = -EFAULT;
        goto finish;
    }

 finish:
    MOD_DEC_USE_COUNT;
    return len;
}


static ssize_t proc_read_rbdata (struct file *filp, char __user *buffer,
			size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;

    if ( rbd == NULL ) {
        printk(KERN_ERR "%s", bd_err_msg);
        goto done;
    }

    if ( count < sizeof(struct rks_boarddata)) {

        printk(KERN_ERR "sizeof(board_data)=%d > count=%d\n",
               sizeof(struct rks_boarddata), count);
        return len;
    } else {
        len = simple_read_from_buffer(buffer, count, pos, rks_boardData, sizeof(struct rks_boarddata));
    }

 done:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t
proc_write_rbdata (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;

    MOD_INC_USE_COUNT;

    if ( count != sizeof(struct rks_boarddata) ) {
        printk(KERN_ERR "count(%lu) != sizeof(board_data)(%d)\n",
               (unsigned long)count, sizeof(struct rks_boarddata));
        len = 0;
        goto finish;
    } else {
        len = count;
    }

    if ( copy_from_user((char *)rks_boardData, buffer, len) ) {
        len = -EFAULT;
        goto finish;
    }


 finish:
    MOD_DEC_USE_COUNT;
    return len;
}


static ssize_t
proc_read_rescan(struct file *filp, char __user *buffer,
			size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;

    if ( rbd == NULL ) {
        printk(KERN_ERR "%s", bd_err_msg);
        goto done;
    }

	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "Write \"1\" to force a rescan of flash area\n");
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

 done:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t
proc_write_rescan(struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len = count;
    char ch;
    MOD_INC_USE_COUNT;

    if ( count < 1 ) {
        printk(KERN_ERR "count(%lu) < 1\n", (unsigned long)count);
        len = 0;
        goto finish;
    }

    if ( copy_from_user(&ch, buffer, 1) ) {
        len = -EFAULT;
        goto finish;
    }
    if ( ch == '1' ) {

        if ( ! ar531xSetupFlash() ) {
            printk("%s: Flash data rescan failed\n", MODULE_NAME);
            len = -EFAULT;
        }
    }

 finish:
    MOD_DEC_USE_COUNT;
    return len;

}

static ssize_t proc_read_bd_offset (struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;

    MOD_INC_USE_COUNT;

	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "0x%lx\n" , get_bd_offset());
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t proc_read_rbd_offset (struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;

    MOD_INC_USE_COUNT;

	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "0x%lx\n" , get_rbd_offset());
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

    MOD_DEC_USE_COUNT;
    return len;
}



static ssize_t proc_read_type (struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;

    if ( rbd == NULL ) {
        printk(KERN_ERR "%s", bd_err_msg);
        goto done;
    }
	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "%s\n", GET_BOARD_TYPE(rbd));
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

 done:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t proc_read_boardType (struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;

    if ( rbd == NULL ) {
        printk(KERN_ERR "%s", bd_err_msg);
        goto done;
    }
	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "%d\n", ((int)(rbd->boardType))&0xFF);
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

 done:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t proc_write_boardType (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    int mylen=4;
    char my_buffer[8];
    unsigned long val;

    MOD_INC_USE_COUNT;

    if (count > mylen )
        len = mylen;
    else
        len = count;

    if ( copy_from_user(my_buffer, buffer, len) ) {
        len = -EFAULT;
        goto finish;
    }
    my_buffer[len] = '\0';
    sscanf(my_buffer, "%lu", &val);
    rbd->boardType = (int)val & 0xff;
    if( rbd->boardType == 0 || rbd->boardType > V54_BTYPE_MAX ) {
        printk("Invalid Board Type: %d\n", rbd->boardType );
        rbd->boardType = V54_BTYPE_PROTO;
    }
    printk(KERN_INFO "%s: board_type=%d\n", MODULE_NAME, rbd->boardType);

 finish:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t proc_read_boardClass (struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;

    if ( rbd == NULL ) {
        printk(KERN_ERR "%s", bd_err_msg);
        goto done;
    }
	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "%d\n", ((int)(rbd->boardClass))&0xFF);
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

 done:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t proc_write_boardClass (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    int mylen=4;
    char my_buffer[8];
    unsigned long val;

    MOD_INC_USE_COUNT;

    if (count > mylen )
        len = mylen;
    else
        len = count;

    if ( copy_from_user(my_buffer, buffer, len) ) {
        len = -EFAULT;
        goto finish;
    }
    my_buffer[len] = '\0';
    sscanf(my_buffer, "%lu", &val);
    rbd->boardClass = (int)val & 0xff;
    if( rbd->boardClass == 0 || rbd->boardClass > V54_BCLASS_MAX ) {
        printk("Invalid Board Class: %d\n", rbd->boardClass );
        rbd->boardClass = V54_BCLASS_PROTO;
    }
    printk(KERN_INFO "%s: board_class=%d\n", MODULE_NAME, rbd->boardClass);

 finish:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t proc_read_antinfo (struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;

    if ( rbd == NULL ) {
        printk(KERN_ERR "%s", bd_err_msg);
        goto done;
    }
	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "0x%08x\n", GET_ANTINFO(rbd));
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

 done:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t proc_write_antinfo (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    int mylen=10;
    char my_buffer[16];
    unsigned long val;
    char *ptr;

    MOD_INC_USE_COUNT;

    if (count > mylen )
        len = mylen;
    else
        len = count;

    if ( copy_from_user(my_buffer, buffer, len) ) {
        len = -EFAULT;
        goto finish;
    }
    my_buffer[len] = '\0';
    val = simple_strtoul(my_buffer, &ptr, 16);
    if (my_buffer == ptr) {
        printk("Invalid input\n");
    } else {
        rbd->antCntl = val;
        rbd->v54Config |= BD_ANTCNTL;
    }
    printk(KERN_INFO "%s: antinfo=%x\n", MODULE_NAME, rbd->antCntl);

 finish:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t proc_read_random (struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;

    if ( bd == NULL ) {
        printk(KERN_ERR "%s", bd_err_msg);
        goto done;
    }

    {

        unsigned long  * wpt = (unsigned long *)GET_RANDOM(rbd);

		char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
	    len = snprintf(buf, sizeof(buf), "%08lX%08lX%08lX%08lX\n", wpt[0], wpt[1], wpt[2], wpt[3]);
		len = simple_read_from_buffer(buffer, count, pos, buf, len);
    }

 done:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t proc_write_random (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    int mylen=sizeof(rbd->randomNumber);
    char *my_buffer=&rbd->randomNumber[0];
    unsigned short *rand;

    MOD_INC_USE_COUNT;

    if (count > mylen )
        len = mylen;
    else
        len = count;

    if ( copy_from_user(my_buffer, buffer, len) ) {
        len = -EFAULT;
        goto finish;
    }
    if (my_buffer[0] == '.')
        memset (rbd->randomNumber, 0, sizeof(rbd->randomNumber));

    rand = (void *)&rbd->randomNumber[0];
    printk(KERN_INFO "%s: random=%04x %04x %04x %04x %04x %04x %04x %04x\n",
           MODULE_NAME, rand[0], rand[1], rand[2], rand[3], rand[4], rand[5], rand[6], rand[7]);

 finish:
    MOD_DEC_USE_COUNT;
    return len;
}

static int proc_show_himem (struct seq_file *m, void *v)
{
    int member = (int) m->private;

    MOD_INC_USE_COUNT;
    switch (member) {
    case HIMEM_TOTAL_BOOT:
        seq_printf(m,"%lu\n", (unsigned long)v54_himem_get_total_boot());
        goto done;
        break;
    default:
        break;
    }
    v54_himem_dump_buf(m);

 done:
    MOD_DEC_USE_COUNT;
    return 0;
}

static int proc_open_himem(struct inode *inode, struct file *file)
{
	return single_open(file, proc_show_himem, RKSV_PDE_DATA(inode));
}

static ssize_t
proc_write_himem(struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=count;
    struct v54_high_mem * v54HighMem = v54_himem_data();
    char ch[6];

    MOD_INC_USE_COUNT;

    if ( len < 1 || len > sizeof(ch)  || v54HighMem == NULL ) {
        len = -EFAULT;
        goto finish;
    }

    if ( copy_from_user(ch, buffer, len) ) {
        len = -EFAULT;
        goto finish;
    }

    if ( ch[0] == '0' ) {

        v54HighMem->reboot_cnt = 0;
        v54HighMem->flag_reset = 0;
        v54HighMem->flag_image_type = 0;

    } else if ( ch[0] == 'r' && ch[1] == '0' ) {

        v54HighMem->reboot_cnt = 0;
    } else if ( ch[0] == 't' && ch[1] == '0' ) {

        v54HighMem->flag_reset = 0;
        v54HighMem->flag_image_type = 0;

    } else if ( ch[0] == 'm' && ch[1] == 'r' && ch[2] == '0' ) {
        v54HighMem->miniboot.reboot_cnt = 0;
    } else if ( ch[0] == 'm' && ch[1] == 'r' && ch[2] == 'i' ) {
        v54HighMem->miniboot.reboot_cnt++;
    }

 finish:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t
proc_read_himem_data (struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    struct v54_high_mem * v54HighMem = v54_himem_data();

    MOD_INC_USE_COUNT;

    if ( v54HighMem == NULL ) {
        printk(KERN_ERR "%s", bd_err_msg);
        goto done;
    }

    if ( count < sizeof(*v54HighMem)) {

        printk(KERN_ERR "sizeof(*v54HighMem)=%d > count=%d\n",
               sizeof(*v54HighMem), count);
        return len;
    } else {
		len = simple_read_from_buffer(buffer, count, pos, (char *)v54HighMem, sizeof(* v54HighMem));
    }

 done:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t
proc_write_himem_data (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    struct v54_high_mem * v54HighMem = v54_himem_data();

    MOD_INC_USE_COUNT;

    if ( count != sizeof(*v54HighMem)) {
        printk(KERN_ERR "count(%lu) != sizeof(*v54HighMem)(%d)\n",
               (unsigned long)count, sizeof(*v54HighMem));
        len = 0;
        goto finish;
    } else {
        len = count;
    }

    if ( copy_from_user((char *)v54HighMem, buffer, len) ) {
        len = -EFAULT;
        goto finish;
    }


 finish:
    MOD_DEC_USE_COUNT;
    return len;
}


static ssize_t proc_read_bootline(struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    struct v54_high_mem * v54HighMem = v54_himem_data();
    int len=0;
	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    MOD_INC_USE_COUNT;

    if ( v54HighMem == NULL ) {
        printk(KERN_ERR "%s", bd_err_msg);
        goto done;
    }

    if ( v54HighMem->boot.magic != V54_BOOT_SCRIPT_MAGIC ) {
	len = snprintf(buf, sizeof(buf), "No Himem Bootline\n");
    } else {
        v54HighMem->boot.script[V54_BOOT_SCRIPT_SIZE] = '\0';
	    len = snprintf(buf, sizeof(buf), "%s\n", v54HighMem->boot.script);
    }
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

 done:
    MOD_DEC_USE_COUNT;
    return len;
}


static ssize_t
proc_write_bootline (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len = 0;
    struct v54_high_mem * v54HighMem = v54_himem_data();

    MOD_INC_USE_COUNT;

    if ( count == 0 ) {
        return 0;
    }

    if ( v54HighMem == NULL ) {
        printk(KERN_ERR "%s", bd_err_msg);
        goto finish;
    }

    if ( count >= sizeof(v54HighMem->boot.script)) {
        printk(KERN_ERR "count(%lu) >= sizeof(boot.script)(%d)\n",
               (unsigned long)count, sizeof(v54HighMem->boot.script));
        len = 0;
        goto finish;
    } else {
        len = count;
    }

    if ( copy_from_user(v54HighMem->boot.script, buffer, len) ) {
        len = -EFAULT;
        v54HighMem->boot.magic = 0;
        v54HighMem->boot.script[0] = '\0';
        goto finish;
    } else {
        v54HighMem->boot.magic = V54_BOOT_SCRIPT_MAGIC ;

        v54HighMem->boot.script[len] = '\0';
    }
 finish:
    MOD_DEC_USE_COUNT;
    return count;
}



static ssize_t proc_read_main_img(struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;

	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "%d\n", v54_get_main_img_id());
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

    MOD_DEC_USE_COUNT;
    return len;
}


static ssize_t
proc_write_main_img (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len = count;
    char ch;

    MOD_INC_USE_COUNT;

    if ( count == 0 ) {
        return 0;
    }

    if ( copy_from_user(&ch, buffer, 1) ) {
        len = -EFAULT;
        goto finish;
    }


    v54_set_main_img_id( (ch == '2') ? 2 : 1 );

 finish:
    MOD_DEC_USE_COUNT;
    return count;
}


static ssize_t proc_read_running_img(struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;

	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "%d\n", v54_himem_get_image_type());
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

    MOD_DEC_USE_COUNT;
    return len;
}

static int proc_show_oops (struct seq_file *m, void *v)
{
    int len=0;
    MOD_INC_USE_COUNT;

    len = v54_himem_oops_dump(m);

    MOD_DEC_USE_COUNT;
    return len;
}

static int proc_open_oops(struct inode *inode, struct file *file)
{
	return single_open(file, proc_show_oops, RKSV_PDE_DATA(inode));
}


static ssize_t
proc_write_oops (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    MOD_INC_USE_COUNT;

    if ( count == 0 ) {
        return 0;
    }

    switch (buffer[0]) {
    case 'r':
    case '0':
        himem_tee_reset();
        break;

    default:
        break;
    }

    MOD_DEC_USE_COUNT;
    return count;
}


static ssize_t
proc_write_reason (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    MOD_INC_USE_COUNT;

    if ( count == 0 ) {
        return 0;
    }

    switch (buffer[0]) {
    case 'u':
    case 'U':
        v54_reboot_reason(REBOOT_SOFT);
        break;
    case 'a':
    case 'A':
        v54_reboot_reason(REBOOT_APPL);
        break;
    default:
        break;
    }

    MOD_DEC_USE_COUNT;
    return count;
}

static int proc_show_reason (struct seq_file *m, void *v)
{
    MOD_INC_USE_COUNT;

    v54_himem_reason_dump(m);

    MOD_DEC_USE_COUNT;
    return 0;
}

static int proc_open_reason(struct inode *inode, struct file *file)
{
	return single_open(file, proc_show_reason, RKSV_PDE_DATA(inode));
}

static ssize_t proc_read_factory(struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    struct v54_high_mem * v54HighMem = v54_himem_data();

    MOD_INC_USE_COUNT;

    if ( v54HighMem == NULL ) {
        printk(KERN_ERR "%s", bd_err_msg);
        goto done;
    }

    char buf[2];
    buf[0] = '0' + v54_himem_get_factory() ;
    buf[1] = '\0';
    len = 2;
    len = simple_read_from_buffer(buffer, count, pos, buf, len);

 done:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t
proc_write_factory (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=count;

    char ch;

    MOD_INC_USE_COUNT;

    if ( count < 1 ) {
        printk(KERN_ERR "%s: no data for write", __FUNCTION__);
        len = 0;
        goto finish;
    }

    if ( copy_from_user(&ch, buffer, 1) ) {
        len = -EFAULT;
        goto finish;
    }

    V54_HIMEM_SET_FACTORY( (ch > '2' || ch < '1') ? 0 : (int)(ch - '0') );


 finish:
    MOD_DEC_USE_COUNT;
    return len;
}


static ssize_t proc_read_sw2(struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    struct v54_high_mem * v54HighMem = v54_himem_data();

    MOD_INC_USE_COUNT;

    if ( v54HighMem == NULL ) {
        printk(KERN_ERR "%s", bd_err_msg);
        goto done;
    }

	char buf[2];
    buf[0] = '0' + v54_himem_get_sw2_event() ;
    buf[1] = '\0';
    len = 2;
    len = simple_read_from_buffer(buffer, count, pos, buf, len);

 done:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t
proc_write_sw2 (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=count;

    char ch;

    MOD_INC_USE_COUNT;

    if ( count < 1 ) {
        printk(KERN_ERR "%s: no data for write", __FUNCTION__);
        len = 0;
        goto finish;
    }

    if ( copy_from_user(&ch, buffer, 1) ) {
        len = -EFAULT;
        goto finish;
    }

    V54_HIMEM_SET_SW2( (ch > '2' || ch < '1') ? 0 : (int)(ch - '0') );

 finish:
    MOD_DEC_USE_COUNT;
    return len;
}


static ssize_t proc_read_major(struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;

	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "%d\n",  GET_MAJOR(bd));
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t
proc_write_major (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0,mylen=4;
    char my_buffer[8];
    unsigned long val;

    MOD_INC_USE_COUNT;

    if (count > mylen )
        len = mylen;
    else
        len = count;

    if ( copy_from_user(my_buffer, buffer, len) ) {
        len = -EFAULT;
        goto finish;
    }
    my_buffer[len] = '\0';
    sscanf(my_buffer, "%lu", &val);
    bd->major = (int)val & 0xffff;

 finish:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t proc_read_minor(struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;

	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "%d\n",  GET_MINOR(bd));
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t
proc_write_minor (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0,mylen=4;
    char my_buffer[8];
    unsigned long val;

    MOD_INC_USE_COUNT;

    if (count > mylen )
        len = mylen;
    else
        len = count;

    if ( copy_from_user(my_buffer, buffer, len) ) {
        len = -EFAULT;
        goto finish;
    }
    my_buffer[len] = '\0';
    sscanf(my_buffer, "%lu", &val);
    bd->minor = (int)val & 0xffff;

 finish:
    MOD_DEC_USE_COUNT;
    return len;
}

static int proc_show_boot_version (struct seq_file *m, void *v)
{
    MOD_INC_USE_COUNT;

    v54_himem_boot_version(m);

    MOD_DEC_USE_COUNT;
    return 0;
}

static int proc_open_boot_version(struct inode *inode, struct file *file)
{
	return single_open(file, proc_show_boot_version, RKSV_PDE_DATA(inode));
}

static ssize_t proc_read_name(struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;

    if ( bd == NULL ) {
        printk(KERN_ERR "%s", bd_err_msg);
        goto done;
    }
	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "%s\n", GET_BOARD_NAME(bd));
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

 done:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t
proc_write_name (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    int mylen = sizeof(bd->boardName) - 1;
    char *my_buffer = &bd->boardName[0];

    MOD_INC_USE_COUNT;

    if (count > mylen )
        len = mylen;
    else
        len = count;

    if ( copy_from_user(my_buffer, buffer, len) ) {
        len = -EFAULT;
        goto finish;
    }
    my_buffer[len] = '\0';
    printk(KERN_INFO "%s: name=%s\n", MODULE_NAME, my_buffer);


 finish:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t proc_read_flash(struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;

    if ( bd == NULL ) {
        printk(KERN_ERR "%s", bd_err_msg);
        goto done;
    }
	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "0x%x\n", flash_id);
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

 done:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t
proc_write_flash (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int          rc   = 0;

    printk("%s: write(0)\n", MODULE_NAME);

    return(rc);
}

static ssize_t proc_read_customer(struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;

    if ( ! IS_RBD_VALID() ) {
        printk(KERN_ERR "%s", bd_err_msg);
        goto done;
    }
	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "%s\n", GET_CUSTOMER(rbd));
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

 done:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t
proc_write_customer (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    int mylen = sizeof(rbd->customerID) - 1;
    char *my_buffer = &rbd->customerID[0];


    MOD_INC_USE_COUNT;

    if (count > mylen )
        len = mylen;
    else
        len = count;

    if ( copy_from_user(my_buffer, buffer, len) ) {
        len = -EFAULT;
        goto finish;
    }
    my_buffer[len] = '\0';
    printk(KERN_INFO "%s: customer=%s\n", MODULE_NAME, my_buffer);


 finish:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t proc_read_model(struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;

    if ( ! IS_RBD_VALID() ) {
        printk(KERN_ERR "%s", bd_err_msg);
        goto done;
    }
	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "%s\n", GET_MODEL(rbd));
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

 done:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t
proc_write_model (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    int mylen = sizeof(rbd->model) - 1;
    char *my_buffer = &rbd->model[0];


    MOD_INC_USE_COUNT;

    if (count > mylen )
        len = mylen;
    else
        len = count;

    if ( copy_from_user(my_buffer, buffer, len) ) {
        len = -EFAULT;
        goto finish;
    }
    my_buffer[len] = '\0';
    printk(KERN_INFO "%s: model=%s\n", MODULE_NAME, my_buffer);


 finish:
    MOD_DEC_USE_COUNT;
    return len;
}



static ssize_t proc_read_serial(struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;

    if ( ! IS_RBD_VALID() ) {
        printk(KERN_ERR "%s", bd_err_msg);
        goto done;
    }
	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "%s\n", ((rbd->rev>=RKS_BD_REV_SERIAL32)?GET_SERIAL32(rbd):GET_SERIAL(rbd)));
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

 done:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t
proc_write_serial (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    int mylen = sizeof(rbd->serialNumber)-1;
    char *my_buffer=&rbd->serialNumber[0];

    MOD_INC_USE_COUNT;

#if ( RKS_BD_REV >= RKS_BD_REV_SERIAL32 )
    if (count > mylen )
        len = mylen;
    else
        len = count;

    if ( copy_from_user(my_buffer, buffer, len) ) {
        len = -EFAULT;
        goto finish;
    }
    my_buffer[len] = '\0';
    printk(KERN_INFO "%s: serial16=%s\n", MODULE_NAME, my_buffer);

    len = 0;
    mylen = sizeof(rbd->serialNumber32)-1;
    my_buffer=&rbd->serialNumber32[0];
#endif

    if (count > mylen )
        len = mylen;
    else
        len = count;

    if ( copy_from_user(my_buffer, buffer, len) ) {
        len = -EFAULT;
        goto finish;
    }
    my_buffer[len] = '\0';
    printk(KERN_INFO "%s: serial32=%s\n", MODULE_NAME, my_buffer);


 finish:
    MOD_DEC_USE_COUNT;
    return len;
}


static ssize_t proc_read_led(struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    int led_num = (int) RKSV_PDE_DATA(filp->f_path.dentry->d_inode);
    int rc;
    MOD_INC_USE_COUNT;

    rc = v54_led_blinking(led_num);

	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "%d\n", (rc > 1) ? rc : (get_sysLed(led_num) ? 1 : 0));
	len = simple_read_from_buffer(buffer, count, pos, buf, len);


    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t
proc_write_led (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len = count;
    char buf[8];
    int led_num = (int) RKSV_PDE_DATA(filp->f_path.dentry->d_inode);
    unsigned long dsec = 0;

    MOD_INC_USE_COUNT;
    if ( count < 1 ) {
        printk(KERN_ERR "%s: no data for write", __FUNCTION__);
        len = 0;
        goto finish;
    }

    if ( len > ( sizeof(buf) - 1 ) ) len = ( sizeof(buf) - 1 );
    if ( copy_from_user(buf, buffer, len) ) {
        len = -EFAULT;
        goto finish;
    }
    buf[len] = '\0';
    sscanf(buf, "%lu", &dsec);


#include "nar5520_hwck.h"
    if ((get_platform_info() == T5520UR_HW ) || (get_platform_info() == COB7402_HW )) {
        sysLed(led_num, dsec);
    }
    else

        if ( dsec <= 1 )  {
            v54_stop_led_blink(led_num);


            sysLed(led_num, dsec);

        } else {

            v54_start_led_blink(led_num, dsec);
        }
    len = count;
finish:
    MOD_DEC_USE_COUNT;
    return len;
}






static ssize_t proc_read_bib_led(struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    int led_num;
    unsigned long val=0;
    MOD_INC_USE_COUNT;

    for (led_num=9; led_num>=6; led_num--)
        val = (val << 1) | (get_sysLed(led_num) ? 1 : 0);

	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "%lu\n", val);
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

    MOD_DEC_USE_COUNT;
    return len;
}

void bib_reset_function(unsigned long reset_time)
{
    int led_num;
    for (led_num=6; led_num<=9; led_num++) {
        sysLed(led_num, 0);
    }
    del_timer(&bib_timer);
    bib_in_reset = 0;
}

static ssize_t
proc_write_bib_led (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len = count;
    char buf[8];
    int led_num;
    unsigned long val=0;

    MOD_INC_USE_COUNT;

    if ( count < 1 ) {
        printk(KERN_ERR "%s: no data for write", __FUNCTION__);
        len = 0;
        goto finish;
    }

    if ( len > ( sizeof(buf) - 1 ) ) len = ( sizeof(buf) - 1 );
    if ( copy_from_user(buf, buffer, len) ) {
        len = -EFAULT;
        goto finish;
    }
    buf[len] = '\0';
    sscanf(buf, "%lu", &val);



    for (led_num=6; led_num<=9; led_num++) {
        sysLed(led_num, (val & 1));
        val = val >> 1;
    }

    len = count;

    if (!bib_in_reset) {
        init_timer(&bib_timer);
        bib_timer.expires = jiffies + RESET_BIB_TIME;
        bib_timer.data = jiffies;
        bib_timer.function = bib_reset_function;
        add_timer(&bib_timer);
        bib_in_reset = 1;
    } else {
        mod_timer(&bib_timer, jiffies + RESET_BIB_TIME);
    }

 finish:
    MOD_DEC_USE_COUNT;
    return len;
}


static ssize_t proc_read_heartbeat(struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    int led_num=10;
    MOD_INC_USE_COUNT;

	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "%d\n", (get_sysLed(led_num) ? 1 : 0));
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t
proc_write_heartbeat (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len = count;
    char buf[8];
    int led_num = 10;
    unsigned long dsec = 0;

    MOD_INC_USE_COUNT;

    if ( count < 1 ) {
        printk(KERN_ERR "%s: no data for write", __FUNCTION__);
        len = 0;
        goto finish;
    }

    if ( len > ( sizeof(buf) - 1 ) ) len = ( sizeof(buf) - 1 );
    if ( copy_from_user(buf, buffer, len) ) {
        len = -EFAULT;
        goto finish;
    }
    buf[len] = '\0';
    sscanf(buf, "%lu", &dsec);



    if ( dsec <= 1 )
        sysLed(led_num, dsec);

    len = count;
 finish:
    MOD_DEC_USE_COUNT;
    return len;
}


static ssize_t proc_read_ctry(struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;

    if ( ! IS_RBD_VALID() ) {
        printk(KERN_ERR "%s", bd_err_msg);
        goto done;
    }
	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "%d\n", (GET_CTRY(rbd) ? 1 : 0 ));
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

 done:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t
proc_write_ctry (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    int mylen=4;
    char my_buffer[8];
    unsigned long val;

    MOD_INC_USE_COUNT;

    if (count > mylen )
        len = mylen;
    else
        len = count;

    if ( copy_from_user(my_buffer, buffer, len) ) {
        len = -EFAULT;
        goto finish;
    }
    my_buffer[len] = '\0';
    sscanf(my_buffer, "%lu", &val);
    if ((int)val & 1)
        rbd->v54Config |= BD_FIXED_CTRY;
    else
        rbd->v54Config &= ~BD_FIXED_CTRY;

    printk(KERN_INFO "%s: fixed_ctry_code=%d\n", MODULE_NAME, (rbd->v54Config&BD_FIXED_CTRY)?1:0);

 finish:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t proc_read_MACpool(struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;
    if ( ! IS_RBD_VALID() ) {
        printk(KERN_ERR "%s", bd_err_msg);
        goto done;
    }

	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "%d\n", (GET_MACPOOL(rbd) ? 1 : 0 ));
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

 done:
    MOD_DEC_USE_COUNT;
    return len;
}


static ssize_t
proc_write_MACpool (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len = count;
    char ch;
    MOD_INC_USE_COUNT;

    if ( count < 1 ) {
        printk(KERN_ERR "count(%lu) < 1\n", (unsigned long)count);
        len = 0;
        goto finish;
    }

    if ( copy_from_user(&ch, buffer, 1) ) {
        len = -EFAULT;
        goto finish;
    }
    if ( ch == '1' ) {

        _init_radio_macpool();
        rbd->v54Config |= BD_MACPOOL;
    } else {
        rbd->v54Config &= ~BD_MACPOOL;
    }

 finish:
    MOD_DEC_USE_COUNT;
    return len;

}

static ssize_t proc_read_numMACs(struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;
    if ( ! IS_RBD_VALID() ) {
        printk(KERN_ERR "%s", bd_err_msg);
        goto done;
    }

	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "%d\n" , GET_NUMMAC(rbd));
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

 done:
    MOD_DEC_USE_COUNT;
    return len;
}

static void
assign_mac_addr( unsigned char *dest, int seqno, unsigned char *src)
{
    dest[5] = seqno & 0xff;
    seqno >>= 8;
    dest[4] = seqno & 0xff;
    seqno >>= 8;
    dest[3] = seqno & 0xff;
    dest[2] = src[2];
    dest[1] = src[1];
    dest[0] = src[0];
}

static ssize_t
proc_write_numMACs (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    int mylen=4;
    char my_buffer[8];
    unsigned long val;

    MOD_INC_USE_COUNT;

    if (count > mylen )
        len = mylen;
    else
        len = count;

    if ( copy_from_user(my_buffer, buffer, len) ) {
        len = -EFAULT;
        goto finish;
    }
    my_buffer[len] = '\0';
    sscanf(my_buffer, "%lu", &val);
    rbd->MACcnt = (int)val & 0xff;
    if(rbd->MACcnt!=8 && rbd->MACcnt!=16) {
        printk("Invalid MACcnt: %d\n", rbd->MACcnt );
        rbd->MACcnt = 16;
    }
    printk(KERN_INFO "%s: MACcnt=%d\n", MODULE_NAME, rbd->MACcnt);

 finish:
    MOD_DEC_USE_COUNT;
    return len;
}


static ssize_t proc_read_MAClimit(struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;
    if ( ! IS_RBD_VALID() ) {
        printk(KERN_ERR "%s", bd_err_msg);
        goto done;
    }

	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "radio 0=%d\nradio 1=%d\n",
                  radio_macpool[0].MAClimit,
                  radio_macpool[1].MAClimit );
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

 done:
    MOD_DEC_USE_COUNT;
    return len;
}


static ssize_t proc_read_freeWlan(struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;

	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "radio 0=%d\nradio 1=%d\n",
                  radio_macpool[0].freeWlancnt,
                  radio_macpool[1].freeWlancnt );
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t proc_read_hiwaterMAC(struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;

	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "radio 0=%d\nradio 1=%d\n",
                  radio_macpool[0].MACalloc,
                  radio_macpool[1].MACalloc);
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t proc_read_baseMAC(struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;
    if ( ! IS_RBD_VALID() ) {
        printk(KERN_ERR "%s", bd_err_msg);
        goto done;
    }

	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "%s\n" , SPRINT_MAC(GET_MACPOOL(rbd) ?
                                            GET_BASEMAC(rbd) : GET_ENET0(bd)));
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

 done:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t
proc_write_baseMAC (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    int mylen=18;
    char my_buffer[20];
    unsigned int mac[6];

    MOD_INC_USE_COUNT;

    if (count > mylen )
        len = mylen;
    else
        len = count;

    if ( copy_from_user(my_buffer, buffer, len) ) {
        len = -EFAULT;
        goto finish;
    }
    my_buffer[len] = '\0';
    sscanf(my_buffer, "%x:%x:%x:%x:%x:%x",
           &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
    rbd->MACbase[0] = mac[0];
    rbd->MACbase[1] = mac[1];
    rbd->MACbase[2] = mac[2];
    rbd->MACbase[3] = mac[3];
    rbd->MACbase[4] = mac[4];
    rbd->MACbase[5] = mac[5];
    printk(KERN_INFO "%s: MACbase=%02x:%02x:%02x:%02x:%02x:%02x\n", MODULE_NAME,
           rbd->MACbase[0], rbd->MACbase[1], rbd->MACbase[2],
           rbd->MACbase[3], rbd->MACbase[4], rbd->MACbase[5]);


    {
        int i;
        unsigned char allocMac[V54_MACLEN];

        memcpy( allocMac, GET_BASEMAC(rbd), V54_MACLEN );

        i = 0;
        i = (allocMac[V54_MACLEN-3]<<16) |
            (allocMac[V54_MACLEN-2]<<8)  |
            (allocMac[V54_MACLEN-1]);

        if (rbd->MACcnt == 8 ) {
            assign_mac_addr( GET_WLAN0(bd), i + 1, allocMac);
            assign_mac_addr( GET_WLAN1(bd), i + 2, allocMac);
        } else {
            assign_mac_addr( GET_WLAN0(bd), i + 8,  allocMac);
            assign_mac_addr( GET_WLAN1(bd), i + 12, allocMac);
        }
        assign_mac_addr( GET_ENET0(bd),     i + 3, allocMac);
        assign_mac_addr( GET_ENET1(bd),     i + 4, allocMac);
        assign_mac_addr( GET_ENETX(0,rbd),  i + 5, allocMac);
        assign_mac_addr( GET_ENETX(1,rbd),  i + 6, allocMac);
        assign_mac_addr( GET_ENETX(2,rbd),  i + 7, allocMac);
    }

 finish:
    MOD_DEC_USE_COUNT;
    return len;
}


static ssize_t proc_read_ethport(struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;

    if ( ! IS_RBD_VALID() ) {
        printk(KERN_ERR "%s", bd_err_msg);
        goto done;
    }
	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "%d\n", GET_ETH_PORT);
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

 done:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t proc_write_ethport (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    int mylen=4;
    char my_buffer[8];
    unsigned long val;

    MOD_INC_USE_COUNT;

    if (count > mylen )
        len = mylen;
    else
        len = count;

    if ( copy_from_user(my_buffer, buffer, len) ) {
        len = -EFAULT;
        goto finish;
    }
    my_buffer[len] = '\0';
    sscanf(my_buffer, "%lu", &val);
    rbd->eth_port = (int)val & 0xff;
    printk(KERN_INFO "%s: ethPort=%d\n", MODULE_NAME, rbd->eth_port);

 finish:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t proc_read_symimgs(struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    MOD_INC_USE_COUNT;

    if ( ! IS_RBD_VALID() ) {
        printk(KERN_ERR "%s", bd_err_msg);
        goto done;
    }
	char buf[BOARD_DATA_PROC_READ_BUF_SIZE];
    len = snprintf(buf, sizeof(buf), "%d\n", GET_SYM_IMGS);
	len = simple_read_from_buffer(buffer, count, pos, buf, len);

 done:
    MOD_DEC_USE_COUNT;
    return len;
}

static ssize_t proc_write_symimgs (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
    int len=0;
    int mylen=4;
    char my_buffer[8];
    unsigned long val;

    MOD_INC_USE_COUNT;

    if (count > mylen )
        len = mylen;
    else
        len = count;

    if ( copy_from_user(my_buffer, buffer, len) ) {
        len = -EFAULT;
        goto finish;
    }
    my_buffer[len] = '\0';
    sscanf(my_buffer, "%lu", &val);
    rbd->sym_imgs = (int)val & 1;
    printk(KERN_INFO "%s: symImgs=%d\n", MODULE_NAME, rbd->sym_imgs);

 finish:
    MOD_DEC_USE_COUNT;
    return len;
}





static ssize_t proc_read_sw_version (struct file *filp, char __user *buffer,
				size_t count, loff_t *pos)
{
	int ret = -EFAULT;

	ret = strlen(sw_version_str);
	ret = simple_read_from_buffer(buffer, count, pos, sw_version_str, ret);

	return ret;
}


static ssize_t proc_write_sw_version (struct file *filp, const char __user *buffer,
				size_t count, loff_t *pos)
{
	memset(sw_version_str, 0, (max_version_len + 1));


	if(count >= max_version_len) {
		printk(KERN_ERR "%s: version string is too long. It should be < 128 bytes.\n", __func__);
		return -EFAULT;
	}

	if(buffer == NULL) {
		printk(KERN_ERR "%s: passing NULL pointer to kernel space.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(sw_version_str, buffer, count)) {
		printk(KERN_ERR "%s: failed to run copy_from_user().\n", __func__);
		return -EFAULT;
	}


	sw_version_str[count] = '\0';

	return count;
}


static struct proc_dir_entry  *proc_board;

static struct file_operations bdata_file
    , rbdata_file
    , rescan_file
    , bd_offset_file
    , rbd_offset_file
    , name_file
    , flash_id_file
    , led0_file
    , led1_file
    , led2_file
    , led3_file
    , led4_file
    , led5_file
    , model_file
    , customer_file
    , serial_file
    , type_file
    , random_file
    , himem_file
    , himem_data_file
    , bootline_file
    , main_img_id
    , running_img_id
    , oops_file
    , reason_file
    , total_boot_file
    , factory_file
    , sw2_file
    , boot_version_file
    , major_file
    , minor_file
    , ctry_file
    , boardType_file
    , boardClass_file
    , antInfo_file
    , numMACs
    , limitMAC
    , baseMAC
    , hiwaterMAC
    , freeWlan
    , MACpool
    , psePwr
    , ethport_file
    , symimgs_file
    , sw_version
    ;

#define MODE_READ   0444
#define MODE_WRITE  0222
#define MODE_RW     0644

#define PROC_BOARD  BOARD_DEV

static inline int
BD_CREATE_READ_PROC(struct file_operations *proc_fops
                  , char * name
                  , ssize_t (*read) (struct file *, char __user *, size_t, loff_t *)
                  , int (*open) (struct inode *, struct file *)
                  , void *data
                  )
{
    return !rks_create_read_proc(proc_fops, name, read, open, data, proc_board);
}

static inline int
BD_CREATE_WRITE_PROC(struct file_operations *proc_fops
                  , char * name
                  , ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *)
                  , void *data
                  )
{
    return !rks_create_write_proc(proc_fops, name, write, data, proc_board);
}

static inline int
BD_CREATE_RDWR_PROC(struct file_operations *proc_fops
                  , char *name
                  , ssize_t (*read) (struct file *, char __user *, size_t, loff_t *)
                  , ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *)
                  , int (*open) (struct inode *, struct file *)
                  , void *data
                  )
{
    return !rks_create_rdwr_proc(proc_fops, name, read, write, open, data, proc_board);
}

static inline int
create_proc_entries(void)
{
    int rv = 0;

    if ( (proc_board=PROC_ROOT_MK(PROC_BOARD)) == NULL ) {
        rv = -ENOMEM;
        goto out;
    }

    if ( BD_CREATE_RDWR_PROC(&bdata_file, BDATA_FILE, proc_read_bdata,proc_write_bdata,NULL,NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_bdata;
    }

    if ( BD_CREATE_RDWR_PROC(&rbdata_file, RBDATA_FILE, proc_read_rbdata,proc_write_rbdata,NULL,NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_rbdata;
    }

    if ( BD_CREATE_RDWR_PROC(&rescan_file, RESCAN_FILE, proc_read_rescan,proc_write_rescan,NULL,NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_rescan;
    }

    if ( BD_CREATE_READ_PROC(&bd_offset_file, BD_OFFSET_FILE, proc_read_bd_offset,NULL,NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_bd_offset;
    }

    if ( BD_CREATE_READ_PROC(&rbd_offset_file, RBD_OFFSET_FILE, proc_read_rbd_offset,NULL,NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_rbd_offset;
    }

    if ( BD_CREATE_RDWR_PROC(&name_file, NAME_FILE, proc_read_name,proc_write_name,NULL,NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_name;
    }

    if ( BD_CREATE_RDWR_PROC(&customer_file, CUSTOMER_FILE, proc_read_customer,proc_write_customer,NULL,NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_customer;
    }

    if ( BD_CREATE_RDWR_PROC(&model_file, MODEL_FILE, proc_read_model,proc_write_model,NULL,NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_model;
    }


    if ( BD_CREATE_RDWR_PROC(&serial_file, SERIAL_FILE, proc_read_serial, proc_write_serial,NULL, NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_serial;
    }


    if ( BD_CREATE_READ_PROC(&type_file, TYPE_FILE, proc_read_type,NULL, NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_type;
    }

    if ( BD_CREATE_RDWR_PROC(&boardType_file, BOARDTYPE_FILE, proc_read_boardType, proc_write_boardType,NULL, NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_boardType;
    }

    if ( BD_CREATE_RDWR_PROC(&boardClass_file, BOARDCLASS_FILE, proc_read_boardClass, proc_write_boardClass,NULL, NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_boardClass;
    }

    if ( BD_CREATE_RDWR_PROC(&antInfo_file, ANT_INFO_FILE, proc_read_antinfo, proc_write_antinfo,NULL, NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_antinfo;
    }


    if ( BD_CREATE_RDWR_PROC(&flash_id_file, FLASH_ID_FILE, proc_read_flash, proc_write_flash,NULL, NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_flash;
    }


    if ( BD_CREATE_RDWR_PROC(&random_file, RANDOM_FILE, proc_read_random, proc_write_random,NULL, NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_random;
    }
    if ( BD_CREATE_RDWR_PROC(&himem_file, HIMEM_FILE,NULL, proc_write_himem, proc_open_himem, NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_himem;
    }

    if ( BD_CREATE_RDWR_PROC(&himem_data_file, HIMEM_DATA_FILE, proc_read_himem_data, proc_write_himem_data,NULL, NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_himem_data;
    }

    if ( BD_CREATE_RDWR_PROC(&bootline_file, BOOTLINE_FILE, proc_read_bootline,
                          proc_write_bootline,NULL,NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_bootline;
    }

    if ( BD_CREATE_RDWR_PROC(&main_img_id, MAIN_IMG_FILE, proc_read_main_img,
                        proc_write_main_img,NULL,NULL) != 0 ) {
      rv = -ENOMEM;
      goto no_main_img;
    }

    if ( BD_CREATE_READ_PROC(&running_img_id, RUNNING_IMG_FILE,
                        proc_read_running_img,NULL, NULL) != 0 ) {
      rv = -ENOMEM;
      goto no_running_img;
    }


    if ( BD_CREATE_RDWR_PROC(&oops_file, OOPS_FILE, NULL, proc_write_oops,proc_open_oops,NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_oops;
    }

    if ( BD_CREATE_RDWR_PROC(&reason_file, REASON_FILE, NULL, proc_write_reason,proc_open_reason,NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_reason;
    }

    if ( BD_CREATE_READ_PROC(&total_boot_file, TOTAL_BOOT_FILE, NULL, proc_open_himem, (void *)HIMEM_TOTAL_BOOT) != 0 ) {
        rv = -ENOMEM;
        goto no_total_boot;
    }

    if ( BD_CREATE_RDWR_PROC(&factory_file, FACTORY_FILE, proc_read_factory, proc_write_factory,NULL, NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_factory;
    }

    if ( BD_CREATE_RDWR_PROC(&sw2_file, SW2_FILE, proc_read_sw2, proc_write_sw2,NULL, NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_sw2;
    }

    if ( BD_CREATE_RDWR_PROC(&major_file, MAJOR_FILE, proc_read_major, proc_write_major,NULL, NULL) != 0) {
        rv = -ENOMEM;
        goto no_major;
    }

    if ( BD_CREATE_RDWR_PROC(&minor_file, MINOR_FILE, proc_read_minor, proc_write_minor,NULL, NULL) != 0) {
        rv = -ENOMEM;
        goto no_minor;
    }

    if ( BD_CREATE_READ_PROC(&boot_version_file, BOOT_VERSION_FILE, NULL,proc_open_boot_version,NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_boot_version;
    }

    if ( BD_CREATE_RDWR_PROC(&led0_file, LED0_FILE, proc_read_led, proc_write_led,NULL,(void*)0) != 0 ) {
        rv = -ENOMEM;
        goto no_led0;
    }

    if ( BD_CREATE_RDWR_PROC(&led1_file, LED1_FILE, proc_read_led, proc_write_led,NULL,(void*)1) != 0 ) {
        rv = -ENOMEM;
        goto no_led1;
    }

    if ( BD_CREATE_RDWR_PROC(&led2_file, LED2_FILE, proc_read_led, proc_write_led,NULL,(void*)2) != 0 ) {
        rv = -ENOMEM;
        goto no_led2;
    }

    if ( BD_CREATE_RDWR_PROC(&led3_file, LED3_FILE, proc_read_led, proc_write_led,NULL,(void*)3) != 0 ) {
        rv = -ENOMEM;
        goto no_led3;
    }

    if (!rks_is_walle_board()) {
        if ( BD_CREATE_RDWR_PROC(&led4_file, LED4_FILE, proc_read_led, proc_write_led,NULL,(void*)4) != 0 ) {
            rv = -ENOMEM;
            goto no_led4;
        }

        if ( BD_CREATE_RDWR_PROC(&led5_file, LED5_FILE, proc_read_led, proc_write_led,NULL,(void*)5) != 0 ) {
            rv = -ENOMEM;
            goto no_led5;
        }

    }


    if ( BD_CREATE_RDWR_PROC(&ctry_file, CTRY_FILE, proc_read_ctry, proc_write_ctry,NULL, NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_ctry;
    }

    if ( BD_CREATE_RDWR_PROC(&numMACs, NUM_MACS, proc_read_numMACs, proc_write_numMACs,NULL, NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_numMACs;
    }

    if ( BD_CREATE_READ_PROC(&limitMAC, LIMIT_MAC, proc_read_MAClimit,NULL,NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_MAClimit;
    }

    if ( BD_CREATE_READ_PROC(&hiwaterMAC, HIWATER_MAC, proc_read_hiwaterMAC,NULL,NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_hiwaterMAC;
    }

    if ( BD_CREATE_READ_PROC(&freeWlan, FREE_WMAC, proc_read_freeWlan,NULL,NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_freeWlan;
    }

    if ( BD_CREATE_RDWR_PROC(&baseMAC, BASE_MAC, proc_read_baseMAC, proc_write_baseMAC, NULL,NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_baseMAC;
    }

    if ( BD_CREATE_RDWR_PROC(&MACpool, MAC_POOL, proc_read_MACpool, proc_write_MACpool, NULL,NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_MACpool;
    }


    if ( BD_CREATE_RDWR_PROC(&ethport_file, ETH_PORT_FILE, proc_read_ethport, proc_write_ethport, NULL, NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_ethport;
    }
    if ( BD_CREATE_RDWR_PROC(&symimgs_file, SYM_IMGS_FILE, proc_read_symimgs, proc_write_symimgs, NULL, NULL) != 0 ) {
        rv = -ENOMEM;
        goto no_symimgs;
    }



	if( BD_CREATE_RDWR_PROC(&sw_version, RKS_SW_VER, proc_read_sw_version, proc_write_sw_version, NULL, NULL) != 0) {
		rv = -ENOMEM;
		goto no_sw_version;
	}


    return 0;
	remove_proc_entry(RKS_SW_VER, proc_board);
no_sw_version:
    remove_proc_entry(SYM_IMGS_FILE, proc_board);
 no_symimgs:
    remove_proc_entry(ETH_PORT_FILE, proc_board);
 no_ethport:
    remove_proc_entry(MAC_POOL, proc_board);
 no_MACpool:
    remove_proc_entry(BASE_MAC, proc_board);
 no_baseMAC:
    remove_proc_entry(FREE_WMAC, proc_board);
 no_freeWlan:
    remove_proc_entry(HIWATER_MAC, proc_board);
 no_hiwaterMAC:
    remove_proc_entry(LIMIT_MAC, proc_board);
 no_MAClimit:
    remove_proc_entry(NUM_MACS, proc_board);
 no_numMACs:
    remove_proc_entry(CTRY_FILE, proc_board);
 no_ctry:
    remove_proc_entry(LED5_FILE, proc_board);
 no_led5:
    remove_proc_entry(LED4_FILE, proc_board);
 no_led4:
    remove_proc_entry(LED3_FILE, proc_board);
 no_led3:
    remove_proc_entry(LED2_FILE, proc_board);
 no_led2:
    remove_proc_entry(LED1_FILE, proc_board);
 no_led1:
    remove_proc_entry(LED0_FILE, proc_board);
 no_led0:
    remove_proc_entry(BOOT_VERSION_FILE, proc_board);
 no_boot_version:
    remove_proc_entry(MINOR_FILE, proc_board);
 no_minor:
    remove_proc_entry(MAJOR_FILE, proc_board);
 no_major:
    remove_proc_entry(SW2_FILE, proc_board);
 no_sw2:
    remove_proc_entry(FACTORY_FILE, proc_board);
 no_factory:
    remove_proc_entry(TOTAL_BOOT_FILE, proc_board);
 no_total_boot:
    remove_proc_entry(REASON_FILE, proc_board);
 no_reason:
    remove_proc_entry(OOPS_FILE, proc_board);
 no_oops:
   remove_proc_entry(RUNNING_IMG_FILE, proc_board);
 no_running_img:
    remove_proc_entry(MAIN_IMG_FILE, proc_board);
 no_main_img:
   remove_proc_entry(BOOTLINE_FILE, proc_board);
 no_bootline:
    remove_proc_entry(HIMEM_DATA_FILE, proc_board);
 no_himem_data:
    remove_proc_entry(HIMEM_FILE, proc_board);
 no_himem:
    remove_proc_entry(RANDOM_FILE, proc_board);
 no_random:
    remove_proc_entry(ANT_INFO_FILE, proc_board);
 no_antinfo:
    remove_proc_entry(BOARDCLASS_FILE, proc_board);
 no_boardClass:
    remove_proc_entry(BOARDTYPE_FILE, proc_board);
 no_boardType:
    remove_proc_entry(TYPE_FILE, proc_board);
 no_type:
    remove_proc_entry(SERIAL_FILE, proc_board);
 no_serial:
    remove_proc_entry(MODEL_FILE, proc_board);
 no_model:
    remove_proc_entry(CUSTOMER_FILE, proc_board);
 no_customer:
    remove_proc_entry(NAME_FILE, proc_board);
 no_flash:
    remove_proc_entry(FLASH_ID_FILE, proc_board);
 no_name:
    remove_proc_entry(RBD_OFFSET_FILE, proc_board);
 no_rbd_offset:
    remove_proc_entry(BD_OFFSET_FILE, proc_board);
 no_bd_offset:
    remove_proc_entry(RESCAN_FILE, proc_board);
 no_rescan:
    remove_proc_entry(RBDATA_FILE, proc_board);
 no_rbdata:
    remove_proc_entry(BDATA_FILE, proc_board);
 no_bdata:
    remove_proc_entry(PROC_BOARD, NULL);
 out:
    return rv;
}

static inline void
cleanup_proc_entries(void)
{

    remove_proc_entry(SYM_IMGS_FILE, proc_board);
    remove_proc_entry(ETH_PORT_FILE, proc_board);
    remove_proc_entry(MAC_POOL, proc_board);
    remove_proc_entry(HIWATER_MAC, proc_board);
    remove_proc_entry(FREE_WMAC, proc_board);
    remove_proc_entry(BASE_MAC, proc_board);
    remove_proc_entry(LIMIT_MAC, proc_board);
    remove_proc_entry(NUM_MACS, proc_board);
    remove_proc_entry(CTRY_FILE, proc_board);
    remove_proc_entry(LED5_FILE, proc_board);
    remove_proc_entry(LED4_FILE, proc_board);
    remove_proc_entry(LED3_FILE, proc_board);
    remove_proc_entry(LED2_FILE, proc_board);
    remove_proc_entry(LED1_FILE, proc_board);
    remove_proc_entry(LED0_FILE, proc_board);
    remove_proc_entry(BOOT_VERSION_FILE, proc_board);
    remove_proc_entry(MINOR_FILE, proc_board);
    remove_proc_entry(MAJOR_FILE, proc_board);
    remove_proc_entry(FACTORY_FILE, proc_board);
    remove_proc_entry(TOTAL_BOOT_FILE, proc_board);
    remove_proc_entry(REASON_FILE, proc_board);
    remove_proc_entry(OOPS_FILE, proc_board);
    remove_proc_entry(RUNNING_IMG_FILE, proc_board);
    remove_proc_entry(MAIN_IMG_FILE, proc_board);
    remove_proc_entry(BOOTLINE_FILE, proc_board);
    remove_proc_entry(HIMEM_DATA_FILE, proc_board);
    remove_proc_entry(HIMEM_FILE, proc_board);
    remove_proc_entry(RANDOM_FILE, proc_board);
    remove_proc_entry(BOARDCLASS_FILE, proc_board);
    remove_proc_entry(BOARDTYPE_FILE, proc_board);
    remove_proc_entry(TYPE_FILE, proc_board);
    remove_proc_entry(ANT_INFO_FILE, proc_board);
    remove_proc_entry(SERIAL_FILE, proc_board);
    remove_proc_entry(MODEL_FILE, proc_board);
    remove_proc_entry(CUSTOMER_FILE, proc_board);
    remove_proc_entry(NAME_FILE, proc_board);
    remove_proc_entry(RBD_OFFSET_FILE, proc_board);
    remove_proc_entry(BD_OFFSET_FILE, proc_board);
    remove_proc_entry(RESCAN_FILE, proc_board);
    remove_proc_entry(RBDATA_FILE, proc_board);
    remove_proc_entry(BDATA_FILE, proc_board);


    PROC_ROOT_RM(PROC_BOARD);

    return;
}
#endif


unsigned int
rks_get_antinfo(void)
{
    if (ANTINFO_INVALID(rbd)) {
        printk(KERN_ERR "%s", bd_err_msg);
        return 0;
    }
    return GET_ANTINFO(rbd);
}
EXPORT_SYMBOL(rks_get_antinfo);

unsigned int
rks_get_boardrev(unsigned char *btype, unsigned short *major,  unsigned short *minor, unsigned char *model_len, char *model)
{
    unsigned char len;
    if (btype == NULL || major == NULL || minor == NULL || model == NULL) {
        printk(KERN_ERR "%s", bd_err_msg);
        return -1;
    }
    *btype = rbd->boardType;
    *major = GET_MAJOR(bd);
    *minor = GET_MINOR(bd);

    len = strlen(GET_MODEL(rbd));
    if (len > *model_len) {
      len = *model_len;
    }
    *model_len = len;
    strncpy(model, GET_MODEL(rbd), len);

    return 0;
}
EXPORT_SYMBOL(rks_get_boardrev);

const char *
rks_get_boardtype(void)
{
    if (rbd == NULL) {
        printk(KERN_ERR "%s", bd_err_msg);
        return 0;
    }
    return GET_BOARD_TYPE(rbd);
}
EXPORT_SYMBOL(rks_get_boardtype);

void
rks_get_memory_info(unsigned long *totalram, unsigned long *freeram)
{
     struct sysinfo si;
     si_meminfo(&si);
     *totalram = si.totalram * 4096;
     *freeram  = si.freeram * 4096;
     return;
}
EXPORT_SYMBOL(rks_get_memory_info);

unsigned char
rks_is_gd6_board(void)
{
    return ((rbd->boardType == 5) || (rbd->boardType == 6));
}
EXPORT_SYMBOL(rks_is_gd6_board);

unsigned char
rks_is_husky_board(void)
{
    return (unsigned char)(rbd ? rbd->boardType == 8 : 0);
}
EXPORT_SYMBOL(rks_is_husky_board);

unsigned char
rks_is_bahama_board(void)
{
    return (unsigned char)(rbd ? rbd->boardType == 11 : 0);
}
EXPORT_SYMBOL(rks_is_bahama_board);

unsigned char
rks_is_retriever_board(void)
{
    return (unsigned char)(rbd ? rbd->boardType == V54_BTYPE_RETRIEVER : 0);
}
EXPORT_SYMBOL(rks_is_retriever_board);

unsigned char
rks_is_gd8_board(void)
{
    return (unsigned char)(rbd ? rbd->boardType == V54_BTYPE_GD8 : 0);
}
EXPORT_SYMBOL(rks_is_gd8_board);

unsigned char
rks_is_gd9_board(void)
{
    return (unsigned char)(rbd ? rbd->boardType == V54_BTYPE_GD9 : 0);
}
EXPORT_SYMBOL(rks_is_gd9_board);

unsigned char
rks_is_gd10_board(void)
{

    return (unsigned char)(rbd ? (rbd->boardType == V54_BTYPE_GD10 || rbd->boardType == V54_BTYPE_BERMUDA) : 0);
}
EXPORT_SYMBOL(rks_is_gd10_board);

unsigned char
rks_is_gd11_board(void)
{
    return (unsigned char)(rbd ? rbd->boardType == V54_BTYPE_GD11 : 0);
}
EXPORT_SYMBOL(rks_is_gd11_board);

unsigned char
rks_is_gd12_board(void)
{
    return (unsigned char)(rbd ? rbd->boardType == V54_BTYPE_GD12 : 0);
}
EXPORT_SYMBOL(rks_is_gd12_board);

unsigned char
rks_is_gd17_board(void)
{
    return (unsigned char)(rbd ? rbd->boardType == V54_BTYPE_GD17 : 0);
}
EXPORT_SYMBOL(rks_is_gd17_board);

unsigned char
rks_is_walle_board(void)
{
    return (unsigned char)(rbd ? rbd->boardType == V54_BTYPE_WALLE : 0);
}
EXPORT_SYMBOL(rks_is_walle_board);

unsigned char
rks_is_bermuda_board(void)
{

    return (unsigned char)(rbd ? rbd->boardType == V54_BTYPE_BERMUDA : 0);
}
EXPORT_SYMBOL(rks_is_bermuda_board);

unsigned char
rks_is_7762s_board(void)
{

    if ( rks_is_gd10_board() &&
         (((strcmp(GET_MODEL(rbd), "ZF7762-S") == 0) ||
           (strcmp(GET_MODEL(rbd), "zf7762-s") == 0) ||
           (strcmp(GET_MODEL(rbd), "zf7762-S") == 0) ||
           (strcmp(GET_MODEL(rbd), "ZF7762-s") == 0))) ) {
        return 1;
    }

    return 0;

}
EXPORT_SYMBOL(rks_is_7762s_board);

unsigned char
rks_is_gd22_board(void)
{
    return (unsigned char)(rbd ? rbd->boardType == V54_BTYPE_GD22 : 0);
}
EXPORT_SYMBOL(rks_is_gd22_board);

unsigned char
rks_is_gd25_board(void)
{
    return (unsigned char)(rbd ? rbd->boardType == V54_BTYPE_GD25 : 0);
}
EXPORT_SYMBOL(rks_is_gd25_board);

unsigned char
rks_is_gd26_board(void)
{
    return (unsigned char)(rbd ? rbd->boardType == V54_BTYPE_GD26 : 0);
}
EXPORT_SYMBOL(rks_is_gd26_board);

unsigned char
rks_is_gd26_ge(void)
{
    return (unsigned char)(GET_GE_PORT(rbd)?1:0);
}
EXPORT_SYMBOL(rks_is_gd26_ge);

unsigned char
rks_is_gd28_board(void)
{
    return (unsigned char)(rbd ? rbd->boardType == V54_BTYPE_GD28 : 0);
}
EXPORT_SYMBOL(rks_is_gd28_board);

unsigned char
rks_is_gd34_board(void)
{
    return (unsigned char)(rbd ? rbd->boardType == V54_BTYPE_GD34 : 0);
}
EXPORT_SYMBOL(rks_is_gd34_board);

unsigned char
rks_is_spaniel_board(void)
{
    return (unsigned char)(rbd ? rbd->boardType == V54_BTYPE_SPANIEL : 0);
}
EXPORT_SYMBOL(rks_is_spaniel_board);

unsigned char
rks_is_spaniel_usb_board(void)
{
    if ( rks_is_spaniel_board() &&
         (((strcmp(GET_MODEL(rbd), "ZF7321-U") == 0) ||
           (strcmp(GET_MODEL(rbd), "zf7321-u") == 0) ||
           (strcmp(GET_MODEL(rbd), "zf7321-U") == 0) ||
           (strcmp(GET_MODEL(rbd), "ZF7321-u") == 0))) ) {
        return 1;
    }

    return 0;
}
EXPORT_SYMBOL(rks_is_spaniel_usb_board);

unsigned char
rks_is_gd36_board(void)
{
    return (unsigned char)(rbd ? rbd->boardType == V54_BTYPE_GD36 : 0);
}
EXPORT_SYMBOL(rks_is_gd36_board);

unsigned char
rks_is_walle2_board(void)
{
    return (unsigned char)(rbd ? rbd->boardType == V54_BTYPE_WALLE2 : 0);
}
EXPORT_SYMBOL(rks_is_walle2_board);

unsigned char
rks_is_gd35_board(void)
{
    return (unsigned char)(rbd ? rbd->boardType == V54_BTYPE_GD35 : 0);
}
EXPORT_SYMBOL(rks_is_gd35_board);

unsigned char
rks_is_gd56_board(void)
{
    return (unsigned char)(rbd ? rbd->boardType == V54_BTYPE_GD56 : 0);
}
EXPORT_SYMBOL(rks_is_gd56_board);

unsigned char
rks_is_gd50_board(void)
{
    return (unsigned char)(rbd ? rbd->boardType == V54_BTYPE_GD50 : 0);
}
EXPORT_SYMBOL(rks_is_gd50_board);

unsigned char
rks_is_gd60_board(void)
{
    return (unsigned char)(rbd ? rbd->boardType == V54_BTYPE_GD60 : 0);
}
EXPORT_SYMBOL(rks_is_gd60_board);

unsigned char
rks_is_gd66_board(void)
{
    return (unsigned char)(rbd ? rbd->boardType == V54_BTYPE_GD66 : 0);
}
EXPORT_SYMBOL(rks_is_gd66_board);

unsigned char
rks_is_cm_board(void)
{
    unsigned char *model, val = 0;
    if (rks_is_gd35_board()){
        model = GET_MODEL(rbd);
        if ((model[6] == 'c' || model[6] == 'C') &&
            (model[7] == 'm' || model[7] == 'M')) {
            val = 1;
        }
    }
    return val;
}
EXPORT_SYMBOL(rks_is_cm_board);

unsigned char
rks_is_puli_board(void)
{
  unsigned char *model, val = 0;
  model = GET_MODEL(rbd);
  if (strcmp((model+2), "7441") == 0)
    val = 1;
  return val;
}
EXPORT_SYMBOL(rks_is_puli_board);

unsigned char
rks_is_r300_board(void)
{
    if ((strcmp(GET_MODEL(rbd), "r300") == 0) ||
        (strcmp(GET_MODEL(rbd), "R300") == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_r300_board);

unsigned char
rks_is_gd43_board(void)
{
    return (unsigned char)(rbd ? rbd->boardType == V54_BTYPE_GD43 : 0);
}
EXPORT_SYMBOL(rks_is_gd43_board);

unsigned char
rks_is_gd42_board(void)
{
    return (unsigned char)(rbd ? rbd->boardType == V54_BTYPE_GD42 : 0);
}
EXPORT_SYMBOL(rks_is_gd42_board);

unsigned char
rks_is_r500_board(void)
{
    if ((strcmp(GET_MODEL(rbd), "r500") == 0) ||
        (strcmp(GET_MODEL(rbd), "R500") == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_r500_board);

unsigned char
rks_is_r600_board(void)
{
    if ((strcmp(GET_MODEL(rbd), "r600") == 0) ||
        (strcmp(GET_MODEL(rbd), "R600") == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_r600_board);

unsigned char
rks_is_t504_board(void)
{
    if ((strcmp(GET_MODEL(rbd), "t504") == 0) ||
        (strcmp(GET_MODEL(rbd), "T504") == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_t504_board);

unsigned char
rks_is_p300_board(void)
{
    if ((strcmp(GET_MODEL(rbd), "p300") == 0) ||
        (strcmp(GET_MODEL(rbd), "P300") == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_p300_board);

unsigned char
rks_is_h500_board(void)
{
    if ((strncmp(GET_MODEL(rbd), "h500",strlen(GET_MODEL(rbd))) == 0) ||
        (strncmp(GET_MODEL(rbd), "H500",strlen(GET_MODEL(rbd))) == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_h500_board);

unsigned char
rks_is_c110_board(void)
{
    if ((strncmp(GET_MODEL(rbd), "c110",strlen(GET_MODEL(rbd))) == 0) ||
        (strncmp(GET_MODEL(rbd), "C110",strlen(GET_MODEL(rbd))) == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_c110_board);

unsigned char
rks_is_r710_board(void)
{
    if ((strcmp(GET_MODEL(rbd), "r710") == 0) ||
        (strcmp(GET_MODEL(rbd), "R710") == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_r710_board);

unsigned char
rks_is_r720_board(void)
{
    if ((strcmp(GET_MODEL(rbd), "r720") == 0) ||
        (strcmp(GET_MODEL(rbd), "R720") == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_r720_board);

unsigned char
rks_is_t710_board(void)
{

    if ((strncmp(GET_MODEL(rbd), "t710", 4) == 0) ||
        (strncmp(GET_MODEL(rbd), "T710", 4) == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_t710_board);

unsigned char
rks_is_t811cm_board(void)
{

    if (strncasecmp(GET_MODEL(rbd), "T811CM", 6) == 0)
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_t811cm_board);

unsigned char
rks_is_c500_board(void)
{
    if ((strncmp(GET_MODEL(rbd), "c500",strlen(GET_MODEL(rbd))) == 0) ||
        (strncmp(GET_MODEL(rbd), "C500",strlen(GET_MODEL(rbd))) == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_c500_board);

unsigned char
rks_is_r310_board(void)
{
    if ((strncmp(GET_MODEL(rbd), "r310",strlen(GET_MODEL(rbd))) == 0) ||
        (strncmp(GET_MODEL(rbd), "R310",strlen(GET_MODEL(rbd))) == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_r310_board);

unsigned char
rks_is_r510_board(void)
{
    if ((strncmp(GET_MODEL(rbd), "r510",strlen(GET_MODEL(rbd))) == 0) ||
        (strncmp(GET_MODEL(rbd), "R510",strlen(GET_MODEL(rbd))) == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_r510_board);

unsigned char
rks_is_r610_board(void)
{

    if ((strncmp(GET_MODEL(rbd), "r610",4) == 0) ||
        (strncmp(GET_MODEL(rbd), "R610",4) == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_r610_board);

unsigned char
rks_is_t610_board(void)
{
    if ((strncmp(GET_MODEL(rbd), "t610", 4) == 0) ||
        (strncmp(GET_MODEL(rbd), "T610", 4) == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_t610_board);

unsigned char
rks_is_h510_board(void)
{
    if ((strncmp(GET_MODEL(rbd), "h510",strlen(GET_MODEL(rbd))) == 0) ||
        (strncmp(GET_MODEL(rbd), "H510",strlen(GET_MODEL(rbd))) == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_h510_board);

unsigned char
rks_is_h320_board(void)
{
    if ((strncmp(GET_MODEL(rbd), "h320",strlen(GET_MODEL(rbd))) == 0) ||
        (strncmp(GET_MODEL(rbd), "H320",strlen(GET_MODEL(rbd))) == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_h320_board);


unsigned char
rks_is_fips_sku(void)
{
    return GET_FIPS_SKU(rbd);
}
EXPORT_SYMBOL(rks_is_fips_sku);

unsigned char
rks_is_t310_board(void)
{

    if ((strncmp(GET_MODEL(rbd), "t310",strlen(GET_MODEL(rbd)) - 1) == 0) ||
        (strncmp(GET_MODEL(rbd), "T310",strlen(GET_MODEL(rbd)) - 1) == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_t310_board);

unsigned char
rks_is_e510_board(void)
{
    if ((strncmp(GET_MODEL(rbd), "e510",strlen(GET_MODEL(rbd))) == 0) ||
        (strncmp(GET_MODEL(rbd), "E510",strlen(GET_MODEL(rbd))) == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_e510_board);

unsigned char
rks_is_r730_board(void)
{
    if ((strcmp(GET_MODEL(rbd), "r730") == 0) ||
        (strcmp(GET_MODEL(rbd), "R730") == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_r730_board);

unsigned char
rks_is_r750_board(void)
{
    if ((strcmp(GET_MODEL(rbd), "r750") == 0) ||
        (strcmp(GET_MODEL(rbd), "R750") == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_r750_board);

unsigned char
rks_is_t750_board(void)
{
    if ((strncmp(GET_MODEL(rbd), "t750", strlen("t750")) == 0) ||
        (strncmp(GET_MODEL(rbd), "T750", strlen("T750"))== 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_t750_board);

unsigned char
rks_is_r650_board(void)
{
    if ((strcmp(GET_MODEL(rbd), "r650") == 0) ||
        (strcmp(GET_MODEL(rbd), "R650") == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_r650_board);

unsigned char
rks_is_r550_board(void)
{
    if ((strcmp(GET_MODEL(rbd), "r550") == 0) ||
        (strcmp(GET_MODEL(rbd), "R550") == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_r550_board);

unsigned char
rks_is_r850_board(void)
{
    if ((strcmp(GET_MODEL(rbd), "r850") == 0) ||
        (strcmp(GET_MODEL(rbd), "R850") == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_r850_board);


unsigned char
rks_is_t750se_board(void)
{
    if ((strncmp(GET_MODEL(rbd), "t750se", strlen("t750se")) == 0) ||
        (strncmp(GET_MODEL(rbd), "T750SE", strlen("T750SE"))== 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_t750se_board);

unsigned char
rks_is_h550_board(void)
{
    if ((strncmp(GET_MODEL(rbd), "h550", strlen("h550")) == 0) ||
        (strncmp(GET_MODEL(rbd), "H550", strlen("H550"))== 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_h550_board);

unsigned char
rks_is_h350_board(void)
{
    if ((strncmp(GET_MODEL(rbd), "h350", strlen("h350")) == 0) ||
        (strncmp(GET_MODEL(rbd), "H350", strlen("H350"))== 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_h350_board);

unsigned char
rks_is_t350_board(void)
{
    if ((strncmp(GET_MODEL(rbd), "t350", strlen("t350")) == 0) ||
        (strncmp(GET_MODEL(rbd), "T350", strlen("T350"))== 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_t350_board);

unsigned char
rks_is_t350se_board(void)
{
    if ((strncmp(GET_MODEL(rbd), "t350se", strlen("t350se")) == 0) ||
        (strncmp(GET_MODEL(rbd), "T350SE", strlen("T350SE"))== 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_t350se_board);


unsigned char
rks_is_r350_board(void)
{
    if ((strncmp(GET_MODEL(rbd), "r350", strlen("r350")) == 0) ||
        (strncmp(GET_MODEL(rbd), "R350", strlen("R350"))== 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_r350_board);

unsigned char
rks_is_m510_board(void)
{
    if ((strncmp(GET_MODEL(rbd), "m510",strlen(GET_MODEL(rbd))) == 0) ||
        (strncmp(GET_MODEL(rbd), "M510",strlen(GET_MODEL(rbd))) == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_m510_board);

unsigned char
rks_is_r320_board(void)
{
    if ((strncmp(GET_MODEL(rbd), "r320",strlen(GET_MODEL(rbd))) == 0) ||
        (strncmp(GET_MODEL(rbd), "R320",strlen(GET_MODEL(rbd))) == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_r320_board);
unsigned char
rks_is_t305_board(void)
{

    if ((strncmp(GET_MODEL(rbd), "t305",strlen(GET_MODEL(rbd)) - 1) == 0) ||
        (strncmp(GET_MODEL(rbd), "T305",strlen(GET_MODEL(rbd)) - 1) == 0))
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(rks_is_t305_board);

int bd_get_basemac(unsigned char *macaddr)
{
	memcpy(macaddr, rks_boardData->MACbase, ETH_ALEN);
	return 0;
}
EXPORT_SYMBOL(bd_get_basemac);





int bsp_reset_init(void);

void bsp_reset_exit(void);
void brownout_12v_init(void);
void power_status_init(void);



static int __init
init_v54_board_data(void)
{
    int rv = 0;
    int result = 0;
#ifdef CONFIG_OF
    unsigned int reg[2];
    struct device_node *np = NULL;
#endif
    extern void v54_register_on_reboot(void (*reboot_callback)(u_int32_t reason));
    printk(KERN_INFO "%s: Version %s\n"
           "Copyright (c) 2004-2007 Ruckus Wireless, Inc."
           "All Rights Reserved\n", MODULE_NAME, MOD_VER);


#include "nar5520_bsp.h"
    nar5520_bodyguard_init();
    nar5520_wdt_init();


    if ( ! ar531xSetupFlash() ) {
        printk("%s: Flash setup failed\n", MODULE_NAME);



    }

    if(_mem_top == 0) {
        _mem_top = (unsigned long )kmalloc(512*1024, GFP_KERNEL);
        printk("%s: After Static allocation of mem_top for himem %ld \n", MODULE_NAME, _mem_top);
    }
    bd = ar531x_boardData;
    rbd = rks_boardData;



    if( rbd->v54Config == 0xffffffff ) {
        rbd->v54Config = 0;
    }

#if 1


    if( !(rbd->v54Config & BD_MACPOOL) ||
        rbd->MACcnt > 256 ) {
        memcpy( GET_BASEMAC(rbd), GET_WLAN0(bd), V54_MACLEN );
        rbd->v54Config |= BD_MACPOOL;
        if (strcmp( GET_MODEL(rbd), "zf2925") == 0) {
            int temp = 0, temp1;

            rbd->MACcnt   = 16;

            memcpy( allocMac, GET_BASEMAC(rbd), V54_MACLEN );
            temp = (allocMac[V54_MACLEN-3]<<16) |
                (allocMac[V54_MACLEN-2]<<8)  |
                (allocMac[V54_MACLEN-1]);
            temp1 = temp + 8;
            allocMac[V54_MACLEN-1] = 0xff & temp1;
            temp1 >>= 8;
            allocMac[V54_MACLEN-2] = 0xff & temp1;
            temp1 >>= 8;
            allocMac[V54_MACLEN-3] = 0xff & temp1;
            memcpy( GET_WLAN0(bd), allocMac, V54_MACLEN );

            temp1 = temp + 12;
            allocMac[V54_MACLEN-1] = 0xff & temp1;
            temp1 >>= 8;
            allocMac[V54_MACLEN-2] = 0xff & temp1;
            temp1 >>= 8;
            allocMac[V54_MACLEN-3] = 0xff & temp1;
            memcpy( GET_WLAN1(bd), allocMac, V54_MACLEN );

        } else {
#define RKS_BD_MAC(MACUnit) &(rbd->enetxMac[MACUnit-2][0])
            rbd->MACcnt   = 8;
            memcpy( GET_WLAN0(bd), GET_WLAN1(bd), V54_MACLEN );
            memcpy( GET_WLAN1(bd), RKS_BD_MAC(5), V54_MACLEN );
        }
    }
#endif

    _init_radio_macpool();


    rv = create_proc_entries();
    if ( rv != 0 ) {
        goto failed;
    }

    v54_register_on_reboot(v54_reboot_reason);

    return rv;
 failed:

    return rv;

}

module_init(init_v54_board_data);

static void __exit
exit_v54_board_data(void)
{

    cleanup_proc_entries();



    bsp_reset_exit();

    printk(KERN_INFO "%s: driver unloaded\n", MODULE_NAME);
}

module_exit(exit_v54_board_data);

MODULE_AUTHOR("Video54 Technologies, Inc.");
MODULE_DESCRIPTION("Board Data Information");
#ifdef MODULE_LICENSE
MODULE_LICENSE("Proprietary");
#endif
