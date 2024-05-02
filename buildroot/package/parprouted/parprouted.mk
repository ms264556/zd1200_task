#############################################################
#
# parprouted
#
#############################################################

TOPDIR=${shell pwd}

PARPROUTED_SOURCE:=${TOPDIR}/../video54/apps/parprouted
PARPROUTED_DIR:=$(BUILD_DIR)/parprouted
PARPROUTED_BINARY:=parprouted
PARPROUTED_TARGET_BINARY:=usr/sbin/parprouted

PARPROUTED_CP:=cp -rfl

$(PARPROUTED_DIR)/$(PARPROUTED_BINARY):
	$(PARPROUTED_CP) $(PARPROUTED_SOURCE) $(PARPROUTED_DIR)
	$(MAKE) CC=$(TARGET_CC) AR=$(TARGET_CROSS)ar -C $(PARPROUTED_DIR)

$(TARGET_DIR)/$(PARPROUTED_TARGET_BINARY): $(PARPROUTED_DIR)/$(PARPROUTED_BINARY)
	cp -f $(PARPROUTED_DIR)/$(PARPROUTED_BINARY) $(TARGET_DIR)/$(PARPROUTED_TARGET_BINARY)

parprouted: uclibc $(TARGET_DIR)/$(PARPROUTED_TARGET_BINARY)

parprouted-clean:
	rm -f $(PARPROUTED_DIR)/$(PARPROUTED_BINARY)
	-$(MAKE) -C $(PARPROUTED_DIR) clean

parprouted-dirclean:
	rm -rf $(PARPROUTED_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_PARPROUTED)),y)
TARGETS+=parprouted
endif

