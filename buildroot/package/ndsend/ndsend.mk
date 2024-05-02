#############################################################
#
# ndsend
#
#############################################################
NDSEND_SRC_DIR         = $(V54_DIR)/apps/ndsend
NDSEND_BUILD_DIR       = $(BUILD_DIR)/ndsend
NDSEND_INSTALL_DIR = $(TARGET_DIR)/bin

NDSEND_MAKE_OPT = -C $(NDSEND_BUILD_DIR) -f $(NDSEND_SRC_DIR)/Makefile


ndsend:
	@mkdir -p $(NDSEND_BUILD_DIR)
	@cp -f -l -r $(NDSEND_SRC_DIR)/* $(NDSEND_BUILD_DIR)
	make $(NDSEND_MAKE_OPT) CC=$(TARGET_CC) NDSEND_SRC_DIR=$(NDSEND_SRC_DIR)
	$(INSTALL) -t $(NDSEND_INSTALL_DIR) $(NDSEND_BUILD_DIR)/ndsend

ndsend-clean:
	@rm -f $(NDSEND_INSTALL_DIR)/ndsend
	@-$(MAKE) $(NDSEND_MAKE_OPT) clean

ndsend-dirclean: ndsend-clean
	rm -rf $(NDSEND_BUILD_DIR)

.PHONY: ndsend ndsend-clean ndsend-dirclean

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_NDSEND)),y)
TARGETS += ndsend
endif # BR2_PACKAGE_NDSEND
