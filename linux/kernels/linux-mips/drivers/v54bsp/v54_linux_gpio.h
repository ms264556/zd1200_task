#ifndef V54_LINUX_GPIO_H
#define V54_LINUX_GPIO_H
int get_sysLed(int led_num);
void     _toggle_led(int led_num);
void     sysLed(int led_num, int state);
void     sysLed0(int state);
void     sysLed1(int state);
void     sysLed2(int state);
void     sysLed3(int state);
void     sysReset(void);

#endif	/* V54_GPIO_H */