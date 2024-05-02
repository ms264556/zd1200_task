#############################################################
#
# bc
#
#############################################################
BC_VER:=1.06
BC_SOURCE:=bc-$(BC_VER).tar.gz
BC_SITE:=http://ftp.gnu.org/gnu/bc/
BC_CAT:=zcat
BC_DIR:=$(BUILD_DIR)/bc-$(BC_VER)
BC_TARGET_BINARY:=sbin
BC_BINARY:=bc

$(DL_DIR)/$(BC_SOURCE):
	 $(WGET) -P $(DL_DIR) $(BC_SITE)/$(BC_SOURCE)

bc-source: $(DL_DIR)/$(BC_SOURCE)

$(BC_DIR)/.unpacked: $(DL_DIR)/$(BC_SOURCE)
	$(BC_CAT) $(DL_DIR)/$(BC_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	touch $(BC_DIR)/.unpacked

$(BC_DIR)/.configured: $(BC_DIR)/.unpacked
	(cd $(BC_DIR); rm -rf config.cache; \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		./configure \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
		--prefix=/usr \
		--exec-prefix=/usr \
		--bindir=/sbin \
		--sbindir=/usr/sbin \
		--libexecdir=/usr/lib \
		--sysconfdir=/etc \
		--datadir=/usr/share \
		--localstatedir=/var \
		--mandir=/usr/man \
		--infodir=/usr/info \
		$(DISABLE_NLS) \
		$(DISABLE_LARGEFILE) \
	);
	touch  $(BC_DIR)/.configured

$(BC_DIR)/$(BC_BINARY): $(BC_DIR)/.configured
	$(MAKE) CC=$(TARGET_CC) -C $(BC_DIR)

$(TARGET_DIR)/$(BC_TARGET_BINARY): $(BC_DIR)/$(BC_BINARY)
	$(MAKE) DESTDIR=$(TARGET_DIR) CC=$(TARGET_CC) -C $(BC_DIR) install
	rm -rf $(TARGET_DIR)/share/locale $(TARGET_DIR)/usr/info \
		$(TARGET_DIR)/usr/man $(TARGET_DIR)/usr/share/doc

bc: uclibc $(TARGET_DIR)/$(BC_TARGET_BINARY)

bc-clean:
	$(MAKE) DESTDIR=$(TARGET_DIR) CC=$(TARGET_CC) -C $(BC_DIR) uninstall
	-$(MAKE) -C $(BC_DIR) clean

bc-dirclean:
	rm -rf $(BC_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_BC)),y)
TARGETS+=bc
endif
