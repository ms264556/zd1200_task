#############################################################
#
# mksquashfs to build to target squashfs filesystems
#
#############################################################
#for kernel 2.6.32, we are using squashfs4.0
ifeq ($(SUBLEVEL),32)
TOUSESQUASHFS4 := y
else
#for kernel 2.6.15, we are using squashfs3.4
TOUSESQUASHFS4 := n
endif

SQUASHFS_LZMA := 1
ifdef SQUASHFS_LZMA
ifeq ($(strip $(TOUSESQUASHFS4)),y)
SQUASHFS_DIR = $(BASE_DIR)/toolchain/squashfs4.0-lzma-snapshot
else
SQUASHFS_DIR = $(BASE_DIR)/../video54/tools/squashfs2.2
endif
MKSQUASHFS = $(STAGING_DIR)/bin/mksquashfs

$(SQUASHFS_DIR)/squashfs-tools/mksquashfs: lzmalib
	$(MAKE) -C $(SQUASHFS_DIR)/squashfs-tools

squashfs: $(SQUASHFS_DIR)/squashfs-tools/mksquashfs
	cp  $(SQUASHFS_DIR)/squashfs-tools/mksquashfs $(MKSQUASHFS)

else
SQUASHFS_DIR=$(BUILD_DIR)/squashfs2.1-r2
SQUASHFS_SOURCE=squashfs2.1-r2.tar.gz
SQUASHFS_SITE=http://$(BR2_SOURCEFORGE_MIRROR).dl.sourceforge.net/sourceforge/squashfs
MKSQUASHFS = $(SQUASHFS_DIR)/squashfs-tools/mksquashfs

$(DL_DIR)/$(SQUASHFS_SOURCE):
	 $(WGET) -P $(DL_DIR) $(SQUASHFS_SITE)/$(SQUASHFS_SOURCE)

$(SQUASHFS_DIR)/.unpacked: $(DL_DIR)/$(SQUASHFS_SOURCE) #$(SQUASHFS_PATCH)
	zcat $(DL_DIR)/$(SQUASHFS_SOURCE) | tar -C $(BUILD_DIR) -xvf -
	toolchain/patch-kernel.sh $(SQUASHFS_DIR) target/squashfs/ squashfs\*.patch
	touch $(SQUASHFS_DIR)/.unpacked

$(SQUASHFS_DIR)/squashfs-tools/mksquashfs: $(SQUASHFS_DIR)/.unpacked
	$(MAKE) -C $(SQUASHFS_DIR)/squashfs-tools;

squashfs: $(SQUASHFS_DIR)/squashfs-tools/mksquashfs

squashfs-source: $(DL_DIR)/$(SQUASHFS_SOURCE)

squashfs-clean:
	-$(MAKE) -C $(SQUASHFS_DIR)/squashfs-tools clean

squashfs-dirclean:
	rm -rf $(SQUASHFS_DIR)
endif

#############################################################
#
# LZMAlib for mksquashfs
#
#############################################################
ifeq ($(strip $(TOUSESQUASHFS4)),y)
LZMALIB_DIR = $(BASE_DIR)/toolchain/lzma465/C/LzmaUtil
else
LZMALIB_DIR = $(BASE_DIR)/../video54/tools/lzma/sdk4.27/SRC/7zip/Compress/LZMA_Lib
endif

lzmalib:
ifeq ($(strip $(TOUSESQUASHFS4)),y)
	make -C $(LZMALIB_DIR) -f makefile.gcc
	cp -f $(LZMALIB_DIR)/liblzma.a  $(STAGING_DIR)/lib
	cp -f $(LZMALIB_DIR)/lzma $(STAGING_DIR)/bin/
else
	make -C $(LZMALIB_DIR)
	cp $(LZMALIB_DIR)/liblzma.a  $(STAGING_DIR)/lib
endif

#############################################################
#
# Build the squashfs root filesystem image
#
#############################################################
SQUASHFS_ENDIANNESS=-le
ifeq ($(strip $(BR2_armeb)),y)
SQUASHFS_ENDIANNESS=-be
endif
ifeq ($(strip $(BR2_mips)),y)
SQUASHFS_ENDIANNESS=-be
endif
ifeq ($(strip $(BR2_powerpc)),y)
SQUASHFS_ENDIANNESS=-be
endif
ifeq ($(strip $(BR2_sh3eb)),y)
SQUASHFS_ENDIANNESS=-be
endif
ifeq ($(strip $(BR2_sh4eb)),y)
SQUASHFS_ENDIANNESS=-be
endif
ifeq ($(strip $(BR2_sparc)),y)
SQUASHFS_ENDIANNESS=-be
endif

