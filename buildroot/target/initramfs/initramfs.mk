#############################################################
#
# to create the initramfs (in cpio gz format) for boot
#
#############################################################

rootinitramfs: host-fakeroot makedevs $(BUILD_DIR)/fakeroot.env

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
	-ln -f -s /sbin/init $(TARGET_DIR)/init
	cd $(TARGET_DIR) ; find . | $(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
		cpio -o -H newc | gzip > $(BR2_KERNEL_COPYTO)/initramfs.gz 

	cp $(BUILD_DIR)/kernel/arch/$(ARCH)/boot/bzImage $(BR2_KERNEL_COPYTO)/bzImage
	chmod 777 $(BR2_KERNEL_COPYTO)/bzImage $(BR2_KERNEL_COPYTO)/initramfs.gz

rootinitramfs-source:

rootinitramfs-clean:

rootinitramfs-dirclean:

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_TARGET_ROOTFS_INITRAMFS)),y)
TARGETS+=rootinitramfs
endif
