#############################################################
#
# rksblistd
#
#############################################################

RKSBLISTD_BUILD_DIR      := $(BUILD_DIR)/rksblistd
RKSBLISTD_SRC_DIR        := $(V54_DIR)/apps/rksblistd

RKSBLISTD_BINARY         := rksblistd
RKSBLISTD_TARGET_BINARY := $(TARGET_DIR)/usr/sbin/$(RKSBLISTD_BINARY)

RKSBLISTD_MAKE_OPT := -C $(RKSBLISTD_BUILD_DIR) -f $(RKSBLISTD_SRC_DIR)/Makefile

RKSBLISTD_CFLAGS  = $(TARGET_DEFINES) $(DASHDRSM) $(DASHIRSM)
RKSBLISTD_LDFLAGS = $(DASHLRSM)


rksblistd: librsm
	@mkdir -p $(RKSBLISTD_BUILD_DIR)
	$(MAKE) $(RKSBLISTD_MAKE_OPT) CC=$(TARGET_CC) RKSBLISTD_LDFLAGS="$(DASHLRSM)" \
		RKSBLISTD_CFLAGS="$(RKSBLISTD_CFLAGS)" RKSBLISTD_SRCDIR=$(RKSBLISTD_SRC_DIR)
	install -D $(RKSBLISTD_BUILD_DIR)/$(RKSBLISTD_BINARY) $(RKSBLISTD_TARGET_BINARY)
	$(STRIP) $(RKSBLISTD_TARGET_BINARY)

rksblistd-clean:
	rm -f $(RKSBLISTD_TARGET_BINARY)
	-$(MAKE) $(RKSBLISTD_MAKE_OPT) clean
	-$(MAKE) -C $(RKSBLISTD_SRC_DIR) clean

rksblistd-dirclean: rksblistd-clean
	rm -rf $(RKSBLISTD_BUILD_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_RKSBLISTD)),y)
TARGETS+=rksblistd
endif
