#!/bin/sh
. /etc/init.d/rescuetool/traphandler
trap _restart 0, 2
trap _restart 0, 20

. /etc/init.d/usbtool/subfunction/DefConfig
. /etc/init.d/usbtool/subfunction/Message
. /etc/init.d/usbtool/subfunction/GetUser
. /etc/init.d/usbtool/subfunction/LedCtrl
. /etc/init.d/usbtool/ImageUpdate
. /etc/init.d/usbtool/Utility

cd $G_UPG_DIR

rootfs=`ls rootfs*`
kernel=bzImage

if [ ! -f $rootfs -o ! -f $kernel ]; then
  return 1
fi

for party in hda2 hda3
do
  zcat $rootfs | dd of=/dev/$party 1>/dev/null 2>&1
  if [ $? -eq 0 ]; then
    resize2fs /dev/$party 1>/dev/null 2>&1
    if [ $? -eq 0 ]; then
      mount -o async,noatime /dev/$party /mnt
      if [ $? -eq 0 ]; then
        cp $kernel /mnt
        sync;sync;sync; sleep 5;
        umount /mnt
      else
        return 2
      fi
    else
      return 3
    fi
  else
    return 4
  fi
done

return 0
