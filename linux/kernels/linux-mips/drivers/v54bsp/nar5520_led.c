/*
 * Copyright (c) 2004-2010 Ruckus Wireless, Inc.
 * All rights reserved.
 *
 */
 //Tony Chen    2008-03-04   Initial
 //Tony Chen    2010-12-07   Add hardware checking.

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include <asm/io.h>

#include "nar5520_superio_gpio.h"
#include "nar5520_hwck.h"
#include <linux/ipmi.h>
#include <linux/ipmi_smi.h>


#if defined(__KERNEL__)
#include <linux/kernel.h>
#define V54_PRINTF printk
#define DBG_LEVEL
#else
#define V54_PRINTF(...)
#define DBG_LEVEL KERN_DEBUG
#endif

#ifdef __DEBUG__
#define LED_TRACK(fmt, args...) V54_PRINTF(DBG_LEVEL "#### [%s] "fmt" ####\n", __FUNCTION__, ## args )
#define LED_PRINTF LED_TRACK
#else
#define LED_TRACK(fmt, args...) V54_PRINTF(DBG_LEVEL "%s: "fmt"\n", __FUNCTION__, ## args )
#define LED_PRINTF(...)
#endif
#define IPMI_MAX_INTF            (4)
#define IPMI_NETFUNC_APP         (0x06)

#define SET_RED		2
#define SET_GREEN	1
#define SET_OFF		0


#define GPIO3_IO_REG		CRF0
#define GPIO3_DATA_REG		CRF1
#define GPIO3_INVERSION_REG	CRF2
#define I2C_PIN_SEL		CR2A
#define MULTI_FUNCTION_SEL	CR2C

#define GPIO2_IO_REG	    	CRE3
#define GPIO2_DATA_REG	  	CRE4
#define GPIO2_INVERSION_REG	CRE5
#define GPIO5_IO_REG    		CRE0
#define GPIO5_DATA_REG   		CRE1
#define GPIO5_INVERSION_REG	CRE2

#define LED_DEVICE		LDN9


#define V54_GPIO_LED0	0x01
#define V54_GPIO_LED1	0x00
#define V54_GPIO_LED2	0x02
#define V54_GPIO_LED3	0x00


#define SUSLED_MASK  0x20
#define PLED_MASK    0x3f
#define GP23_MASK    0xf7
#define GP55_MASK    0xdf
#define ENROM_MASK   0xfd

#define MNR_MASK 0xf7
#define MJR_MASK 0xfb
#define CRT_MASK 0xfd
#define PWR_MASK 0xfe
#define COLOR_MASK 0xbf
#define IPMI_ALARMS_DATA1 0x03
#define IPMI_ALARMS_DATA2 0x40
#define IPMI_ALARMS_DATA3 0x1
#define IPMI_ALARMS_CMD 0x52
#define IPMI_ALARMS_INIT_VALUE 0x0
#define IPMI_ALARMS_RESET_VALUE 0xff

#define IPMI_ALARMS_MNR 1
#define IPMI_ALARMS_MJR 3
#define IPMI_ALARMS_CRT 4
#define IPMI_ALARMS_PWR 5

#define IPMI_LED_COLOR_OFF 0
#define IPMI_LED_COLOR_AMBER 1
#define IPMI_LED_COLOR_RED 2

static unchar cur_status = 0x0;
static ipmi_user_t t5520ur_led_user = NULL;


static void t5520ur_led_msg_handler(struct ipmi_recv_msg *msg,void* handler_data)
{
	ipmi_free_recv_msg(msg);
}
static struct ipmi_user_hndl t5520ur_led_ipmi_hndlrs = {   .ipmi_recv_hndl = t5520ur_led_msg_handler,};



static int8_t nar5520_led_open(void)
{
   unchar val = 0x00;

   switch(get_platform_info()) {
      case NAR5520_HW: {

         W627_OPEN_PORT();


         W627_READ_PORT(I2C_PIN_SEL, val);
         W627_WRITE_PORT(I2C_PIN_SEL, (val&0xfd));


         W627_READ_PORT(MULTI_FUNCTION_SEL, val);
         W627_WRITE_PORT(MULTI_FUNCTION_SEL, (val&0x1f));


         outb(CR07, W627_CONFIG);
         outb(LED_DEVICE, W627_DATA);

         W627_READ_PORT(CR30, val);
         W627_WRITE_PORT(CR30, (val|0x02));


         W627_WRITE_PORT(GPIO3_INVERSION_REG, 0x00);


         W627_READ_PORT(GPIO3_IO_REG, val);
         W627_WRITE_PORT(GPIO3_IO_REG, (val&0xfc));
         return 0; break;
      }
      case T5520UR_HW: {


         return 0; break;
      }
      default:
         break;
   }

   return -1;
}