# if building backup image, delete some files
ifeq ($(strip $(BR2_TARGET_BUILD_BACKUP_IMAGE)),y)
JUNK_FILES += $(TARGET_DIR)/usr/bin/ani
JUNK_FILES += $(TARGET_DIR)/usr/bin/80211*
JUNK_FILES += $(TARGET_DIR)/usr/bin/ath*
JUNK_FILES += $(TARGET_DIR)/usr/bin/dump*
JUNK_FILES += $(TARGET_DIR)/usr/bin/eeprom
endif

ifeq ($(strip $(BR2_TARGET_ROOTFS_SQUASHFS)),y)
SQROOTFS_BASE :=	$(BUILD_DIR)/$(subst ",,$(BR2_TARGET_ROOTFS_SQUASHFS_OUTPUT))
else
SQROOTFS_BASE := /
endif

SUFFIX=.img

SQROOTFS_TARGET := $(SQROOTFS_BASE)$(SUFFIX)

RAMDISK_COPYTO_ROOT = $(KERNEL_OBJ_PATH)
RAMDISK_H= $(KERNEL_OBJ_PATH)/include/linux/ramdisk.h

FAKEROOT_CMDS=chown -R root:root $(TARGET_DIR)
FAKEROOT_CMDS+=; $(STAGING_DIR)/bin/makedevs -d $(TARGET_DEVICE_TABLE) $(TARGET_DIR)
ifeq ($(strip $(TOUSESQUASHFS4)),y)
FAKEROOT_CMDS+=; $(MKSQUASHFS) $(TARGET_DIR) $(SQROOTFS_BASE) -noappend
else
FAKEROOT_CMDS+=; $(MKSQUASHFS) $(TARGET_DIR) $(SQROOTFS_BASE) -noappend $(SQUASHFS_ENDIANNESS)
endif

# dependency on depmod finishing before creating target filesystem
# kernel module dependency handled in release, linux-kernel makefiles
#
#SQROOTFS_TARGETS = $(TARGET_DIR) linux-kernel-depmod
SQROOTFS_TARGETS = $(TARGET_DIR)
ifdef NOT_DEF
$(SQROOTFS_BASE): $(SQROOTFS_TARGETS) host-fakeroot makedevs $(BUILD_DIR)/fakeroot.env squashfs
else
$(SQROOTFS_BASE): $(SQROOTFS_TARGETS) host-fakeroot makedevs
endif
	-rm -f $(JUNK_FILES)
	-@find $(TARGET_DIR) -type f -perm +111 | xargs $(STRIP) 2>/dev/null || true;
	@rm -rf $(TARGET_DIR)/usr/man
	@rm -rf $(TARGET_DIR)/usr/info
	-/sbin/ldconfig -r $(TARGET_DIR) 2>/dev/null

ifeq ($(strip $(BR2_TARGET_ROOTFS_SQUASHFS_FILELIST)),y)
	echo "Starting to generate file list ..."; \
	rm -rf $(TARGET_DIR)/file_list.txt; \
	pushd $(TARGET_DIR); \
	ls -1R > /tmp/file_list.tmp; \
	while read name; \
	do \
		if [ "$$name" != "" ]; then \
		echo "$$name"|grep '^\..*:$$' >/dev/null; \
			if [ $$? -eq 0 ]; then \
				dir=`echo "$$name"|sed 's/\(.*\):$$/\1/'`; \
				if [ "$$dir" != "." ]; then \
					echo "DIR:$$dir" >> $(TARGET_DIR)/file_list.txt; \
				fi \
			else \
				if [ -h $$dir/$$name ]; then \
					echo "LINK:$$dir/$$name" >> $(TARGET_DIR)/file_list.txt; \
				elif [ -f $$dir/$$name ]; then \
					md5sum=`/usr/bin/md5sum $$dir/$$name`; \
					if [ $$? -eq 0 ];then \
						echo "FILE:$$md5sum" >> $(TARGET_DIR)/file_list.txt; \
					else \
						echo "Error: generate md5 error, file [$$dir/$$name] ..."; \
					fi \
				else \
					echo "SKIP:$$dir/$$name" >> $(TARGET_DIR)/file_list.txt; \
				fi \
			fi \
		fi \
	done < /tmp/file_list.tmp; \
	popd; \
	rm -rf /tmp/file_list.tmp; \
	echo "List generated ...";
endif

ifdef NOT_DEF
# Use fakeroot to pretend all target binaries are owned by root
	-$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
		chown -R root:root $(TARGET_DIR)
# Use fakeroot to pretend to create all needed device nodes
	$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
		$(STAGING_DIR)/bin/makedevs \
		-d $(TARGET_DEVICE_TABLE) \
		$(TARGET_DIR)
# Use fakeroot so mksquashfs believes the previous fakery
	$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
		$(MKSQUASHFS) \
		    $(TARGET_DIR) \
		    $(SQROOTFS_BASE) \
		    -noappend $(SQUASHFS_ENDIANNESS)
else
# Use fakeroot to pretend all target binaries are owned by root
# Use fakeroot to pretend to create all needed device nodes
# Use fakeroot so mksquashfs believes the previous fakery
	$(STAGING_DIR)/usr/bin/fakeroot \
		-- /bin/sh -c "$(FAKEROOT_CMDS)"
endif
ifneq ($(USE_RAMDISK_H),)
# *** obsoleted *** 11/06/08
# determine size of resulting image and put in include/linux/ramdisk.h
	ROOTFS_REALSIZE=`LANG=C ls -s $(SQROOTFS_BASE) | cut -d' ' -f1`; \
	ROOTFS_ADDTOROOTSIZE=`if [ $$ROOTFS_REALSIZE -ge 20000 ] ; then echo 16384; else echo 400; fi`; \
	ROOTFS_SIZE=`expr $$ROOTFS_REALSIZE + $$ROOTFS_ADDTOROOTSIZE`; \
	set -x; \
	if [ -f $(RAMDISK_H) ] ; then chmod +w $(RAMDISK_H) ; fi ;\
	echo  "ROOTFS_SIZE = $$ROOTFS_SIZE" ;\
	echo  "#define ROOTFS_RAMDISK_SIZE ( $$ROOTFS_SIZE + 100 )" > $(RAMDISK_H)
endif


$(SQROOTFS_TARGET): $(SQROOTFS_BASE)
	/bin/ln -sf $(SQROOTFS_BASE) $@

# ifneq ($(strip $(subst ",, $(BR2_MOUNT_FLASH_ROOTFS))),y)
# location to copy ramdisk.img to the ramdisk directory in the kernel
RAMDISK_COPYTO=$(strip $(subst ",, $(BR2_ROOTFS_RAMDISK_COPYTO)))
ifneq ($(RAMDISK_COPYTO),)
SQROOTFS_COPYTO := $(RAMDISK_COPYTO_ROOT)/$(RAMDISK_COPYTO)
else
SQROOTFS_COPYTO := $(strip $(subst ",,$(BR2_TARGET_ROOTFS_SQUASHFS_COPYTO)))
endif
# endif

squashfsroot: $(SQROOTFS_BASE) $(SQROOTFS_TARGET)
	@ls -l $(SQROOTFS_TARGET)
ifneq ($(SQROOTFS_COPYTO),)
	set -x ;\
	if [ -d "$(SQROOTFS_COPYTO)" ] ; then \
		cp -Lf $(SQROOTFS_TARGET) $(SQROOTFS_COPYTO) ; \
	fi
endif

squash-cpp-lib:
	rm -f $(TARGET_DIR)/lib/libstdc++.*

squashfsroot-source: squashfs-source

squashfsroot-clean:
	-$(MAKE) -C $(SQUASHFS_DIR) clean

squashfsroot-dirclean:
	rm -f $(SQUASHFS_DIR)/squashfs-tools/*.o
	rm -f $(SQUASHFS_DIR)/squashfs-tools/mksquashfs
	rm -f $(MKSQUASHFS)

.PHONY: squashfsroot squashfsroot-source squashfsroot-clean squash-cpp-lib

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_TARGET_ROOTFS_SQUASHFS)),y)
TARGETS+=squashfsroot
ROOTFS_TARGET = squashfsroot
ROOTFS_OBJS = $(SQROOTFS_BASE) $(SQROOTFS_TARGET)
endif
