#############################################################
#
# libccl (Configuration File parsing library)
#
#############################################################

LIBCCL_VER:=0.1.1
LIBCCL_DIR:=$(BUILD_DIR)/libccl
LIBCCL_SITE:=ftp://sbooth.org/
LIBCCL_SOURCE:=ccl-$(LIBCCL_VER).tar.gz
LIBCCL_CAT:=zcat
LIBCCL_BINARY:=libccl.so

$(DL_DIR)/$(LIBCCL_SOURCE):
	 $(WGET) -P $(DL_DIR) $(LIBCCL_SITE)/$(LIBCCL_SOURCE)

libccl-source: $(DL_DIR)/$(LIBCCL_SOURCE)

$(LIBCCL_DIR)/.unpacked: $(DL_DIR)/$(LIBCCL_SOURCE)
	$(LIBCCL_CAT) $(DL_DIR)/$(LIBCCL_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	mv $(BUILD_DIR)/ccl-$(LIBCCL_VER) $(LIBCCL_DIR)
	touch $(LIBCCL_DIR)/.unpacked

$(LIBCCL_DIR)/.configured: $(LIBCCL_DIR)/.unpacked
	( \
		(cd $(LIBCCL_DIR); rm -rf config.cache; \
			$(TARGET_CONFIGURE_OPTS) \
			CFLAGS="$(TARGET_CFLAGS)" \
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
				--enable-static=no \
				--enable-shared=yes \
		); \
	)
	touch $(LIBCCL_DIR)/.configured

$(LIBCCL_DIR)/.libs/$(LIBCCL_BINARY): $(LIBCCL_DIR)/.configured
	$(MAKE) CC=$(TARGET_CC) -C $(LIBCCL_DIR)/ccl

$(STAGING_DIR)/lib/$(LIBCCL_BINARY): $(LIBCCL_DIR)/.libs/$(LIBCCL_BINARY)
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
	    -C $(LIBCCL_DIR)/ccl install;

$(TARGET_DIR)/lib/$(LIBCCL_BINARY): $(STAGING_DIR)/lib/$(LIBCCL_BINARY)
	cp -dpf $(STAGING_DIR)/lib/$(LIBCCL_BINARY)* $(TARGET_DIR)/lib/
	$(STRIP) --strip-unneeded $(TARGET_DIR)/lib/$(LIBCCL_BINARY)

LIBCCL_SRC=$(BUILD_DIR)/libccl
LIBCCL_INC=$(LIBCCL_SRC)/ccl
LIBCCL_LIB=$(TARGET_DIR)/lib/$(LIBCCL_BINARY)
export LIBCCL_SRC LIBCCL_INC LIBCCL_LIB

libccl: uclibc $(TARGET_DIR)/lib/$(LIBCCL_BINARY)

libccl-clean:
	-$(MAKE) -C $(LIBCCL_DIR) clean

libccl-dirclean:
	rm -rf $(LIBCCL_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_LIBCCL)),y)
TARGETS+=libccl
endif
