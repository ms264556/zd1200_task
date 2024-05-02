#############################################################
#
# linux-kernel
#
#############################################################

KBUILD_MODPOST_WARN=1
export KBUILD_MODPOST_WARN

ifeq ($(BR2_KERNEL_BUILD_DEBUG),y)
KERNEL_BUILD_TYPE=debug
else
KERNEL_BUILD_TYPE=release
endif

ifeq ($(strip $(BR2_KERNEL_HEADERS_2_4_25)),y)
RKS_KERNEL_CONFIG=profiles/$(PROFILE)/linux-$(KERNEL_BUILD_TYPE).config
buildtree = $(strip $(subst ",, $(BR2_KERNEL_PATH)))

else
# 2.6 kernel
RKS_KERNEL_CONFIG=profiles/$(PROFILE)/linux_2.6-$(KERNEL_BUILD_TYPE).config

ifeq ($(strip $(BR2_KERNEL_OBJTREE)),y)

# use seperate objtree for kernel objects
buildtree := $(BUILD_DIR)/kernel

else

buildtree = $(strip $(subst ",, $(BR2_KERNEL_PATH)))

endif

endif

export KERNEL_OBJ_PATH = $(buildtree)

#
#	pass V54_BSP flag to kernel modules
#

ifneq ($(strip $(BR2_PACKAGE_BSP)),)

BSP_PATH := $(TOPDIR)/$(strip $(subst ",, $(BR2_PACKAGE_BSP_PATH)))
ifeq ($(BSP_PATH),)
$(error *** BR2_PACKAGE_BSP_PATH not specified)
endif

EXTRA_DEFS=-DV54_BSP=1 -I$(BSP_PATH)
ifeq ($(strip $(BR2_KERNEL_RKS_SYSTEM_TRACE)),y)
EXTRA_DEFS+= -DRKS_SYSTEM_TRACE -DRKS_TRACE -DRKS_PROC_FS
endif
endif

ifeq ($(strip $(BR2_KERNEL_RKS_SYSTEM_HOOKS)),y)
EXTRA_DEFS+= -DRKS_SYSTEM_HOOKS=1
endif

ifeq ($(strip $(BR2_RKS_BOARD_NAR5520)),y)
EXTRA_DEFS+= -DNAR5520=1
CONFIG_PLATFORM=nar5520
ifeq ($(strip $(BR2_KERNEL_BUILD_FOR_VM)),y)
EXTRA_DEFS+= -D__VM_SIM__
endif
endif

#snoop dogg
ifeq ($(strip $(BR2_RKS_BOARD_GD28)),y)
CONFIG_PLATFORM=P10XX
endif

#cyprus2
ifeq ($(strip $(BR2_RKS_BOARD_GD34)),y)
CONFIG_PLATFORM=P10XX
endif

ifeq ($(strip $(BR2_RKS_BOARD_P10XX)),y)
CONFIG_PLATFORM=P10XX
endif

ifneq (,$(filter $(strip $(PROFILE)),director-aussie ap-11n-aussie-boot))
EXTRA_DEFS+= -DZD1100_MBR
endif

ifeq ($(strip $(BR2_FLASH_ROOTFS)),y)
EXTRA_DEFS+=-DV54_FLASH_ROOTFS
FLASH_ROOTFS_DEFS=CONFIG_FLASH_ROOTFS=y
else
FLASH_ROOTFS_DEFS=CONFIG_FLASH_ROOTFS=n
endif

ifeq ($(strip $(BR2_MOUNT_FLASH_ROOTFS)),y)
EXTRA_DEFS+=-DV54_MOUNT_FLASH_ROOTFS
endif

ifeq ($(strip $(BR2_PACKAGE_TUBE)),y)
EXTRA_DEFS+= -DRKS_TUBE_SUPPORT
endif

ifeq ($(strip ${BR2_BUILD_WSG}),y)
EXTRA_DEFS+= -DRKS_WSG
endif

KBUILD_MODPOST_WARN=1
export KBUILD_MODPOST_WARN

#############################################################
#
# SDRAM size
#
#############################################################
ifeq ($(strip $(BR2_TARGET_DRAM_256MB)),y)
DRAM_MB=256
else ifeq ($(strip $(BR2_TARGET_DRAM_128MB)),y)
DRAM_MB=128
else ifeq ($(strip $(BR2_TARGET_DRAM_64MB)),y)
DRAM_MB=64
else ifeq ($(strip $(BR2_TARGET_DRAM_32MB)),y)
DRAM_MB=32
else ifeq ($(strip $(BR2_TARGET_DRAM_16MB)),y)
DRAM_MB=16
else
DRAM_MB=8
endif

EXTRA_DEFS+=-DDRAM_MB=$(DRAM_MB)

# GD6-1 requires a slow flash clock 
ifeq ($(strip $(BR2_RKS_BOARD_GD6-1)),y)
#EXTRA_DEFS+=-DSLOW_SPI_FLASH
endif

RKS_KERNEL_OPTIONS=BR2_ARCH=$(BR2_ARCH) \
	KBUILD_OUTPUT=$(buildtree) \
	KERNELPATH=$(strip $(subst ",, $(BR2_KERNEL_PATH))) \
	TOOLPREFIX=$(KERNEL_CROSS) \
	EXTRA_DEFS="$(EXTRA_DEFS)" \
	CROSS_COMPILE=$(KERNEL_CROSS) \
	MODPATH=$(TARGET_DIR)/lib/modules/ \
	$(FLASH_ROOTFS_DEFS) \
	CONFIG_PLATFORM=$(CONFIG_PLATFORM) \
	INSTALL_MOD_PATH=$(TARGET_DIR)

VMLINUX_OBJ = $(buildtree)/vmlinux

kernel_build_dir:
	if [ ! -d $(buildtree) ] ; then mkdir -p $(buildtree) ; fi

$(buildtree)/.config: $(RKS_KERNEL_CONFIG)
	cp -u --preserve=timestamps  $(RKS_KERNEL_CONFIG) $(buildtree)/.config
	make -C $(BR2_KERNEL_PATH) $(RKS_KERNEL_OPTIONS) oldconfig
	make -C $(BR2_KERNEL_PATH) $(RKS_KERNEL_OPTIONS) dep

.PHONY:$(VMLINUX_OBJ)
$(VMLINUX_OBJ): $(buildtree)/.config
	@echo "+++ Building target $(VMLINUX_OBJ)"
	make -C $(BR2_KERNEL_PATH) $(RKS_KERNEL_OPTIONS)

kernel-menuconfig: kernel_build_dir
	cp -u --preserve=timestamps $(RKS_KERNEL_CONFIG) $(buildtree)/.config
	make -C $(BR2_KERNEL_PATH) $(RKS_KERNEL_OPTIONS) menuconfig
	cp -u --preserve=timestamps $(buildtree)/.config $(RKS_KERNEL_CONFIG)

kernel-modules: $(VMLINUX_OBJ)
	make -C $(BR2_KERNEL_PATH) $(RKS_KERNEL_OPTIONS) modules modules_install
ifneq ($(USB_TOOL_BUILD_DIR),)
	mkdir -p $(strip $(subst ",, $(BR2_USBTOOL_DIR)))/lib
	cp -a $(TARGET_DIR)/lib/modules $(strip $(subst ",, $(BR2_USBTOOL_DIR)))/lib/
endif

$(BUILD_DIR)/vmlinux.bin: $(VMLINUX_OBJ)
	$(TARGET_CROSS)objcopy -O binary -g $(buildtree)/vmlinux $@

linux-kernel: kernel_build_dir kernel-modules
	mkdir -p $(BUILD_DIR)/root/boot
#   cp -f $(BUILD_DIR)/kernel/System.map $(BUILD_DIR)/root/boot/
# pick up version.h, moved from kernel-headers_fixup target
ifeq ($(strip $(BR2_KERNEL_HEADERS_FROM_KERNEL_SRC)),y)
	@if [[ $(strip $(subst ",, $(BR2_DEFAULT_KERNEL_HEADERS))) == 2.6.9 ]] \
	 || [[ $(strip $(subst ",, $(BR2_DEFAULT_KERNEL_HEADERS))) == 2.6.10 ]] \
	 || [[ $(strip $(subst ",, $(BR2_DEFAULT_KERNEL_HEADERS))) == 2.6.11 ]] \
	 || [[ $(strip $(subst ",, $(BR2_DEFAULT_KERNEL_HEADERS))) == 2.6.12 ]] \
	 || [[ $(strip $(subst ",, $(BR2_DEFAULT_KERNEL_HEADERS))) == 2.6.13 ]] \
	 || [[ $(strip $(subst ",, $(BR2_DEFAULT_KERNEL_HEADERS))) == 2.6.14 ]] \
	 || [[ $(strip $(subst ",, $(BR2_DEFAULT_KERNEL_HEADERS))) == 2.6.15 ]] \
	 || [[ $(strip $(subst ",, $(BR2_DEFAULT_KERNEL_HEADERS))) == 2.6.16 ]] \
	 || [[ $(strip $(subst ",, $(BR2_DEFAULT_KERNEL_HEADERS))) == 2.6.17 ]] \
	; then \
		if [[ $(strip $(BR2_KERNEL_OBJTREE)) == y ]] ; then \
			cp -pf $(buildtree)/include/linux/version.h  $(STAGING_DIR)/include/linux/; \
		else \
			cp -pf $(TOPDIR)/$(strip $(subst ",, $(BR2_KERNEL_PATH)))/include/linux/version.h $(STAGING_DIR)/include/linux/; \
		fi; \
	fi;
endif

ifdef DEF_OBSOLETE
# $(ROOTFS_TARGET) defined in target/{ext2,squashfs}/*.mk
ifneq ($(strip $(BR2_FLASH_ROOTFS)),y)
EMBEDDED_RAMDISK=$(ROOTFS_TARGET)
else
EMBEDDED_RAMDISK =
endif

kernel-rootfs: $(EMBEDDED_RAMDISK)
	@echo EMBEDDED_RAMDISK=$(EMBEDDED_RAMDISK)
	make -C $(BR2_KERNEL_PATH) $(RKS_KERNEL_OPTIONS) vmlinux

else

kernel-rootfs:

endif

ifeq ($(strip $(BR2_FLASH_ROOTFS)),y)
VMLINUX_NAME=vmlinux-root
else
VMLINUX_NAME=vmlinux
endif

linux-kernel-clean linux-clean kernel-clean:
	rm -f $(BUILD_DIR)/vmlinux*
ifeq ($(strip $(BR2_KERNEL_OBJTREE)),y)
	make -C $(BR2_KERNEL_PATH) $(RKS_KERNEL_OPTIONS) mrproper
else
	-make -C $(BR2_KERNEL_PATH) $(RKS_KERNEL_OPTIONS) mrproper
endif
	rm -rf $(buildtree)

linux-kernel-depend kernel-depend:
	make -C $(BR2_KERNEL_PATH) $(RKS_KERNEL_OPTIONS) depend

linux-kernel-depmod:
	@echo "++++ Building linux-kernel-depmod"
	make -C $(BR2_KERNEL_PATH) $(RKS_KERNEL_OPTIONS) _modinst_post

linux-kernel-info:
	@echo "CROSS_COMPILE            = $(KERNEL_CROSS)"
	@echo "CC                       = $(TARGET_CC)"
	@echo "BR2_KERNEL_PATH          = $(BR2_KERNEL_PATH)"
	@echo "RKS_KERNEL_CONFIG        = $(RKS_KERNEL_CONFIG)"
	@echo "TOPDIR                   = $(TOPDIR)"
	@echo "PROFILE                  = $(PROFILE)"
	@echo "buildtree                = $(buildtree)"
	@echo "RKS_KERNEL_OPTIONS       = $(RKS_KERNEL_OPTIONS)"
	@echo "CONFIG_NET_ATHEROS_ETHER = $(CONFIG_NET_ATHEROS_ETHER)"

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_LINUX_KERNEL)),y)
TARGETS+=linux-kernel
endif

