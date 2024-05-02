# Makefile for buildroot2
#
# Copyright (C) 1999-2005 by Erik Andersen <andersen@codepoet.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
#

#--------------------------------------------------------------
# Just run 'make menuconfig', configure stuff, then run 'make'.
# You shouldn't need to mess with anything beyond this point...
#--------------------------------------------------------------
# version number defined
include release_version

#If you are not overriding the profile, then include the default one
ifeq ($(PROFILE),)
include .profile
export PROFILE
endif

ifeq ($(JAVA_HOME),)
-include .java_home
endif

ifeq ($(COVERITY),)
-include .coverity
endif

ifeq ($(NO_GDB),)
-include .no_gdb
endif

ifeq ($(TFTPBOOT),)
TFTPBOOT=/tftpboot$(if $(MAKE_EXPRESS),/${shell id -u -n})
endif

ifeq ($(strip $(BR2_DOT_TOPDIR)),y)
TOPDIR=./
else
TOPDIR=${shell pwd}
endif
CONFIG_CONFIG_IN = Config.in
CONFIG_DEFCONFIG = .defconfig
CONFIG = package/config


ifndef PCONFIG
PCONFIG=profiles/$(PROFILE)/br2.config
endif

ifndef BUSYBOXCONFIG
BUSYBOXCONFIG=profiles/$(PROFILE)/busybox-release.config
endif

#PROJDIR=build/$(PROFILE)
TARGET_COMMON=target/common
COMMON_SKELETON=$(TARGET_COMMON)/skel.tar.gz
COMMON_SKEL_DIR=$(TARGET_COMMON)/target_skeleton
COMMON_DEVICE_TABLE=$(TARGET_COMMON)/device_table.txt
TARGET_SKELETON=profiles/$(PROFILE)/skel.tar.gz
TARGET_SKEL_DIR=profiles/$(PROFILE)/target_skeleton
USB_SKEL_DIR=profiles/$(PROFILE)/usbtool_skeleton
GRUB_SKEL_DIR=profiles/$(PROFILE)/grub_skeleton

# give the profile a chance to customize
# e.g. the device_table.txt for director
PROFILE_TARGET_DEVICE_TABLE=$(shell if [ -r profiles/$(PROFILE)/device_table.txt ]; then echo 1;  else echo 0 ; fi)
ifeq ($(PROFILE_TARGET_DEVICE_TABLE),1)
TARGET_DEVICE_TABLE=profiles/$(PROFILE)/device_table.txt
else
TARGET_DEVICE_TABLE=$(COMMON_DEVICE_TABLE)
endif

TARGET_CUSTOM=target/custom
TARGET_PRIVATE=$(TARGET_CUSTOM)/private

TOPDIR_PREFIX=profiles/$(PROFILE)/

noconfig_targets := menuconfig config oldconfig randconfig \
	defconfig allyesconfig allnoconfig 

# Pull in the user's configuration file
-include $(PCONFIG)
-include $(BUSYBOXCONFIG)


# Make sure pkg-config doesn't look outside the buildroot tree
unexport PKG_CONFIG_PATH
unexport PKG_CONFIG_SYSROOT_DIR
unexport PKG_CONFIG_LIBDIR


