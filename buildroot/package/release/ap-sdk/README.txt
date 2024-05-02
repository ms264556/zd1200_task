##########################################################################
Copyright (C) 2012 Ruckus Wireless, Inc.  All rights reserved.

This file and content that came packaged with this file are considered
Ruckus Proprietary Information. You may not redistribute the package or
any of its content without explicit written consent from Ruckus
Wireless, Inc. 

You may not reverse engineer, disassemble, decompile, or translate the
software, or otherwise attempt to derive the source code of the
software. 

In no event shall Ruckus Wireless, Inc. or its suppliers be liable to
you for any consequential, special, incidental, or indirect damages of
any kind arising out of the delivery, performance, or use of the
software. 
##########################################################################


***********************************
* Ruckus AP SDK Application Notes *
***********************************
Version: 1.0
Release: Apr. 25, 2012

-----------------------------------
1. Content of AP SDK
	o. README.txt     :  This document.
	o. Kernel Source  :  RUCKUS_MIPS_SDK_v1.0/linux/kernel/mips-linux-2.6.15
	o. Tool Chain     :  RUCKUS_MIPS_SDK_v1.0/tools/rks_ap_sdk_toolchain-1.0.tar.bz2
	o. Prebuild Image :  RUCKUS_MIPS_SDK_v1.0/images
                         x. rcks_fw.bl7          - firmware image
                         x. vmlinux              - network boot image
                         x. datafs.jffs2.ap-sdk  - could be ignored this. 
                         x. sdk.rcks             - control file used for the first time(as 'Step 4' below)
-----------------------------------
2. Development environment
	o. Setup toolchain
		x. un-tar rks_ap_sdk_toolchain-1.0.tar.bz2 to /opt/2.6.15_gcc4.2.4 as the following:
			tar -jxvf rks_ap_sdk_toolchain-1.0.tar.bz2 -C /opt/2.6.15_gcc4.2.4
		x. content of toolchain
			x. binary utils:     /opt/2.6.15_gcc4.2.4/bin
			x. linker, compiler: /opt/2.6.15_gcc4.2.4/bin
			x. libraries:        /opt/2.6.15_gcc4.2.4/lib
			x. headers:          /opt/2.6.15_gcc4.2.4/include

	o. Setup TFTP server
		x. setup a tftp server.
		x. put RUCKUS_MIPS_SDK_v1.0/images to tftp server.
                   e.g., under /tftpboot/ap-sdk directory
		x. content of AP images:
			x. rcks_fw.bl7         : firmware image
			x. vmlinux             : network boot image                      
			x. datafs.jffs2.ap-sdk : datafs image, could be ignored
            x. sdk.rcks            : control file for first-time setup
-----------------------------------
3. Connect to Ruckus AP
	o. AP default Setting:
		x. default IP:      192.168.0.77/24
	o. connecting to AP by eth cable.
        o. factory units : follow steps in 5: First Time Setup 
	o. to connect to the AP running SDK firmware, use telnet command
           as this example. 
		> telnet 192.168.0.77
		Trying 192.168.0.77...
		Connected to 192.168.0.77 (192.168.0.77).
		Escape character is '^]'.

        BusyBox v1.15.2 (2012-04-12 20:38:24 CST) built-in shell (ash)
        Enter 'help' for a list of built-in commands.
		
		#
-----------------------------------	
4. First Time Setup
        o. verify the configurations in sdk.rcks (the firmware control
           file) is correct.
           - file size needs to match rcks_fw.bl7
           - correct tftp path
        o. ssh 192.168.0.77
        o. user: super pass: sp-admin
        o. set up the AP to find the tftp server:

        rkscli: fw set proto tftp
        rkscli: fw set host 192.168.0.9    --> Need to change to your TFTP server.
        rkscli: fw set control sdk.rcks
        rkscli: fw show  

current primary boot image is Image1
--------------------------------------------------------------
Auto F/W upgrade                          = disabled
Running on image                          = Image1
FW Control Control File                   = sdk.rcks
Control File Server                       = 192.168.0.9
Protocol                                  = TFTP
Port                                      = auto
Boot Flags (Main,Backup,Factory,Reset)    = M. ..  [MB FR]
--------------------------------------------------------------
OK

        o. Point the TFTP_SERVER of your sdk.rcks 
        [rcks_fw.main]
        YOUR_TFTP_SERVER_IP   -> Replace to your IP address
        /rcks_fw.bl7
        1949696

        rkscli: fw update

fw: Updating rcks_wlan.main ...
_erase_flash: offset=0x0 count=30
Erase Total 30 Units
Performing Flash Erase of length 65536 at offset 0x1d0000 done
..............................................................................................................................................................................................................................................New Firmware Image1 written to Flash
bdSave: sizeof(bd)=0x7c, sizeof(rbd)=0xd0
  caching flash data from /dev/mtd5 [ 0x00000000 - 0x00010000 ]
  updating flash data [0x00008000 - 0x000080d0] from [0x7ffb5be0 - 0x7ffb5cb0]
