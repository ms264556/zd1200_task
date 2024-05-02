#############################################################
#
# libglib1.2
#
#############################################################
LIBICONV_SOURCE:=libiconv-1.13.1.tar.gz
LIBICONV_SITE:=http://ftp.gnu.org/pub/gnu/libiconv/
LIBICONV_CAT:=zcat
LIBICONV_DIR:=$(BUILD_DIR)/libiconv-1.13.1
LIBICONV_BINARY:=libiconv.so.2.5.0

$(DL_DIR)/$(LIBICONV_SOURCE):
	 $(WGET) -P $(DL_DIR) $(LIBICONV_SITE)/$(LIBICONV_SOURCE)

libiconv-source: $(DL_DIR)/$(LIBICONV_SOURCE)

$(LIBICONV_DIR)/.unpacked: $(DL_DIR)/$(LIBICONV_SOURCE)
	$(LIBICONV_CAT) $(DL_DIR)/$(LIBICONV_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(LIBICONV_DIR) package/libiconv/ \*.patch*
	$(CONFIG_UPDATE) $(LIBICONV_DIR)
	touch $(LIBICONV_DIR)/.unpacked

$(LIBICONV_DIR)/.configured: $(LIBICONV_DIR)/.unpacked
	(cd $(LIBICONV_DIR); rm -rf config.cache; \
		$(TARGET_CONFIGURE_OPTS) \
		./configure \
		--host=$(REAL_GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
		--prefix=/usr \
		--exec-prefix=/usr \
		--bindir=/usr/bin \
		--sbindir=/usr/sbin \
		--libexecdir=/usr/lib \
		--sysconfdir=/etc \
		--datadir=/usr/share \
		--localstatedir=/var \
		--mandir=/usr/man \
		--infodir=/usr/info \
		$(DISABLE_NLS) \
		--enable-shared \
	);
	touch  $(LIBICONV_DIR)/.configured

$(LIBICONV_DIR)/.libs/$(LIBICONV_BINARY): $(LIBICONV_DIR)/.configured
	$(MAKE) CC=$(TARGET_CC) -C $(LIBICONV_DIR)

$(STAGING_DIR)/lib/$(LIBICONV_BINARY): $(LIBICONV_DIR)/.libs/$(LIBICONV_BINARY)
	$(MAKE) prefix=$(STAGING_DIR) \
	    exec_prefix=$(STAGING_DIR) \
	    bindir=$(STAGING_DIR)/bin \
	    sbindir=$(STAGING_DIR)/sbin \
	    libexecdir=$(STAGING_DIR)/libexec \
	    datadir=$(STAGING_DIR)/share \
	    sysconfdir=$(STAGING_DIR)/etc \
	    sharedstatedir=$(STAGING_DIR)/com \
	    localstatedir=$(STAGING_DIR)/var \
	    libdir=$(STAGING_DIR)/lib \
	    includedir=$(STAGING_DIR)/include \
	    oldincludedir=$(STAGING_DIR)/include \
	    infodir=$(STAGING_DIR)/info \
	    mandir=$(STAGING_DIR)/man \
	    -C $(LIBICONV_DIR) install;

$(TARGET_DIR)/lib/$(LIBICONV_BINARY): $(STAGING_DIR)/lib/$(LIBICONV_BINARY)
	cp -a $(STAGING_DIR)/lib/$(LIBICONV_BINARY) $(TARGET_DIR)/lib/
	cp -a $(STAGING_DIR)/lib/libiconv.so $(TARGET_DIR)/lib/
	cp -a $(STAGING_DIR)/lib/libiconv.so.2 $(TARGET_DIR)/lib/
	$(STRIP) --strip-unneeded $(TARGET_DIR)/lib/$(LIBICONV_BINARY)

libiconv: uclibc $(TARGET_DIR)/lib/$(LIBICONV_BINARY)

libiconv-clean:
	rm -f $(TARGET_DIR)/lib/$(LIBICONV_BINARY)
	-$(MAKE) -C $(LIBICONV_DIR) clean

libiconv-dirclean:
	rm -rf $(LIBICONV_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_LIBICONV)),y)
TARGETS+=libiconv
endif