export KERNELPATH    = $(strip $(subst ",, $(BR2_KERNEL_PATH)))
export VERSION       = $(strip $(shell grep -E "^[ \t]*VERSION[ \t]*:?=[ \t]*"      $(KERNELPATH)/Makefile | cut -d '=' -f 2))
export PATCHLEVEL    = $(strip $(shell grep -E "^[ \t]*PATCHLEVEL[ \t]*:?=[ \t]*"   $(KERNELPATH)/Makefile | cut -d '=' -f 2))
export SUBLEVEL      = $(strip $(shell grep -E "^[ \t]*SUBLEVEL[ \t]*:?=[ \t]*"     $(KERNELPATH)/Makefile | cut -d '=' -f 2))
export EXTRAVERSION  = $(strip $(shell grep -E "^[ \t]*EXTRAVERSION[ \t]*:?=[ \t]*" $(KERNELPATH)/Makefile | cut -d '=' -f 2))
export KERNELVERSION = $(VERSION).$(PATCHLEVEL).$(SUBLEVEL)$(EXTRAVERSION)

export KERNELPATH_ABS = $(shell pwd)/$(strip $(subst ",, $(BR2_KERNEL_PATH)))

ifeq ($(BR2_TAR_VERBOSITY),y)
TAR_OPTIONS=-xvf
else
TAR_OPTIONS=-xf
endif

ifeq ($(strip $(BR2_PACKAGE_DIRECTOR)),y)
# For zd build only
ZD_SKEL_DIR=$(TARGET_COMMON)/director
endif

#############################################################
#
# The list of stuff to build for the target toolchain
# along with the packages to build for the target.
#
##############################################################
TOOLS_TARGETS:=host-sed kernel-headers uclibc-configured binutils gcc uclibc-target-utils uclibc squashfs
ifeq ($(TOOL_TOPDIR),)
TOOL_TOPDIR=$(TOPDIR)
endif
include toolchain/Makefile.in
include package/Makefile.in

TARGETS :=
EXTRAS_TRAGETS :=

ifeq ($(BR2_TARGET_USBTOOL_KIT),y)
USB_TOOL_BUILD_DIR=$(BR2_USBTOOL_DIR)
USB_TOOL_TARGET_DIR=$(BR2_USB_TOOL_TARGET_DIR)
endif

ifeq ($(BR2_TARGET_RESTORETOOL_KIT),y)
RESTORE_TOOL_BUILD_DIR=$(BR2_RESTORETOOL_DIR)
endif

ifeq ($(BR2_TARGET_PXETOOL_KIT),y)
PXE_TOOL_BUILD_DIR=$(BR2_PXETOOL_DIR)
endif

GRUB_TARGET_DIR=$(BR2_PACKAGE_GRUB_OUTPUT_PATH)

######################
# AP EXPRESS BUILD
######################

#
# Only regular AP needs this now.
#

ifneq ($(strip $(BR2_PACKAGE_DIRECTOR)),y) # limit to AP
ifneq ($(strip $(BR2_PACKAGE_MADWIFI_PATH)),) # exclude tg profile
MAKE_EXPRESS=$(shell if [ ! -d ${BR2_PACKAGE_MADWIFI_PATH}/madwifi/ath_hal ] ; \
                     then \
                         if [ ! -f ${BR2_PACKAGE_MADWIFI_PATH}/madwifi/net80211/ieee80211_input.c ] ; \
                         then echo 2 ; \
                         else echo 1 ; \
                         fi; fi;)
endif
endif


#############################################################
#
# You should probably leave this stuff alone unless you know
# what you are doing.
#
#############################################################


# first (global) target, default to this if no target specified
all:   world

# In this section, we need .config
include $(PCONFIG).cmd

ifeq ($(BR2_GCC_VERSION_4_3_X),y)
BR2_TOOLCHAIN_BUILDROOT=y
STAMP_DIR:=$(TOOL_BUILD_DIR)/stamps
HOST_DIR=$(STAGING_DIR)/usr
endif
ZCAT:=zcat
BZCAT:=bzcat

# We also need the various per-package makefiles, which also add
# each selected package to TARGETS if that package was selected
# in the .config file.
include $(sort $(wildcard toolchain/[a-t]*/*.mk))
UCLIBC_VER:=$(strip $(subst ",, $(BR2_UCLIBC_VERSION)))
include toolchain/uClibc/uclibc-$(UCLIBC_VER).mk

# build the kernel first after the toolchain
TARGETS += linux-kernel
include exp.mk
include binaries.mk
include symbol.mk
include $(sort $(wildcard package/*/*.mk))

# Common toolchain config may include cpp
# remove it from image if not needed to save space
ifeq ($(strip $(BR2_JUNK_LIBSTDCPP)),y)
TARGETS += squash-cpp-lib
endif

include $(sort $(wildcard target/*/*.mk))

# target stuff is last so it can override anything else
include target/Makefile.in

# local compile
#
# TARGET_CFLAGS need to be change after include  package/Makefile.in
# set define to support customized defaults
TARGET_CFLAGS += $(TARGET_DEFINES)

TARGETS_CLEAN:=$(patsubst %,%-clean,$(TARGETS))
TARGETS_SOURCE:=$(patsubst %,%-source,$(TARGETS))
TARGETS_DIRCLEAN:=$(patsubst %,%-dirclean,$(TARGETS))

BOOTLOADER_RAM:=$(TOPDIR)$(PROFILE)/$(BR2_TARGET_BOOTLOADER_RAM)
BOOTLOADER_ROM:=$(TOPDIR)$(PROFILE)/$(BR2_TARGET_BOOTLOADER_ROM)
BOOTLOADER_TYPE:=$(BR2_TARGET_BOOTLOADER_TYPE)

VERSION_DATE_STR := ${shell date +%s}
VERSION_DATE_STR := ${shell expr $(VERSION_DATE_STR) / 100}


# determine BUILD_VERSION  to use
ifeq ($(RKS_RELEASE_VERSION),)
BR_BUILD_VARIANT=$(strip $(BR2_BUILD_VARIANT))
ifeq ($(BR_BUILD_VARIANT),)
BR_BUILD_VARIANT=0
endif
RKS_BUILD_VERSION=$(BR2_BUILD_VERSION).$(BR_BUILD_VARIANT)
else
RKS_BUILD_VERSION=$(RKS_RELEASE_VERSION).$(RKS_RELEASE_VARIANT)
endif
export RKS_BUILD_VERSION

BR_BUILD_OFFICIAL   := `if [ -r .p4buildversion ]; then echo 1;  else echo 0 ; fi`
BR_BUILD_SUBVERSION := `if [ -r $(TOPDIR)/.p4buildversion ]; then cat $(TOPDIR)/.p4buildversion;  else echo $(VERSION_DATE_STR) ; fi`

BR_BUILD_VERSION := $(RKS_BUILD_VERSION).$(BR_BUILD_SUBVERSION)
BR_BUILD_VERSION_E := $(RSK_BUILD_VERSION).$(BR_BUILD_SUBVERSION)M

##  jenkins build environment
##  this is used to display the correct 
##  version string in the project page.
ifneq ($(P4_CHANGELIST),)
   P4CL := @$(P4_CHANGELIST)
endif
JENKINS_DESCRIPTION := $(BR_BUILD_VERSION)$(P4CL)
##  end

VERSION_FILE = $(TARGET_DIR)/etc/version
VERSION_E_FILE=$(BUILD_DIR)/etc.version+e

BOARDTYPE_FILE = $(TARGET_DIR)/etc/board_type
PRODUCT_FILE   = $(TARGET_DIR)/etc/product


ifeq ($(filter 2.6%, ${LINUX_HEADERS_VERSION}), )
LINUX_VERSION=2.4
else
LINUX_VERSION=2.6
endif

include gpl.mk


world:
ifeq ($(strip $(BR2_HAVE_DOT_CONFIG)),y)
ifeq ($(strip $(BR2_BUILD_MULTI_IMAGES)),y)
ifeq ($(strip $(BR2_FLASH_ROOTFS)),y)
	make SHELL=/bin/bash buildextras flash-info
	@echo
	@echo "Built $(BR_BUILD_VERSION)"
	@echo
else
	@echo "*** ALERT ***"
	@echo "Basic/full multi-images not supported with embedded ROOTFS in kernel"
	@echo "Set CONFIG_FLASH_ROOTFS=y in br2.config"
	@echo ""
endif
else
	make SHELL=/bin/bash buildworld $(POST_TARGETS) flash-info
	@echo
	@echo "Version =  $(BR_BUILD_VERSION)"
	@echo
endif
else
	make menuconfig
endif

__buildworld: info $(DL_DIR) $(BUILD_DIR) $(STAGING_DIR) $(TARGET_DIR) \
	$(GRUB_TARGET_DIR) $(USB_TOOL_BUILD_DIR) $(RESTORE_TOOL_BUILD_DIR) $(PXE_TOOL_BUILD_DIR)\
	version_file $(PRODUCT_FILE) $(TOOLS_TARGETS) $(EXP_TARGETS) $(TARGETS)

ifeq ($(strip $(BR2_PACKAGE_DIRECTOR)),y)
# rks_release is already a dependency for director-package
_MK_RKS_RELEASE =
else
_MK_RKS_RELEASE = rks_release
endif

.PHONY :  buildworld __buildworld

buildworld: __buildworld $(_MK_RKS_RELEASE)
	@echo "RKS release build completed successfully @" `date`


# re-build the bl7 file with all extras
#  Have to explicitly pass in ROOTFS_TARGET variable.
#
REBUILD_IMAGE= make BL7_EXTRAS=.full KEEP_TARGET_DIR=yes \
		IMG_VERSION=$(BR_BUILD_VERSION_E) \
		ROOTFS_TARGET=$(ROOTFS_TARGET) fix_ramdisk rks_clean_release

buildextras: buildworld $(EXTRAS_TARGETS)
	@echo "-----------------------------------------"
	@echo "$(EXTRAS_TARGETS) built ..."
	@echo "-----------------------------------------"
	@cp -fl $(VERSION_E_FILE) $(VERSION_FILE)
	$(REBUILD_IMAGE)
	@echo "RKS full relase build completed successfully @" `date`


rebuild_image:
	$(REBUILD_IMAGE)

info:
	@echo
	@echo "LINUX_VERSION              = ${LINUX_VERSION}"
	@echo
#	@echo "PCONFIG                    = $(PCONFIG)"
#	@echo "PROJDIR                    = $(PROJDIR)"
	@echo "MAKE_EXPRESS               = $(MAKE_EXPRESS)"
	@echo "CODE_TREE                  = $(CODE_TREE)"
	@echo "BIN_DEPOT                  = $(BIN_DEPOT)"
	@echo "BINBLD_TREE                = $(BINBLD_TREE)"
	@echo "BINBLD_DIR                 = $(BINBLD_DIR)"
	@echo "BINBLD_IMPORT_TARBALL      = $(BINBLD_IMPORT_TARBALL)"
	@echo "BINBLD_EXPORT_TARBALL      = $(BINBLD_EXPORT_TARBALL)"
#	@echo "RKS_RELEASE_VERSION        = $(RKS_RELEASE_VERSION)"
	@echo "RKS_BUILD_VERSION          = $(RKS_BUILD_VERSION)"
	@echo "BR_BUILD_VERSION           = $(BR_BUILD_VERSION)"
	@echo "JENKINS_DESCRIPTION        = $(JENKINS_DESCRIPTION)"
	@echo "PROFILE                    = $(PROFILE)"
	@echo "BR2_TOOLCHAIN_ID           = $(BR2_TOOLCHAIN_ID)"
	@echo "BUILD_DIR                  = $(BUILD_DIR)"
	@echo "TARGET_DIR                 = $(TARGET_DIR)"
	@echo "TOOL_BUILD_DIR             = $(TOOL_BUILD_DIR)"
	@echo "STAGING_DIR                = $(STAGING_DIR)"
	@echo "TOP_DIR                    = $(TOP_DIR)"
	@echo "BR2_KERNEL_PATH            = $(BR2_KERNEL_PATH)"
	@echo "KERNEL_OBJ_PATH            = $(KERNEL_OBJ_PATH)"
	@echo "BR2_KERNEL_COPYTO          = $(BR2_KERNEL_COPYTO)"
	@echo "USB_TOOL_BUILD_DIR         = $(BR2_USBTOOL_DIR)"
	@echo "RESTORE_TOOL_BUILD_DIR     = $(BR2_RESTORETOOL_DIR)"
	@echo "PXE_TOOL_BUILD_DIR         = $(BR2_PXETOOL_DIR)"
	@echo "GRUB_TARGET_DIR            = $(GRUB_TARGET_DIR)"
	@echo "LINUX_HEADERS_VERSION      = $(LINUX_HEADERS_VERSION)"
	@echo "BR2_GCC_VERSION            = $(BR2_GCC_VERSION)"
	@echo "BR2_UCLIBC_VERSION         = $(BR2_UCLIBC_VERSION)"
	@echo "STRIP                      = $(STRIP)"
	@echo "TFTPBOOT                   = $(TFTPBOOT)"
	@echo "TARGET_DEVICE_TABLE        = $(TARGET_DEVICE_TABLE)"
	@echo
	@echo "TARGET_CC                  = $(TARGET_CC)"
	@echo "TARGET_CFLAGS              = $(TARGET_CFLAGS)"
	@echo "TARGET_CROSS               = $(TARGET_CROSS)"
	@echo "SQUASHFS_DIR               = $(SQUASHFS_DIR)"
	@echo "ZD_SKEL_DIR                = $(ZD_SKEL_DIR)"
	@echo "PRIVATE_SKEL_DIR           = $(PRIVATE_SKEL_DIR)"
	@echo
	@echo "EXP_TARGETS                = $(EXP_TARGETS)"
	@echo
	@echo "TOOLS_TARGETS : "
	@echo   $(TOOLS_TARGETS)
	@echo
	@echo "TARGETS : "
	@echo   $(sort $(TARGETS))
	@echo
ifneq ($(POST_TARGETS),)
	@echo "POST_TARGETS : "
	@echo   $(sort $(POST_TARGETS))
	@echo
endif
	@echo "EXTRAS_TARGETS : "
	@echo   $(sort $(EXTRAS_TARGETS))
	@echo

#
# Target to build tools
build_tools:
	make tool_build

tool_build: info $(TOOL_BUILD_DIR) $(STAGING_DIR) $(TOOLS_TARGETS)
	@echo "Toolchain build completed"

#############################################################
#
# staging and target directories do NOT list these as
# dependancies anywhere else
#
#############################################################
$(DL_DIR):
	@mkdir -p $(DL_DIR)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/release

$(TOOL_BUILD_DIR):
	@mkdir -p $(TOOL_BUILD_DIR)

$(USB_TOOL_BUILD_DIR):
	@mkdir -p $(USB_TOOL_BUILD_DIR)
	if [ -d "$(USB_SKEL_DIR)" ] ; then \
		cp -af $(USB_SKEL_DIR)/* $(USB_TOOL_BUILD_DIR)/; \
		mkdir -p $(USB_TOOL_BUILD_DIR)/proc; \
		mkdir -p $(USB_TOOL_BUILD_DIR)/mnt;  \
		mkdir -p $(USB_TOOL_BUILD_DIR)/tmp;  \
		mkdir -p $(USB_TOOL_BUILD_DIR)/sys;  \
	fi

$(RESTORE_TOOL_BUILD_DIR):
	@mkdir -p $(RESTORE_TOOL_BUILD_DIR)
$(PXE_TOOL_BUILD_DIR):
	@mkdir -p $(PXE_TOOL_BUILD_DIR)

$(GRUB_TARGET_DIR):
	@mkdir -p $(GRUB_TARGET_DIR)
	if [ -d "$(GRUB_SKEL_DIR)" ] ; then \
		cp -af $(GRUB_SKEL_DIR)/* $(GRUB_TARGET_DIR)/; \
	fi

$(STAGING_DIR):
	@mkdir -p $(STAGING_DIR)/lib
	@mkdir -p $(STAGING_DIR)/include
	@mkdir -p $(STAGING_DIR)/$(REAL_GNU_TARGET_NAME)
	@ln -snf ../lib $(STAGING_DIR)/$(REAL_GNU_TARGET_NAME)/lib

ifeq ($(KEEP_TARGET_DIR),yes)
$(TARGET_DIR): info
	@echo "TARGET_DIR is being preserved"

else
$(TARGET_DIR): info
	@echo "TARGET_DIR is being recreated"
	rm -rf $(TARGET_DIR)
	if [ -f "$(COMMON_SKELETON)" ] ; then \
		zcat $(COMMON_SKELETON) | tar -C $(BUILD_DIR) -xf -; \
	fi;
	if [ -d "$(COMMON_SKEL_DIR)" ] ; then \
		mkdir -p $(TARGET_DIR); \
		cp -af $(COMMON_SKEL_DIR)/* $(TARGET_DIR)/; \
	fi;
	-cp -af $(COMMON_SKEL_DIR)/.[a-zA-Z0-9]* $(TARGET_DIR)/;
ifneq ($(strip $(BR2_PACKAGE_BRIDGE_DHCP_HOOK)),y)
	rm -f $(TARGET_DIR)/etc/dhcp_fingerprints.conf;
endif
	if [ -f "$(TARGET_SKELETON)" ] ; then \
		zcat $(TARGET_SKELETON) | tar -C $(BUILD_DIR) -xf -; \
	fi;
# common target_skeleton for ZD builds
ifeq ($(strip $(BR2_PACKAGE_DIRECTOR)),y)
	if [ -d "$(ZD_SKEL_DIR)" ] ; then \
		rsync -ca $(ZD_SKEL_DIR)/ $(TARGET_DIR)/; \
	fi;
endif
	if [ -d "$(TARGET_SKEL_DIR)" ] ; then \
		rsync -ca $(TARGET_SKEL_DIR)/* $(TARGET_DIR)/; \
	fi;
ifeq ($(CUSTOM_PRIVATE),1)
	if [ -d $(TARGET_PRIVATE) ] ; then \
		rsync -ca -L $(TARGET_PRIVATE)/ $(TARGET_DIR)/; \
	fi;
endif
	pwd
ifneq ($(strip $(BR2_PACKAGE_DIRECTOR)),y)
	sed -i -e '/Load CA Store/r $(CA-CERTS_SRC_DIR)/load-ca.sh' $(TARGET_DIR)/etc/init.d/sys_init
	sed -i -e 's/SSL_EXTENSION/$(SSL_EXTENSION)/' $(TARGET_DIR)/etc/init.d/sys_init
endif
	-rm -f $(BUILD_DIR)/fakeroot.env
	touch $(BUILD_DIR)/fakeroot.env
	-find $(TARGET_DIR) -type d -name CVS | xargs rm -rf
	-find $(TARGET_DIR) -type d -name .svn | xargs rm -rf
endif

#############################################################
#
# setup /etc/version /etc/board_type and /etc/product
#
MDY_STR := ${shell date "+%b %e %Y %H:%M"}
version_file: $(TARGET_DIR) kernel_build_dir
	if [ ! -r .p4buildversion ] ; then \
		system=`whoami`@`uname -n`; \
		echo $(BR_BUILD_VERSION) $$system $(MDY_STR) > $(VERSION_FILE); \
		echo $(BR_BUILD_VERSION_E) $$system $(MDY_STR) > $(VERSION_E_FILE); \
	else \
		echo $(BR_BUILD_VERSION) > $(VERSION_FILE); \
		echo $(BR_BUILD_VERSION_E) > $(VERSION_E_FILE); \
	fi; \
	echo $(BR2_RKS_TARGET_SYSTEM) > $(BOARDTYPE_FILE)
	@cp -fl $(VERSION_FILE) $(KERNEL_OBJ_PATH)

ifeq ($(strip $(BR2_PRODUCT_BARE)),y)
ETC_PRODUCT=bare
else
ifeq ($(strip $(BR2_PRODUCT_ROUTER)),y)
ETC_PRODUCT=router
else
ifeq ($(strip $(BR2_PRODUCT_ADAPTER)),y)
ETC_PRODUCT=adapter
else
ETC_PRODUCT=unknown
endif
endif
endif
# 
#  ETC_PRODUCT values are used by autoFW update (rfwd) to 
#  determine whether we are running a bare image
#  This needs to be the first word in /etc/product
#
$(PRODUCT_FILE): $(TARGET_DIR)
	echo $(ETC_PRODUCT) $(BR2_PRODUCT_TYPE) > $(PRODUCT_FILE)
#############################################################


source: $(TARGETS_SOURCE)

sourceball:
	rm -rf $(BUILD_DIR)
	set -e; \
	cd ..; \
	rm -f buildroot.tar.bz2; \
	tar -cvf buildroot.tar buildroot; \
	bzip2 -9 buildroot.tar; \

# moved to package/release/release.mk
#flash-info: image-flash-info redboot-flash-info datafs-flash-info


##########################################################################
#
# Coverity Integration
#
##########################################################################

## Control where to place intermediate analysis
ifneq ($(strip $(COV_TMP)),)
COV_PATH_ANALYSIS=$(COV_TMP)/$(BUILD_DIR)/../cov-analysis
else
COV_PATH_ANALYSIS=$(BUILD_DIR)/../cov-analysis
endif

## Control where to place DB
ifneq ($(strip $(COV_DB)),)
COV_PATH_DB=$(COV_DB)
else
COV_PATH_DB=/tmp
endif

cov_path_analysis:
	@echo $(COV_PATH_ANALYSIS)
	@mkdir -p $(COV_PATH_ANALYSIS)
	@exit 0;

cov_path_db:
	@echo $(COV_PATH_DB)

cov_path_install:
	@echo $(COVERITY)

cov_port:
ifneq ($(strip $(COVPORT)),)
	@echo $(COVPORT)
else
	@echo `expr $$RANDOM "+" $$RANDOM`
endif

cov_prereq_check:
	@if [ -e $(TARGET_CC) ] ; then \
        exit 0; \
    else \
        exit -1; \
    fi


#############################################################
#
# Configuration
#
#############################################################


$(CONFIG)/conf:
	$(MAKE) -C $(CONFIG) conf
	-@if [ ! -f $(PCONFIG) ] ; then \
		cp $(CONFIG_DEFCONFIG) $(PCONFIG); \
	fi
$(CONFIG)/mconf:
	$(MAKE) -C $(CONFIG) ncurses conf mconf
	-@if [ ! -f $(PCONFIG) ] ; then \
		cp $(CONFIG_DEFCONFIG) $(PCONFIG); \
	fi

menuconfig: $(CONFIG)/mconf
	-@if [ -f $(PCONFIG) ] ; then 	\
		rm -f .config* ;	\
		cp $(PCONFIG) .config;	\
	fi
	@$(CONFIG)/mconf $(CONFIG_CONFIG_IN)	
	-@if [ -f .config.cmd ] ; then 		\
		cp .config $(PCONFIG) ;		\
		cp .config.cmd $(PCONFIG).cmd ;	\
		rm -f .config* ;		\
	fi


config: $(CONFIG)/conf
	@$(CONFIG)/conf $(CONFIG_CONFIG_IN)

oldconfig: $(CONFIG)/conf
	@$(CONFIG)/conf -o $(CONFIG_CONFIG_IN)

randconfig: $(CONFIG)/conf
	@$(CONFIG)/conf -r $(CONFIG_CONFIG_IN)

allyesconfig: $(CONFIG)/conf
#	@$(CONFIG)/conf -y $(CONFIG_CONFIG_IN)
#	sed -i -e "s/^CONFIG_DEBUG.*/# CONFIG_DEBUG is not set/" .config
	@$(CONFIG)/conf -o $(CONFIG_CONFIG_IN)

allnoconfig: $(CONFIG)/conf
	@$(CONFIG)/conf -n $(CONFIG_CONFIG_IN)

defconfig: $(CONFIG)/conf
	@$(CONFIG)/conf -d $(CONFIG_CONFIG_IN)


#############################################################
#
# Cleanup and misc junk
#
#############################################################

kernelclean: kernel-clean kernel-headers-clean madwifi-clean

#
# WARNING: this will cause partial toolchain rebuild due to removal of
#   $(BUILD_DIR)/linux
#
buildclean:
	rm -rf $(BUILD_DIR)

targetclean target-clean:
	@echo "clean out everything in $(BUILD_DIR), except $(BUILD_DIR)/linux"
	rm -rf $(filter-out %linux,$(wildcard $(BUILD_DIR)/*))

clean: kernelclean target-clean
	@echo "clean kernel and taget done"

realclean: $(sort $(TARGETS_CLEAN))
	@echo "-->>  -->>  Clean Targets -->>  -->>"
	@echo $(sort $(TARGETS_CLEAN))
	@echo
ifeq ($(strip $(BR2_HAVE_DOT_CONFIG)),y)
	rm -rf  $(TARGET_DIR) $(IMAGE)
else
	rm -f .config .config.old .config.cmd .tmpconfig.h
	- $(MAKE) -C $(CONFIG) clean
endif

toolclean:
	rm -rf $(STAGING_DIR) $(TOOL_BUILD_DIR)

#
# WARNING: this target wipes out part of toolchain (ccache)
#   such that doing 'make' next will fail. Manual intervention
#   would be required, either wipe out toolchain cleanly to
#   rebuild toolchain... (or, is it sufficient or even possible
#   to just make ccache to restore it)? @ any rate, this manual
#   intervention trap renders this target dangerous and unlikely
#   to be what the user intends. Remove this trap.
#
#dirclean: $(TARGETS_DIRCLEAN)
#	rm -rf  $(TARGET_DIR) $(IMAGE)

distclean:
ifeq ($(DL_DIR),$(BASE_DIR)/dl)
	rm -rf $(DL_DIR)
endif
	rm -rf $(BUILD_DIR) $(LINUX_KERNEL) $(IMAGE)


.PHONY: all world clean dirclean distclean source $(TARGETS) \
	$(EXP_TARGETS) version_file \
	$(TARGETS_CLEAN) $(TARGETS_DIRCLEAN) $(TARGETS_SOURCE) \
	$(DL_DIR) $(BUILD_DIR) $(TOOL_BUILD_DIR) $(STAGING_DIR) \
	$(USB_TOOL_BUILD_DIR) $(RESTORE_TOOL_BUILD_DIR) $(PXE_TOOL_BUILD_DIR) $(GRUB_TARGET_DIR)\
	config oldconfig menuconfig defconfig FORCE


