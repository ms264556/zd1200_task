#############################################################
#
# genext2fs to build to target ext2 filesystems
#
#############################################################
GENEXT2_DIR=$(BUILD_DIR)/genext2fs-1.3
GENEXT2_SOURCE=genext2fs_1.3.orig.tar.gz
GENEXT2_SITE=http://ftp.debian.org/debian/pool/main/g/genext2fs

$(DL_DIR)/$(GENEXT2_SOURCE):
#	$(WGET) -P $(DL_DIR) $(GENEXT2_SITE)/$(GENEXT2_SOURCE)

$(GENEXT2_DIR)/.unpacked: $(DL_DIR)/$(GENEXT2_SOURCE)
	zcat $(DL_DIR)/$(GENEXT2_SOURCE) | tar -C $(BUILD_DIR) -xvf -
	mv $(GENEXT2_DIR).orig $(GENEXT2_DIR)
	toolchain/patch-kernel.sh $(GENEXT2_DIR) target/ext2/ genext2fs\*.patch
	touch $(GENEXT2_DIR)/.unpacked

$(GENEXT2_DIR)/.configured: $(GENEXT2_DIR)/.unpacked
	chmod a+x $(GENEXT2_DIR)/configure
	(cd $(GENEXT2_DIR); rm -rf config.cache; \
		./configure \
		--prefix=$(STAGING_DIR) \
	);
	touch  $(GENEXT2_DIR)/.configured

$(GENEXT2_DIR)/genext2fs: $(GENEXT2_DIR)/.configured
	$(MAKE) CFLAGS="-Wall -O2 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE \
		-D_FILE_OFFSET_BITS=64" -C $(GENEXT2_DIR);
	touch -c $(GENEXT2_DIR)/genext2fs

genext2fs: $(GENEXT2_DIR)/genext2fs



#############################################################
#
# Build the ext2 root filesystem image
#
#############################################################

EXT2_OPTS :=

ifeq ($(strip $(BR2_TARGET_ROOTFS_EXT2_SQUASH)),y)
EXT2_OPTS += -U
endif

ifneq ($(strip $(BR2_TARGET_ROOTFS_EXT2_BLOCKS)),0)
EXT2_OPTS += -b $(strip $(BR2_TARGET_ROOTFS_EXT2_BLOCKS))
endif

ifneq ($(strip $(BR2_TARGET_ROOTFS_EXT2_INODES)),0)
EXT2_OPTS += -i $(strip $(BR2_TARGET_ROOTFS_EXT2_INODES))
endif

ifneq ($(strip $(BR2_TARGET_ROOTFS_EXT2_RESBLKS)),0)
EXT2_OPTS += -r $(strip $(BR2_TARGET_ROOTFS_EXT2_RESBLKS))
endif

