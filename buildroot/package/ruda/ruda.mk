#############################################################
#
# RUDA
#
#############################################################

RUDA_BUILD_DIR         := $(BUILD_DIR)/ruda
RUDA_SRC_DIR           := $(TOPDIR)/../video54/apps/ruda
RUDA_TARGET_DIR        := $(TARGET_DIR)/usr/sbin

RUDA_BINARIES          := ruda


RUDA_CFLAGS += $(TARGET_CFLAGS) -Wall -fno-strict-aliasing
RUDA_CFLAGS += $(DASHDRSM) $(DASHIRSM) -DLINUX
RUDA_LDFLAGS += $(DASHLRSM)

RUDA_MAKE_OPT := -C $(RUDA_BUILD_DIR) -f $(V54_DIR)/apps/ruda/Makefile


ruda: librsm
	@mkdir -p $(RUDA_BUILD_DIR)
	make $(RUDA_MAKE_OPT) CC=$(TARGET_CC) RUDA_SRC_DIR=$(RUDA_SRC_DIR) \
		 RUDA_CFLAGS="$(RUDA_CFLAGS)" RUDA_LDFLAGS="$(RUDA_LDFLAGS)"
	@install -D $(foreach f,$(RUDA_BINARIES),$(RUDA_BUILD_DIR)/$f) $(RUDA_TARGET_DIR)
	@chmod a+x $(RUDA_TARGET_DIR)/$(RUDA_BINARIES)

ruda-clean:
	-$(MAKE) $(RUDA_MAKE_OPT) clean
	rm -f $(RUDA_TARGET_DIR)/$(RUDA_BINARIES)

ruda-dirclean:
	rm -rf $(RUDA_BUILD_DIR)


#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_RUDA)),y)
TARGETS+=ruda
endif
