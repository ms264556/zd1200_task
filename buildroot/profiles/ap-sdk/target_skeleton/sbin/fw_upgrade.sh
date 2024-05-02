#!/bin/sh

usage()
{
	echo "$0 <protocol>://<server ip>/<path/image>"
}

new_pat=99
new_idx=99
_find_new()
{
	root_fs=`cat /proc/mtd|grep 'rootfs'|sed 's/mtd\([0-9]\).*/\1/'`
	case $root_fs in
		3)
			new_pat=7
			new_idx=2
			;;
		7)
			new_pat=2
			new_idx=1
			;;
		*)
			;;
	esac
}

upgrade()
{
	echo "**************************"
	echo "* starting image upgrade *"
	echo "**************************"
	_find_new;
	set `echo "$*"|sed 's/\(.*\):\/\/\([0-9a-zA-Z\.]*\)\/\(.*\)/\1 \2 \3/'`
	case $1 in
		"tftp")
			echo "--->downloading [$3]..."
			tftp -g -r $3 -l /tmp/dl.img $2
			if [ $? -eq 0 ]; then
				echo "--->upgrading [/dev/mtd$new_pat]..."
                flashcp -v /tmp/dl.img /dev/mtd$new_pat 2>/dev/null
                rbd boot $new_idx
                reboot
			fi
			;;
		*)
			echo "---> $1 not support"
			;;
	esac
}

if [ $# -eq 0 ]; then
	usage;
	exit 1;
else
	upgrade "$*";
	exit 0;
fi


