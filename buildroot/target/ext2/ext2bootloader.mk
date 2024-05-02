#############################################################
#
# genext2fs to build to target ext2 filesystems
#
#############################################################
EXT2_BOOTLOADER_BASE :=	$(subst ",,$(BR2_TARGET_BOOTLOADER_EXT2_OUTPUT))

SUFFIX=.img #.glz
ifeq ($(strip $(BR2_TARGET_BOOTLOADER_EXT2_LZMA)),y)
EXT2_BOOTLOADER_TARGET := $(EXT2_BOOTLOADER_BASE)$(SUFFIX)
else
ifeq ($(strip $(BR2_TARGET_BOOTLOADER_EXT2_GZ)),y)
EXT2_BOOTLOADER_TARGET := $(EXT2_BOOTLOADER_BASE)$(SUFFIX)
else
EXT2_BOOTLOADER_TARGET := $(EXT2_BOOTLOADER_BASE)
endif
endif

# dependency on depmod finishing before creating target filesystem
# 
$(EXT2_BOOTLOADER_BASE): grub host-fakeroot $(BUILD_DIR)/fakeroot.env genext2fs
	# Use fakeroot to pretend all target binaries are owned by root
	-$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
		chown -R root:root $(GRUB_TARGET_DIR)
	# Use fakeroot so genext2fs believes the previous fakery
ifeq ($(strip $(BR2_TARGET_ROOTFS_EXT2_BLOCKS)),0)
	#GENEXT2_REALSIZE=`LANGUAGE=C du -l -s -c -k $(GRUB_TARGET_DIR) | grep total | sed -e "s/total//"`;
	GENEXT2_REALSIZE=`du -l -s -c -k $(GRUB_TARGET_DIR) | tail -n1 | awk -F' ' '{ print $$1}'`; \
	GENEXT2_ADDTOROOTSIZE=`if [ $$GENEXT2_REALSIZE -ge 20000 ] ; then echo 16384; else echo 400; fi`; \
	GENEXT2_SIZE=`expr $$GENEXT2_REALSIZE + $$GENEXT2_ADDTOROOTSIZE`; \
	GENEXT2_ADDTOINODESIZE=`find $(GRUB_TARGET_DIR) | wc -l`; \
	GENEXT2_INODES=`expr $$GENEXT2_ADDTOINODESIZE + 400`; \
	set -x; \
	$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
	$(GENEXT2_DIR)/genext2fs \
		-b $$GENEXT2_SIZE \
		-i $$GENEXT2_INODES \
		-d $(GRUB_TARGET_DIR) \
		$(EXT2_OPTS) $(EXT2_BOOTLOADER_BASE) ;
ifdef USE_RAMDISK_H
	chmod +w $(RAMDISK_H) ;\
	echo -e "#define ROOTFS_RAMDISK_SIZE ( $$GENEXT2_SIZE + 100 )" > $(RAMDISK_H)
endif
else
	$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
	$(GENEXT2_DIR)/genext2fs \
		-d $(GRUB_TARGET_DIR) \
		$(EXT2_OPTS) \
		$(EXT2_BOOTLOADER_BASE)
endif

$(EXT2_BOOTLOADER_BASE)$(SUFFIX):: $(EXT2_BOOTLOADER_BASE) lzma
ifeq ($(strip $(BR2_TARGET_BOOTLOADER_EXT2_LZMA)),y)
	$(STAGING_DIR)/bin/lzma e $(EXT2_BOOTLOADER_BASE) $@ -d20
else
	@gzip --best -fv -S $(SUFFIX) $(EXT2_BOOTLOADER_BASE) 
endif

EXT2_BOOTLOADER_COPYTO := $(strip $(subst ",,$(BR2_TARGET_BOOTLOADER_EXT2_COPYTO)))

ext2bootloader: $(EXT2_BOOTLOADER_TARGET)
	@echo "create ext2 for bootloader done"
ifneq ($(EXT2_BOOTLOADER_COPYTO),)
	@echo "LISTD /tftpboot:["`ls /tftpboot`"]"
	@echo "LISTD /tftpboot/$(PROFILE):["`ls /tftpboot/$(PROFILE)`"]"
	mkdir -p $(EXT2_BOOTLOADER_COPYTO)
	cp -f $(EXT2_BOOTLOADER_TARGET) $(EXT2_BOOTLOADER_COPYTO)
	@echo "LISTD /tftpboot:["`ls /tftpboot`"]"
	@echo "LISTD /tftpboot/$(PROFILE):["`ls /tftpboot/$(PROFILE)`"]"
	@echo "LISTD /tftpboot/$(PROFILE)/usbtool:["`ls /tftpboot/$(PROFILE)/usbtool`"]"
endif

.PHONY: $(EXT2_BOOTLOADER_BASE)$(SUFFIX)

ifeq ($(strip $(BR2_TARGET_BOOTLOADER_EXT2)),y)
TARGETS+=ext2bootloader
endif

