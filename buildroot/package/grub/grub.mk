#############################################################
#
# grub
#
#############################################################
GRUB_VER:=0.97
GRUB_SOURCE:=grub-$(GRUB_VER).tar.gz
GRUB_SITE:=ftp://alpha.gnu.org/gnu/grub
GRUB_DIR:=$(BUILD_DIR)/grub-$(GRUB_VER)
GRUB_CAT:=zcat
#GRUB_TARGET_DIR=$(BUILD_DIR)/grub_bld
GRUB_INSTALL_DIR:=$(GRUB_TARGET_DIR)
GRUB_EXTRA_DIR:=$(strip $(subst ",,$(BR2_PACKAGE_GRUB_COPYTO)))

GRUB_FLAG=--disable-ffs --disable-ufs2 --disable-minix --disable-vstafs --disable-jfs --disable-xfs --disable-iso9660 --disable-hercules --without-curses

ifneq ($(BR2_PACKAGE_GRUB_EXT2),y)
GRUB_FLAG+=--disable-ext2fs
endif
ifneq ($(BR2_PACKAGE_GRUB_REISER),y)
GRUB_FLAG+=--disable-reiserfs
endif
ifneq ($(BR2_PACKAGE_GRUB_FAT),y)
GRUB_FLAG+=--disable-fat
endif
ifeq ($(BR2_PACKAGE_GRUB_E1000),y)
GRUB_FLAG+=--enable-e1000
endif

#ifeq ($(BR2_PACKAGE_GRUB_USBTOOL),y)
GRUB_INSTALL_DIR+=$(BR2_USBTOOL_DIR)
#endif
ifneq ($(GRUB_EXTRA_DIR),)
GRUB_INSTALL_DIR+=$(GRUB_EXTRA_DIR)
endif

$(DL_DIR)/$(GRUB_SOURCE):
	 $(WGET) -P $(DL_DIR) $(GRUB_SITE)/$(GRUB_SOURCE)

grub-source: $(DL_DIR)/$(GRUB_SOURCE)

$(GRUB_DIR)/.unpacked: $(DL_DIR)/$(GRUB_SOURCE)
	$(GRUB_CAT) $(DL_DIR)/$(GRUB_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(GRUB_DIR) package/grub/ grub\*.patch
	touch $(GRUB_DIR)/.unpacked

$(GRUB_DIR)/.configured: $(GRUB_DIR)/.unpacked
	(cd $(GRUB_DIR); rm -rf config.cache; \
		cp -af configure configure.old; \
		sed "s/\( VERSION=.*\)'/\1\.$(BR2_PACKAGE_GRUB_BUILD)'/g" configure.old > configure; \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		./configure \
		$(GRUB_FLAG) \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
		--prefix=/usr \
		--exec-prefix=/ \
		--bindir=/sbin \
		--sbindir=/sbin \
		--libexecdir=/boot \
		--sysconfdir=/etc \
		--datadir=/usr/share/misc \
		--localstatedir=/var \
		--mandir=/usr/man \
		--infodir=/usr/info \
	);
	touch  $(GRUB_DIR)/.configured

grub-build: $(GRUB_DIR)/.configured
	$(MAKE) CC=$(TARGET_CC) -C $(GRUB_DIR)

grub-install:
	for d in $(GRUB_INSTALL_DIR); do \
	$(MAKE) DESTDIR=$$d CC=$(TARGET_CC) -C $(GRUB_DIR) install-exec; \
	done;
ifneq ($(GRUB_EXTRA_DIR),)
	mkdir -p $(GRUB_EXTRA_DIR)/grub
	cp -f package/grub/grub.conf $(GRUB_EXTRA_DIR)/grub/
endif

grub: uclibc grub-build grub-install 
	@echo "$@ done.."

grub-clean:
	for d in $(GRUB_INSTALL_DIR); do \
	$(MAKE) DESTDIR=$$d CC=$(TARGET_CC) -C $(GRUB_DIR) uninstall; \
	cd $$d; rm -f grub; \
	done;
	-$(MAKE) -C $(GRUB_DIR) clean

grub-dirclean:
	rm -rf $(GRUB_DIR)
#	rm -rf $(GRUB_TARGET_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_GRUB)),y)
TARGETS+=grub
endif
