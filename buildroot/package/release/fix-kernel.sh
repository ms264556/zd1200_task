#!/bin/bash

KERNEL_PATH=$1
BR2_KERNEL_PATH=$2
KERNEL_OBJ_PATH=$3

# Remove symbolic link
rm  ${KERNEL_PATH}/net/802.1X
rm  ${KERNEL_PATH}/net/rks_ioctl
rm  ${KERNEL_PATH}/net/rudb
rm  ${KERNEL_PATH}/net/RKS_NL
rm  ${KERNEL_PATH}/drivers/v54bsp
rm  ${KERNEL_PATH}/drivers/net/ae531x
rm  ${KERNEL_PATH}/arch/mips/ar7100/watchdog

# Re-create folder to avoid compiler complain
mkdir  ${KERNEL_PATH}/net/802.1X
touch  ${KERNEL_PATH}/net/802.1X/Kconfig
mkdir  ${KERNEL_PATH}/net/rks_ioctl
touch  ${KERNEL_PATH}/net/rks_ioctl/Kconfig
mkdir  ${KERNEL_PATH}/net/rudb
touch  ${KERNEL_PATH}/net/rudb/Kconfig
mkdir  ${KERNEL_PATH}/net/RKS_NL
mkdir  ${KERNEL_PATH}/drivers/v54bsp
mkdir  ${KERNEL_PATH}/drivers/net/ae531x
touch  ${KERNEL_PATH}/drivers/net/ae531x/Kconfig
mkdir  ${KERNEL_PATH}/arch/mips/ar7100/watchdog

# Copy file to v54bsp, and remove un-needed files.
cp  ${BR2_KERNEL_PATH}/drivers/v54bsp/*  ${KERNEL_PATH}/drivers/v54bsp
rm -f ${KERNEL_PATH}/drivers/v54bsp/nar5520*
rm -f ${KERNEL_PATH}/drivers/v54bsp/rcks_hwck*
if [ "${CONFIG_PLATFORM}" = "" ]; then
    rm -f ${KERNEL_PATH}/drivers/v54bsp/*p10xx*
elif [ "${CONFIG_PLATFORM}" = "P10XX" ]; then 
    rm -f ${KERNEL_PATH}/drivers/v54bsp/v54_linux_gpio.c
    rm -f ${KERNEL_PATH}/drivers/v54bsp/bsp_reset.c
fi

# Copy RKS_NL folder
cp ${BR2_KERNEL_PATH}/net/RKS_NL/* ${KERNEL_PATH}/net/RKS_NL

# Copy watchdog
cp ${BR2_KERNEL_PATH}/arch/mips/ar7100/watchdog/* ${KERNEL_PATH}/arch/mips/ar7100/watchdog

cp ${BR2_KERNEL_PATH}/drivers/net/ae531x/macProc.h ${KERNEL_PATH}/drivers/net/ae531x

# touch version
touch ${KERNEL_PATH}/version

pushd ${KERNEL_PATH}/include
ln -s asm-mips asm
popd

# Copy default config 
cp ${KERNEL_OBJ_PATH}/.config                  ${KERNEL_PATH}/.config
cp ${KERNEL_OBJ_PATH}/Module.symvers           ${KERNEL_PATH}
cp ${KERNEL_OBJ_PATH}/include/linux/autoconf.h ${KERNEL_PATH}/include/linux
cp ${KERNEL_OBJ_PATH}/include/linux/version.h  ${KERNEL_PATH}/include/linux
cp -r ${KERNEL_OBJ_PATH}/scripts/*             ${KERNEL_PATH}/scripts
