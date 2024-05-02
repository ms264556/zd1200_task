#!/bin/sh

NODE=/mnt
USBDEV=sda1

DoMountUSB () {
  ret=1

  while [ $ret -ne 0 ]; do
    mount -o ro -t ext2 /dev/$USBDEV $NODE > /dev/null 2>&1
    ret=$?
    sleep 1
  done
}

trap 0, 2
DoMountUSB
/etc/init.d/usbtool/USBtool.sh
umount $NODE
reboot
#/bin/busybox sh
