#############################################################
#
# scand
#
#############################################################

SCAND_BUILD_DIR      := $(BUILD_DIR)/scand
SCAND_SRC_DIR        := $(V54_DIR)/apps/scand

SCAND_BINARY         := scand
SCAND_TARGET_BINARY  := $(TARGET_DIR)/usr/sbin/$(SCAND_BINARY)

SCAND_MAKE_OPT := -C $(SCAND_BUILD_DIR) -f $(SCAND_SRC_DIR)/Makefile

SCAND_CFLAGS := $(TARGET_CFLAGS) $(DASHDRSM) $(DASHIRSM)

scand_make:
ifneq ($(MAKE_EXPRESS),2)
	@mkdir -p $(SCAND_BUILD_DIR)
	@echo "DSD_CFLAGS=$(XSCAND_CFLAGS)"
	$(MAKE) $(SCAND_MAKE_OPT) CC=$(TARGET_CC) SCAND_LDFLAGS="$(DASHLRSM)" \
		SCAND_CFLAGS="$(SCAND_CFLAGS)" SCAND_SRCDIR=$(SCAND_SRC_DIR)
endif

scand: libRutil librsm scand_make
	install -D $(SCAND_BUILD_DIR)/$(SCAND_BINARY) $(SCAND_TARGET_BINARY)
	$(STRIP) $(SCAND_TARGET_BINARY)

scand-clean:
	rm -f $(SCAND_TARGET_BINARY)
	-$(MAKE) $(SCAND_MAKE_OPT) clean
	-$(MAKE) -C $(SCAND_SRC_DIR) clean

scand-dirclean: scand-clean
	rm -rf $(SCAND_BUILD_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_SCAND)),y)
TARGETS += scand
endif
