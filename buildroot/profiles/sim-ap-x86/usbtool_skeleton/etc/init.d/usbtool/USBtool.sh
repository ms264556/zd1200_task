#!/bin/sh

#Include sub-functions
. /etc/init.d/usbtool/subfunction/DefConfig
. /etc/init.d/usbtool/subfunction/Message
. /etc/init.d/usbtool/subfunction/GenCommand
. /etc/init.d/usbtool/subfunction/GetUser
. /etc/init.d/usbtool/subfunction/LedCtrl
. /etc/init.d/usbtool/ImageUpdate
. /etc/init.d/usbtool/Utility

trap 0, 2
echo "0" > /proc/sys/kernel/printk
echo "D" > /dev/watchdog
setLed "red" "blink"

#TODO: Should I move this one to Message??
ShowMenu () {
  echo "************************************************"
  echo "**       Welcome to King Solomon's Ring       **"
  echo "************************************************"
  echo "=< Auto Update >--------------------------------"
  echo "   s. Update All (With Dependency Checking)"
  echo "   a. Update All (Without Dependency Checking)"
  echo "=< Manual Update >------------------------------"
  echo "   0, Update partition table"
  echo "   1. Update bootloader"
  echo "   2. Update rootfs"
  echo "   3. Update kernel"
  echo "   4. Update datafs"
  echo "=< Network Boot >-------------------------------"
  echo "   n. Boot-up from NFS server (NFS Boot)"
  echo "   t. Boot-up from TFTP server (TFTP Boot)"
  echo "   d. Download file from TFTP server"
  echo "=< System Tools >-------------------------------"
  echo "   l. Show logs"
  echo "   i. Setup network"
  echo "   k. Clean CF card sectors"
  echo "   b. Update board data"
  echo "   x. Shell"
  echo "   r. Reboot"
}

#Handler, link to sub script file
HandleOperation () {
  ret=0
  case $1 in
    0)
      dbg "selected partition table update"
      UpdatePartitionTable
      if [ $? -eq 0 ]; then
        FormatPartitions
      fi

      ShowError $CAT_UPD_PARTITION $?
    ;;
    1)
      dbg "selected bootloader update"
      UpdateBootfs
      ShowError $CAT_UPD_BOOTFS $?
    ;;
    2)
      dbg "selected rootfs update"
      UpdateImages_rootfs
      ShowError $CAT_UPD_ROOTFS $?
    ;;
    3)
      dbg "selected kernel update"
      UpdateKernel
      ShowError $CAT_UPD_KERNEL $?
    ;;
    4)
      dbg "selected datafs update"
      UpdateImages_datafs
      ShowError $CAT_UPD_DATAFS $?
    ;;
    b)
      dbg "selected write board data"
      UpdateBoardData
    ;;
    a)
      dbg "selected do all"
      UpdatePartitionTable
      ShowError $CAT_UPD_PARTITION $?
      FormatPartitions
      ShowError $CAT_UPD_PARTITION $?
      UpdateBootfs
      ShowError $CAT_UPD_BOOTFS $?
      UpdateImages_rootfs
      ShowError $CAT_UPD_ROOTFS $?
      UpdateKernel
      ShowError $CAT_UPD_KERNEL $?
      UpdateImages_datafs
      ShowError $CAT_UPD_DATAFS $?
    ;;
    s)
      dbg "selected do all (save mode)"
      GenAutoCommand
      for cmd in $AUTOCMD
      do
        HandleOperation $cmd
      done
    ;;
    #=================================
    n)
      dbg "select nfs boot"
      CheckNetworkSet
      if [ $? -eq 0 ];then
        DoNFSBoot
      fi
    ;;
    t)
      dbg "selected tftp boot"
      CheckNetworkSet
      if [ $? -eq 0 ];then
        DoTFTPBoot
      fi
    ;;
    d)
      dbg "selected download"
      CheckNetworkSet
      DoDownload
    ;;
    i)
      dbg "selected network"
      AskUser "$MSG_PLUG_NIC $MSG_ANYKEY"
      DoNetworkSet
    ;;
    x)
      dbg "selected shell"
      echo $MSG_GO_SHELL
      /bin/busybox sh
    ;;
    l)
      dbg "selected log"
      ShowLogs
    ;;
    r)
      AskUser "$MSG_CONFIRM"
      if [ "$INPUTSTR" = "y" -o "$INPUTSTR" = "" ]; then
        DoReboot
      fi
    ;;
    k)
      dbg "selected clean all"
      AskUser "$MSG_CONFIRM"
      if [ "$INPUTSTR" = "y" -o "$INPUTSTR" = "" ]; then
        DoClean
      fi
    ;;
    *)
      dbg "unknown option"
      ret=1
    ;;
  esac
  return $ret
}

#Main menu sub-function================================
AutoLoop () {
  while [ 0 -ne $# ]
  do
  trap 0, 2
  echo "0" > /proc/sys/kernel/printk
  setLed "red" "blink"
  AUTOCONFIRM="yes"
  option=$1
  clear
  echo "$MSG_AUTO_START"

  HandleOperation $option
  shift

  done
  echo $MSG_DONE_AUTO
}

ManualLoop () {
  setLed "green"
  while [ 1 ]
  do
    trap 0, 2
    echo "0" > /proc/sys/kernel/printk
    AUTOCONFIRM="no"
    clear
    ShowMenu
    read -p ">" option
    clear
    setLed "red" "blink"
    HandleOperation $option
    setLed "green"
    if [ 0 -eq $? ]; then
      #echo $MSG_ANYKEY
      #read anykey
      AskUser "$MSG_ANYKEY"
    fi

  done;
}

#Main menu counter=====================================
AbortAutoCmd () {
  trap 0, 2
  AUTOCONFIRM="no"
}

DoCountDown () {
  trap AbortAutoCmd 2
  counter=$1
  echo "$2"
  while [ $counter -ne 0 ]
  do
    echo "$counter"
    counter=`expr $counter - 1`
    sleep 1
  done
}

#Main==================================================
if [ "$_DEF_ACTION" != "" ]; then
  INPUTSTR=""
  AskUser "$MSG_ABORT_AUTO" "y" "force"
  if [ "$INPUTSTR" = "n" ]; then
    AUTOCONFIRM="no"
  fi
  #DoCountDown 3 "$MSG_ABORT_AUTO"
  if [ "$AUTOCONFIRM" = "yes" ]; then
    AutoLoop $_DEF_ACTION
    DoReboot
  fi
fi

ManualLoop
