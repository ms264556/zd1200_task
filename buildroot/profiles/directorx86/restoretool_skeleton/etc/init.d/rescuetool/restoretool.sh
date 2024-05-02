#!/bin/sh

. /etc/init.d/rescuetool/traphandler
trap _restart 0, 2
trap _restart 0, 20

/etc/init.d/rescuetool/set_env.sh
/etc/init.d/rescuetool/set_net.sh
/etc/init.d/rescuetool/get_img.sh
/etc/init.d/rescuetool/ver_img.sh "console"
/etc/init.d/rescuetool/do_rsto.sh "console"

reboot