EXT2_BASE :=	$(subst ",,$(BR2_TARGET_ROOTFS_EXT2_OUTPUT))

SUFFIX=.img #.glz
ifeq ($(strip $(BR2_TARGET_ROOTFS_EXT2_LZMA)),y)
EXT2_TARGET := $(EXT2_BASE)$(SUFFIX)
else
ifeq ($(strip $(BR2_TARGET_ROOTFS_EXT2_GZ)),y)
EXT2_TARGET := $(EXT2_BASE)$(SUFFIX)
else
EXT2_TARGET := $(EXT2_BASE)
endif
endif


ifdef USE_RAMDISK_H
RAMDISK_H= $(BR2_KERNEL_PATH)/include/linux/ramdisk.h
endif

# dependency on depmod finishing before creating target filesystem
# 
$(EXT2_BASE): $(TARGET_DIR) linux-kernel-depmod host-fakeroot makedevs $(BUILD_DIR)/fakeroot.env genext2fs
	-@find $(TARGET_DIR) -type f -perm +111 | xargs $(STRIP) 2>/dev/null || true;
	@rm -rf $(TARGET_DIR)/usr/man
	@rm -rf $(TARGET_DIR)/usr/share/man
	@rm -rf $(TARGET_DIR)/usr/info
	-/sbin/ldconfig -r $(TARGET_DIR) 2>/dev/null
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
	# Use fakeroot so genext2fs believes the previous fakery
ifeq ($(strip $(BR2_TARGET_ROOTFS_EXT2_FILELIST)),y)
	echo "Starting to generate file list ..."; \
	rm -rf $(TARGET_DIR)/file_list.txt; \
	pushd $(TARGET_DIR); \
	file_list_tmp=`mktemp -u`; \
	ls -1R > $$file_list_tmp; \
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
	done < $$file_list_tmp; \
	popd; \
	rm -rf $$file_list_tmp; \
	echo "List generated ...";
endif
ifeq ($(strip $(BR2_TARGET_ROOTFS_EXT2_BLOCKS)),0)
	#GENEXT2_REALSIZE=`LANG=C du -l -s -c -k $(TARGET_DIR) | grep total | sed -e "s/total//"`;
	GENEXT2_REALSIZE=`du -l -s -c -k $(TARGET_DIR) | tail -n1 | awk -F' ' '{ print $$1}'`; \
	GENEXT2_ADDTOROOTSIZE=`if [ $$GENEXT2_REALSIZE -ge 10000 ] ; then echo 16384; else echo 400; fi`; \
	GENEXT2_SIZE=`expr $$GENEXT2_REALSIZE + $$GENEXT2_ADDTOROOTSIZE`; \
	GENEXT2_ADDTOINODESIZE=`find $(TARGET_DIR) | wc -l`; \
	GENEXT2_INODES=`expr $$GENEXT2_ADDTOINODESIZE + 400`; \
	set -x; \
	$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
	$(GENEXT2_DIR)/genext2fs \
		-b $$GENEXT2_SIZE \
		-i $$GENEXT2_INODES \
		-d $(TARGET_DIR) \
		$(EXT2_OPTS) $(EXT2_BASE) ;
ifdef USE_RAMDISK_H
	chmod +w $(RAMDISK_H) ;\
	echo -e "#define ROOTFS_RAMDISK_SIZE ( $$GENEXT2_SIZE + 100 )" > $(RAMDISK_H)
endif
else
	$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
	$(GENEXT2_DIR)/genext2fs \
		-d $(TARGET_DIR) \
		$(EXT2_OPTS) \
		$(EXT2_BASE)
endif

$(EXT2_BASE)$(SUFFIX):: $(EXT2_BASE) lzma
ifeq ($(strip $(BR2_TARGET_ROOTFS_EXT2_LZMA)),y)
	$(STAGING_DIR)/bin/lzma e $(EXT2_BASE) $@ -d20
else
	@gzip --best -fv -S $(SUFFIX) $(EXT2_BASE) 
endif


EXT2_COPYTO := $(strip $(subst ",,$(BR2_TARGET_ROOTFS_EXT2_COPYTO)))

ext2root: $(EXT2_TARGET)
	@ls -l $(EXT2_TARGET)
ifneq ($(EXT2_COPYTO),)
	@echo "LISTD /tftpboot:["`ls /tftpboot`"]"
	@echo "LISTD /tftpboot/$(PROFILE):["`ls /tftpboot/$(PROFILE)`"]"
	mkdir -p $(EXT2_COPYTO)
	cp -f $(EXT2_TARGET) $(EXT2_COPYTO)
	@echo "LISTD /tftpboot:["`ls /tftpboot`"]"
	@echo "LISTD /tftpboot/$(PROFILE):["`ls /tftpboot/$(PROFILE)`"]"
	@echo "LISTD /tftpboot/$(PROFILE)/usbtool:["`ls /tftpboot/$(PROFILE)/usbtool`"]"
endif

ext2root-source: $(DL_DIR)/$(GENEXT2_SOURCE)

ext2root-clean:
	-$(MAKE) -C $(GENEXT2_DIR) clean

ext2root-dirclean:
	rm -rf $(GENEXT2_DIR)

.PHONY: $(EXT2_BASE)$(SUFFIX)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_TARGET_ROOTFS_EXT2)),y)
TARGETS+=ext2root
ROOTFS_TARGET = ext2root
ROOTFS_OBJS = $(EXT2_BASE) $(EXT2_TARGET)
ifneq ($(strip $(BR2_TARGET_DATAFS_JFFS2)),y)
TARGETS+=redboot-flash-info kernel-flash-info
endif
endif
