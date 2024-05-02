#############################################################
#
# wget
#
#############################################################
#GKERMIT_VER:=1.9.1
GKERMIT_VER:=100
GKERMIT_SOURCE:=gku$(GKERMIT_VER).tar.gz
GKERMIT_SITE:=ftp://kermit.columbia.edu/kermit/archives/$(GKERMIT_SOURCE)
GKERMIT_DIR:=$(BUILD_DIR)/gku$(GKERMIT_VER)
GKERMIT_CAT:=zcat
GKERMIT_BINARY:=gkermit
GKERMIT_TARGET_BINARY:=bin/$(GKERMIT_BINARY)

$(GKERMIT_DIR)/.unpacked: $(DL_DIR)/$(GKERMIT_SOURCE)
	mkdir $(GKERMIT_DIR)
	$(GKERMIT_CAT) $(DL_DIR)/$(GKERMIT_SOURCE) | tar -C $(GKERMIT_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(GKERMIT_DIR) package/gkermit/ *.patch
	touch $(GKERMIT_DIR)/.unpacked

$(GKERMIT_DIR)/$(GKERMIT_BINARY): $(GKERMIT_DIR)/.unpacked
	$(TARGET_CONFIGURE_OPTS) $(MAKE) TARGET_CC=$(TARGET_CC) HOST_CC=gcc -C $(GKERMIT_DIR)

$(TARGET_DIR)/$(GKERMIT_TARGET_BINARY): $(GKERMIT_DIR)/$(GKERMIT_BINARY)
	install -D $(GKERMIT_DIR)/$(GKERMIT_BINARY) $(TARGET_DIR)/$(GKERMIT_TARGET_BINARY)
	$(TARGET_STRIP) $(TARGET_DIR)/$(GKERMIT_TARGET_BINARY)
ifeq ($(BR2_TARGET_USBTOOL_KIT),y)
	install -D $(GKERMIT_DIR)/$(GKERMIT_BINARY) $(USB_TOOL_BUILD_DIR)/$(GKERMIT_TARGET_BINARY)
	$(TARGET_STRIP) $(USB_TOOL_BUILD_DIR)/$(GKERMIT_TARGET_BINARY)
endif

gkermit: uclibc $(TARGET_DIR)/$(GKERMIT_TARGET_BINARY)

gkermit-clean:
	rm -f $(TARGET_DIR)/$(GKERMIT_TARGET_BINARY)
	-$(MAKE) -C $(GKERMIT_DIR) clean

gkermit-dirclean:
	rm -rf $(GKERMIT_DIR)
#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_GKERMIT)),y)
TARGETS+=gkermit
endif
