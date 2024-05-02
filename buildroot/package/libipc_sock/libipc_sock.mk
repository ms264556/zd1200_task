#############################################################
#
# libipc_sock
#
#############################################################
LIBIPC_SOCK_SRC_DIR         = $(V54_DIR)/lib/libipc_sock
LIBIPC_SOCK_BUILD_DIR       = $(BUILD_DIR)/libipc_sock
LIBIPC_SOCK_LIB_INSTALL_DIR = $(TARGET_DIR)/usr/lib

LIBIPC_SOCK_MAKE_OPT = $(TARGET_CONFIGURE_OPTS) -C $(LIBIPC_SOCK_BUILD_DIR)

.PHONY: libipc_sock
libipc_sock: libipc_sock-build libipc_sock-install

$(LIBIPC_SOCK_BUILD_DIR):
	$(CP) -rl $(LIBIPC_SOCK_SRC_DIR) $(LIBIPC_SOCK_BUILD_DIR)

.PHONY: libipc_sock-build
libipc_sock-build: $(LIBIPC_SOCK_BUILD_DIR) libcache_pool
	$(MAKE) $(LIBIPC_SOCK_MAKE_OPT)

.PHONY: libipc_sock-install
libipc_sock-install: libipc_sock-build
	$(INSTALL) -t $(LIBIPC_SOCK_LIB_INSTALL_DIR) $(LIBIPC_SOCK_BUILD_DIR)/libipc_sock.so

.PHONY: libipc_sock-uninstall
libipc_sock-uninstall:
	rm -rf $(LIBIPC_SOCK_LIB_INSTALL_DIR)/libipc_sock.so

.PHONY: libipc_sock-clean
libipc_sock-clean: libipc_sock-uninstall
	-$(MAKE) $(LIBIPC_SOCK_MAKE_OPT) clean

.PHONY: libipc_sock-dirclean
libipc_sock-dirclean: libipc_sock-clean
	rm -rf $(LIBIPC_SOCK_BUILD_DIR)

.PHONY: libipc_sock-info
libipc_sock-info:
	@echo "BR2_PACKAGE_LIBIPC_SOCK             = $(BR2_PACKAGE_LIBIPC_SOCK)"

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_LIBIPC_SOCK)),y)
TARGETS += libipc_sock
endif # BR2_PACKAGE_LIBIPC_SOCK
