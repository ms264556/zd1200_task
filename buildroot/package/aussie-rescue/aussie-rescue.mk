#############################################################
#
# rkscli
#
#############################################################
RESCUE_BUILD_DIR      :=${BUILD_DIR}/aussie-rescue
RESCUE_SRC_DIR        :=${TOPDIR}/../video54/apps/aussie-rescue

RESCUE_BINARY         := tac_encrypt
RESCUE_BUILD_BINARY   := $(RESCUE_BUILD_DIR)/$(RESCUE_BINARY)
RESCUE_TARGET_BINARY  := $(TARGET_DIR)/usr/bin/$(RESCUE_BINARY)

RESCUE_MAKE_OPT       := -C $(RESCUE_BUILD_DIR) -f $(RESCUE_SRC_DIR)/Makefile

aussie-rescue: 
	@mkdir -p $(RESCUE_BUILD_DIR)
	$(MAKE) $(RESCUE_MAKE_OPT) CC=$(TARGET_CC) \
		RESCUE_SRC_DIR=$(RESCUE_SRC_DIR)
	install -D $(RESCUE_BUILD_BINARY) $(RESCUE_TARGET_BINARY)

aussie-rescue-clean:
	@rm -f $(RESCUE_TARGET_BINARY)
	-$(MAKE) $(RESCUE_MAKE_OPT) clean


# refer to rkscli-clean, so it can clean out librsc
aussie-rescue-dirclean:
	rm -rf $(RESCUE_BUILD_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_AUSSIE_RESCUE)),y)
TARGETS+=aussie-rescue
endif
