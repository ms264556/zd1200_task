#############################################################
#
# to create iso installer 
#
#############################################################
# Where ths ISO installer output to.
ISO_INSTALLER_TARGET_DIR=$(strip $(subst ",, $(BR2_ISO_INSTALLER_TARGET_DIR)))
ISO_IMAGE_NAME=$(PROFILE)_$(BR_BUILD_VERSION).iso

#Points of USB installer output. ISO installer depends on USB tool
USB_TOOL_TARGET_DIR=$(strip $(subst ",, $(BR2_USB_TOOL_TARGET_DIR)))

isoinstaller: isoinstaller-source
	@echo generate iso installer to $(ISO_INSTALLER_TARGET_DIR)
	grub-mkrescue -o $(ISO_INSTALLER_TARGET_DIR)/$(ISO_IMAGE_NAME) /$(USB_TOOL_TARGET_DIR)
	cd $(ISO_INSTALLER_TARGET_DIR); ln -sf $(ISO_IMAGE_NAME) $(PROFILE).iso

isoinstaller-source:
	@echo copy iosinstaller source to directory $(USB_TOOL_TARGET_DIR)
	cp -af $(BASE_DIR)/profiles/$(PROFILE)/isoinstaller_skeleton/* /$(USB_TOOL_TARGET_DIR)

isoinstaller-clean:

isoinstaller-dirclean:

#############################################################
#
# Toplevel Makefile options
#
#############################################################
# isoinstaller is checked in usbtool
#ifeq ($(strip $(BR2_TARGET_ISO_INSTALLER)),y)
#TARGETS+=usbinitramfs isoinstaller
#endif
