#############################################################
#
# dhcp-forwarder
#
#############################################################

TOPDIR=${shell pwd}

DHCP-FORWARDER_DIR          :=${BUILD_DIR}/dhcp-forwarder

DHCP-FORWARDER_SOURCE       :=${TOPDIR}/../video54/apps/dhcp-forwarder

DHCP-FORWARDER_TARGET_DIR   :=$(TARGET_DIR)/usr/sbin

DHCP-FORWARDER_CP           :=cp -r -f -l
DHCP-FORWARDER_BINARY       :=dhcp-fwd
DHCP-FORWARDER_CFLAGS       +=$(TARGET_CFLAGS)

## in alphabetic order


$(DHCP-FORWARDER_DIR)/.configured:
	$(DHCP-FORWARDER_CP) $(DHCP-FORWARDER_SOURCE) $(DHCP-FORWARDER_DIR)
	(cd $(DHCP-FORWARDER_DIR); \
		./configure \
		CC="$(TARGET_CC)" \
		ac_cv_func_malloc_0_nonnull=yes \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
		--prefix=/usr \
		--disable-dietlibc \
	);
	touch $(DHCP-FORWARDER_DIR)/.configured

$(DHCP-FORWARDER_DIR)/$(DHCP-FORWARDER_BINARY): $(DHCP-FORWARDER_DIR)/.configured
	$(MAKE) -C $(DHCP-FORWARDER_DIR)

$(DHCP-FORWARDER_TARGET_DIR)/$(DHCP-FORWARDER_BINARY): $(DHCP-FORWARDER_DIR)/$(DHCP-FORWARDER_BINARY)
	cp -f $(DHCP-FORWARDER_DIR)/$(DHCP-FORWARDER_BINARY) $(DHCP-FORWARDER_TARGET_DIR)/$(DHCP-FORWARDER_BINARY)
	$(STRIP) $(DHCP-FORWARDER_TARGET_DIR)/$(DHCP-FORWARDER_BINARY)

dhcp-forwarder: uclibc $(DHCP-FORWARDER_TARGET_DIR)/$(DHCP-FORWARDER_BINARY)

dhcp-forwarder-clean:
	@rm -f $(DHCP-FORWARDER_TARGET_DIR)/$(DHCP-FORWARDER_BINARY)
	-$(MAKE) -C $(DHCP-FORWARDER_DIR) clean

dhcp-forwarder-dirclean:
	@rm -f $(DHCP-FORWARDER_TARGET_DIR)/$(DHCP-FORWARDER_BINARY)
	rm -rf $(DHCP-FORWARDER_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_DHCP-FORWARDER)),y)
TARGETS+=dhcp-forwarder
endif