static int8_t nar5520_led_close(void)
{
   switch(get_platform_info()) {
      case NAR5520_HW: {
         W627_CLOSE_PORT();
         return 0;
      }
      case T5520UR_HW: {


         return 0;
      }
      default:
         break;
   }

   return -1;
}

static void cob7402_led_red(void)
{
    unchar val = 0x00;
    W627_OPEN_PORT();

    W627_READ_PORT(CR24, val);
    W627_WRITE_PORT(CR24, (val & ENROM_MASK));

    W627_READ_PORT(CR2D, val);
    W627_WRITE_PORT(CR2D, (val | SUSLED_MASK));

    outb(CR07, W627_CONFIG);
    outb(LED_DEVICE, W627_DATA);

    W627_READ_PORT(CR30, val);
    W627_WRITE_PORT(CR30, (val|0x09));

    W627_READ_PORT(GPIO2_INVERSION_REG, val);
    W627_WRITE_PORT(GPIO2_INVERSION_REG, val & 0xf7);

    W627_READ_PORT(GPIO2_IO_REG, val);
    W627_WRITE_PORT(GPIO2_IO_REG, (val & 0xf7));

    W627_READ_PORT(GPIO2_DATA_REG, val);
    W627_WRITE_PORT(GPIO2_DATA_REG, (val & GP23_MASK) | 0x00);

    W627_READ_PORT(GPIO5_INVERSION_REG, val);
    W627_WRITE_PORT(GPIO5_INVERSION_REG, val & 0xdf);

    W627_READ_PORT(GPIO5_IO_REG, val);
    W627_WRITE_PORT(GPIO5_IO_REG, (val & 0xdf));

    W627_READ_PORT(GPIO5_DATA_REG, val);
    W627_WRITE_PORT(GPIO5_DATA_REG, (val & GP55_MASK) | 0x20);

    W627_CLOSE_PORT();
}
static void cob7402_led_red_flash(void)
{
    unchar val = 0x00;
    W627_OPEN_PORT();

    W627_READ_PORT(CR24, val);
    W627_WRITE_PORT(CR24, (val & ENROM_MASK));

    W627_READ_PORT(CR2D, val);
    W627_WRITE_PORT(CR2D, (val | SUSLED_MASK));

    outb(CR07, W627_CONFIG);
    outb(LED_DEVICE, W627_DATA);

    W627_READ_PORT(CR30, val);
    W627_WRITE_PORT(CR30, (val|0x09));

    W627_READ_PORT(GPIO2_INVERSION_REG, val);
    W627_WRITE_PORT(GPIO2_INVERSION_REG, val & 0xf7);

    W627_READ_PORT(GPIO2_IO_REG, val);
    W627_WRITE_PORT(GPIO2_IO_REG, (val & 0xf7));

    W627_READ_PORT(GPIO2_DATA_REG, val);
    W627_WRITE_PORT(GPIO2_DATA_REG, (val & GP23_MASK) | 0x00);

    W627_READ_PORT(GPIO5_INVERSION_REG, val);
    W627_WRITE_PORT(GPIO5_INVERSION_REG, val & 0xdf);

    W627_READ_PORT(GPIO5_IO_REG, val);
    W627_WRITE_PORT(GPIO5_IO_REG, (val & 0xdf));

    W627_READ_PORT(GPIO5_DATA_REG, val);
    W627_WRITE_PORT(GPIO5_DATA_REG, (val & GP55_MASK) | 0x20);

    W627_READ_PORT(CR2D, val);
    W627_WRITE_PORT(CR2D, val & (~SUSLED_MASK));

    W627_CLOSE_PORT();
}
static void cob7402_led_green(void)
{
    unchar val = 0x00;
    W627_OPEN_PORT();

    W627_READ_PORT(CR24, val);
    W627_WRITE_PORT(CR24, (val & ENROM_MASK));

    W627_READ_PORT(CR2D, val);
    W627_WRITE_PORT(CR2D, (val | SUSLED_MASK));

    outb(CR07, W627_CONFIG);
    outb(LED_DEVICE, W627_DATA);

    W627_READ_PORT(CR30, val);
    W627_WRITE_PORT(CR30, (val|0x09));

    W627_READ_PORT(GPIO2_INVERSION_REG, val);
    W627_WRITE_PORT(GPIO2_INVERSION_REG, val & 0xf7);

    W627_READ_PORT(GPIO2_IO_REG, val);
    W627_WRITE_PORT(GPIO2_IO_REG, (val & 0xf7));

    W627_READ_PORT(GPIO2_DATA_REG, val);
    W627_WRITE_PORT(GPIO2_DATA_REG, (val & GP23_MASK) | 0x08);

    W627_READ_PORT(GPIO5_INVERSION_REG, val);
    W627_WRITE_PORT(GPIO5_INVERSION_REG, val & 0xdf);

    W627_READ_PORT(GPIO5_IO_REG, val);
    W627_WRITE_PORT(GPIO5_IO_REG, (val & 0xdf));

    W627_READ_PORT(GPIO5_DATA_REG, val);
    W627_WRITE_PORT(GPIO5_DATA_REG, (val & GP55_MASK) | 0x00);

    W627_CLOSE_PORT();
}
static void cob7402_led_green_flash(void)
{
    unchar val = 0x00;
    W627_OPEN_PORT();

    W627_READ_PORT(CR24, val);
    W627_WRITE_PORT(CR24, (val & ENROM_MASK));

    W627_READ_PORT(CR2D, val);
    W627_WRITE_PORT(CR2D, (val | SUSLED_MASK));

    outb(CR07, W627_CONFIG);
    outb(LED_DEVICE, W627_DATA);

    W627_READ_PORT(CR30, val);
    W627_WRITE_PORT(CR30, (val|0x09));

    W627_READ_PORT(GPIO2_INVERSION_REG, val);
    W627_WRITE_PORT(GPIO2_INVERSION_REG, val & 0xf7);

    W627_READ_PORT(GPIO2_IO_REG, val);
    W627_WRITE_PORT(GPIO2_IO_REG, (val & 0xf7));

    W627_READ_PORT(GPIO2_DATA_REG, val);
    W627_WRITE_PORT(GPIO2_DATA_REG, (val & GP23_MASK) | 0x08);

    W627_READ_PORT(GPIO5_INVERSION_REG, val);
    W627_WRITE_PORT(GPIO5_INVERSION_REG, val & 0xdf);

    W627_READ_PORT(GPIO5_IO_REG, val);
    W627_WRITE_PORT(GPIO5_IO_REG, (val & 0xdf));

    W627_READ_PORT(GPIO5_DATA_REG, val);
    W627_WRITE_PORT(GPIO5_DATA_REG, (val & GP55_MASK) | 0x00);

    W627_READ_PORT(CR2D, val);
    W627_WRITE_PORT(CR2D, val & (~SUSLED_MASK));

    W627_CLOSE_PORT();
}

