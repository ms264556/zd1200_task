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


AUTOCONFIRM="no"
ifconfig $G_PORT $G_HOST_IP netmask $G_NET_MASK
ifconfig lo up
/etc/init.d/rescuetool/run_web.sh
while [ 1 ]; do
  trap _restart 0, 2
  trap _restart 0, 20
  clear
  DisplayNetowrkSet "$MSG_CURRENT_NIC_SETTING"
  AskUser "Use current setting? (y/n)"
  if [ "$INPUTSTR" = "n" -o "$INPUTSTR" = "N" ]; then
  #  NetworkSet
     NetworkSetStatic
  else
    ifconfig|grep eth > /dev/null 2>&1
    if [ $? -eq 0 ]; then
      break
    fi
    echo "Error: Please setup your network environment first..."
    sleep 3
  fi
done

