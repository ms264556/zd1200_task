#!/bin/sh

NODE=/mnt
DEV_LIST="sr0 hda hdb hdc hdd"
SRC_DEV=

detect_cdrom () {
    echo "Detecting source for installation..."
    for d in $1; do
        cat /proc/diskstats | grep $d 2>/dev/null
        if [ $? -eq 0 ]; then
            mount /dev/$d $NODE 2>/dev/null
            if [ $? -eq 0 ]; then
                SRC_DEV=$d
                return 0;
            fi
        fi
    done

    return 1;
}

trap 0, 2

detect_cdrom "${DEV_LIST}"
if [ $? -eq 0 ]; then
    echo "Found source on ${SRC_DEV}, preparing for installation"
    /etc/init.d/usbtool/USBtool.sh
else
    echo "Cannot find source for installation"
    echo "CD-ROM supports either IDE or SCSI only"
    echo "Going to reboot the system..."

    sleep 3
fi

umount $NODE
reboot
#/bin/busybox sh