static int8_t T5520UR_led_control(int led_num, int state)
{
    int i, rv = -1;

    struct ipmi_system_interface_addr addr;
    uint8_t data[4] = {0};
    struct kernel_ipmi_msg msg;

    if (cur_status == IPMI_ALARMS_INIT_VALUE) {
        cur_status = IPMI_ALARMS_RESET_VALUE;
    }
    switch (led_num) {
        case IPMI_ALARMS_MNR:
            if (state == IPMI_LED_COLOR_OFF || state == IPMI_LED_COLOR_AMBER) {
                (state ? (cur_status = (cur_status & MNR_MASK)) : (cur_status = (cur_status | ~MNR_MASK)));
            } else {
                V54_PRINTF("sysLed1: Value %d invalid. Only 0/1 supported.\n", state);
                return -1;
            }
            break;
        case IPMI_ALARMS_MJR:
            if (state == IPMI_LED_COLOR_OFF || state == IPMI_LED_COLOR_AMBER || state == IPMI_LED_COLOR_RED) {
                if (state > 0) {

                    cur_status = (cur_status & MJR_MASK);
                    if (state == IPMI_LED_COLOR_RED) {

                        cur_status = (cur_status & COLOR_MASK);
                    } else {

                        cur_status = (cur_status | ~COLOR_MASK);
                    }
                } else {

                    cur_status = (cur_status | ~MJR_MASK);
                }
            } else {
                V54_PRINTF("sysLed3: Value %d invalid. Only 0/1/2 supported.\n", state);
                return -1;
            }
            break;
        case IPMI_ALARMS_CRT:
            if (state == IPMI_LED_COLOR_OFF || state == IPMI_LED_COLOR_AMBER || state == IPMI_LED_COLOR_RED) {
                if (state > 0) {

                    cur_status = (cur_status & CRT_MASK);
                    if (state == IPMI_LED_COLOR_RED) {

                        cur_status = (cur_status & COLOR_MASK);
                    } else {

                        cur_status = (cur_status | ~COLOR_MASK);
                    }
                } else {

                    cur_status = (cur_status | ~CRT_MASK);
                }
            } else {
                V54_PRINTF("sysLed4: Value %d invalid. Only 0/1/2 supported.\n", state);
                return -1;
            }
            break;
        case IPMI_ALARMS_PWR:
            if (state == IPMI_LED_COLOR_OFF || state == IPMI_LED_COLOR_AMBER) {
                (state ? (cur_status = (cur_status & PWR_MASK)) : (cur_status = (cur_status | ~PWR_MASK)));
            } else {
                V54_PRINTF("sysLed5: Value %d invalid. Only 0/1 supported.\n", state);
                return -1;
            }
            break;
        default:
            V54_PRINTF("Incorrect LED number\n");
            return -1;
    }

    for (i=0; i<IPMI_MAX_INTF && rv; i++) {
        rv = ipmi_create_user(i, &t5520ur_led_ipmi_hndlrs, NULL, &t5520ur_led_user);
    }

    if (rv < 0) {
        V54_PRINTF("warning: create user fail, watchdog broken\n");
    }

    addr.addr_type = IPMI_SYSTEM_INTERFACE_ADDR_TYPE;
    addr.channel = IPMI_BMC_CHANNEL;
    addr.lun = 0;

    msg.netfn = IPMI_NETFUNC_APP;
    msg.cmd = IPMI_ALARMS_CMD;
    data[0] = IPMI_ALARMS_DATA1;
    data[1] = IPMI_ALARMS_DATA2;
    data[2] = IPMI_ALARMS_DATA3;
    data[3] = cur_status;
    msg.data_len = sizeof(data);
    msg.data = data;
    rv = ipmi_request_settime(t5520ur_led_user, (struct ipmi_addr*)&addr, 0, &msg,NULL, 1, -1, 0);

    if (rv) {
        printk("warning: send request fail, led broken\n");
    }
    ipmi_destroy_user(t5520ur_led_user);

    return 0;
}