_erase_flash: offset=0x0 count=1
Erase Total 1 Units
Performing Flash Erase of length 65536 at offset 0x0 done
  caching flash data from /dev/mtd5 [ 0x00000000 - 0x00010000 ]
  verifying flash data [0x00008000 - 0x000080d0] from [0x7ffb5be0 - 0x7ffb5cb0]
**fw(716) : Completed

        rkscli: reboot

        o. The AP will reboot into SDK firmware.
        o. Proceed to Step 6 below.  The AP contains two firmware
        partitions on the flash.  You need to perform update one more
        time to ensure the second partition also gets flashed with the
        SDK firmware.  The secondary partition is used for backup. In
        case the main image gets corrupted, the AP should boot into the
        secondary image.

	
-----------------------------------
5. Image upgrade (always use this after the AP has gone through First Time Setup):
	o. type fw_upgrade.sh to get helps
		fw_upgrade.sh <protocol>://<server ip>/<path/image>
	o. to upgarde your AP image, giving fw_upgrade.sh parameters as following example:
        # fw_upgrade.sh tftp://192.168.0.9/rcks_fw.bl7
        **************************
        * starting image upgrade *
        **************************
        --->downloading [rcks_fw.bl7]...
        --->upgrading [/dev/mtd7]...
        Erasing blocks: 31/31 (100%)
        Writing data: 1984k/1984k (100%)
        Verifying data: 1984k/1984k (100%)bdSave: sizeof(bd)=0x7c, sizeof(rbd)=0xd0
         caching flash data from /dev/mtd6 [ 0x00000000 - 0x00010000 ]
          updating flash data [0x00008000 - 0x000080d0] from [0x00411a3c - 0x00411b0c]
        _erase_flash: offset=0x0 count=1
        Erase Total 1 Units
        Performing Flash Erase of length 65536 at offset 0x0 done
          caching flash data from /dev/mtd6 [ 0x00000000 - 0x00010000 ]
          verifying flash data [0x00008000 - 0x000080d0] from [0x00411a3c - 0x00411b0c]
        # System Shutdown ...
        The system is going down NOW!
        Sent SIGTERM to all processes
        Sent SIGKILL to all processes
        Requesting system reboot
        *** Hardware Watchdog stopped ***
        *** soft watchdog stopped ***
        Restarting system.
        .
	
-----------------------------------
6. Network boot
	o. you need a tftp server and put vmlinux image in it.
	o. type net_boot.sh to get helps:
		net_boot.sh <host ip> <server ip>/<path/image>
	o. to boot up from network, type net_boot.sh as example following:
		# net_boot.sh 192.168.0.7 192.168.0.9/vmlinux
		*******************************
		* set network boot parameters *
		*******************************
		--->host ip:   [192.168.0.1]
		--->server ip: [192.168.0.9]
		--->image:     [vmlinux]
		please reboots to take effect
		-------------------------------
	o. verify paramters, then type 'reboot' to take effect.
        o. This is a one-time only command.  Must re-run the command to
           network boot the next time.
           - Power cycling without running this command will cause the
           AP to load/run the image from the flash.
           - We highly recommend that you develop using network boot and
           verify the image (especially look for any bugs that may cause a
           reboot) before flashing it onto the AP.
	
-----------------------------------
7. Build Environment

	o. The SDK directory has the following file structure:

