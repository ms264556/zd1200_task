#############################################################
#
# tool4tpm
#
#############################################################
TOPDIR=${shell pwd}
TOOL4TPM_DIR:=$(BUILD_DIR)/tool4tpm
TOOL4TPM_CP:=cp -fau
TOOL4TPM_BINARY:=Linux/tool4tpm
TOOL4TPM_SOURCE:=tool4tpm.tar.gz

TOOL4TPM_TARGET_BINARY:=usr/sbin/tool4tpm


$(TOOL4TPM_DIR)/.unpacked:
	zcat $(DL_DIR)/$(TOOL4TPM_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(TOOL4TPM_DIR) package/tool4tpm/ *tool4tpm.patch
	touch $@

$(TOOL4TPM_DIR)/$(TOOL4TPM_BINARY): $(TOOL4TPM_DIR)/.unpacked 
	$(TARGET_CONFIGURE_OPTS) $(MAKE) TOPDIR=$(TOPDIR) CC=$(TARGET_CC) CFLAGS="-DRCKS" -C $(TOOL4TPM_DIR)/Linux

$(TARGET_DIR)/$(TOOL4TPM_TARGET_BINARY): $(TOOL4TPM_DIR)/$(TOOL4TPM_BINARY)
	install -D $(TOOL4TPM_DIR)/$(TOOL4TPM_BINARY) $(TARGET_DIR)/$(TOOL4TPM_TARGET_BINARY)
	install -D $(TOOL4TPM_DIR)/Linux/tool4tpm.cfg $(TARGET_DIR)/usr/sbin/tool4tpm.cfg

tool4tpm: uclibc libRutil $(TARGET_DIR)/$(TOOL4TPM_TARGET_BINARY)

tool4tpm-clean:
	rm -f $(TARGET_DIR)/$(TOOL4TPM_TARGET_BINARY)
	rm -f $(TOOL4TPM_DIR)/Linux/*.o

tool4tpm-dirclean:
	rm -rf $(TOOL4TPM_DIR)
#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_TOOL4TPM)),y)
TARGETS+=tool4tpm
endif
