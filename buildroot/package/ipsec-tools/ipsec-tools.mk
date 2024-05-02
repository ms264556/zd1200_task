#############################################################
#
# ipsec-tools
#
#############################################################

# TARGETS

IPSEC_TOOLS_SOURCE := ipsec-tools-0.7.tar.gz
IPSEC_TOOLS_BINARY := ipsec-tools-binary
IPSEC_TOOLS_DIR    := ${BUILD_DIR}/ipsec-tools-0.7
OPENSSL_VER        :=0.9.7f
OPENSSL_DIR        :=$(BUILD_DIR)/openssl-$(OPENSSL_VER)

$(IPSEC_TOOLS_DIR)/.unpacked: $(DL_DIR)/$(IPSEC_TOOLS_SOURCE)
	zcat $(DL_DIR)/$(IPSEC_TOOLS_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	touch $(IPSEC_TOOLS_DIR)/.unpacked

$(IPSEC_TOOLS_DIR)/.configured: $(IPSEC_TOOLS_DIR)/.unpacked
	(cd $(IPSEC_TOOLS_DIR); \
		$(TARGET_CONFIGURE_OPTS) \
		./configure \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--sbindir=$(TARGET_DIR) \
		--enable-natt \
		--enable-dpd \
		--enable-adminport --sysconfdir=/tmp --localstatedir=/tmp \
		--with-flex \
		--with-flexlib=$(TARGET_DIR)/usr/lib/libfl.a \
		--with-openssl=$(OPENSSL_DIR) \
		--with-kernel-headers=${KERNELPATH_ABS}/include \
	);

	# There is an error complaining about endian header file...
	(cd $(IPSEC_TOOLS_DIR)/src/racoon; sed -i.orig 's/-Werror/ /' Makefile)

	# Fix racoonctl timeout for vpn-connect
	(cd $(IPSEC_TOOLS_DIR)/src/racoon; mv racoonctl.c racoonctl.c.orig; cp $(BASE_DIR)/package/ipsec-tools/racoon/racoonctl.c racoonctl.c)

	# Fix vendorid checking (esp. VENDORID_DPD)
	(cd $(IPSEC_TOOLS_DIR)/src/racoon; mv isakmp_inf.h isakmp_inf.h.orig; cp $(BASE_DIR)/package/ipsec-tools/racoon/isakmp_inf.h isakmp_inf.h)
	(cd $(IPSEC_TOOLS_DIR)/src/racoon; mv isakmp_inf.c isakmp_inf.c.orig; cp $(BASE_DIR)/package/ipsec-tools/racoon/isakmp_inf.c isakmp_inf.c)
	(cd $(IPSEC_TOOLS_DIR)/src/racoon; mv isakmp_ident.c isakmp_ident.c.orig; cp $(BASE_DIR)/package/ipsec-tools/racoon/isakmp_ident.c isakmp_ident.c)
	(cd $(IPSEC_TOOLS_DIR)/src/racoon; mv isakmp_agg.c isakmp_agg.c.orig; cp $(BASE_DIR)/package/ipsec-tools/racoon/isakmp_agg.c isakmp_agg.c)
	(cd $(IPSEC_TOOLS_DIR)/src/racoon; mv isakmp_base.c isakmp_base.c.orig; cp $(BASE_DIR)/package/ipsec-tools/racoon/isakmp_base.c isakmp_base.c)

	touch  $(IPSEC_TOOLS_DIR)/.configured

$(IPSEC_TOOLS_DIR)/$(IPSEC_TOOLS_BINARY): $(IPSEC_TOOLS_DIR)/.configured
	$(MAKE) -C $(IPSEC_TOOLS_DIR)
	$(STRIP) $(IPSEC_TOOLS_DIR)/src/setkey/setkey
	$(STRIP) $(IPSEC_TOOLS_DIR)/src/racoon/racoon
	$(STRIP) $(IPSEC_TOOLS_DIR)/src/racoon/racoonctl
	touch $(IPSEC_TOOLS_DIR)/$(IPSEC_TOOLS_BINARY)

$(TARGET_DIR)/$(IPSEC_TOOLS_BINARY): $(IPSEC_TOOLS_DIR)/$(IPSEC_TOOLS_BINARY)
#	$(MAKE) -C $(IPSEC_TOOLS_DIR) install
	cp $(IPSEC_TOOLS_DIR)/src/setkey/setkey $(TARGET_DIR)/sbin/setkey
	cp $(IPSEC_TOOLS_DIR)/src/racoon/racoon $(TARGET_DIR)/sbin/racoon
	cp $(IPSEC_TOOLS_DIR)/src/racoon/racoonctl $(TARGET_DIR)/sbin/racoonctl

ipsec-tools: uclibc flex openssl $(TARGET_DIR)/$(IPSEC_TOOLS_BINARY)

ipsec-tools-clean:
	rm -f $(IPSEC_TOOLS_DIR)/$(IPSEC_TOOLS_BINARY)
	rm -f $(TARGET_DIR)/sbin/setkey
	rm -f $(TARGET_DIR)/sbin/racoon 
	rm -f $(TARGET_DIR)/sbin/racoonctl 
	-$(MAKE) -C $(IPSEC_TOOLS_DIR) clean

ipsec-tools-dirclean:
	rm -rf $(IPSEC_TOOLS_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_IPSEC_TOOLS)),y)
TARGETS+=ipsec-tools
endif