RUCKUS_MIPS_SDK_v1.0
|-- build                     //main directory used to build kernel/image/apps
|   |-- config.in
|   |-- Makefile
|   `-- mips
|       |-- ld.script
|       `-- release.mk
|-- config
|   |-- dev_table.txt         
|   `-- kernel-config         //kernel default configuration
|-- env-setup.sh              //main script to guide you to set environment and show available command
|-- examples                 
|   `-- hello                 //hello user space program and kernel module example
|       |-- app
|       `-- module
|-- images                    //prebuild image 
|   |-- datafs.jffs2.ap-sdk
|   |-- rcks_fw.bl7
|   |-- sdk.rcks
|   `-- vmlinux
|-- linux
|   |-- kernel
|   |   `-- mips-linux-2.6.15 //kernel source tree
|   |-- kernel1
|   |   |-- kernel1.o
|   |   |-- ldflags_vmlinux
|   |   `-- vmlinux.lds
|   `-- rootfs                //rootfs default skeleton
|       |-- ash_env
|       |-- bin
|       |-- boot
|       |-- dev
|       |-- etc
|       |-- fl -> /writable
|       |-- home
|       |-- lib
|       |-- linuxrc -> bin/busybox
|       |-- mnt
|       |-- opt
|       |-- proc
|       |-- root
|       |-- sbin
|       |-- sys
|       |-- tmp
|       |-- usr
|       |-- var
|       `-- writable
|-- output                    
|   |-- ap-sdk                //image output here
|-- README.txt
`-- tools
    |-- rks_ap_sdk_toolchain-1.0.tar.bz2   //toolchain bzip2 tarball
    `-- toolchain -> /opt/2.6.15_gcc4.2.4


	o. There is a config.in for you to setup build environemnt.
	o. For example change project name, version, build...etc.
    #over all project parameters
    PROJECT     = ap-sdk
    PROJECT_VERSION = 1.0.0.0
    PROJECT_BUILD   = 1 

    SDK_VERSION = 1.0 

    #project directories
    PROJECT_ROOT    = ${shell pwd}/..
    PACKAGE_DIR = ${PROJECT_ROOT}/$(SDK_VERSION)
    OUTPUT_DIR  = ${PROJECT_ROOT}/output/${PROJECT}
    KERNEL_SRC  = ${PROJECT_ROOT}/linux/kernel/mips-linux-2.6.15
    KERNEL_OBJ  = ${PROJECT_ROOT}/linux/kernel1
    TOOLCHAIN_DIR   = ${PROJECT_ROOT}/tools/toolchain
    ROOTFS_DIR  = ${PROJECT_ROOT}/linux/rootfs
    ROOTFS_NAME = ramdisk.img
    AP_FLASH_IMG    = rcks_fw.bl7
    NET_BOOT_IMG    = vmlinux

    KERNE_CONFIG    = ${PROJECT_ROOT}/config/kernel-config
    DEV_TABLE   = ${PROJECT_ROOT}/config/dev_table.txt

    #compile options
    ARCH        = mips
    TARGET      = ${ARCH}-linux
    HOST        := i686-linux
    PREFIX      = ${TOOLCHAIN_DIR}
    TARGET_PREFIX   = ${PREFIX}/${TARGET}
    CROSS_COMPILE   = ${TARGET}-
    AS      = ${CROSS_COMPILE}as
    AR      = ${CROSS_COMPILE}ar
    CC      = ${CROSS_COMPILE}gcc
    CPP     = ${CROSS_COMPILE}gcc -E
    LD      = ${CROSS_COMPILE}ld
    NM      = ${CROSS_COMPILE}nm
    OBJCOPY     = ${CROSS_COMPILE}objcopy
    OBJDUMP     = ${CROSS_COMPILE}objdump
    RANLIB      = ${CROSS_COMPILE}ranlib
    READELF     = ${CROSS_COMPILE}readelf
    SIZE        = ${CROSS_COMPILE}size
    STRINGS     = ${CROSS_COMPILE}strings
    STRIP       = ${CROSS_COMPILE}strip
    LZMA        = ${PREFIX}/bin/lzma
    BINMD5      = ${PREFIX}/bin/binmd5
    FAKEROOT_LIB    = ${PREFIX}/usr/lib/libfakeroot.so
    FAKEROOT_BIN    = ${PREFIX}/usr/bin/faked
    KERNELPATH_ABS = ${KERNEL_SRC}
    CFLAGS_KERNEL   = -I${KERNEL_SRC}/drivers/v54bsp -DRKS_SYSTEM_HOOKS=1 -DV54_FLASH_ROOTFS -DV54_MOUNT_FLASH_ROOTFS -DDRAM_MB=64 -DV54_BSP=1
    CONFIG_PLATFORM =
    KERNELVERSION = 2.6 

    #export params
    export PROJECT PROJECT_VERSION PROJECT_BUILD 
    export ARCH PATH TARGET HOST PREFIX CROSS_COMPILE CFLAGS_KERNEL TOOLCHAIN_DIR

	o. After you extract the RUCKUS_MIPS_SDK_v1.0.tar.bz2, use
 
      $source ./env_setup.sh

        If you saw console outputs the following message - 
    
            *** You need to install toolchain to /opt/2.6.15_gcc4.2.4 ***
            *** Step 1 - sudo mkdir /opt/2.6.15_gcc4.2.4 ***
            *** Step 2 - extract /home/chris/sdk/RUCKUS_MIPS_SDK_v1.0/tools/rks_ap_sdk_toolchain-1.0.tar.bz2 ***
            sudo tar jxf /home/chris/sdk/RUCKUS_MIPS_SDK_v1.0/tools/rks_ap_sdk_toolchain-1.0.tar.bz2 -C /home/chris/sdk/RUCKUS_MIPS_SDK_v1.0/opt/2.6.15_gcc4.2.4

        It means you haven't install toolchain yet as mentiond above.

        Otherwise, you'll see the message -
            === Set environment done ===
            Usages:
            Build kernel_config:    make kernel_config
            Build kernel:make       build_kernel build_modules
            Build rootfs:make       build_rootfs
            Build images:make       build_images
            Check parameters:       make info

        o. Use 'make', it'll start to build the image and output to RUCKUS_MIPS_SDK_v1.0/output/ap-sdk.
           You can use network boot or burn the image directly to the flash to verify the changes you had done.
           When system boots up successfully, invoke hello, You can see the message, '--- Welcome to Ruckus SDK v1.0 --- ' shown on the console.
           And use, 'insmod /lib/modules/2.6.15/kernel/hello.ko', you can see the message, 'Hello, Ruckus SDK 1.0' shown by using dmesg. These program are under examples folder.
