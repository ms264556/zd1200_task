#############################################################
#
# libusb
#
#############################################################

# TARGETS
LIBUSB_VER                := 0.1.12
LIBUSB_DIR                := ${BUILD_DIR}/libusb-${LIBUSB_VER}
LIBUSB_SOURCE             := libusb-${LIBUSB_VER}.tar.gz
LIBUSB_BIN                := libusb.so
LIBUSB_VERSIONED_BIN      := libusb-0.1.so.*

# Build and install libusb 
$(LIBUSB_DIR)/.unpacked: $(DL_DIR)/$(LIBUSB_SOURCE)
	zcat $(DL_DIR)/$(LIBUSB_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	touch $(LIBUSB_DIR)/.unpacked

$(LIBUSB_DIR)/.configured: $(LIBUSB_DIR)/.unpacked
	(cd $(LIBUSB_DIR); sed -i.orig 's|ac_cv_c_bigendian=unknown|ac_cv_c_bigendian=yes|' configure)
	(cd $(LIBUSB_DIR); sed -i.orig 's|^available_tags=|vailable_tags=CXX|' configure)
	(cd $(LIBUSB_DIR); \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		./configure \
		CXX=$(TARGET_CROSS)cpp \
		--host=$(GNU_TARGET_NAME) \
		--with-tags=CXX \
		--prefix=/usr \
	);
	touch $(LIBUSB_DIR)/.configured

$(LIBUSB_DIR)/$(LIBUSB_BIN): $(LIBUSB_DIR)/.configured
	$(MAKE) -C $(LIBUSB_DIR) libusb.la
	$(STRIP) $(LIBUSB_DIR)/.libs/$(LIBUSB_BIN)
	cp $(LIBUSB_DIR)/.libs/$(LIBUSB_BIN) ${TARGET_DIR}/usr/lib
	cp $(LIBUSB_DIR)/.libs/$(LIBUSB_VERSIONED_BIN) ${TARGET_DIR}/usr/lib

libusb: uclibc $(LIBUSB_DIR)/$(LIBUSB_BIN)

libusb-clean:
	rm -f $(TARGET_DIR)/usr/lib/$(LIBUSB_BIN)
	-$(MAKE) -C $(LIBUSB_DIR) clean

libusb-dirclean:
	rm -rf $(LIBUSB_DIR)

#############################################################

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_LIBUSB)),y)
TARGETS+=libusb
endif


