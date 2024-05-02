#############################################################
#
# zip
#
#############################################################
ZIP_VER:=232
ZIP_SOURCE:=zip$(ZIP_VER).zip
ZIP_SITE:=http://www.info-zip.org/
ZIP_DIR:=$(BUILD_DIR)/zip$(ZIP_VER)
ZIP_BINARY:=zip
ZIP_TARGET_BINARY:=bin/zip

# zip should be under buildroot/dl/...
#$(DL_DIR)/$(ZIP_SOURCE):
#	 $(ZIP) -P $(DL_DIR) $(ZIP_SITE)/$(ZIP_SOURCE)

zip-source: $(DL_DIR)/$(ZIP_SOURCE)

$(ZIP_DIR)/.unpacked: $(DL_DIR)/$(ZIP_SOURCE)
	unzip $(DL_DIR)/$(ZIP_SOURCE) -d $(ZIP_DIR)
	toolchain/patch-kernel.sh $(ZIP_DIR) package/zip/ zip\*.patch
	touch $(ZIP_DIR)/.unpacked

$(ZIP_DIR)/$(ZIP_BINARY): $(ZIP_DIR)/.unpacked
	(cd $(ZIP_DIR); \
	$(MAKE) CC=$(TARGET_CROSS)gcc -f unix/Makefile zip \
	);

$(ZIP_DIR)/$(ZIP_TARGET_BINARY): $(ZIP_DIR)/$(ZIP_BINARY)
	install -D $(ZIP_DIR)/$(ZIP_BINARY) $(TARGET_DIR)/$(ZIP_TARGET_BINARY)

zip: uclibc $(ZIP_DIR)/$(ZIP_TARGET_BINARY)

zip-clean:
	$(MAKE) -f $(ZIP_DIR)/unix/Makefile clean

zip-dirclean:
	rm -rf $(ZIP_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_ZIP)),y)
TARGETS+=zip
endif
