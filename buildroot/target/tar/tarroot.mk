#############################################################
#
# tar to archive target filesystem
#
#############################################################

TAR_OPTS := $(strip $(BR2_TARGET_ROOTFS_TAR_OPTIONS))

tarroot: host-fakeroot makedevs $(BUILD_DIR)/fakeroot.env
	-@find $(TARGET_DIR) -type f -perm +111 | xargs $(STRIP) 2>/dev/null || true;
	@rm -rf $(TARGET_DIR)/usr/man
	@rm -rf $(TARGET_DIR)/usr/info
	-@/sbin/ldconfig -r $(TARGET_DIR) 2>/dev/null
	-@$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
		chown -R root:root $(TARGET_DIR)
	@$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
		$(STAGING_DIR)/bin/makedevs \
		-d $(TARGET_DEVICE_TABLE) \
		$(TARGET_DIR)
ifeq ($(strip $(PROFILE)),director-aussie)
	@echo "Long link causes Busybox unable to un-tar the file"
	rm -rf $(TARGET_DIR)/lib/modules/$(BR2_DEFAULT_KERNEL_HEADERS)/build
	rm -rf $(TARGET_DIR)/lib/modules/$(BR2_DEFAULT_KERNEL_HEADERS)/source
endif
	@$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
		tar -c$(TAR_OPTS)f $(IMAGE).tar -C $(TARGET_DIR) .

tarroot-source:

tarroot-clean:

tarroot-dirclean:

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_TARGET_ROOTFS_TAR)),y)
TARGETS+=tarroot
endif
