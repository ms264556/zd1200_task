#!/bin/sh

TMP_USB_DIR=/tmp/usb
MNT_USB_DIR=/mnt/usb

# Create directory in tmpfs if not yet
if [ ! -e $TMP_USB_DIR ] ; then
    mkdir -p $TMP_USB_DIR
fi

# Exit if already mounted
if [ -e $MNT_USB_DIR ] ; then
    exit 0
fi

mount_datafs2() {
    # Mount datafs2 file system
    # For flash boot (production), use 8.
    # For network boot (testing), use 7. 
    DEV_MTD_FLASH_BOOT=/dev/mtdblock8
    DEV_MTD_NET_BOOT=/dev/mtdblock7

    # Try mount the device with flash boot
    mount -t jffs2 $DEV_MTD_FLASH_BOOT /mnt > /dev/null 2>&1
    if [ "$?" == "0" ] ; then
        return 0
    fi 

    # Try mount the device with net boot
    mount -t jffs2 $DEV_MTD_NET_BOOT /mnt > /dev/null 2>&1
    if [ "$?" == "0" ] ; then
        return 0
    fi 
    
    return 255
}

# Models with datafs2 flash partition:
# Snoop Dogg (7982)
# Spaniel (7321)
# St Bernard (7372, 7352)
MODEL=`rbd dump | grep "Model:" | sed s/Model:[\ ]*[Zz][fF]//g`
BRDTYPE=`rbd dump | grep "V54 Board Type:" | sed s/"V54 Board Type:[\ ][Gg][Dd]"//g`
if [ "$MODEL" == "7982" -o "$MODEL" == "7321" -o "$MODEL" == "7372" -o "$MODEL" == "7352" -o "$MODEL" == "7321-u" -o "$MODEL" == "7321-U" -o "$BRDTYPE" == "42" ] ;
then
    mount_datafs2
    exit $?
fi

exit 0

