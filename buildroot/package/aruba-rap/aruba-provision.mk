#############################################################
#
# Aruba Provisioning Document Parsing Utilities
#
#############################################################
TOPDIR=${shell pwd}
ARRAP_VER:=0.0.1
ARRAP_SRC_DIR:=$(TOPDIR)/../video54/lib/expat
ARRAP_DIR:=$(BUILD_DIR)/arrap-provision
RKS_CP:=cp -fapu
ARRAP_BINARY:=arrap-provision
ARRAP_TARGET_BINARY:=usr/local/sbin/arrap-provision

ALL_TARGETS = $(TARGET_DIR)/$(ARRAP_TARGET_BINARY)

$(ARRAP_DIR)/.unpacked: $(ARRAP_SRC_DIR)/Makefile
	-mkdir $(ARRAP_DIR)
	-mkdir $(ARRAP_DIR)/xmltok
	-mkdir $(ARRAP_DIR)/xmlparse
	-mkdir $(ARRAP_DIR)/gennmtab
	$(RKS_CP) $(ARRAP_SRC_DIR)/Makefile $(ARRAP_DIR)
	touch $(ARRAP_DIR)/.unpacked


arrap-provision_make:
	$(TARGET_CONFIGURE_OPTS) $(MAKE) CC=$(TARGET_CC) \
	TOPDIR=$(TOPDIR) ARRAP_SRC_DIR=$(ARRAP_SRC_DIR) \
	BR2_ENDIAN_BIG=$(BR2_ENDIAN_BIG) \
	-C$(ARRAP_DIR)

$(ARRAP_DIR)/$(ARRAP_BINARY): $(ARRAP_DIR)/.unpacked arrap-provision_make
	echo

$(TARGET_DIR)/$(ARRAP_TARGET_BINARY): $(ARRAP_DIR)/$(ARRAP_BINARY)
	install -D $(ARRAP_DIR)/$(ARRAP_BINARY) $(TARGET_DIR)/$(ARRAP_TARGET_BINARY)

aruba-provision: uclibc $(ALL_TARGETS)

aruba-provision-clean: $(ARRAP_DIR)/.unpacked
	rm -f $(ALL_TARGETS) $(ARRAP_DIR)/*.d $(ARRAP_DIR)/.unpacked
	-$(MAKE) -C $(ARRAP_DIR) clean

aruba-provision-dirclean:
	rm -rf $(ARRAP_DIR)
#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_ARUBA_PROVISION)),y)
TARGETS+=aruba-provision
endif
