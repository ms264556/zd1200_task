#!/bin/sh

_CMD_TYPE=$1
_TGPATH_PREFIX=/
_FILE_LIST=file_list.txt
_DIR2_AP_IMGS=/etc/airespider-images/firmwares
_ERRORS=0 #started from 10. real # of errors=return code - 10

show_usage()
{
    echo "Usage: $0 <check|check-rootfs|check-ap-image>"
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
fi

if [ "$_CMD_TYPE" = "check" -o "$_CMD_TYPE" = "check-rootfs" ]; then
    _TGPATH_PREFIX=$_REMOUNT_PATH;
    _FILE_LIST=$_TGPATH_PREFIX/$_FILE_LIST;
    _START_POINT=$_TGPATH_PREFIX/;
    check_md5sum $_START_POINT $_FILE_LIST $_CMD_TYPE;
    _ERRORS=$?
    if [ $_ERRORS -ne 0 ];  then
        _ERRORS=`expr $_ERRORS + 20`
        exit $_ERRORS;
    fi
fi

if [ "$_CMD_TYPE" = "check" -o "$_CMD_TYPE" = "check-ap-image" ]; then
    check_md5sum $_DIR2_AP_IMGS $_DIR2_AP_IMGS/$_FILE_LIST check
    _ERRORS=$?
    if [ $_ERRORS -ne 0 ];  then
        _ERRORS=`expr $_ERRORS + 30`
        exit $_ERRORS;
    fi
fi

cd /;

exit 0;

