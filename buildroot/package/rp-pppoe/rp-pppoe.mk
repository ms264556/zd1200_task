#############################################################
#
# pppoe
#
#############################################################
RP_PPPOE_SOURCE:=rp-pppoe-3.7.tar.gz
RP_PPPOE_SITE:=http://www.roaringpenguin.com/penguin/pppoe/
RP_PPPOE_DIR:=$(BUILD_DIR)/rp-pppoe-3.7
RP_PPPOE_CAT:=zcat
RP_PPPOE_BINARY:=pppoe
RP_PPPOE_TARGET_BINARY:=usr/sbin/pppoe

$(DL_DIR)/$(RP_PPPOE_SOURCE):
	 $(WGET) -P $(DL_DIR) $(RP_PPPOE_SITE)/$(RP_PPPOE_SOURCE)

pppoe-source: $(DL_DIR)/$(RP_PPPOE_SOURCE)

$(RP_PPPOE_DIR)/.unpacked: $(DL_DIR)/$(RP_PPPOE_SOURCE)
	$(RP_PPPOE_CAT) $(DL_DIR)/$(RP_PPPOE_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	$(SED) 's/ "no defaults for cross-compiling"; exit 0/ "defaulting to reversed" ; rpppoe_cv_pack_bitfields=rev/' $(RP_PPPOE_DIR)/src/configure
	touch $(RP_PPPOE_DIR)/.unpacked

$(RP_PPPOE_DIR)/.configured: $(RP_PPPOE_DIR)/.unpacked
	(cd $(RP_PPPOE_DIR)/src; rm -rf config.cache; \
		$(TARGET_CONFIGURE_OPTS) \
		./configure \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
		$(DISABLE_NLS) \
	);
	touch  $(RP_PPPOE_DIR)/.configured

$(RP_PPPOE_DIR)/$(RP_PPPOE_BINARY): $(RP_PPPOE_DIR)/.configured
	$(MAKE) CC=$(TARGET_CC) AR=$(TARGET_CROSS)ar -C $(RP_PPPOE_DIR)/src

$(TARGET_DIR)/$(RP_PPPOE_TARGET_BINARY): $(RP_PPPOE_DIR)/$(RP_PPPOE_BINARY)
	$(STRIP) $(RP_PPPOE_DIR)/src/pppoe
	$(STRIP) $(RP_PPPOE_DIR)/src/pppoe-relay
	$(STRIP) $(RP_PPPOE_DIR)/src/pppoe-server
	$(STRIP) $(RP_PPPOE_DIR)/src/pppoe-sniff
	-mkdir -p $(TARGET_DIR)/usr/sbin
	$(INSTALL) -m 755 $(RP_PPPOE_DIR)/src/pppoe $(TARGET_DIR)/usr/sbin
	$(INSTALL) -m 755 $(RP_PPPOE_DIR)/src/pppoe-relay $(TARGET_DIR)/usr/sbin
	$(INSTALL) -m 755 $(RP_PPPOE_DIR)/src/pppoe-sniff $(TARGET_DIR)/usr/sbin
	$(INSTALL) -m 755 $(RP_PPPOE_DIR)/scripts/pppoe-connect $(TARGET_DIR)/usr/sbin
	$(INSTALL) -m 755 $(RP_PPPOE_DIR)/scripts/pppoe-start $(TARGET_DIR)/usr/sbin
	$(INSTALL) -m 755 $(RP_PPPOE_DIR)/scripts/pppoe-status $(TARGET_DIR)/usr/sbin
	$(INSTALL) -m 755 $(RP_PPPOE_DIR)/scripts/pppoe-stop $(TARGET_DIR)/usr/sbin
	$(INSTALL) -m 755 $(RP_PPPOE_DIR)/scripts/pppoe-setup $(TARGET_DIR)/usr/sbin
	-mkdir -p $(TARGET_DIR)/etc/ppp
	$(INSTALL) -m 644 $(RP_PPPOE_DIR)/configs/pppoe.conf $(TARGET_DIR)/etc/ppp
	$(INSTALL) -m 644 $(RP_PPPOE_DIR)/configs/firewall-standalone $(TARGET_DIR)/etc/ppp
	$(INSTALL) -m 644 $(RP_PPPOE_DIR)/configs/firewall-masq $(TARGET_DIR)/etc/ppp
	$(INSTALL) -m 755 $(RP_PPPOE_DIR)/scripts/pppoe-init $(TARGET_DIR)/etc/init.d

pppoe: uclibc $(TARGET_DIR)/$(RP_PPPOE_TARGET_BINARY)

pppoe-clean:
	rm -f  $(TARGET_DIR)/usr/sbin/pppoe
	rm -f  $(TARGET_DIR)/usr/sbin/chat
	rm -rf $(TARGET_DIR)/etc/ppp
	$(MAKE) DESTDIR=$(TARGET_DIR)/usr CC=$(TARGET_CC) -C $(RP_PPPOE_DIR) uninstall
	-$(MAKE) -C $(RP_PPPOE_DIR) clean

pppoe-dirclean:
	rm -rf $(RP_PPPOE_DIR)


#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_RP_PPPOE)),y)
TARGETS+=pppoe
endif
