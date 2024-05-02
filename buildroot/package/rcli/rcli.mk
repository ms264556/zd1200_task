#############################################################
#
#  Remote CLI
#
#############################################################

RCLI_VER            := v0.0.1
RCLI_SRC_DIR        := $(TOP_DIR)/../wsg/rkssrc/rcli
RCLI_BIN_DIR        := $(BUILD_DIR)/rcli
RCLI_BINARY         := remoteclid
RCLI_TARGET_BINARY  := $(TARGET_DIR)/usr/sbin/$(RCLI_BINARY)

RCLI_MAKE_OPT       := -C $(RCLI_BIN_DIR) -f $(RCLI_SRC_DIR)/Makefile
RCLI_CFLAGS         += -DREMOTECLID

$(RCLI_BIN_DIR)/.configure:
	mkdir $(RCLI_BIN_DIR)
	@touch $(RCLI_BIN_DIR)/.configure

rcli: $(RCLI_BIN_DIR)/.configure
	make $(RCLI_MAKE_OPT) \
	    CC="$(TARGET_CC)" \
	    STRIP="$(STRIP)" \
	    RCLI_BIN_DIR="$(RCLI_BIN_DIR)" \
	    RCLI_SRC_DIR="$(RCLI_SRC_DIR)" \
	    RCLI_CFLAGS="$(RCLI_CFLAGS)" \
	    RCLI_TARGET_BINARY="$(RCLI_BIN_DIR)/$(RCLI_BINARY)" \
	    $(RCLI_BINARY)
	$(STRIP) $(RCLI_BIN_DIR)/$(RCLI_BINARY) -o $(RCLI_TARGET_BINARY)

rcli-clean:
	make $(RCLI_MAKE_OPT) clean

rcli-dirclean: rcli-clean
	rm -rf $(RCLI_BIN_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_REMOTECLID)),y)
TARGETS+=rcli
endif
