#############################################################
#
# flex
#
#############################################################

FLEX_VERSION:=2.5.35
FLEX_SOURCE:=flex-$(FLEX_VERSION).tar.gz
FLEX_SITE:=http://prdownloads.sourceforge.net/flex
FLEX_DIR:=$(BUILD_DIR)/flex-$(FLEX_VERSION)
FLEX_LIB:=libfl.a
FLEX_TARGET_LIB:=usr/lib/libfl.a

$(DL_DIR)/$(FLEX_SOURCE):
	 $(WGET) -P $(DL_DIR) $(FLEX_SITE)/$(FLEX_SOURCE)

$(FLEX_DIR)/.unpacked: $(DL_DIR)/$(FLEX_SOURCE)
	zcat $(DL_DIR)/$(FLEX_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	touch $(FLEX_DIR)/.unpacked

$(FLEX_DIR)/.configured: $(FLEX_DIR)/.unpacked
	(cd $(FLEX_DIR); \
		$(TARGET_CONFIGURE_OPTS) \
		ac_cv_func_malloc_0_nonnull=yes \
		ac_cv_func_realloc_0_nonnull=yes \
		./configure \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
	);
	touch  $(FLEX_DIR)/.configured

$(FLEX_DIR)/$(FLEX_LIB): $(FLEX_DIR)/.configured
	$(MAKE) -C $(FLEX_DIR)

$(TARGET_DIR)/$(FLEX_TARGET_LIB): $(FLEX_DIR)/$(FLEX_LIB)
	install -T $(FLEX_DIR)/$(FLEX_LIB) $(TARGET_DIR)/$(FLEX_TARGET_LIB)
#	$(STRIP) $(TARGET_DIR)/$(FLEX_TARGET_LIB)

flex: uclibc $(TARGET_DIR)/$(FLEX_TARGET_LIB)

flex-clean:
	rm -f $(TARGET_DIR)/$(FLEX_TARGET_LIB)
	-$(MAKE) -C $(FLEX_DIR) clean

flex-dirclean:
	rm -rf $(FLEX_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_FLEX)),y)
TARGETS+=flex
endif
