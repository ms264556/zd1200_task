#!/bin/sh

#G_UPG_DIR=/tmp/upg
RESTORE_CMD=/etc/init.d/rescuetool/rsto_sys.sh

. /etc/init.d/rescuetool/traphandler
trap _restart 0, 2
trap _restart 0, 20

. /etc/init.d/usbtool/subfunction/DefConfig
. /etc/init.d/usbtool/subfunction/Message
. /etc/init.d/usbtool/subfunction/GetUser
. /etc/init.d/usbtool/subfunction/LedCtrl
. /etc/init.d/usbtool/ImageUpdate
. /etc/init.d/usbtool/Utility

TOCONSOLE=$1
if [ "$TOCONSOLE" != "" ]; then
  clear
  echo "Starting system restore..."

  echo -n "Extracting files..."
  /etc/init.d/rescuetool/printdot 1 &
fi

rm -rf $G_UPG_DIR 1>/dev/null 2>&1
mkdir -p $G_UPG_DIR 1>/dev/null 2>&1
cd $G_UPG_DIR
tar zxvf $G_TEMP_FILE.decrypted 1>/dev/null 2>&1
#result=$?

if [ "$TOCONSOLE" != "" ]; then
  killall printdot
  #if [ $result -ne 0 ]; then
  #  echo ""
  #  echo "Error: Cannot extracted files, please try again..."
  #  sleep 5
  #  _restart
  #fi
  echo "...Files are extracted"
  echo ""
  echo -n "Restoring system..."
  /etc/init.d/rescuetool/printdot 2 &
fi

if [ -f $G_UPG_DIR/restore_system.sh ]; then
  RESTORE_CMD=$G_UPG_DIR/restore_system.sh
fi
  
$RESTORE_CMD 1>/dev/null 2>&1
result=$?

if [ "$TOCONSOLE" = "" ]; then
  if [ $result -eq 0 ]; then
    echo "OK"
  else
    echo "FAIL"
  fi
  return $result
fi

killall printdot
if [ $result -ne 0 ]; then
  echo ""
  echo "Error: System restore failed (code:$result), please try again..."
  sleep 4
  _restart
fi

echo "...System restored successfully"
sleep 4
reboot
