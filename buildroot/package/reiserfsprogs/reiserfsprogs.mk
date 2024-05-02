#############################################################
#
# reiserfsprogs
#
#############################################################
REISERFSPROGS_VER:=3.6.6
REISERFSPROGS_SOURCE=reiserfsprogs-$(REISERFSPROGS_VER).tar.gz
#REISERFSPROGS_SITE=http://$(BR2_SOURCEFORGE_MIRROR).dl.sourceforge.net/sourceforge/reiserfsprogs
REISERFSPROGS_DIR=$(BUILD_DIR)/reiserfsprogs-$(REISERFSPROGS_VER)
REISERFSPROGS_CAT:=zcat
REISERFSPROGS_BINARY:=mkreiserfs/mkreiserfs
REISERFSPROGS_TARGET_BINARY:=sbin/mkreiserfs
REISERFSPROGS_BIN_STRIP:=fsck/reiserfsck mkreiserfs/mkreiserfs resize_reiserfs/resize_reiserfs tune/reiserfstune debugreiserfs/debugreiserfs

############################################################
#
# reiserfsprogs for target system
#
############################################################
REISERPROGS_BIN_INSTALL=
ifeq ($(BR2_PACKAGE_REISERFSPROGS_SYSTEM),y)
REISERPROGS_BIN_INSTALL+=$(TARGET_DIR)
endif
ifeq ($(BR2_PACKAGE_REISERFSPROGS_USBTOOL),y)
REISERPROGS_BIN_INSTALL+=$(USB_TOOL_BUILD_DIR)
endif

$(DL_DIR)/$(REISERFSPROGS_SOURCE):
#	 $(WGET) -P $(DL_DIR) $(REISERFSPROGS_SITE)/$(REISERFSPROGS_SOURCE)

reiserfsprogs-source: $(DL_DIR)/$(REISERFSPROGS_SOURCE)

$(REISERFSPROGS_DIR)/.unpacked: $(DL_DIR)/$(REISERFSPROGS_SOURCE)
	$(REISERFSPROGS_CAT) $(DL_DIR)/$(REISERFSPROGS_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(REISERFSPROGS_DIR) package/reiserfsprogs/ reiserfsprogs\*.patch
	touch $(REISERFSPROGS_DIR)/.unpacked

$(REISERFSPROGS_DIR)/.configured: $(REISERFSPROGS_DIR)/.unpacked
	(cd $(REISERFSPROGS_DIR); rm -rf config.cache; \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		./configure \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
		--with-cc=$(TARGET_CC) \
		--with-linker=$(TARGET_CROSS)ld \
		--prefix=/usr \
		--exec-prefix=/usr \
		--bindir=/bin \
		--sbindir=/sbin \
		--libexecdir=/usr/lib \
		--sysconfdir=/etc \
		--datadir=/usr/share \
		--localstatedir=/var \
		--mandir=/usr/man \
		--infodir=/usr/info \
	);
	touch  $(REISERFSPROGS_DIR)/.configured

$(REISERFSPROGS_DIR)/$(REISERFSPROGS_BINARY): $(REISERFSPROGS_DIR)/.configured
	$(MAKE1) PATH=$(TARGET_PATH) -C $(REISERFSPROGS_DIR)
	( \
		cd $(REISERFSPROGS_DIR) ; \
		$(STRIP) $(REISERFSPROGS_BIN_STRIP) ; \
	)
	touch -c $(REISERFSPROGS_DIR)/$(REISERFSPROGS_BINARY)

$(USB_TOOL_BUILD_DIR)/$(REISERFSPROGS_TARGET_BINARY): $(REISERFSPROGS_DIR)/$(REISERFSPROGS_BINARY)
	for d in $(REISERPROGS_BIN_INSTALL); do \
		$(MAKE1) PATH=$(TARGET_PATH) DESTDIR=$$d -C $(REISERFSPROGS_DIR) install; \
		rm -rf $$d/share/locale $$d/usr/info \
			$$d/usr/man $$d/usr/share/doc; \
		touch -c $$d/$(REISERFSPROGS_TARGET_BINARY); \
	done;

reiserfsprogs: uclibc $(USB_TOOL_BUILD_DIR)/$(REISERFSPROGS_TARGET_BINARY)

reiserfsprogs-clean:
	for d in $(REISERPROGS_BIN_INSTALL); do \
		$(MAKE1) DESTDIR=$$d CC=$(TARGET_CC) -C $(REISERFSPROGS_DIR) uninstall; \
	done;

	-$(MAKE1) -C $(REISERFSPROGS_DIR) clean

reiserfsprogs-dirclean:
	rm -rf $(REISERFSPROGS_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_REISERFSPROGS)),y)
TARGETS+=reiserfsprogs
endif

