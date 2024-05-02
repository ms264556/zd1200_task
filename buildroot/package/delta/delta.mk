#############################################################
#
# delta
#
#############################################################

DELTA_BUILD_DIR     := $(BUILD_DIR)/delta
DELTA_SRC_DIR       := $(V54_DIR)/apps/delta
DELTA_TARGET_DIR    := $(TARGET_DIR)/usr/sbin

DELTA_BINARY        := delta
DELTA_BUILD_BINARY  := $(DELTA_BUILD_DIR)/$(DELTA_BINARY)
DELTA_TARGET_BINARY := $(DELTA_TARGET_DIR)/$(DELTA_BINARY)


DELTA_MAKE_OPT := -C $(DELTA_BUILD_DIR) -f $(DELTA_SRC_DIR)/Makefile.br

DELTA_CFLAGS = $(TARGET_CFLAGS) -MMD
DELTA_LDFLAGS = $(filter-out -lnl,$(filter-out %rsm,$(DASHLRSM)))


delta: uclibc libRutil bsp
	@mkdir -p $(DELTA_BUILD_DIR)
	$(MAKE) $(DELTA_MAKE_OPT) CC=$(TARGET_CC) DELTA_SRC_DIR=$(DELTA_SRC_DIR) \
		DELTA_CFLAGS="$(DELTA_CFLAGS)" DELTA_LDFLAGS="$(DELTA_LDFLAGS)"
	install -D $(DELTA_BUILD_BINARY) $(DELTA_TARGET_BINARY)

delta-clean:
	rm -f $(DELTA_BUILD_BINARY) $(DELTA_TARGET_BINARY)
	-$(MAKE) $(DELTA_MAKE_OPT) clean

delta-dirclean:
	rm -rf $(DELTA_BUILD_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_DELTA_MAC)),y)
TARGETS+=delta
endif
