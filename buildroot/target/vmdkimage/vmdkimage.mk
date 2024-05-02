#############################################################
#
# to generate VMDK disk image 
#
#############################################################
VMDK_INSTALLER_TARGET_DIR=$(strip $(subst ",, $(BR2_VMDK_IMAGE_TARGET_DIR)))
VMDK_IMAGE_SIZE=$(strip $(subst ",, $(BR2_VMDK_IMAGE_SIZE)))
VMDK_IMAGE_NAME=$(PROFILE)_$(BR_BUILD_VERSION).vmdk

#How to find ISO images, VMDK depends on ISO installer
ISO_INSTALLER_TARGET_DIR=$(strip $(subst ",, $(BR2_ISO_INSTALLER_TARGET_DIR)))
ISO_INSTALLER_NAME=$(PROFILE).iso

vmdkimage: vmdkimage-source
	@echo install $(ISO_INSTALLER_NAME) into VMDK disk image $(VMDK_IMAGE_NAME)
	@echo Please wait, it might take times....
	qemu-system-i386 -hda $(VMDK_INSTALLER_TARGET_DIR)/$(VMDK_IMAGE_NAME) \
		-cdrom $(ISO_INSTALLER_TARGET_DIR)/$(ISO_INSTALLER_NAME) \
		-boot d -net none -m 128 -smp 2 -no-reboot -display none
	cd $(VMDK_INSTALLER_TARGET_DIR); ln -sf $(VMDK_IMAGE_NAME) $(PROFILE).vmdk

vmdkimage-source:
	@echo create VMDK disk image file $(VMDK_INSTALLER_TARGET_DIR)/$(VMDK_IMAGE_NAME), size $(VMDK_IMAGE_SIZE)
	qemu-img create -f vmdk $(VMDK_INSTALLER_TARGET_DIR)/$(VMDK_IMAGE_NAME) $(VMDK_IMAGE_SIZE)

vmdkimage-clean:

vmdkimage-dirclean:

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_TARGET_VMDK_IMAGE)),y)
TARGETS+=vmdkimage
endif
