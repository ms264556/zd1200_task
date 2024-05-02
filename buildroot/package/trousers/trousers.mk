#############################################################
#
# trousers
#
#############################################################
TROUSERS_VER      := 0.3.11.2
TROUSERS_BINARY   := trousers
TROUSERS_DIR      := $(BUILD_DIR)/trousers-$(TROUSERS_VER)
TROUSERS_SOURCE   := trousers-$(TROUSERS_VER).tar.gz

# Fix kernel cross compile, source dir, readline include path
TROUSERS_MAKEOPTS += CROSS_COMPILE=$(TARGET_CROSS)
TROUSERS_MAKEOPTS += DESTDIR=$(TARGET_DIR)

$(TROUSERS_DIR)/.unpacked:
	zcat $(DL_DIR)/$(TROUSERS_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(TROUSERS_DIR) package/trousers/ *trousers.patch
	touch $@

$(TROUSERS_DIR)/.configured: $(TROUSERS_DIR)/.unpacked
	(cd $(TROUSERS_DIR); \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		./configure --with-gui=none \
		--host=$(GNU_TARGET_NAME) \
	);
	touch $@

#		./configure --with-gui=none --enable-debug=yes \

$(TROUSERS_DIR)/$(TROUSERS_BINARY): $(TROUSERS_DIR)/.configured
	$(MAKE) CPPFLAGS="-I$(BUILD_INCLUDE) -DRCKS" -C $(TROUSERS_DIR) $(TROUSERS_MAKEOPTS)
	$(STRIP) $(TROUSERS_DIR)/src/tcsd/tcsd

$(TARGET_DIR)/$(TROUSERS_BINARY): $(TROUSERS_DIR)/$(TROUSERS_BINARY)
	cp $(TROUSERS_DIR)/src/tcsd/tcsd    ${TARGET_DIR}/usr/sbin
	cp $(TROUSERS_DIR)/src/tspi/.libs/libtspi.so.1.2.0    ${TARGET_DIR}/usr/lib
	ln -sf  libtspi.so.1.2.0   ${TARGET_DIR}/usr/lib/libtspi.so
	ln -sf  libtspi.so.1.2.0   ${TARGET_DIR}/usr/lib/libtspi.so.1
	cp $(TROUSERS_DIR)/src/tspi/libtspi.la    ${TARGET_DIR}/usr/lib
	cp $(TROUSERS_DIR)/src/tddl/libtddl.a     ${TARGET_DIR}/usr/lib
	cp $(TROUSERS_DIR)/dist/tcsd.conf   ${TARGET_DIR}/etc

trousers: uclibc $(TARGET_DIR)/$(TROUSERS_BINARY)

trousers-clean:
	rm -f $(TARGET_DIR)/usr/sbin/tcsd
	-$(MAKE) -C $(TROUSERS_DIR) clean

trousers-dirclean:
	if [ -h $(TARGET_CROSS)install ]; then rm -f $(TARGET_CROSS)install; fi
	rm -rf $(TROUSERS_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_TROUSERS)),y)
TARGETS+=trousers
endif
