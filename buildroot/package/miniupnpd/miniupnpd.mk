#############################################################
#
# Miniupnpd
#
#############################################################

MINIUPNPD:=miniupnpd-1.0-RC6
UPNPD_SRC:=../video54/apps/$(MINIUPNPD)
MINIUPNPD_BUILD_DIR:=$(BUILD_DIR)/$(MINIUPNPD)

MINIUPNPD_BINARY:=miniupnpd
MINIUPNPD_TARGET_BINARY:=usr/sbin/miniupnpd

$(MINIUPNPD_BUILD_DIR)/.unpacked:
	cp -Rf $(UPNPD_SRC) $(BUILD_DIR)/
	touch $(MINIUPNPD_BUILD_DIR)/.unpacked


$(MINIUPNPD_BUILD_DIR)/$(MINIUPNPD_BINARY): $(MINIUPNPD_BUILD_DIR)/.unpacked
	$(MAKE) CC=$(TARGET_CC) IPTABLEDIR=$(BUILD_DIR)/iptables-1.3.4 -C $(MINIUPNPD_BUILD_DIR) PREFIX=$(TARGET_DIR) 

$(TARGET_DIR)/$(MINIUPNPD_TARGET_BINARY): $(MINIUPNPD_BUILD_DIR)/$(MINIUPNPD_BINARY)
	$(STRIP) $(MINIUPNPD_BUILD_DIR)/$(MINIUPNPD_BINARY)
	$(MAKE) CC=$(TARGET_CC) IPTABLEDIR=$(BUILD_DIR)/iptables-1.3.4 -C $(MINIUPNPD_BUILD_DIR) PREFIX=$(TARGET_DIR) install

miniupnpd: uclibc iptables $(TARGET_DIR)/$(MINIUPNPD_TARGET_BINARY)

miniupnpd-clean:
	rm -f  $(MINIUPNPD_BUILD_DIR)/$(MINIUPNPD_BINARY)
	rm -rf $(MINIUPNPD_BUILD_DIR)

miniupnpd-dirclean:
	rm -f  $(MINIUPNPD_BUILD_DIR)/$(MINIUPNPD_BINARY)
	rm -rf $(MINIUPNPD_BUILD_DIR)



#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_MINIUPNPD)),y)
TARGETS+=miniupnpd
endif
