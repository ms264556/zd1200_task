#!/bin/sh

. /etc/init.d/rescuetool/traphandler
trap _restart 0, 2
trap _restart 0, 20

#link to customization dir
mkdir -p /writable 1>/dev/null 2>&1
mount /dev/hda4 /writable 1>/dev/null 2>&1
ln -sf /writable/etc/airespider-images /etc/airespider-images
ln -sf /etc/airespider-images/support_logo.png /web/images/logo.png
ln -sf /etc/airespider-images/support_logo.png /web/images/logo.gif

#create a back door for customizations.
if [ -f /etc/airespider-images/custom_restore.sh ]; then
  /etc/airespider-images/custom_restore.sh 1>/dev/null 2>&1
fi

#start webs
mkdir -p /etc/airespider/dump
cd /bin; webs 1>/dev/null 2>&1 &

