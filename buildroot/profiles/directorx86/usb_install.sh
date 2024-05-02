#!/bin/sh

TMP_DIR=/tmp/usbworking
clear
echo "******************************************"
echo "*        == USB tool generator ==        *"
echo "* Which device are you going to install? *"
echo "* Example: /dev/sdb                      *"
read TARGET_USB

echo "===Checking==========================="
fdisk -l|grep ^$TARGET_USB|grep Linux
if [ $? -ne 0 ]; then
  echo "No such device or no root privilege"
  echo "Please double check and try again:" 
  echo "  1. create a linux partition."
  echo "  2. login as root"
  exit 2
fi

echo "===Umount partition==================="
for d in `mount|grep $TARGET_USB|cut -d' ' -f3`
do
  echo "umount $d"
  umount $d
done

echo "===Format partition==================="
mkfs.ext2 -I 128 ${TARGET_USB}1
if [ $? -ne 0 ]; then
  echo "Failed"
  exit 1;
fi

#prepare -- mount point
echo "===Prepare mount point================"
mkdir -p $TMP_DIR
mount -v ${TARGET_USB}1 $TMP_DIR
if [ $? -ne 0 ]; then
  echo "Failed"
  exit 1;
fi

#prepare -- grub
echo "===Copy images========================"
cp -av * $TMP_DIR

mv -v $TMP_DIR/grub/lib/grub/i386-pc/* $TMP_DIR/grub
mv -v $TMP_DIR/grub/grub/grub.conf $TMP_DIR/grub
ln -sf grub.conf $TMP_DIR/grub/menu.lst
rm -rf $TMP_DIR/grub/lib
rm -rf $TMP_DIR/grub/grub

if [ $? -ne 0 ]; then
  sudo umount $TMP_DIR
  echo "Failed"
  exit 1;
fi
umount $TMP_DIR
#install -- grub
echo "===Install bootloader================="

grub --batch <<EOT
  device (hd1) ${TARGET_USB}
  root (hd1,0)
  setup (hd1)
  quit
EOT

if [ $? -ne 0 ]; then
  echo "=== USB tool Failed================="
  exit 1;
fi

echo "===USB tool generated=================" 
exit 0;
