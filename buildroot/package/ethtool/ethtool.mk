#############################################################
#
# ethtool 
#
#############################################################

ETHTOOL_VER:=2.6.35
ETHTOOL_DIR:=$(BUILD_DIR)/ethtool-$(ETHTOOL_VER)
ETHTOOL_SITE:=http://sourceforge.net/projects/gkernel/files/ethtool/$(ETHTOOL_VER)/
ETHTOOL_SOURCE:=ethtool-$(ETHTOOL_VER).tar.gz
ETHTOOL_CAT:=zcat

$(DL_DIR)/$(ETHTOOL_SOURCE):
	 $(WGET) $(DL_DIR) $(ETHTOOL_SITE)/$(ETHTOOL_SOURCE)

ethtool-source: $(DL_DIR)/$(ETHTOOL_SOURCE)

$(ETHTOOL_DIR)/.unpacked: $(DL_DIR)/$(ETHTOOL_SOURCE)
	$(ETHTOOL_CAT) $(DL_DIR)/$(ETHTOOL_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -

$(ETHTOOL_DIR)/.configured: $(ETHTOOL_DIR)/.unpacked
	( \
		cd $(ETHTOOL_DIR) ; \
		ac_cv_linux_vers=$(BR2_DEFAULT_KERNEL_HEADERS) \
		BUILD_CC=$(TARGET_CC) HOSTCC=$(HOSTCC) \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		./configure \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
		--prefix=$(STAGING_DIR) \
		--libdir=$(STAGING_DIR)/lib \
		--includedir=$(STAGING_DIR)/include \
	)
	touch $(ETHTOOL_DIR)/.configured

$(ETHTOOL_DIR)/ethtool: $(ETHTOOL_DIR)/.configured
	$(MAKE) \
		LDFLAGS="-L$(STAGING_DIR)/lib" \
		LIBS="-lpcap" \
		INCLS="-I. -I$(STAGING_DIR)/include" \
		-C $(ETHTOOL_DIR)
	
$(TARGET_DIR)/sbin/ethtool: $(ETHTOOL_DIR)/ethtool
	cp -af $< $@

ethtool: uclibc zlib libpcap $(TARGET_DIR)/sbin/ethtool

ethtool-clean:
	rm -f $(TARGET_DIR)/sbin/ethtool
	-$(MAKE) -C $(ETHTOOL_DIR) clean

ethtool-dirclean:
	rm -rf $(ETHTOOL_DIR)
#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_ETHTOOL)),y)
TARGETS+=ethtool
endif
