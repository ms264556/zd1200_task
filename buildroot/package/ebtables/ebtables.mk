#############################################################
#
# ebtables
#
#############################################################
EBTABLES_VER:=v2.0.6
EBTABLES_SOURCE_URL:=http://ebtables.sourceforge.net
EBTABLES_SOURCE:=ebtables-$(EBTABLES_VER).tar.gz
LINUX_DIR=${shell pwd}/$(strip $(subst ",, $(BR2_KERNEL_PATH)))
EBTABLES_BUILD_DIR:=$(BUILD_DIR)/ebtables-$(EBTABLES_VER)

$(DL_DIR)/$(EBTABLES_SOURCE):
	 $(WGET) -P $(DL_DIR) $(EBTABLES_SOURCE_URL)/$(EBTABLES_SOURCE) 

$(EBTABLES_BUILD_DIR)/.unpacked: $(DL_DIR)/$(EBTABLES_SOURCE)
	zcat $(DL_DIR)/$(EBTABLES_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	touch $(EBTABLES_BUILD_DIR)/.unpacked

$(EBTABLES_BUILD_DIR)/.configured: $(EBTABLES_BUILD_DIR)/.unpacked
	toolchain/patch-kernel.sh $(EBTABLES_BUILD_DIR) package/ebtables/ ebtables\*.patch
	#
	$(SED) "s;\[ -f /usr/include/netinet/ip6.h \];grep -q '__UCLIBC_HAS_IPV6__ 1' \
		$(STAGING_DIR)/include/bits/uClibc_config.h;" $(EBTABLES_BUILD_DIR)/Makefile
	touch  $(EBTABLES_BUILD_DIR)/.configured

$(EBTABLES_BUILD_DIR)/ebtables: $(EBTABLES_BUILD_DIR)/.configured
	$(TARGET_CONFIGURE_OPTS) \
	$(MAKE) -C $(EBTABLES_BUILD_DIR) \
		KERNEL_DIR=$(LINUX_DIR) \
		CC=$(TARGET_CC) COPT_FLAGS="$(TARGET_CFLAGS)"

$(TARGET_DIR)/usr/sbin/ebtables: $(EBTABLES_BUILD_DIR)/ebtables
	$(CP) $(EBTABLES_BUILD_DIR)/ebtables $(TARGET_DIR)/usr/sbin/ebtables
	$(STRIP) $(TARGET_DIR)/usr/sbin/ebtables*

ebtables: $(TARGET_DIR)/usr/sbin/ebtables

ebtables-source: $(DL_DIR)/$(EBTABLES_SOURCE)

ebtables-clean:
	-$(MAKE) DESTDIR=$(TARGET_DIR) CC=$(TARGET_CC) -C $(EBTABLES_BUILD_DIR) uninstall
	-$(MAKE) -C $(EBTABLES_BUILD_DIR) clean

ebtables-dirclean:
	rm -rf $(EBTABLES_BUILD_DIR)
#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_EBTABLES)),y)
TARGETS+=ebtables
endif
