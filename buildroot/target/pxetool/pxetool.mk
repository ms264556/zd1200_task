###################################################################
#
# to create the initramfs (in cpio gz format) for pxe tool kit 
#
###################################################################
PXE_TOOL_TARGET_DIR=$(strip $(subst ",, $(BR2_PXE_TOOL_TARGET_DIR)))
PXE_TOOL_VER=$(strip $(subst ",, $(BR2_PXE_TOOL_TARGET_VER)))


pxeinitramfs-prepare: host-fakeroot $(BUILD_DIR)/fakeroot.env ext2root ext2bootloader usbinitramfs restoreinitramfs tardatafs
	mkdir -p $(PXE_TOOL_TARGET_DIR)
	@echo  $(PXE_TOOL_BUILD_DIR)
	$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
		cp -af $(USB_TOOL_BUILD_DIR)/* $(PXE_TOOL_BUILD_DIR)
	$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
		cp -af $(TOPDIR)/profiles/$(PROFILE)/pxetool_skeleton/* $(PXE_TOOL_BUILD_DIR)
	$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
		cp -f $(TFTPBOOT)/$(PROFILE)/usbtool/*fs*.img $(PXE_TOOL_BUILD_DIR)/mnt/
	$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
		cp -f $(TFTPBOOT)/$(PROFILE)/usbtool/bzImage $(PXE_TOOL_BUILD_DIR)/mnt/
	$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
		cp -f $(TFTPBOOT)/$(PROFILE)/usbtool/restoreinitramfs.* $(PXE_TOOL_BUILD_DIR)/mnt/

pxeinitramfs: pxeinitramfs-prepare director-package
	cd $(PXE_TOOL_BUILD_DIR) ; find . | $(STAGING_DIR)/usr/bin/fakeroot \
		-i $(BUILD_DIR)/fakeroot.env \
		-s $(BUILD_DIR)/fakeroot.env -- \
		cpio -o -H newc | gzip -9 > /$(PXE_TOOL_TARGET_DIR)/pxeinitramfs.gz
ifneq ($(strip $(USB_TOOL_TARGET_DIR)),$(strip $(PXE_TOOL_TARGET_DIR)))
	cp -f /$(PXE_TOOL_TARGET_DIR)/pxeinitramfs.gz $(USB_TOOL_TARGET_DIR)
endif
	echo "Build system pxe tool done"

pxeinitramfs-source:

pxeinitramfs-clean:

pxeinitramfs-dirclean:

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_TARGET_PXETOOL_KIT)),y)
TARGETS+=pxeinitramfs
endif
