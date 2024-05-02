#############################################################
#
# rpkitpm
#
#############################################################
TOPDIR=${shell pwd}
RPKITPM_SRC_DIR:=$(TOPDIR)/../apps/rpkitpm
RPKITPM_DIR:=$(BUILD_DIR)/rpkitpm
RPKITPM_CP:=cp -fau
RPKITPM_BINARY:=rpki-tpm-tool
RPKITPM_TARGET_BINARY:=usr/sbin/$(RPKITPM_BINARY)

TROUSERS_VER      := 0.3.11.2
TROUSERS_BINARY   := trousers
TROUSERS_DIR      := $(BUILD_DIR)/trousers-$(TROUSERS_VER)

$(RPKITPM_DIR)/.unpacked: $(RPKITPM_SRC_DIR)/Makefile
	-mkdir $(RPKITPM_DIR)
	cp -a $(RPKITPM_SRC_DIR)/* $(RPKITPM_DIR)
#	rm -f $(RPKITPM_DIR)/$(RPKITPM_BINARY)
	rm -f $(RPKITPM_DIR)/*.o
	touch $(RPKITPM_DIR)/.unpacked

$(RPKITPM_DIR)/$(RPKITPM_BINARY): $(RPKITPM_DIR)/.unpacked 
#	$(TARGET_CONFIGURE_OPTS) $(MAKE) TOPDIR=$(TOPDIR) CC=$(TARGET_CC) CFLAGS="-I$(TROUSERS_DIR)/src/include/ -g -c" LDFLAGS="-L$(TROUSERS_DIR)/src/tspi/.libs/ -lcrypto -ltspi" -C $(RPKITPM_DIR)

$(TARGET_DIR)/$(RPKITPM_TARGET_BINARY): $(RPKITPM_DIR)/$(RPKITPM_BINARY)
	install -D $(RPKITPM_DIR)/$(RPKITPM_BINARY) $(TARGET_DIR)/$(RPKITPM_TARGET_BINARY)

rpkitpm: uclibc trousers $(TARGET_DIR)/$(RPKITPM_TARGET_BINARY)

rpkitpm-clean:
	rm -f $(TARGET_DIR)/$(RPKITPM_TARGET_BINARY)
	rm -f $(RPKITPM_DIR)/*.o

rpkitpm-dirclean:
	rm -f $(TARGET_DIR)/$(RPKITPM_TARGET_BINARY)
	rm -rf $(RPKITPM_DIR)
#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_RPKITPM)),y)
TARGETS+=rpkitpm
endif
