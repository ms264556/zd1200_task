#############################################################
#
# lipe tools
#
#############################################################
LIPE_SOURCE=lipe-1.12.tar.bz2
LIPE_DIR=$(BUILD_DIR)/lipe-1.12
LIPE_BINARY=lipe
LIPE_TARGET_BINARY=usr/bin/lipe

ifeq ($(USB_TOOL_BUILD_DIR),)
LIPE_DESTDIR := $(TARGET_DIR)
else
LIPE_DESTDIR := $(USB_TOOL_BUILD_DIR)
endif

$(LIPE_DIR)/.source:
	bzcat $(DL_DIR)/$(LIPE_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(LIPE_DIR) package/lipe/ lipe-1.12\*.patch
	touch $(LIPE_DIR)/.source

$(LIPE_DIR)/.configured: $(LIPE_DIR)/.source
	(cd $(LIPE_DIR); \
		$(TARGET_CONFIGURE_OPTS) \
		ac_cv_func_malloc_0_nonnull=yes \
		./configure \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
		--prefix=/usr \
		--sysconfdir=/etc \
		--disable-ncurses \
		--disable-gtk2 \
		--disable-glibtest \
		--disable-gtktest \
	);
	touch $(LIPE_DIR)/.configured;

lipe: uclibc $(LIPE_DIR)/.configured
	$(MAKE) CC=$(TARGET_CC) LD=$(TARGET_CROSS)ld \
		-C $(LIPE_DIR)
	@mkdir -p $(LIPE_DESTDIR)/usr/sbin
	@cp -f $(LIPE_DIR)/src/$(LIPE_BINARY) $(LIPE_DESTDIR)/usr/sbin

lipe-clean:
	-$(MAKE) -C $(LIPE_DIR) clean

lipe-dirclean:
	rm -rf $(LIPE_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_LIPE)),y)
TARGETS+=lipe
endif
