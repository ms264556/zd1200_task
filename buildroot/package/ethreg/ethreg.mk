#############################################################
#
# ethreg
#
#############################################################

ETHREG_BUILD_DIR      := $(BUILD_DIR)/ethreg
ETHREG_SRC_DIR        := $(V54_DIR)/apps/ethreg

ETHREG_BINARY         := ethreg
ETHREG_TARGET_BINARY  := $(TARGET_DIR)/usr/bin/ethreg

ETHREG_MAKE_OPT := -C $(ETHREG_BUILD_DIR) -f $(ETHREG_SRC_DIR)/Makefile

ETHREG_CFLAGS := $(TARGET_DEFINES) $(DASHDRSM) $(DASHIRSM)

ethreg_make:
	@mkdir -p $(ETHREG_BUILD_DIR)
	$(MAKE) $(ETHREG_MAKE_OPT) CC=$(TARGET_CC) ETHREG_LDFLAGS="$(DASHLRSM)" \
		ETHREG_CFLAGS="$(ETHREG_CFLAGS)" ETHREG_SRCDIR=$(ETHREG_SRC_DIR) V54_DIR=$(V54_DIR)

ethreg: ethreg_make
	install -D $(ETHREG_BUILD_DIR)/$(ETHREG_BINARY) $(ETHREG_TARGET_BINARY)

ethreg-clean:
	rm -f $(ETHREG_TARGET_BINARY)
	-$(MAKE) $(ETHREG_MAKE_OPT) clean
	-$(MAKE) -C $(ETHREG_SRC_DIR) clean

ethreg-dirclean: ethreg-clean
	rm -rf $(ETHREG_BUILD_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_ETHREG)),y)
TARGETS+=ethreg
endif
