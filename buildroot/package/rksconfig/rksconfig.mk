#############################################################
#
# rksconfig
#
#############################################################

RKSCONF_BUILD_DIR       := ${BUILD_DIR}/rksconfig
RKSCONF_SRC_DIR         := $(V54_DIR)/apps/rksconfig

RKSCONF_BINARY          := rksconfig
RKSCONF_BUILD_BINARY    := $(RKSCONF_BUILD_DIR)/$(RKSCONF_BINARY)
RKSCONF_TARGET_BINARY   := $(TARGET_DIR)/usr/bin/$(RKSCONF_BINARY)

RKSCONF_CONFIG          := rksconfig.conf
RKSCONF_SRC_CONFIG      := $(RKSCONF_SRC_DIR)/$(RKSCONF_CONFIG)
RKSCONF_TARGET_CONFIG   := $(TARGET_DIR)/etc/$(RKSCONF_CONFIG)

RKSCONF_CFLAGS   += $(TARGET_CFLAGS) $(DASHDRSM)  $(DASHIRSM)
RKSCONF_LDFLAGS  += -lccl $(DASHLRSM)

RKSCONF_MAKE_OPT := -C $(RKSCONF_BUILD_DIR) -f $(RKSCONF_SRC_DIR)/Makefile.br

ifeq ($(strip ${BR2_NETGEAR_11N_PRODUCT}),y)
export RKS_NETGEAR_11N=1
RKSCONF_CFLAGS += -DRKS_NETGEAR_11N
endif


rksconf:          rksconfig

rksconf-clean:    rksconfig-clean

rksconf-dirclean: rksconfig-dirclean

rksconfig: libccl librsc
	@mkdir -p $(RKSCONF_BUILD_DIR) $(TARGET_DIR)/root/usr/lib $(RKSCONF_TARGET_CONF_DIR)
	$(MAKE) $(RKSCONF_MAKE_OPT) CC=$(TARGET_CC) RKSCONF_SRC_DIR=$(RKSCONF_SRC_DIR) \
		RKSCONF_CFLAGS="$(RKSCONF_CFLAGS)" RKSCONF_LDFLAGS="$(RKSCONF_LDFLAGS)"
	install -D $(RKSCONF_BUILD_BINARY) $(RKSCONF_TARGET_BINARY)
	install -D -m ugo+rw $(RKSCONF_SRC_CONFIG) $(RKSCONF_TARGET_CONFIG)

rksconfig-clean:
	@rm -f $(RKSCONF_TARGET_BINARY) $(RKSCONF_TARGET_CONFIG)
	-$(MAKE) $(RKSCONF_MAKE_OPT) clean

rksconfig-dirclean: rksconfig-clean
	rm -rf $(RKSCONF_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_RKSCONFIG)),y)
TARGETS+=rksconfig
endif

