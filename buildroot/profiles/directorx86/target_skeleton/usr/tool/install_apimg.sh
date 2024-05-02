#!/bin/sh

VER="0.0.1"
IMG_TARGET=/etc/airespider-images/firmwares
IMG_FILE=rcks_fw.bl7
IMG_MAIN=$IMG_FILE.main
IMG_BKUP=$IMG_FILE.bkup

usage ()
{
	if [ "$2" != "" ]; then
		echo "Error: $2"
	fi
	echo "Usage:"
	echo "	$1 </absolute path/image> <image version> <AP models>"
	echo "Example:"
	echo "	$1 /tmp/rcks_fw.bl7 8.6.0.0.66 zf2741 zf2942"
}

clean_image ()
{
	set $*
	until [ "$1" = "" ]
	do
		if [ -d $IMG_TARGET/$1 ]; then
			echo "  >Move $1 to $1.bak"
			rm -rf $IMG_TARGET/$1.bak 2>/dev/null
			mv $IMG_TARGET/$1 $IMG_TARGET/$1.bak
		fi
		shift;
	done
	return 0;
}

install_image ()
{
	set $*
	image=$1;
	shift;
	version=$1;
	short_ver=`echo $version|sed 's/\([0-9]*\)\.\([0-9]*\)\.\([0-9]*\)\..*/\1\2\3/'`
	img_size=`ls -la $image|sed 's/.\{30\}[^ ]* *\([^ ]*\) *.*/\1/'`
	shift;
	until [ "$1" = "" ]
	do
		echo "  >Install $image to $1"
		echo "  >Version:$version, size:$img_size"
		mkdir -p $IMG_TARGET/$1/$version
		cp $image $IMG_TARGET/$1/$version/$IMG_FILE
		cd $IMG_TARGET/$1/$version/; ln -s $IMG_FILE $IMG_MAIN; ln -s ../../$1/$version/$IMG_FILE $IMG_BKUP

		#create info files
		echo "$version" > $IMG_TARGET/$1/$version/rcks_fw.bl7.bkup.version
		if [ -f $IMG_TARGET/$1.bak/$version/rcks_fw.bl7.option ]; then
			cp $IMG_TARGET/$1.bak/$version/rcks_fw.bl7.option $IMG_TARGET/$1/$version/rcks_fw.bl7.option
		else
			echo "main first" > $IMG_TARGET/$1/$version/rcks_fw.bl7.option
			echo "bkup upgrade 8.1" >> $IMG_TARGET/$1/$version/rcks_fw.bl7.option
		fi

		cat > $IMG_TARGET/$1/$version/$1_${short_ver}_cntrl.rcks << :END
[rcks_fw.main]
0.0.0.0
$1/$version/$IMG_MAIN
$img_size

[rcks_fw.bkup]
0.0.0.0
$1/$version/$IMG_BKUP
$img_size

:END
		cd $IMG_TARGET/$1/; ln -s $version/$1_${short_ver}_cntrl.rcks control
		shift;
	done
}


echo "***************************************"
echo "* Start AP image installer, ver $VER *"
echo "***************************************"
if [ $# -lt 3 ]; then
	usage $0 ;
	exit 1;
fi

ap_image=$1
if [ ! -f $ap_image ]; then
	usage $0 "AP image not found";
	exit 2;
fi

shift;
image_version=$1
shift;

echo "---------------------------------------"
echo "-->Backup old image"
clean_image "$@"
if [ $? -ne 0 ]; then
	usage $0 "Cannot remove previous image";
	exit 3;
fi

echo "---------------------------------------"
echo "-->Install image"
install_image "$ap_image" "$image_version" "$@"
if [ $? -ne 0 ]; then
	usage $0 "Installation fails";
	exit 4;
fi

echo "---------------------------------------"
echo "-->Image installed";
echo "======================================="
exit 0;
