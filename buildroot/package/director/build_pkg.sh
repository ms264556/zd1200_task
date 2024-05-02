
#!/bin/sh -x

#buildroot/package
AC_DIR=`dirname $0`
BUILD_FDIMG=${BUILD_RELEASE_DIR}/$fw
BUILD_ROOTFS_DIR=$AC_DIR/../../root
TMPIMG_DIR=/tmp/fd_img_dir_$USER.$$

if [ "${BLD_RELEASE_DIR}" != "" ] ; then
BUILD_RELEASE_DIR=${BLD_RELEASE_DIR}
else
BUILD_RELEASE_DIR=$AC_DIR/../../release
fi


build_upgrade_image()
{
    set -x
    rm -rf $TMPIMG_DIR
    mkdir -p $TMPIMG_DIR
    mkdir -p $BUILD_RELEASE_DIR

    cd $TMPIMG_DIR
    BUILD_MODEL=$BR2_PROFILE
    ROOTFSIMG="$BUILD_ROOTFS_DIR/../release/rootfs.$BR2_ARCH.ext2.$BR2_PROFILE.img"
    KERNELIMG="$BUILD_ROOTFS_DIR/../kernel/arch/$BR2_ARCH/boot/bzImage"
    if [ -f "$ROOTFSIMG" ] ; then
        if [ -f "$KERNELIMG" ] ; then
            #tar -zcvf $BUILD_FDIMG $ROOTFSIMG $KERNELIMG
            cp -f $KERNELIMG .
            cp -f $ROOTFSIMG .
        else
            echo "$KERNELIMG does not exist. abort!"
            exit 1
        fi
    else
        echo "$ROOTFSIMG does not exist. abort!"
        exit 1
    fi

    cp -f $BUILD_RELEASE_DIR/restoreinitramfs.* .
    cp -f $BUILD_RELEASE_DIR/../grub_bld/lib/grub/i386-pc/menu.lst .

    # Last, create the metadata
    echo "PURPOSE=upgrade" > metadata
    echo "VERSION=${RKS_BUILD_VERSION}" >> metadata
    echo "BUILD=${BUILD_NUM}" >> metadata
    echo "REQUIRE_SIZE=`du -s . | cut -f 1`" >> metadata
    echo "REQUIRE_PLATFORM=$BR2_PLATFORM" >> metadata
    echo "REQUIRE_SUBPLATFORM=$BR2_SUBPLATFORM" >> metadata
    echo "KERNEL_MD5SUM=`/usr/bin/md5sum $KERNELIMG | cut -d ' ' -f 1`" >> metadata
    echo "ROOTFS_MD5SUM=`/usr/bin/md5sum $ROOTFSIMG | cut -d ' ' -f 1`" >> metadata
    echo "" >> metadata

 
    
    # copy to /tftpboot/director/

    _tftp=${TFTPBOOT:=/tftpboot}

    out_dir=${_tftp}/${BR2_PROFILE}
    VERSION_BUILD_NUM=${RKS_BUILD_VERSION}.${BUILD_NUM}

    # Pick up one AP image version to use as part of ZD image file name
    # Try zf79* first. 
    # Otherwise, use any ap image version (smallest ap model number will be used, such as zf2741)
    apversion=`ls $AC_DIR/etc/apimages/zf79* | grep -e '^[0-9.]\+' | head -n 1`
    if [ "$apversion" = "" ]; then
        apversion=`ls $AC_DIR/etc/apimages/* | grep -e '^[0-9.]\+' | head -n 1`
    fi
    mkdir -p ${out_dir}
    if [ "$apversion" = "" ]; then
        latest=${out_dir}/zd${BUILD_MODEL}_${VERSION_BUILD_NUM}.no_ap.img
    else
        latest=${out_dir}/zd${BUILD_MODEL}_${VERSION_BUILD_NUM}.ap_${apversion}.img
    fi
    tar cvzf $latest *
    rm -f ${out_dir}/zd_upg.img
    ln -s -f `basename $latest` ${out_dir}/zd_upg.img
    rm -rf $TMPIMG_DIR
}

build_upgrade_image