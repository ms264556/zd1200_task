#############################################################
#
# c2lib
#
#############################################################
C2LIB_SRC_DIR         = $(V54_DIR)/apps/c2lib
C2LIB_BUILD_DIR       = $(BUILD_DIR)/c2lib
C2LIB_LIB_INSTALL_DIR = $(TARGET_DIR)/usr/lib

C2LIB_MAKE_OPT = -C $(C2LIB_BUILD_DIR) -f $(C2LIB_SRC_DIR)/Makefile

C2LIB_CFLAGS  += $(TARGET_CFLAGS)

c2lib:
	@mkdir -p $(C2LIB_BUILD_DIR)
	make $(C2LIB_MAKE_OPT) CC=$(TARGET_CC) C2LIB_SRC_DIR=$(C2LIB_SRC_DIR) \
		C2LIB_CFLAGS="$(C2LIB_CFLAGS)" C2LIB_LDFLAGS="$(C2LIB_LDFLAGS)"
	$(INSTALL) -t $(C2LIB_LIB_INSTALL_DIR) $(C2LIB_BUILD_DIR)/libc2lib.so

c2lib-clean:
	@rm -f $(C2LIB_LIB_INSTALL_DIR)/libc2lib.so
	@-$(MAKE) $(C2LIB_MAKE_OPT) clean

c2lib-dirclean: c2lib-clean
	rm -rf $(C2LIB_BUILD_DIR)

.PHONY: c2lib c2lib-clean c2lib-dirclean

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_C2LIB)),y)
TARGETS += c2lib
endif # BR2_PACKAGE_C2LIB
