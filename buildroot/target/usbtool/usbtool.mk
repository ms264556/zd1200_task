#############################################################
#
# to create the initramfs (in cpio gz format) for usbtool kit 
#
#############################################################
USB_TOOL_TARGET_DIR=$(strip $(subst ",, $(BR2_USB_TOOL_TARGET_DIR)))

usbinitramfs: host-fakeroot makedevs $(BUILD_DIR)/fakeroot.env
	-@$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
		chown -R root:root $(TARGET_DIR)
	@$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
		$(STAGING_DIR)/bin/makedevs \
		-d $(TARGET_DEVICE_TABLE) \
		$(USB_TOOL_BUILD_DIR)
	-ln -f -s /sbin/init $(USB_TOOL_BUILD_DIR)/init
	cd $(USB_TOOL_BUILD_DIR) ; find . | $(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
		cpio -o -H newc | gzip > /$(USB_TOOL_TARGET_DIR)/usbinitramfs.gz 
	cp -f $(BASE_DIR)/profiles/$(PROFILE)/usb_install.sh /$(USB_TOOL_TARGET_DIR)
	@echo TFTPBOOT=$(TFTPBOOT)
	cp -f $(BUILD_DIR)/kernel/arch/$(ARCH)/boot/bzImage $(TFTPBOOT)/$(PROFILE)/usbtool/bzImage
	chmod 777 $(TFTPBOOT)/$(PROFILE)/usbtool/bzImage 

usbinitramfs-source:

usbinitramfs-clean:

usbinitramfs-dirclean:

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_TARGET_USBTOOL_KIT)),y)
TARGETS+=usbinitramfs
endif
ifeq ($(strip $(BR2_TARGET_ISO_INSTALLER)),y)
TARGETS+=isoinstaller
endif

