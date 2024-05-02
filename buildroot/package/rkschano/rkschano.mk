#############################################################
#
# rkschano
#
#############################################################

RKSCHANO_BUILD_DIR      := $(BUILD_DIR)/rkschano
RKSCHANO_SRC_DIR        := $(V54_DIR)/apps/rkschano

RKSCHANO_BINARY         := rkschano
RKSCHANOD_BINARY        := rkschanod
RKSCHANO_TARGET_BINARY  := $(TARGET_DIR)/usr/bin/rkschano
RKSCHANOD_TARGET_BINARY := $(TARGET_DIR)/usr/sbin/$(RKSCHANOD_BINARY)

RKSCHANO_MAKE_OPT := -C $(RKSCHANO_BUILD_DIR) -f $(RKSCHANO_SRC_DIR)/Makefile

RKSCHANO_CFLAGS := $(TARGET_DEFINES) $(DASHDRSM) $(DASHIRSM)

rkschano_make:
	@mkdir -p $(RKSCHANO_BUILD_DIR)
	$(MAKE) $(RKSCHANO_MAKE_OPT) CC=$(TARGET_CC) RKSCHANO_LDFLAGS="$(DASHLRSM)" \
		RKSCHANO_CFLAGS="$(RKSCHANO_CFLAGS)" RKSCHANO_SRCDIR=$(RKSCHANO_SRC_DIR) V54_DIR=$(V54_DIR)

rkschano: uclibc bsp libRutil librsm rkschano_make
	install -D $(RKSCHANO_BUILD_DIR)/$(RKSCHANO_BINARY) $(RKSCHANOD_TARGET_BINARY)
	ln -fs ../sbin/$(RKSCHANOD_BINARY) $(RKSCHANO_TARGET_BINARY)

rkschano-clean:
	rm -f $(RKSCHANO_TARGET_BINARY) $(RKSCHANOD_TARGET_BINARY)
	-$(MAKE) $(RKSCHANO_MAKE_OPT) clean
	-$(MAKE) -C $(RKSCHANO_SRC_DIR) clean

rkschano-dirclean: rkschano-clean
	rm -rf $(RKSCHANO_BUILD_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_RKSCHANO)),y)
TARGETS+=rkschano
endif
