#############################################################
#
# tar to archive target filesystem
#
#############################################################
TARGET_DATAFS := $(BUILD_DIR)/data
TAR_DATAFS_OUTPUT := $(strip $(BR2_TARGET_DATAFS_TAR_OUTPUT))
TAR_DATAFS_OPTS := $(strip $(substr ",,$(BR2_TARGET_DATAFS_TAR_OPTIONS)))

SUFFIX:=.img

TARDATAFS_COPYTO := $(strip $(subst ",,$(BR2_TARGET_DATAFS_TAR_COPYTO)))

#ifeq ($(strip $(BR2_TARGET_DATAFS_TAR_APIMG)),y)
#APIMG_OPTS=director 
#endif

tardatafs: host-fakeroot makedevs $(BUILD_DIR)/fakeroot.env #$(APIMG_OPTS)
	@echo "TARGET=$(TAR_DATAFS_OUTPUT)"
	@echo "TARGET_DATA_DIR=$(TARGET_DATAFS)"
	-mkdir -p $(TARGET_DATAFS)/data
	-ln -s /tmp/non_persist $(TARGET_DATAFS)/non_persist
	-mkdir -p $(TARGET_DATAFS)/etc/
	-mkdir -p $(TARGET_DATAFS)/certs/
	-mkdir -p $(TARGET_DATAFS)/certs/client-cert/
	-mkdir -p $(TARGET_DATAFS)/certs/ca-cert/
ifeq ($(strip $(BR2_TARGET_DATAFS_TAR_APIMG)),y)
	-mkdir -p $(TARGET_DATAFS)/etc/airespider-images
	-mkdir -p $(TARGET_DATAFS)/etc/airespider-images/firmwares
	-cp -a $(strip $(subst ",,$(BR2_TARGET_DATAFS_TAR_APIMG_SOURCE)))/* \
		$(TARGET_DATAFS)/etc/airespider-images/firmwares
endif

ifeq ($(strip $(BR2_TARGET_DATAFS_TAR_ROOT)),y)
	-@$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
		chown -R root:root $(TARGET_DATAFS)
endif
	$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
		tar -c$(TAR_DATAFS_OPTS)f $(TAR_DATAFS_OUTPUT)$(SUFFIX) -C $(TARGET_DATAFS) .
ifneq ($(TARDATAFS_COPYTO),)
	@echo "LISTD /tftpboot:["`ls /tftpboot`"]"
	@echo "LISTD /tftpboot/$(PROFILE):["`ls /tftpboot/$(PROFILE)`"]"
	mkdir -p $(TARDATAFS_COPYTO)
	cp -f $(TAR_DATAFS_OUTPUT)$(SUFFIX) $(TARDATAFS_COPYTO)
	@echo "LISTD /tftpboot:["`ls /tftpboot`"]"
	@echo "LISTD /tftpboot/$(PROFILE):["`ls /tftpboot/$(PROFILE)`"]"
	@echo "LISTD /tftpboot/$(PROFILE):["`ls /tftpboot/$(PROFILE)/usbtool`"]"
endif

tardatafs-source:

tardatafs-clean:

tardatafs-dirclean:

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_TARGET_DATAFS_TAR)),y)
TARGETS+=tardatafs
endif
