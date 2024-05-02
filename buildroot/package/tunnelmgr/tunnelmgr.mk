#############################################################
#
# TUNNELMGR
#
#############################################################

TUNNELMGR_VER           := v0.0.1
TUNNELMGR_SRC_DIR       := $(V54_DIR)/apps/tunnelmgr
TUNNELMGR_BIN_DIR       := $(BUILD_DIR)/tunnelmgr
TUNNELMGR_BINARY        := tunnelmgr
TUNNELMGR_TARGET_BINARY := $(TARGET_DIR)/usr/sbin/$(TUNNELMGR_BINARY)

TUNNELMGR_MAKE_OPT := -C $(TUNNELMGR_BIN_DIR) -f $(TUNNELMGR_SRC_DIR)/Makefile TUNNELMGR_BIN_DIR="$(TUNNELMGR_BIN_DIR)" TUNNELMGR_SRC_DIR="$(TUNNELMGR_SRC_DIR)"

TUNNELMGR_LDFLAGS     += $(DASHLRSM)
TUNNELMGR_CFLAGS      += $(TARGET_CFLAGS) $(DASHDRSM) $(DASHIRSM)
TUNNELMGR_CFLAGS      += -DIP_USE_STRING

ifeq ($(strip $(BR2_PACKAGE_OPENSSL)),y)
TUNNELMGR_CFLAGS  += -DV54_OPENSSL -I$(SSLINC)
TUNNELMGR_LDFLAGS += -lssl -lcrypto
endif

ifeq ($(strip $(BR2_PACKAGE_ZLIB)),y)
TUNNELMGR_CFLAGS  += -DV54_ZLIB
TUNNELMGR_LDFLAGS += -lz
endif

ifeq ($(strip $(BR2_KERNEL_HEADERS_2_6_32)),y)
TUNNELMGR_CFLAGS  += -DV54_KERNEL_2_6_32
endif

tunnelmgr: librsm openssl
	mkdir -p $(TUNNELMGR_BIN_DIR)
	$(MAKE) $(TUNNELMGR_MAKE_OPT) CC="$(TARGET_CC)" TUNNELMGR_CFLAGS="$(TUNNELMGR_CFLAGS)" TUNNELMGR_LDFLAGS="$(TUNNELMGR_LDFLAGS)"
	$(STRIP) $(TUNNELMGR_BIN_DIR)/$(TUNNELMGR_BINARY) -o $(TUNNELMGR_TARGET_BINARY)

tunnelmgr-clean:
	$(MAKE) $(TUNNELMGR_MAKE_OPT) clean

tunnelmgr-dirclean: tunnelmgr-clean
	rm -rf $(TUNNELMGR_BIN_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_TUNNELMGR)),y)
TARGETS+=tunnelmgr
endif
