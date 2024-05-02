#!/bin/sh

. /etc/init.d/rescuetool/traphandler
trap _restart 0, 2
trap _restart 0, 20

#insert LAN driver
insmod /lib/modules/`uname -r`/kernel/drivers/net/e1000e/e1000e.ko

#stop watchdog
echo "0" > /proc/sys/kernel/printk
echo "D" > /dev/watchdog

#TODO: create Christma tree LED
#setLed "red" "blink"

