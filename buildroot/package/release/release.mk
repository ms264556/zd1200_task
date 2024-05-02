#############################################################
#
# Release image packaging
#
#############################################################


system_arch = $(strip $(subst ",, $(BR2_ARCH)))

# KERNEL_OBJ_PATH set in package/linux-kernel/linux-kernel.mk
VMLINUX_OBJ=$(KERNEL_OBJ_PATH)/vmlinux
RELEASE_DIR=$(BUILD_DIR)/release
VMLINUX_BIN=$(RELEASE_DIR)/vmlinux.bin
VMLINUX_BL7=$(RELEASE_DIR)/vmlinux.bin.l7
RCKS_FW_BL7=$(RELEASE_DIR)/rcks_fw.bl7


# TFTP_SUBDIR = $(subst /tftpboot/,, $(BR2_KERNEL_COPYTO))/
RELEASE_COPYTO = $(strip $(subst ",, $(BR2_KERNEL_COPYTO)))
TFTP_SUBDIR = $(RELEASE_COPYTO)/
ifeq ($(BR2_KERNEL_COPYTO), $(TFTP_SUBDIR))
TFTP_SUBDIR = 
endif

############################################################
#
# flash partition settings


ifeq ($(strip $(BR2_RKS_BOARD_GD4)),y)

MAIN_LEN=0x300000
PART_MAIN=0xbfc30000

else

ifeq ($(strip $(BR2_TARGET_FLASH_4MB)),y)
MAIN_LEN=0x340000
PART_MAIN=0xa8030000
else

#ifeq ($(strip $(BR2_TARGET_DRAM_16MB)),y)
#endif

ifeq ($(strip $(BR2_TARGET_FLASH_SYMMETRY)),y)
## router16
MAIN_LEN=0x390000
PART_MAIN=0xa8030000
BKUP_LEN=0x390000
PART_BKUP=0xa83C0000

else
## router32

MAIN_LEN=0x500000
PART_MAIN=0xa8030000
BKUP_LEN=0x220000
PART_BKUP=0xa8530000

endif

endif

endif

# GD7 (8MB flash) router16-11a/adapter16-11a
ifeq ($(strip $(BR2_RKS_BOARD_GD7)),y)

MAIN_LEN=0x390000
PART_MAIN=0xbe030000
BKUP_LEN=0x390000
PART_BKUP=0xbe3C0000

endif


ifeq ($(strip $(BR2_RKS_BOARD_DF1)),y)

MAIN_LEN=0x500000
PART_MAIN=0xbf040000
BKUP_LEN=0x220000
PART_BKUP=0xbf540000

endif

ifeq ($(strip $(BR2_RKS_BOARD_RETRIEVER)),y)

MAIN_LEN=0x700000
PART_MAIN=0xbf040000
BKUP_LEN=0x700000
PART_BKUP=0xbf740000

endif

ifeq ($(PROFILE), router32)
MAIN_LEN_16M=0x6c0000
PART_MAIN_16M=0xa8040000
BKUP_LEN_16M=0x6c0000
PART_BKUP_16M=0xa8700000
endif

############################################################

ifeq ($(PROFILE), ap-11n-ppc)
KERNEL_ENTRY_POINT=0x00000000
KERNEL_MEMSTART=0x00000000
else
KERNEL_ENTRY_POINT=$(shell $(TARGET_CROSS)objdump -f $(VMLINUX_OBJ) | grep start | cut -f3 -d' ')
# KERNEL_MEMSTART=$(shell $(TARGET_CROSS)nm $(VMLINUX_OBJ) | grep _ftext | cut -f1 -d' ' | sed -e 's/^ffffffff//')
KERNEL_MEMSTART=$(shell $(TARGET_CROSS)objdump -p $(VMLINUX_OBJ) | grep LOAD | awk '{ if ( NR == 1 ) print $$5;}')
endif

ifeq ($(PROFILE), ap-11n-ppc-wsg)
KERNEL_ENTRY_POINT=0x00000000
KERNEL_MEMSTART=0x00000000
endif

ifeq ($(system_arch),mips)

ifeq  ($(PROFILE),ap-11n-wasp)
flash-info: wasp-image-flash-info
else
flash-info : image-flash-info redboot-flash-info datafs-flash-info
endif
else
flash-info : ppc-image-flash-info
endif


image-flash-info:
ifeq ($(strip $(BR2_LINUX_KERNEL)),y)
	@echo -e "\nFlash linux kernel using RedBoot commands:"
ifeq ($(strip $(BR2_BUILD_MULTI_IMAGES)),y)
	@echo " Choose from one of two images:"
	@echo "  the normal image (rcks_fw.bl7, vmlinux)"
	@echo "  and the image with extras (rcks_fw.full.bl7, vmlinux.full)"
endif
ifeq ($(strip $(BR2_TARGET_ROOTFS_JFFS2)),y)
	@echo "  RedBoot> fis create -l 0xb0000 -f 0xbff10000 -e $(KERNEL_ENTRY_POINT) -r 0x$(KERNEL_MEMSTART) vmlinux.bin.l7"
	@echo "  To mount rootfs via nfs Use"
	@echo "  Redboot> exec -c \"nfsroot=<nfs-server-ip>:<exported-rootfs-path> ip=<device-ip>::<gateway-ip>:<netmask>::eth0:off rw panic=10\""
	@echo " To export generated root fs from your dev/build system :"
	@echo " mkdir /x "
	@echo " tar -x -C /x -f $(IMAGE).tar"
	@echo " Add following line /etc/exports file on your dev/build system"
	@echo " /x              *(rw,no_root_squash)"
	@echo " As root execute exportfs -a"
else
ifeq ($(strip $(BR2_KERNEL_BINMD5)),y)
	@echo -e "\n  RedBoot> load -r -b %{FREEMEMLO} $(TFTP_SUBDIR)rcks_fw.bl7"
else
	@echo -e "\n  RedBoot> load -r -b %{FREEMEMLO} $(TFTP_SUBDIR)vmlinux.bin.l7"
endif
	@echo -e "\nTo save as primary image:"
	@echo "  RedBoot> fis create -l $(MAIN_LEN) -f $(PART_MAIN) -e $(KERNEL_ENTRY_POINT) -r $(KERNEL_MEMSTART) -t 1 rcks_wlan.main"
ifeq ($(PROFILE),router32)
	@echo -e "\nTo save as primary image: (16MB Flash)"
	@echo "  RedBoot> fis create -l $(MAIN_LEN_16M) -f $(PART_MAIN_16M) -e $(KERNEL_ENTRY_POINT) -r $(KERNEL_MEMSTART) -t 1 rcks_wlan.main"
endif
ifdef PART_BKUP
	@echo -e "\nTo save as backup image:"
	@echo "  RedBoot> fis create -l $(BKUP_LEN) -f $(PART_BKUP) -e $(KERNEL_ENTRY_POINT) -r $(KERNEL_MEMSTART) -t 2 rcks_wlan.bkup"
ifeq ($(PROFILE),router32)
	@echo -e "\nTo save as backup image: (16MB Flash)"
	@echo "  RedBoot> fis create -l $(BKUP_LEN_16M) -f $(PART_BKUP_16M) -e $(KERNEL_ENTRY_POINT) -r $(KERNEL_MEMSTART) -t 2 rcks_wlan.bkup"
endif
	@echo -e "\nFor network boot:"
	@echo -e "  RedBoot> run $(TFTP_SUBDIR)vmlinux\n"
endif
endif
endif

wasp-image-flash-info:
ifeq ($(strip $(BR2_LINUX_KERNEL)),y)
	@echo -e "\nFlash linux kernel using U-Boot commands:"

ifeq ($(strip $(BR2_KERNEL_BINMD5)),y)
	@echo -e "\nTo save as primary image:"
	@echo -e "\n tftpboot 0x1000000 $(TFTP_SUBDIR)rcks_fw.bl7"
	@echo -e " erase 1:4-0xd3;cp.l 0x81000000 0xbf040000 0x340000"
	@echo -e "\nTo save as backup image:"
	@echo -e "\n tftpboot 0x1000000 $(TFTP_SUBDIR)rcks_fw.bl7"
	@echo -e " erase 1:0x100-0x1cf;cp.l 0x81000000 0xc0000000 0x340000"
	@echo -e "\nFor network boot:"
	@echo -e "\n setenv bootcmd bootnet $(TFTP_SUBDIR)vmlinux\n"
endif
endif

ppc-image-flash-info:
ifeq ($(strip $(BR2_LINUX_KERNEL)),y)
	@echo -e "\nFlash linux kernel using U-Boot commands:"

ifeq ($(strip $(BR2_KERNEL_BINMD5)),y)
	@echo -e "\nTo save as primary image:"
	@echo -e "\n tftpboot 0x1000000 $(TFTP_SUBDIR)rcks_fw.bl7"
	@echo -e " sf probe 0;sf erase 0x80000 0xd00000;sf write 0x1000000 0x80000 0xd00000"
	@echo -e "\nTo save as backup image:"
	@echo -e "\n tftpboot 0x1000000 $(TFTP_SUBDIR)rcks_fw.bl7"
	@echo -e " sf probe 0;sf erase 0x1000000 0xd00000;sf write 0x1000000 0x1000000 0xd00000"
	@echo -e "\nFor network boot:"
	@echo -e "\n setenv bootcmd bootnet $(TFTP_SUBDIR)vmlinux\n"
endif
endif



#for kernel 2.6.32, we are using squashfs4.0 and lzma sdk 4.65
ifeq ($(SUBLEVEL),32)
TOUSELZMA465 := y
else
#for kernel 2.6.15, we are using squashfs3.4 and lzma sdk 4.27
TOUSELZMA465 := n
endif

ifeq ($(strip $(TOUSELZMA465)),y)
LZMADIR= $(BASE_DIR)/toolchain/lzma465/C/LzmaUtil
else
LZMADIR= $(BASE_DIR)/../video54/tools/lzma/sdk4.27/SRC/7zip/Compress/LZMA_Alone
endif
lzma:
ifeq ($(strip $(TOUSELZMA465)),y)
	make -C $(LZMADIR) -f makefile.gcc
	cp -f $(LZMADIR)/lzma $(STAGING_DIR)/bin/
	cp -f $(LZMADIR)/liblzma.a  $(STAGING_DIR)/lib
else
	make -C $(LZMADIR)
	cp $(LZMADIR)/lzma $(STAGING_DIR)/bin/
endif

BIN_DEPS = lzma
ifeq ($(strip $(BR2_KERNEL_BINMD5)),y)
BIN_DEPS += binmd5_host
BINMD5_HOST_BINARY=$(STAGING_DIR)/bin/binmd5
endif


IMG_VERSION ?= $(BR_BUILD_VERSION)

BINMD5_OPTS = -d -v"$(IMG_VERSION)" -e$(KERNEL_ENTRY_POINT) -a$(KERNEL_MEMSTART)
BINMD5_OPTS += -p$(BR2_PRODUCT_TYPE)
ifneq ($(strip $(BR2_RKS_TARGET_SYSTEM)),)
BINMD5_OPTS += -b$(BR2_RKS_TARGET_SYSTEM)
endif
ifneq ($(strip $(BR2_RKS_TARGET_SYSTEM_CLASS)),)
BINMD5_OPTS += -c$(BR2_RKS_TARGET_SYSTEM_CLASS)
endif


RAMDISK_IMG = $(BUILD_DIR)/ramdisk
ifneq ($(strip $(BR2_KERNEL_BINMD5)),y)
RAMDISK_IMG =
endif

KERNEL1_O_CP =

ifneq ($(PROFILE), director-aussie)

build_ramdisk=n
ifeq ($(system_arch),mips)
build_ramdisk=y
endif
ifeq ($(system_arch),powerpc)
build_ramdisk=y
endif

ifeq ($(build_ramdisk),y)

KERNEL1_OBJ=$(KERNEL_OBJ_PATH)/kernel1.o
vmlinux-lds := $(KERNEL_OBJ_PATH)/arch/$(system_arch)/kernel/vmlinux.lds
ldflags_vmlinux = $(KERNEL_OBJ_PATH)/ldflags_vmlinux

KERNEL_RAMDISK = $(TOPDIR)/package/linux-kernel/$(system_arch)
RD_LD_SCRIPT = $(KERNEL_RAMDISK)/ld.script
RAMDISK_OBJ = $(RELEASE_DIR)/ramdisk.o
include $(KERNEL_RAMDISK)/ramdisk.mk

VMLINUX_RD=$(RELEASE_DIR)/vmlinux.root

$(KERNEL1_OBJ): FORCE
	make -C $(BR2_KERNEL_PATH) $(RKS_KERNEL_OPTIONS) build_kernel1_obj

$(ldflags_vmlinux): FORCE
	make -C $(BR2_KERNEL_PATH) $(RKS_KERNEL_OPTIONS) build_ldflags_vmlinux

$(VMLINUX_RD): $(KERNEL1_OBJ) $(ldflags_vmlinux) $(RAMDISK_OBJ) $(vmlinux-lds)
	$(TARGET_CROSS)ld $(shell cat $(ldflags_vmlinux)) -T $(vmlinux-lds) -o $@ $(KERNEL1_OBJ) $(RAMDISK_OBJ)
#	$(CP) -f $@ $(KERNEL_OBJ_PATH)/

vmlinux_rd : $(VMLINUX_RD)
	@echo $(VMLINUX_RD) done

ifeq ($(PROFILE), ap-sdk)
# extra files to save
KERNEL1_O_CP = $(KERNEL1_OBJ) $(vmlinux-lds) $(ldflags_vmlinux)
endif

else

KERNEL1_OBJ =
vmlinux-lds =
VMLINUX_RD = $(VMLINUX_OBJ)
#RAMDISK_IMG =

endif
else
vmlinux-lds =
VMLINUX_RD = $(VMLINUX_OBJ)
endif

ifeq ($(strip $(BR2_FLASH_ROOTFS)),y)
BINMD5_OPTS +=  -r$(RAMDISK_IMG)
VMLINUX_FL = $(VMLINUX_OBJ)
else
VMLINUX_FL = $(VMLINUX_RD)
endif

# support info , signature and certificate location
MODEL_TXT=$(BASE_DIR)/profiles/$(PROFILE)/model.txt
RSA_SIGN=$(TOP_DIR)/docs/signature.bin
RSA_CERT=$(TOP_DIR)/docs/newcert.pem
ifeq ($(strip $(BR2_TARGET_SIGN)),y)
BINMD5_OPTS +=  -m$(MODEL_TXT) -s$(RSA_SIGN) -t$(RSA_CERT)
endif

ifeq ($(strip $(BR2_PACKAGE_DIRECTOR)),y)
# run fixup scripts before building ramdisk
ROOTFS_FIXUP = $(DIRECTOR_FIXUP)

$(RAMDISK_IMG) : $(ROOTFS_FIXUP)
endif

$(VMLINUX_BL7) : $(BIN_DEPS) $(RAMDISK_IMG) $(VMLINUX_FL)
	$(TARGET_CROSS)objcopy -O binary -g $(VMLINUX_FL) $(VMLINUX_BIN)
	$(STAGING_DIR)/bin/lzma e $(VMLINUX_BIN) $@
ifeq ($(strip $(BR2_KERNEL_BINMD5)),y)
	$(BINMD5_HOST_BINARY) $(BINMD5_OPTS) -i$(VMLINUX_BL7) -o$(RCKS_FW_BL7)
endif


BL7_EXTRAS ?=

VMLINUX_NAME=vmlinux$(BL7_EXTRAS)


.PHONY: $(RELEASE_DIR)

$(RELEASE_DIR):
	if [ ! -d $(RELEASE_DIR) ] ; then mkdir $(RELEASE_DIR) ; fi

ifdef DEF_OBSOLETE
KERNEL_ROOTFS = kernel-rootfs
else
KERNEL_ROOTFS =
endif

rks_release : $(KERNEL_ROOTFS) $(RELEASE_DIR) $(VMLINUX_BL7) $(VMLINUX_RD)
ifeq ($(PROFILE), director-aussie)
# TODO: Update for Aussie release
	@echo "Skip rks_release"
else
ifneq ($(RELEASE_COPYTO),)
	if [ ! -d ${RELEASE_COPYTO} ] ; then mkdir ${RELEASE_COPYTO} ; fi
ifeq ($(strip $(BR2_KERNEL_STRIP_DEBUG)),y)
	$(TARGET_CROSS)strip -g $(VMLINUX_RD) -o $(RELEASE_COPYTO)/$(VMLINUX_NAME)
else
	cp -f $(VMLINUX_RD)  $(RELEASE_COPYTO)/$(VMLINUX_NAME)
endif
	cp -f $(KERNEL_OBJ_PATH)/System.map $(RELEASE_COPYTO)/System.map$(BL7_EXTRAS)
ifeq ($(strip $(BR2_KERNEL_BINMD5)),y)
	cp -f   $(RCKS_FW_BL7)   $(RELEASE_COPYTO)/rcks_fw$(BL7_EXTRAS).bl7
endif
ifneq ($(KERNEL1_O_CP),)
	mkdir -p $(RELEASE_COPYTO)/kernel1 ; cp -f $(KERNEL1_O_CP) $(RELEASE_COPYTO)/kernel1/
endif
endif
endif
	@echo "******* rks_release$(BL7_EXTRAS) *******************************"

ifeq ($(PROFILE), ap-sdk)
RKS_SDK_VER = 1.0
SDK_REL_DIR = $(RELEASE_COPYTO)/sdk
RKS_SDK_DIR = RUCKUS_$(shell echo $(ARCH) | tr a-z A-Z)_SDK_v$(RKS_SDK_VER)
RKS_SDK_PKG = $(RKS_SDK_DIR).tar.bz2
SDK_PKG_DIR = $(SDK_REL_DIR)/$(RKS_SDK_DIR)
SDK_PKG_TEMPLATE_DIR = images tools linux/rootfs linux/kernel output/ap-sdk 
SDK_PKG_COPYFILES = build config examples env-setup.sh README.txt
SDK_RCKS = sdk.rcks
sdk_package:
	@echo "******* sdk package *******************************"
	@echo "=== Generate sdk.rcks ==="
	@echo '[rcks_fw.main]' > $(RELEASE_COPYTO)/$(SDK_RCKS)
	@echo 'YOUR_TFTP_SERVER_IP' >> $(RELEASE_COPYTO)/$(SDK_RCKS)
	@echo "/rcks_fw.bl7" >> $(RELEASE_COPYTO)/$(SDK_RCKS)
	@echo $$(ls -l $(RELEASE_COPYTO)/rcks_fw.bl7 | awk '{print $$5}') >> $(RELEASE_COPYTO)/$(SDK_RCKS)
	@echo "=== Generate sdk.rcks done ==="
	rm -rf $(SDK_REL_DIR)/
	-mkdir -p $(SDK_PKG_DIR)/
	@for dir in $(SDK_PKG_TEMPLATE_DIR); do mkdir -p $(SDK_PKG_DIR)/$$dir; done
	cp $(RELEASE_COPYTO)/{vmlinux,rcks_fw.bl7,datafs.jffs2.$(PROFILE),$(SDK_RCKS)} $(SDK_PKG_DIR)/images
	rsync -ca $(RELEASE_COPYTO)/kernel1 $(SDK_PKG_DIR)/linux/
	rsync -ca $(TARGET_DIR)/* $(SDK_PKG_DIR)/linux/rootfs
	cp -a $(BR2_KERNEL_PATH) $(SDK_PKG_DIR)/linux/kernel
	${TOP_DIR}/package/release/fix-kernel.sh $(SDK_PKG_DIR)/linux/kernel/$(shell basename $(BR2_KERNEL_PATH)) ${BR2_KERNEL_PATH} ${KERNEL_OBJ_PATH}
	tar -jc -C $(STAGING_DIR) -f $(SDK_PKG_DIR)/tools/rks_ap_sdk_toolchain-$(RKS_SDK_VER).tar.bz2 .
	ln -sf /opt/$(BR2_TOOLCHAIN_ID) $(SDK_PKG_DIR)/tools/toolchain
	@for dir in $(SDK_PKG_COPYFILES); do cp -a package/release/$(PROFILE)/$$dir $(SDK_PKG_DIR); done
	tar -jc -C $(SDK_REL_DIR) -f $(RELEASE_COPYTO)/$(RKS_SDK_PKG) .
endif



# $(ROOTFS_OBJS) from squashfsroot.mk
# ROOTFS_OBJS ?= $(BUILD_DIR)/ramdisk $(BUILD_DIR)/ramdisk.img
fix_ramdisk:
	-rm -f $(ROOTFS_OBJS)
	@echo ROOTFS_TARGET=$(ROOTFS_TARGET)

rks_clean_release: ${ROOTFS_TARGET} rks_release
	@echo ""

.PHONY: release_dir-clean

release_dir-clean:
	-rm -rf $(RELEASE_DIR)

rks_release-clean:
	rm -f $(VMLINUX_BIN) $(VMLINUX_BL7)

.PHONY:	rks_release remove_ramdisk rks_clean_release rks_release-clean

.PHONY: flash-info image-flash-info

.PHONY: vmlinux_rd

#############################################################
#
# Toplevel Makefile options
#
#############################################################
# rks_release and rks_full_release targets are referenced explicitly
# in the top level makefile

#TARGETS+=rks_release
#ifeq ($(strip $(BR2_BUILD_MULTI_IMAGES)),y)
#EXTRAS_TARGETS+=rks_full_release
#endif

