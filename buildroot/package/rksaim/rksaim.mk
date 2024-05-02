#############################################################
#
# rksaim
#
#############################################################

RKSAIM_BUILD_DIR      := $(BUILD_DIR)/rksaim
RKSAIM_SRC_DIR        := $(V54_DIR)/apps/rksaim

RKSAIM_BINARY         := rksaim
RKSAIMD_BINARY        := rksaimd
RKSAIM_TARGET_BINARY  := $(TARGET_DIR)/usr/bin/rksaim
RKSAIMD_TARGET_BINARY := $(TARGET_DIR)/usr/sbin/$(RKSAIMD_BINARY)

RKSAIM_MAKE_OPT := -C $(RKSAIM_BUILD_DIR) -f $(RKSAIM_SRC_DIR)/Makefile

RKSAIM_CFLAGS := $(TARGET_DEFINES) $(DASHDRSM) $(DASHIRSM)

rksaim_make: libnl
	@mkdir -p $(RKSAIM_BUILD_DIR)
	$(MAKE) $(RKSAIM_MAKE_OPT) CC=$(TARGET_CC) RKSAIM_LDFLAGS="$(DASHLRSM)" \
		RKSAIM_CFLAGS="$(RKSAIM_CFLAGS)" RKSAIM_SRCDIR=$(RKSAIM_SRC_DIR) V54_DIR=$(V54_DIR)

rksaim: uclibc bsp libRutil librsm rksaim_make
	install -D $(RKSAIM_BUILD_DIR)/$(RKSAIM_BINARY) $(RKSAIMD_TARGET_BINARY)
	ln -fs ../sbin/$(RKSAIMD_BINARY) $(RKSAIM_TARGET_BINARY)

rksaim-clean:
	rm -f $(RKSAIM_TARGET_BINARY) $(RKSAIMD_TARGET_BINARY)
	-$(MAKE) $(RKSAIM_MAKE_OPT) clean
	-$(MAKE) -C $(RKSAIM_SRC_DIR) clean

rksaim-dirclean: rksaim-clean
	rm -rf $(RKSAIM_BUILD_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_RKSAIM)),y)
TARGETS+=rksaim
endif
