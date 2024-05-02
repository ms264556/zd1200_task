#!/bin/sh

DoMountUSB () {
  ret=1

  while [ $ret -ne 0 ]; do
    mount -o ro -t ext2 /dev/$USBDEV $TMPNODE > /dev/null 2>&1
    ret=$?
    sleep 1
  done
}

trap 0, 2
DoMountUSB
/etc/init.d/usbtool/USBtool.sh
umount $TMPNODE