static int8_t nar5520_led_color(unchar pin, int8_t state)
{
   unchar val;
   switch(get_platform_info()) {
      case NAR5520_HW: {
         nar5520_led_open();

         W627_READ_PORT(GPIO3_DATA_REG, val);
         W627_WRITE_PORT(GPIO3_DATA_REG, ( state?(val|pin):(val&~pin) ));

         nar5520_led_close();

         return 0;
      }
      case T5520UR_HW: {


         return 0;
      }
      case COB7402_HW: {

         switch(state)
         {
           case 0:
             cob7402_led_red();
             break;
           case 1:
             cob7402_led_red_flash();
             break;
           case 2:
             cob7402_led_green();
             break;
           case 3:
             cob7402_led_green_flash();
             break;
           default:
             break;
         }
         return 0;
      }
      default:
         break;
   }

   return -1;
}




int8_t nar5520_led_init(void)
{
   LED_PRINTF("NAR5520 LED init");

 return 0;
}

void nar5520_led_exit(void)
{
   LED_PRINTF("NAR5520 LED exit");

   return;
}





struct v54_led {
   int state;
   unsigned long gpio;
};

static struct v54_led led_info[] = {
   { 0, V54_GPIO_LED0 },
   { 0, V54_GPIO_LED1 },
   { 0, V54_GPIO_LED2 },
   { 0, V54_GPIO_LED3 },
   { 0, 0 },
   { 0, 0 },
   };
#define MAX_NUM_LED (sizeof(led_info)/sizeof(struct v54_led))

void sysLed(int led_num, int state)
{
    if ( led_num >= MAX_NUM_LED ) {
      printk(KERN_ERR "%s: led_num(%d) >= MAX_NUM_LED(%d) !", __FUNCTION__, led_num, MAX_NUM_LED);
      return;
    }
    LED_PRINTF("sysLed(led_num:%d,state:%d)", led_num, state);

    switch (get_platform_info()) {
        case NAR5520_HW: {
            nar5520_led_color(led_info[led_num].gpio, !state);
            break;
        }
        case T5520UR_HW: {
            T5520UR_led_control(led_num, state);
            break;
        }
        case COB7402_HW: {
            nar5520_led_color(led_info[led_num].gpio, state);
            break;
        }
        default:
            break;
    }
    led_info[led_num].state = state;
}


static inline int
_getSysLed(int led_num)
{
   if ( led_num >= MAX_NUM_LED ) {
      return 0;
   }

   return (led_info[led_num].state);
}

int get_sysLed(int led_num)
{
   LED_PRINTF("get_sysLed(led_num:%d)", led_num);
   return _getSysLed(led_num);
}


void _toggle_led(int led_num)
{
   LED_PRINTF("_toggle_led(led_num:%d)", led_num);
   sysLed(led_num, (_getSysLed(led_num) ? 0 : 1));
}
