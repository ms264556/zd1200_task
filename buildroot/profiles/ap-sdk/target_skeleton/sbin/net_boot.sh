#!/bin/sh

usage()
{
	echo "$0 <host ip> <server ip>/<path/image>"
}

set_netboot()
{
	echo "*******************************"
	echo "* set network boot parameters *"
	echo "*******************************"
	set `echo "$*"|sed 's/\([0-9\.]*\) \([0-9\.]*\)\/\(.*\)/\1 \2 \3/'`
	echo "--->host ip:   [$1]"
	echo "--->server ip: [$2]" 
	echo "--->image:     [$3]"
	cat >/proc/v54bsp/bootline << :END
set serverip $2; setipaddr $1; bootnet $3
:END
	echo "please reboots to take effect"
	echo "-------------------------------"
}

if [ $# -eq 0 ]; then
	usage;
	exit 1;
else
	set_netboot "$*";
	exit 0;
fi


