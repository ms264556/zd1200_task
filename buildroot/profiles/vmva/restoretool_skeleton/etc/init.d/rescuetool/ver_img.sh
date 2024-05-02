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

try_decrypt_and_check_purpose() {
  /bin/tac_encrypt -d -i $1 -o $1 1>/dev/null 2>&1
  if [ $? -ne 0 ] ; then
    return 1
  fi
  mv $1 $1.decrypted
  cd /tmp
  cat $1.decrypted | gunzip | tar x metadata 1>/dev/null 2>&1
  # check metadata
  if [ ! -f "metadata" ] ; then
    return 2
  fi
  PURPOSE=`grep PURPOSE metadata | cut -d = -f 2` 1>/dev/null 2>&1
  if [ "$PURPOSE" != "$2" ]; then
    return 3
  fi

  reqplatform=`grep "^REQUIRE_PLATFORM=" metadata | cut -d = -f 2`
  if [ "$reqplatform" != "$G_PLATFORM" ] ; then
    return 4
  fi

  G_VERSION=`grep "^VERSION=" metadata | cut -d = -f 2`
  if [ $G_VERSION = "" ]; then
    return 5
  fi

  return 0
}

AUTOCONFIRM="no"
TOCONSOLE=$1
if [ "$TOCONSOLE" != "" ]; then
  echo -n "Verifying image..."
  /etc/init.d/rescuetool/printdot 1 &
fi

try_decrypt_and_check_purpose $G_TEMP_FILE "upgrade"
code=$?
if [ "$TOCONSOLE" = "" ]; then
  if [ $code -ne 0 ]; then
    echo "FAIL"
  else
    echo "OK"

  fi
  return $code
fi

killall printdot
if [ $code -ne 0 ] ; then
  rm -f $G_TEMP_FILE $G_TEMP_FILE.decrypted metadata
  echo ""
  echo "Error: Invalid image format (code:$code), please try again..."
  sleep 4
  _restart
fi

echo "...Image verified OK"
echo ""
AskUser "Restore system to this version $G_VERSION? (y/n)"
if [ "$INPUTSTR" = "n" -o "$INPUTSTR" = "N" ]; then
  _restart
fi

