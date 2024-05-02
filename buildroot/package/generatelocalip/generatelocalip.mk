#############################################################
#
# generatelocalip
#
#############################################################
LOCALIP_SRC_DIR         = $(V54_DIR)/apps/generatelocalip
LOCALIP_BUILD_DIR       = $(BUILD_DIR)/generatelocalip
LOCALIP_LIB_INSTALL_DIR = $(TARGET_DIR)/usr/lib

LOCALIP_MAKE_OPT = -C $(LOCALIP_BUILD_DIR) -f $(LOCALIP_SRC_DIR)/Makefile


localip:
	@mkdir -p $(LOCALIP_BUILD_DIR)
	@cp -f -l -r $(LOCALIP_SRC_DIR)/* $(LOCALIP_BUILD_DIR)
	make $(LOCALIP_MAKE_OPT) CC=$(TARGET_CC) LOCALIP_SRC_DIR=$(LOCALIP_SRC_DIR)
	$(INSTALL) -t $(LOCALIP_LIB_INSTALL_DIR) $(LOCALIP_BUILD_DIR)/liblocalip.so

localip-clean:
	@rm -f $(LOCALIP_LIB_INSTALL_DIR)/liblocalip.so
	@-$(MAKE) $(LOCALIP_MAKE_OPT) clean

localip-dirclean: localip-clean
	rm -rf $(LOCALIP_BUILD_DIR)

.PHONY: localip localip-clean localip-dirclean

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_LOCALIP)),y)
TARGETS += localip
endif # BR2_PACKAGE_LOCALIP
