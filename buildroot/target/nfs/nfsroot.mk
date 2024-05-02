#############################################################
#
# tar to archive target filesystem
#
#############################################################

nfsroot: host-fakeroot makedevs $(STAGING_DIR)/fakeroot.env
	-@find $(TARGET_DIR) -type f -perm +111 | xargs $(STRIP) 2>/dev/null || true;
	@rm -rf $(TARGET_DIR)/usr/man
	@rm -rf $(TARGET_DIR)/usr/info
	-/sbin/ldconfig -r $(TARGET_DIR) 2>/dev/null
	# Use fakeroot to pretend all target binaries are owned by root
	-$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(STAGING_DIR)/fakeroot.env \
		-s $(STAGING_DIR)/fakeroot.env -- \
		chown -R root:root $(TARGET_DIR)
	# Use fakeroot to pretend to create all needed device nodes
	$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(STAGING_DIR)/fakeroot.env \
		-s $(STAGING_DIR)/fakeroot.env -- \
		$(STAGING_DIR)/bin/makedevs \
		-d $(TARGET_DEVICE_TABLE) \
		$(TARGET_DIR)
	# Use fakeroot so tar believes the previous fakery
	$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(STAGING_DIR)/fakeroot.env \
		-s $(STAGING_DIR)/fakeroot.env -- \
		tar -czf $(IMAGE).tgz -C $(TARGET_DIR) . ;
	sudo tar xzf $(IMAGE).tgz -C $(BR2_TARGET_ROOTFS_NFS_PATH)
	

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_TARGET_ROOTFS_NFS)),y)
TARGETS+=nfsroot
endif
