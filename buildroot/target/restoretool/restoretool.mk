###################################################################
#
# to create the initramfs (in cpio gz format) for restore tool kit 
#
###################################################################
RESTORE_TOOL_TARGET_DIR=$(strip $(subst ",, $(BR2_RESTORE_TOOL_TARGET_DIR)))
RESTORE_TOOL_VER=$(strip $(subst ",, $(BR2_RESTORE_TOOL_TARGET_VER)))


restoreinitramfs-prepare: host-fakeroot $(BUILD_DIR)/fakeroot.env usbinitramfs
	mkdir -p $(RESTORE_TOOL_TARGET_DIR)
	$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
		cp -af $(USB_TOOL_BUILD_DIR)/* $(RESTORE_TOOL_BUILD_DIR)
	$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
		cp -af $(RESTORE_TOOL_BUILD_DIR)/../../../../profiles/$(PROFILE)/restoretool_skeleton/* $(RESTORE_TOOL_BUILD_DIR)

restoreinitramfs: restoreinitramfs-prepare #openssl director-package
	cd $(RESTORE_TOOL_BUILD_DIR) ; find . | $(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
		cpio -o -H newc | gzip -9 > /$(RESTORE_TOOL_TARGET_DIR)/restoreinitramfs.gz
	$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
		echo $(RESTORE_TOOL_VER) > /$(RESTORE_TOOL_TARGET_DIR)/restoreinitramfs.ver
ifneq ($(strip $(USB_TOOL_BUILD_DIR)),)
	cp -f /$(RESTORE_TOOL_TARGET_DIR)/restoreinitramfs.* $(USB_TOOL_TARGET_DIR)/
endif
	echo "Build system restore tool done"

restoreinitramfs-source:

restoreinitramfs-clean:

restoreinitramfs-dirclean:

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_TARGET_RESTORETOOL_KIT)),y)
TARGETS+=restoreinitramfs
endif
