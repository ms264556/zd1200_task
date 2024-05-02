#!/bin/sh

_CMD_TYPE=$1
_TARGET_PAT=$2
_TGPATH_PREFIX=/
_FILE_LIST=file_list.txt
_DIR2_AP_IMGS=/etc/airespider-images/firmwares
_TMP_POINT=/mnt
_START_POINT=/
_UNMOUNT=0
_REMOUNT_PATH=""
_REMOUNT_OPT=""
_ERRORS=0 #started from 10. real # of errors=return code - 10

show_usage()
{
    echo "Usage: $0 <clone|check|check-rootfs|check-ap-image> <target partition>"
    echo "       $0 Example 1: clone /dev/hda2 (clone / to /dev/hda2 on ZD3k)"
    echo "       $0 Example 2: clone /dev/sda2 (clone / to /dev/sda2 on ZD5k)"
    echo "       $0 Example 3: check /dev/hda3 (integrity check for /dev/hda3 on ZD3k)"
    echo "       $0 Example 3: check /dev/sda3 (integrity check for /dev/sda3 on ZD5k)"
}

do_exit()
{
    if [ $_UNMOUNT -ne 0 ]; then
        umount $_TMP_POINT;
    fi

    if [ "$_REMOUNT_PATH" != "" -a "$_CMD_TYPE" = "clone" ]; then
        mount -o `echo "$_REMOUNT_OPT"|sed 's/(\(.*\))/\1/g'` $_TARGET_PAT $_REMOUNT_PATH;
    fi
}

# check_md5sum <starts from> <checksum file> <command> [dest dir]
#                   $1             $2           $3         $4
check_md5sum()
{
    ret=0;
    if [ ! -f $2 ]; then
        echo "No file list founded ...";
        return 1;
    fi

    popd=`pwd`
    cd $1;

    while read line; do
        #TODO: I/O loading detections
        #await=(ruse+wuse)/(rio+wio)

        type=`echo $line|cut -d: -f1`;
        data=`echo $line|cut -d: -f2`;
        case "$type" in
        FILE)
            md5=`echo $data|cut -d' ' -f1`
            file=`echo $data|cut -d' ' -f2`

            #If it is a char or blk dev, skip...
            if [ ! -b $file -a ! -c $file ]; then
                echo "$md5  $file"|/usr/bin/md5sum -c >/dev/null 2>&1
            fi

            if [ $? -eq 0 ]; then
                if [ "$3" = "clone" ]; then
                    cp -a /$file $4/$file;
                fi
            else
                echo "file:[$file] corrupted"
                ret=`expr $ret + 1`
            fi
        ;;
        DIR)
            if [ -d $data ]; then
                if [ "$3" = "clone" ]; then
                    mkdir -p $4/$data;
                fi
            else
                echo "dir:[$data] corrupted"
                ret=`expr $ret + 1`
            fi
        ;;
        LINK)
            if [ -h $data ]; then
                if [ "$3" = "clone" ]; then
                    cp -a $data $4/$data;
                fi
            else
                echo "link:[$data] corrupted"
                ret=`expr $ret + 1`
            fi
        ;;
        OTHER)
            if [ "$3" = "clone" ]; then
                cp -a $data $4/$data;
            fi
        ;;
        esac
    done < $2

    cd $popd
    return $ret;
}

if [ "$_CMD_TYPE" = "" ]; then
    show_usage;
    exit 1;
elif [ "$_TARGET_PAT" = "" -a "$_CMD_TYPE" != "check-ap-image" ]; then
    show_usage;
    exit 2;
fi

if [ "$_TARGET_PAT" != "" ]; then
    _REMOUNT_PATH=`mount | grep $_TARGET_PAT | cut -d' ' -f3`
    _REMOUNT_OPT=`mount | grep $_TARGET_PAT | cut -d' ' -f6`
fi

if [ "$_CMD_TYPE" = "clone" ]; then
    if [ "$_REMOUNT_PATH" = "/" ]; then
        echo "Cannot clone current system itself ..."
        exit 2;
    elif [ "$_REMOUNT_PATH" != "" ]; then
        umount $_REMOUNT_PATH;
    fi

    /sbin/mkfs.ext2 -q $_TARGET_PAT;
    mount -o async,noexec $_TARGET_PAT $_TMP_POINT;
    _TGPATH_PREFIX=$_TMP_POINT;
    _UNMOUNT=1;

    _FILE_LIST=/$_FILE_LIST;
    _START_POINT=/;
    check_md5sum $_START_POINT $_FILE_LIST $_CMD_TYPE $_TMP_POINT;
    _ERRORS=$?
    if [ $_ERRORS -ne 0 ];  then
        do_exit;
        _ERRORS=`expr $_ERRORS + 10`
        exit $_ERRORS;
    fi

    cp $_FILE_LIST $_TGPATH_PREFIX/$_FILE_LIST;

    if [ ! -f $_TGPATH_PREFIX/bzImage ]; then
        cp /bzImage $_TGPATH_PREFIX/bzImage;
        popd=`pwd`
        cd $_TGPATH;
        echo "FILE:`md5sum ./bzImage`" >> $_TGPATH_PREFIX/$_FILE_LIST;
        cd $popd
    fi
fi

if [ "$_CMD_TYPE" = "check" -o "$_CMD_TYPE" = "check-rootfs" ]; then
    if [ "$_REMOUNT_PATH" = "" ]; then
        mount -o async,noexec $_TARGET_PAT $_TMP_POINT;
        _TGPATH_PREFIX=$_TMP_POINT;
        _UNMOUNT=1;
    else
        _TGPATH_PREFIX=$_REMOUNT_PATH;
    fi

    _FILE_LIST=$_TGPATH_PREFIX/$_FILE_LIST;
    _START_POINT=$_TGPATH_PREFIX/;
    check_md5sum $_START_POINT $_FILE_LIST $_CMD_TYPE;
    _ERRORS=$?
    if [ $_ERRORS -ne 0 ];  then
        do_exit;
        _ERRORS=`expr $_ERRORS + 20`
        exit $_ERRORS;
    fi
fi

if [ "$_CMD_TYPE" = "check" -o "$_CMD_TYPE" = "check-ap-image" ]; then
    check_md5sum $_DIR2_AP_IMGS $_DIR2_AP_IMGS/$_FILE_LIST check
    _ERRORS=$?
    if [ $_ERRORS -ne 0 ];  then
        do_exit;
        _ERRORS=`expr $_ERRORS + 30`
        exit $_ERRORS;
    fi
fi

cd /;
do_exit;

exit 0;

