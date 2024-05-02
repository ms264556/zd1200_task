#############################################################
#
# tpm-tools
#
#############################################################
TPM_VER      := 1.3.8
TPM_BINARY   := tpm-tools
TPM_DIR      := $(BUILD_DIR)/tpm-tools-$(TPM_VER)
TPM_SOURCE   := tpm-tools-$(TPM_VER).tar.gz

TROUSERS_VER      := 0.3.11.2
TROUSERS_BINARY   := trousers
TROUSERS_DIR      := $(BUILD_DIR)/trousers-$(TROUSERS_VER)

# Fix kernel cross compile, source dir, readline include path
TPM_MAKEOPTS += CROSS_COMPILE=$(TARGET_CROSS)
TPM_MAKEOPTS += DESTDIR=$(TARGET_DIR)

$(TPM_DIR)/.unpacked:
	zcat $(DL_DIR)/$(TPM_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(TPM_DIR) package/tpm-tools/ *tpm-tools.patch
	touch $@

$(TPM_DIR)/.configured: $(TPM_DIR)/.unpacked
	(cd $(TPM_DIR); \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		./configure LDFLAGS=-L$(TROUSERS_DIR)/src/tspi/.libs/ CFLAGS="-I$(TROUSERS_DIR)/src/include/ -I$(BUILD_DIR)../../../tools/2.6.32.24_gcc4.3.5/include/" \
		--host=$(GNU_TARGET_NAME) \
	);
	touch $@

$(TPM_DIR)/$(TPM_BINARY): $(TPM_DIR)/.configured
	$(MAKE) CPPFLAGS="-I$(BUILD_INCLUDE)" -C $(TPM_DIR) $(TPM_MAKEOPTS)

$(TARGET_DIR)/$(TPM_BINARY): $(TPM_DIR)/$(TPM_BINARY)
	cp $(TPM_DIR)/src/tpm_mgmt/.libs/tpm_changeownerauth    ${TARGET_DIR}/usr/sbin
	cp $(TPM_DIR)/src/tpm_mgmt/.libs/tpm_clear              ${TARGET_DIR}/usr/sbin
	cp $(TPM_DIR)/src/tpm_mgmt/.libs/tpm_createek           ${TARGET_DIR}/usr/sbin
	cp $(TPM_DIR)/src/tpm_mgmt/.libs/tpm_getpubek           ${TARGET_DIR}/usr/sbin
	cp $(TPM_DIR)/src/tpm_mgmt/.libs/tpm_nvdefine           ${TARGET_DIR}/usr/sbin
	cp $(TPM_DIR)/src/tpm_mgmt/.libs/tpm_nvinfo             ${TARGET_DIR}/usr/sbin
	cp $(TPM_DIR)/src/tpm_mgmt/.libs/tpm_nvread             ${TARGET_DIR}/usr/sbin
	cp $(TPM_DIR)/src/tpm_mgmt/.libs/tpm_nvrelease          ${TARGET_DIR}/usr/sbin
	cp $(TPM_DIR)/src/tpm_mgmt/.libs/tpm_nvwrite            ${TARGET_DIR}/usr/sbin
#	cp $(TPM_DIR)/src/tpm_mgmt/.libs/tpm_reset              ${TARGET_DIR}/usr/sbin
	cp $(TPM_DIR)/src/tpm_mgmt/.libs/tpm_resetdalock        ${TARGET_DIR}/usr/sbin
	cp $(TPM_DIR)/src/tpm_mgmt/.libs/tpm_restrictpubek      ${TARGET_DIR}/usr/sbin
	cp $(TPM_DIR)/src/tpm_mgmt/.libs/tpm_restrictsrk        ${TARGET_DIR}/usr/sbin
	cp $(TPM_DIR)/src/tpm_mgmt/.libs/tpm_revokeek           ${TARGET_DIR}/usr/sbin
	cp $(TPM_DIR)/src/tpm_mgmt/.libs/tpm_selftest           ${TARGET_DIR}/usr/sbin
	cp $(TPM_DIR)/src/tpm_mgmt/.libs/tpm_setactive          ${TARGET_DIR}/usr/sbin
	cp $(TPM_DIR)/src/tpm_mgmt/.libs/tpm_setclearable       ${TARGET_DIR}/usr/sbin
	cp $(TPM_DIR)/src/tpm_mgmt/.libs/tpm_setenable          ${TARGET_DIR}/usr/sbin
	cp $(TPM_DIR)/src/tpm_mgmt/.libs/tpm_setoperatorauth    ${TARGET_DIR}/usr/sbin
	cp $(TPM_DIR)/src/tpm_mgmt/.libs/tpm_setownable         ${TARGET_DIR}/usr/sbin
	cp $(TPM_DIR)/src/tpm_mgmt/.libs/tpm_setpresence        ${TARGET_DIR}/usr/sbin
#	cp $(TPM_DIR)/src/tpm_mgmt/.libs/tpm_startup            ${TARGET_DIR}/usr/sbin
	cp $(TPM_DIR)/src/tpm_mgmt/.libs/tpm_takeownership      ${TARGET_DIR}/usr/sbin
	cp $(TPM_DIR)/src/tpm_mgmt/.libs/tpm_version            ${TARGET_DIR}/usr/sbin
	cp $(TPM_DIR)/src/cmds/.libs/tpm_sealdata               ${TARGET_DIR}/usr/bin
	cp $(TPM_DIR)/src/cmds/.libs/tpm_unsealdata             ${TARGET_DIR}/usr/bin




tpm-tools: uclibc trousers $(TARGET_DIR)/$(TPM_BINARY)

tpm-tools-clean:
	rm -f $(TARGET_DIR)/usr/sbin/tpm-tools
	-$(MAKE) -C $(TPM_DIR) clean

tpm-tools-dirclean:
	if [ -h $(TARGET_CROSS)install ]; then rm -f $(TARGET_CROSS)install; fi
	rm -rf $(TPM_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_TPM_TOOLS)),y)
TARGETS+=tpm-tools
endif
