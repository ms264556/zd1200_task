#############################################################
#
# wide-dhcpv6 project for DHCPv6 client and server
#
#############################################################

NDISC6_SOURCE:=ndisc6-1.0.0.tar.bz2
NDISC6_SITE:=http://www.remlab.net/files/ndisc6
NDISC6_DIR:=$(BUILD_DIR)/ndisc6-1.0.0
NDISC6_SRC_DIR:=$(V54_DIR)/apps/ndisc6

$(DL_DIR)/$(NDISC6_SOURCE):
	$(WGET) -P $(DL_DIR) $(NDISC6_SITE)/$(NDISC6_SOURCE)

$(NDISC6_DIR)/.unpacked: $(DL_DIR)/$(NDISC6_SOURCE)
	bzcat $(DL_DIR)/$(NDISC6_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(NDISC6_DIR) package/ndisc6/ *.patch
	cp package/ndisc6/ndisc.c $(NDISC6_DIR)/src
	cp package/ndisc6/traceroute.c $(NDISC6_DIR)/src
	touch $(NDISC6_DIR)/.unpacked

$(NDISC6_DIR)/.configured: $(NDISC6_DIR)/.unpacked
	(cd $(NDISC6_DIR); \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS=-DRKS_PATCHES \
		./configure \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
		--with-localdbdir=/tmp \
		--sysconfdir=/tmp \
		--exec-prefix=/sbin \
		--prefix=/usr \
	);
	touch $(NDISC6_DIR)/.configured

ndisc6: $(NDISC6_DIR)/.configured
	$(MAKE) -C $(NDISC6_DIR)
ifeq ($(strip $(BR2_PACKAGE_NDISC6_NDISC6)),y)
	cp -f $(NDISC6_DIR)/src/ndisc6 $(TARGET_DIR)/usr/bin
	$(STRIP) $(TARGET_DIR)/usr/bin/ndisc6
endif
ifeq ($(strip $(BR2_PACKAGE_NDISC6_TRACEROUTE6)),y)
	cp -f $(NDISC6_DIR)/src/rltraceroute6 $(TARGET_DIR)/usr/bin/traceroute6
	$(STRIP) $(TARGET_DIR)/usr/bin/traceroute6
endif
ifeq ($(strip $(BR2_PACKAGE_NDISC6_RDISC6)),y)
	cp -f $(NDISC6_DIR)/src/rdisc6 $(TARGET_DIR)/usr/bin/rdisc6
	$(STRIP) $(TARGET_DIR)/usr/bin/rdisc6
endif

ndisc6-clean:
	rm -f $(TARGET_DIR)/usr/bin/ndisc6
	rm -f $(TARGET_DIR)/usr/bin/traceroute6
	-$(MAKE) -C $(NDISC6_DIR) clean

ndisc6-dirclean:
	rm -rf $(NDISC6_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_NDISC6)),y)
TARGETS+=ndisc6
endif
