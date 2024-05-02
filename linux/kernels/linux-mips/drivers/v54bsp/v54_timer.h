#ifndef _V54_TIMER_H_
#define _V54_TIMER_H_

#include <linux/timer.h>

#include "v54_timer.h"

typedef void (*void_ul_func)(unsigned long data);
extern void v54_init_timer(struct timer_list *my_timer,
            void_ul_func my_func, unsigned long my_data, unsigned long dsec);

extern void v54_del_timer(struct timer_list *my_timer);
extern void v54_update_timer(struct timer_list *my_timer, unsigned long dsec);


extern int v54_start_led_blink(int led_num, unsigned long dsec);
extern void v54_stop_led_blink(int led_num);
extern int v54_led_blinking(int led_num);
extern int v54_start_sensor_timer(unsigned long dsec);
extern void v54_stop_sensor_timer(void);

extern void init_cm_ctrl_timer(struct timer_list *cm_timer,
            void_ul_func cm_func, unsigned long cm_data, unsigned long cm_delay);
extern void cpld_cm_ctrl(unsigned long data);

#endif
