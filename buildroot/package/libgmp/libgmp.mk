#############################################################
#
# libgmp
#
#############################################################
LIBGMP_VER:=4.2.1
LIBGMP_SOURCE:=gmp-${LIBGMP_VER}.tar.gz
LIBGMP_SITE:=ftp://ftp.gnu.org/gnu/gmp
LIBGMP_CAT:=zcat
LIBGMP_DIR:=$(BUILD_DIR)/gmp-${LIBGMP_VER}
LIBGMP_BINARY:=libgmp.a


$(DL_DIR)/$(LIBGMP_SOURCE):
	 $(WGET) -P $(DL_DIR) $(LIBGMP_SITE)/$(LIBGMP_SOURCE)

libgmp-source: $(DL_DIR)/$(LIBGMP_SOURCE)

$(LIBGMP_DIR)/.unpacked: $(DL_DIR)/$(LIBGMP_SOURCE)
	$(LIBGMP_CAT) $(DL_DIR)/$(LIBGMP_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	touch $(LIBGMP_DIR)/.unpacked

$(LIBGMP_DIR)/.configured: $(LIBGMP_DIR)/.unpacked
	(cd $(LIBGMP_DIR); rm -rf config.cache; \
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
	touch  $(LIBGMP_DIR)/.configured

$(LIBGMP_DIR)/.libs/$(LIBGMP_BINARY): $(LIBGMP_DIR)/.configured
	$(MAKE) CC=$(TARGET_CC) -C $(LIBGMP_DIR)

$(STAGING_DIR)/lib/$(LIBGMP_BINARY): $(LIBGMP_DIR)/.libs/$(LIBGMP_BINARY)
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
	    -C $(LIBGMP_DIR) install;

# @@@ do we need both dynamic and static libraries??
$(TARGET_DIR)/lib/$(LIBGMP_BINARY): $(STAGING_DIR)/lib/$(LIBGMP_BINARY)
	cp -a $(STAGING_DIR)/lib/$(LIBGMP_BINARY) $(TARGET_DIR)/lib/
	cp -a $(STAGING_DIR)/lib/libgmp.so $(TARGET_DIR)/lib/
	cp -a $(STAGING_DIR)/lib/libgmp.so.3 $(TARGET_DIR)/lib/
	cp -a $(STAGING_DIR)/lib/libgmp.so.3.4.1 $(TARGET_DIR)/lib/
	$(STRIP) --strip-unneeded $(TARGET_DIR)/lib/$(LIBGMP_BINARY)

libgmp: uclibc $(TARGET_DIR)/lib/$(LIBGMP_BINARY)

libgmp-clean:
	rm -f $(TARGET_DIR)/lib/$(LIBGMP_BINARY)
	-$(MAKE) -C $(LIBGMP_DIR) clean

libgmp-dirclean:
	rm -rf $(LIBGMP_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_LIBGMP)),y)
TARGETS+=libgmp
endif
