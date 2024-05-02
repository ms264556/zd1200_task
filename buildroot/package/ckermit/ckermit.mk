#############################################################
#
# wget
#
#############################################################
#CKERMIT_VER:=1.9.1
CKERMIT_VER:=211
CKERMIT_SOURCE:=cku$(CKERMIT_VER).tar.gz
CKERMIT_SITE:=ftp://kermit.columbia.edu/kermit/archives/$(CKERMIT_SOURCE)
CKERMIT_DIR:=$(BUILD_DIR)/cku$(CKERMIT_VER)
CKERMIT_CAT:=zcat
CKERMIT_BINARY:=wermit
CKERMIT_TARGET_BINARY:=bin/wermit

$(CKERMIT_DIR)/.unpacked: $(DL_DIR)/$(CKERMIT_SOURCE)
	mkdir $(CKERMIT_DIR)
	$(CKERMIT_CAT) $(DL_DIR)/$(CKERMIT_SOURCE) | tar -C $(CKERMIT_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(CKERMIT_DIR) package/ckermit/ *.patch
	touch $(CKERMIT_DIR)/.unpacked

$(CKERMIT_DIR)/$(CKERMIT_BINARY): $(CKERMIT_DIR)/.unpacked
	$(TARGET_CONFIGURE_OPTS) $(MAKE) CC=$(TARGET_CC) CC2=$(TARGET_CC) -C $(CKERMIT_DIR) linuxrks

$(TARGET_DIR)/$(CKERMIT_TARGET_BINARY): $(CKERMIT_DIR)/$(CKERMIT_BINARY)
	install -D $(CKERMIT_DIR)/$(CKERMIT_BINARY) $(TARGET_DIR)/$(CKERMIT_TARGET_BINARY)
	$(TARGET_STRIP) $(TARGET_DIR)/$(CKERMIT_TARGET_BINARY)

ckermit: uclibc $(TARGET_DIR)/$(CKERMIT_TARGET_BINARY)

ckermit-clean:
	rm -f $(TARGET_DIR)/$(CKERMIT_TARGET_BINARY)
	-$(MAKE) -C $(CKERMIT_DIR) clean

ckermit-dirclean:
	rm -rf $(CKERMIT_DIR)
#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_CKERMIT)),y)
TARGETS+=ckermit
endif
