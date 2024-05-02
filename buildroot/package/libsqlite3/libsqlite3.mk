#############################################################
#
# libsqlite3
#
#############################################################
LIBSQLITE_SRC_DIR         = $(V54_DIR)/lib/libsqlite3
LIBSQLITE_BUILD_DIR       = $(BUILD_DIR)/libsqlite3
LIBSQLITE_LIB_INSTALL_DIR = $(TARGET_DIR)/usr/lib

LIBSQLITE_MAKE_OPT = $(TARGET_CONFIGURE_OPTS) \
    -C $(LIBSQLITE_BUILD_DIR)

.PHONY: libsqlite3
libsqlite3: libsqlite-build libsqlite-install

$(LIBSQLITE_BUILD_DIR):
	$(CP) -rl $(LIBSQLITE_SRC_DIR) $(LIBSQLITE_BUILD_DIR)

.PHONY: libsqlite-build
libsqlite-build: $(LIBSQLITE_BUILD_DIR)
	$(MAKE) $(LIBSQLITE_MAKE_OPT)

.PHONY: libsqlite-install
libsqlite-install: libsqlite-build
	$(INSTALL) -t $(LIBSQLITE_LIB_INSTALL_DIR) $(LIBSQLITE_BUILD_DIR)/libsqlite3.so

.PHONY: libsqlite-uninstall
libsqlite-uninstall:
	rm -rf $(LIBSQLITE_LIB_INSTALL_DIR)/libsqlite3.so

.PHONY: libsqlite-clean
libsqlite-clean: libsqlite-uninstall
	-$(MAKE) $(LIBSQLITE_MAKE_OPT) clean

.PHONY: libsqlite-dirclean
libsqlite-dirclean: libsqlite-clean
	rm -rf $(LIBSQLITE_BUILD_DIR)

.PHONY: librsqlite-info
libsqlite-info:
	@echo "BR2_PACKAGE_LIBSQLITE=$(BR2_PACKAGE_LIBSQLITE)"


#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_LIBSQLITE)),y)
TARGETS          += libsqlite3
endif # BR2_PACKAGE_LIBSQLITE
