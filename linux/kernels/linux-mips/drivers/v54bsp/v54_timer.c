/*
 *
 * Copyright (c) 2006 Ruckus Wireless, Inc.
 * Copyright (c) 2004,2005 Video54 Technologies, Inc.
 * All rights reserved.
 *
 */

#include <linux/version.h>

#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>

#include "v54_timer.h"
#include "ar531x_bsp.h"
extern struct rks_boarddata *rbd;

#define DSEC_TO_JIFFIES(dsec) ( (dsec * HZ)/10 )
void
v54_init_timer(struct timer_list *my_timer,
            void_ul_func my_func, unsigned long my_data, unsigned long dsec)
{
    if ( dsec == 0 )  {
        printk("%s: sleeptime=0, ignored ...\n", __FUNCTION__);
        return;
    }
    init_timer(my_timer);
    my_timer->function = (void (*)(unsigned long)) my_func ;
    my_timer->data = my_data;
    my_timer->expires = jiffies + DSEC_TO_JIFFIES(dsec);


    add_timer(my_timer);
    return;
}

void
v54_del_timer(struct timer_list *my_timer)
{
    del_timer_sync(my_timer);
    return;
}

void
v54_update_timer(struct timer_list *my_timer, unsigned long dsec)
{
    mod_timer(my_timer, jiffies + DSEC_TO_JIFFIES(dsec));
    return;
}

#include <linux/workqueue.h>
struct v54_led_work
{
    struct work_struct work;
    int id;
};

struct v54_timer_info
{
    int  id;
    struct timer_list * timer;
    unsigned long state;
    int d_sec;
    struct v54_led_work *led_work;
};

#include "v54_linux_gpio.h"
static void
blink_led(unsigned long data)
{
    struct v54_timer_info * my_info;

    my_info = (struct v54_timer_info *) data;
    if (!rks_is_gd35_board())
        _toggle_led(my_info->id);

    v54_update_timer(my_info->timer, my_info->d_sec);
    return;
}

static void
blink_led_task(struct work_struct *work)
{
    _toggle_led(((struct v54_led_work *)work)->id);
    return;
}

#define MAX_LED_TIMER   11
static struct timer_list led_timer[MAX_LED_TIMER];
static struct v54_timer_info led_timer_info[] = {
             {0,&led_timer[0],0,0,0},
             {1,&led_timer[1],0,0,0},
             {2,&led_timer[2],0,0,0},
             {3,&led_timer[3],0,0,0},
             {4,&led_timer[4],0,0,0},
             {5,&led_timer[5],0,0,0},
             {6,&led_timer[6],0,0,0},
             {7,&led_timer[7],0,0,0},
             {8,&led_timer[8],0,0,0},
             {9,&led_timer[9],0,0,0},
             {10,&led_timer[10],0,0,0},
            };

#define LED_BLINK_MASK 0x10
#define IS_BLINKING(led_num) ((led_timer_info[led_num].state & LED_BLINK_MASK) != 0)
static void
v54_setup_led_blink(int led_num, unsigned long dsec)
{

    led_timer_info[led_num].d_sec = dsec;
    led_timer_info[led_num].state |= LED_BLINK_MASK;


    v54_init_timer(led_timer_info[led_num].timer, (void_ul_func) blink_led,
            (unsigned long)(&led_timer_info[led_num]), dsec);
    return;
}

int
v54_start_led_blink(int led_num, unsigned long dsec)
{
    if ( led_num >= MAX_LED_TIMER ) {
        return -1;
    }

    if ( IS_BLINKING(led_num) ) {


        led_timer_info[led_num].d_sec = dsec;
        v54_update_timer(led_timer_info[led_num].timer, dsec);
    } else {

        v54_setup_led_blink(led_num, dsec);
    }
    return 0;
}

void
v54_stop_led_blink(int led_num)
{

    if ( IS_BLINKING(led_num) ) {
        led_timer_info[led_num].state &= ~LED_BLINK_MASK;

        v54_del_timer(led_timer_info[led_num].timer);
    }
}

int
v54_led_blinking(int led_num)
{
    return ((led_timer_info[led_num].state & LED_BLINK_MASK) ?
                led_timer_info[led_num].d_sec : 0);
}
