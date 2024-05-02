#############################################################
#
# APMgr
#
#############################################################

ifeq ($(strip $(BR2_PACKAGE_BRIDGE_DHCP_HOOK)),y)
LIBRSM_CFLAGS += -DV54_BRIDGE_DHCP_HOOK
endif

APMGR_BUILD_DIR         := $(BUILD_DIR)/apmgr
APMGR_SRC_DIR           := $(TOPDIR)/../controller/common
APMGR_TARGET_DIR        := $(TARGET_DIR)/usr/sbin

APMGR_BINARIES          := apmgr apmgrinfo

APMGR_SHOW_WLAN_SH      := apmgr_show_wlan.sh
APMGR_TARGET_SHOW_WLAN_SH := $(APMGR_TARGET_DIR)/$(APMGR_SHOW_WLAN_SH)


APMGR_CFLAGS += $(TARGET_CFLAGS) -Wall -fno-strict-aliasing
APMGR_CFLAGS += $(DASHDRSM) $(DASHIRSM) -DLINUX
APMGR_CFLAGS += -I $(TOPDIR)/../video54/apps/generatelocalip 
ifeq ($(strip $(BR2_PACKAGE_LIBRSM_AV)),y)
APMGR_CFLAGS += -DV54_AVP
endif


apmgr: localip
APMGR_LDFLAGS += $(DASHLRSM) -L  $(TARGET_DIR)/usr/lib  -ldl -llocalip

ifeq ($(BR2_PACKAGE_MDNSPROXY),y)
APMGR_CFLAGS += -DMDNS_ON_AP 
endif

APMGR_MAKE_OPT := -C $(APMGR_BUILD_DIR) -f $(V54_DIR)/apps/apmgr/Makefile

apmgr: librsm librkscrypto 
	-mkdir -p $(APMGR_BUILD_DIR)
	make $(APMGR_MAKE_OPT) CC=$(TARGET_CC) APMGR_SRC_DIR=$(APMGR_SRC_DIR) \
		 APMGR_CFLAGS="$(APMGR_CFLAGS)" APMGR_LDFLAGS="$(APMGR_LDFLAGS)" 
	install -D $(foreach f,$(APMGR_BINARIES),$(APMGR_BUILD_DIR)/$f) $(APMGR_TARGET_DIR)
	cp -f $(APMGR_SRC_DIR)/lwapp/$(APMGR_SHOW_WLAN_SH) $(APMGR_TARGET_SHOW_WLAN_SH)
	chmod a+x $(APMGR_TARGET_SHOW_WLAN_SH)

apmgr-clean:
	-$(MAKE) $(APMGR_MAKE_OPT) clean
	rm -f $(OMAC_TARGET_BINARY)

apmgr-dirclean:
	rm -rf $(APMGR_BUILD_DIR)


#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_APMGR)),y)
TARGETS+=apmgr
endif
