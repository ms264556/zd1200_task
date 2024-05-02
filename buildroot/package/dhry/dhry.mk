#############################################################
#
# DHRY (Cachebench and Dhrystone tests for Ruckus Products 
#
#############################################################
TOPDIR=${shell pwd}
DHRY_SRC_DIR:=$(TOPDIR)/../apps/dhry
DHRY_DIR:=$(BUILD_DIR)/dhry
DHRY_CP:=cp -fau
DHRY_BINARY:=dhry
DHRY_CACHEBENCH_BINARY:=cachebench

DHRY_TARGET_BINARY:=usr/sbin/dhry
DHRY_CACHEBENCH_TARGET_BINARY:=usr/sbin/cachebench

$(DHRY_DIR)/.unpacked: $(DHRY_SRC_DIR)/Makefile
	-mkdir $(DHRY_DIR)
	cp -a $(DHRY_SRC_DIR)/* $(DHRY_DIR)
	rm -f $(DHRY_DIR)/$(DHRY_BINARY)
	rm -f $(DHRY_DIR)/$(DHRY_CACHEBENCH_BINARY)
	touch $(DHRY_DIR)/.unpacked

$(DHRY_DIR)/$(DHRY_BINARY): $(DHRY_DIR)/.unpacked 
	echo ...$(TARGET_CONFIGURE_OPTS)
	$(TARGET_CONFIGURE_OPTS) $(MAKE) TOPDIR=$(TOPDIR) CC=$(TARGET_CC) -C $(DHRY_DIR)

$(TARGET_DIR)/$(DHRY_TARGET_BINARY): $(DHRY_DIR)/$(DHRY_BINARY)
	install -D $(DHRY_DIR)/$(DHRY_BINARY) $(TARGET_DIR)/$(DHRY_TARGET_BINARY)
	install -D $(DHRY_DIR)/$(DHRY_CACHEBENCH_BINARY) $(TARGET_DIR)/$(DHRY_CACHEBENCH_TARGET_BINARY)

dhry: uclibc $(TARGET_DIR)/$(DHRY_TARGET_BINARY)

dhry-clean:
	rm -f $(TARGET_DIR)/$(DHRY_TARGET_BINARY)
	rm -f $(TARGET_DIR)/$(DHRY_CACHEBENCH_TARGET_BINARY)
	rm -rf $(DHRY_DIR)

dhry-dirclean:
	rm -rf $(DHRY_DIR)
#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_DHRY)),y)
TARGETS+=dhry
endif
