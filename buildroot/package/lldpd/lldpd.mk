#############################################################
#
# lldpd
#
#############################################################
LLDPD_VER      := 0.7.1
LLDPD_BINARY   := lldpd
LLDPD_DIR      := $(BUILD_DIR)/lldpd-$(LLDPD_VER)
LLDPD_SOURCE   := lldpd-$(LLDPD_VER).tar.gz

# Fix kernel cross compile, source dir, readline include path
LLDPD_MAKEOPTS += CROSS_COMPILE=$(TARGET_CROSS)
LLDPD_MAKEOPTS += DESTDIR=$(TARGET_DIR)

$(LLDPD_DIR)/.unpacked:
	zcat $(DL_DIR)/$(LLDPD_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(LLDPD_DIR) package/lldpd/ *lldpd.patch
	touch $@

$(LLDPD_DIR)/.configured: $(LLDPD_DIR)/.unpacked
	(cd $(LLDPD_DIR); \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		./configure \
		--host=$(GNU_TARGET_NAME) \
	);
	touch $@

$(LLDPD_DIR)/$(LLDPD_BINARY): $(LLDPD_DIR)/.configured
	$(MAKE) CPPFLAGS="-I$(BUILD_INCLUDE)" LDFLAGS="-L$(LIBRSM_LIBDIR) -lIpc -lRutil -ldl" -C $(LLDPD_DIR) $(LLDPD_MAKEOPTS)
	$(STRIP) $(LLDPD_DIR)/src/daemon/lldpd

$(TARGET_DIR)/$(LLDPD_BINARY): $(LLDPD_DIR)/$(LLDPD_BINARY)
	cp $(LLDPD_DIR)/src/daemon/lldpd ${TARGET_DIR}/usr/sbin
	cp $(LLDPD_DIR)/src/client/.libs/lldpcli ${TARGET_DIR}/usr/sbin
	cp $(LLDPD_DIR)/src/lib/.libs/liblldpctl.so.0 ${TARGET_DIR}/usr/lib

lldpd: uclibc $(TARGET_DIR)/$(LLDPD_BINARY)

lldpd-clean:
	rm -f $(TARGET_DIR)/usr/sbin/lldpd
	-$(MAKE) -C $(LLDPD_DIR) clean

lldpd-dirclean:
	if [ -h $(TARGET_CROSS)install ]; then rm -f $(TARGET_CROSS)install; fi
	rm -rf $(LLDPD_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_LLDPD)),y)
TARGETS+=lldpd
endif
