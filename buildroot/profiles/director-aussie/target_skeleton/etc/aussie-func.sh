#!/bin/sh -x
#***********************************************************
#  Copyright (c) 2005-2006    AireSpider Networks Inc.
#  All rights reserved
#***********************************************************

IMGDIR=/etc/airespider-images
MINIBOOT_DATAFS=/tmp/miniboot

# mount datafs
data_num=99
_grep_datafs()
{
    while read pn sz esz name ; do
        case "${name}" in
        '"datafs"')  data_num=${pn#mtd}
             data_num=${data_num%:}
             break ;;
        *) ;;
        esac
    done < /proc/mtd
}

mount_miniboot_datafs()
{
    _grep_datafs
    mkdir -p ${MINIBOOT_DATAFS}
    if [ ${data_num} -lt 20 ]; then
        mount -t jffs2 /dev/mtdblock${data_num} ${MINIBOOT_DATAFS}
    else
        # It sholud never be here ...
        echo "*** Cannot find datafs partition"
    fi
}

# --- For old miniboot compatible
boot_status="/writable/boot-status"
upgraded="0"

set_boot_status()
{
    target_dev=`cat /proc/cmdline | sed 's/.*root=\(.*\)[ ]ro.*/\1/g'` 

    # If booting here, we regard it as successful boot. So, set failed_count to zero.
    if [ "${target_dev}" = "/dev/sda2" ]; then
        bkup_dev="/dev/sda3"
        # Set main_img
        echo "1" > "${MINIBOOT_DATAFS}"/main_img
    else
        bkup_dev="/dev/sda2"
        # Set main_img
        echo "2" > "${MINIBOOT_DATAFS}"/main_img
    fi
    echo "${target_dev} ${upgraded} 0" > ${boot_status}
}
# ---

decide_bkup_dev()
{
    main_img=`cat "${MINIBOOT_DATAFS}"/main_img`
    if [ "${main_img}" -eq 1 ]; then
        main_img=2
        bkup_dev="/dev/sda3"
    elif [ "${main_img}" -eq 2 ]; then
        main_img=1
        bkup_dev="/dev/sda2"
    fi
}

_do_upgrade_backup_partition()
{
    mount

    echo "FDISK ${bkup_dev}" >/dev/console
    /sbin/mkfs.ext2 ${bkup_dev}

    echo "Mount backup partition, ${bkup_dev}, on /mnt"
    mount -o rw,noatime -t ext2 ${bkup_dev} /mnt

    echo "Copy the kernel image to the backup partition"
    cp -f /kernel-vmlinux /mnt/
    sync; sleep 1

    echo "Copy the rootfs file to the backup partition"
    cp -f /rootfs.tar /mnt/
    cd /mnt/
    sync; sleep 1

    echo "Un-tar the rootfs file on /mnt"
    tar -xf ./rootfs.tar
    cd /

    echo "Sync with the USB storage"
    mount -o remount -t ext2 ${bkup_dev} /mnt
    sync; sync; sync; sync; sync; sync; sleep 8

    echo "Umount /mnt"
    umount /mnt

    echo "remove duplicate flag under ${IMGDIR}"
    rm -f ${IMGDIR}/duplicate
    echo "Done"
}

start_aussie()
{
    mount_miniboot_datafs

    # --- For old miniboot compatible
    if [ -e "${boot_status}" ]; then
        upgraded=`cut -d \  -f 2 $boot_status`
        set_boot_status
    else
        decide_bkup_dev
    fi

    if [ -e "${IMGDIR}/duplicate" ] || [ "${upgraded}" -eq 1 ]; then
        _do_upgrade_backup_partition
        if [ -e "${boot_status}" ]; then
            echo "${target_dev} 0 0" > ${boot_status}
        fi
    fi

    umount ${MINIBOOT_DATAFS}
}

upgrade_aussie()
{
    echo "Upgrade start"
    ZD_UPG_IMG=$1

    mount_miniboot_datafs
    decide_bkup_dev

    echo "Update ZD-1100 image ${bkup_dev}" >/dev/console
    mount -t ext2 -o rw,noatime ${bkup_dev} /mnt    

    echo "Delete all files" >/dev/console
    rm -rf /mnt/*    
    sync; sleep 1

    if [ -f "$ZD_UPG_IMG" ] ; then
        tar xvf $ZD_UPG_IMG kernel-vmlinux rootfs.tar -C /mnt
    else
        mv -f /etc/airespider-images/kernel-vmlinux /mnt/kernel-vmlinux 
        mv -f /etc/airespider-images/rootfs.tar /mnt/rootfs.tar
    fi
    sync; sleep 1
    ls /mnt
    
    echo "Sync..." >/dev/console
    tar -xf /mnt/rootfs.tar -C /mnt/
    sync; sync; sync; sync; sync; sync; sleep 10
    mount -o remount ${bkup_dev} /mnt

    mv /mnt/file_list.txt /tmp/
    sync ; sleep 1 
    
    # For old miniboot compatible
    if [ -e "${boot_status}" ]; then
        echo "${bkup_dev} 1 0" > ${boot_status}
    fi

    echo "Update miniboot main_img ..." >/dev/console
    echo "${main_img}" > "${MINIBOOT_DATAFS}"/main_img
    umount ${MINIBOOT_DATAFS}
    echo "Done" >/dev/console
}
