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
#AskUser "Upload image by console or browser? (c/b)"
#if [ "$INPUTSTR" != "b" -a "$INPUTSTR" != "B" ]; then
  while [ 1 ]; do
    trap _restart 0, 2
    trap _restart 0, 20
    clear
    DisplayImageInfo "Current server and image info:"
    AskUser "Use current setting? (y/n)"
    if [ "$INPUTSTR" = "n" -o "$INPUTSTR" = "N" ]; then
      ServerSet
    else
      echo -n "Downloading image..."
      if [ "$G_PROTOCOL" != "kermit" ]; then
        /etc/init.d/rescuetool/printdot 2 &
      fi
      GetImage
      result=$?
      killall -9 printdot
      if [ $result -eq 0 ]; then
        echo "...Image downloaded OK"
        echo ""
        break
      fi

      echo ""
      echo "Error: Cannot download the image (code:$result), please try again..."
      sleep 4
    fi
  done
#else
#    AskUser "Please uploaded your image by browser, press ENTER while done..."
    #if [ -f ]; then
    #fi
#    break;
#fi

