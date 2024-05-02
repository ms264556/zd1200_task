#############################################################
#
# ipmitool
#
#############################################################
IPMITOOL_VER:=1.8.11
IPMITOOL_SOURCE:=ipmitool-$(IPMITOOL_VER).tar.gz
IPMITOOL_SITE:=http://sourceforge.net/projects/ipmitool/files/ipmitool/$(IPMITOOL_VER)
IPMITOOL_DIR:=$(BUILD_DIR)/ipmitool-$(IPMITOOL_VER)
IPMITOOL_CAT:=zcat
IPMITOOL_INSTALL_DIR:=$(TARGET_DIR)/usr/sbin
IPMITOOL_BIN:=ipmitool
IPMITOOL_FLAG="ac_cv_func_malloc_0_nonnull=yes"

$(DL_DIR)/$(IPMITOOL_SOURCE):
	 $(WGET) -P $(DL_DIR) $(IPMITOOL_SITE)/$(IPMITOOL_SOURCE)

ipmitool-source: $(DL_DIR)/$(IPMITOOL_SOURCE)

$(IPMITOOL_DIR)/.unpacked: $(DL_DIR)/$(IPMITOOL_SOURCE)
	$(IPMITOOL_CAT) $(DL_DIR)/$(IPMITOOL_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(IPMITOOL_DIR) package/ipmitool/ ipmitool\*.patch
	touch $(IPMITOOL_DIR)/.unpacked

$(IPMITOOL_DIR)/.configured: $(IPMITOOL_DIR)/.unpacked
	(cd $(IPMITOOL_DIR); rm -rf config.cache; \
		cp -af configure configure.old; \
		echo $(IPMITOOL_FLAG)>config.cache; \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		./configure \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
		--cache-file=config.cache \
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
	touch  $(IPMITOOL_DIR)/.configured

ipmitool-build: $(IPMITOOL_DIR)/.configured
	$(MAKE) CC=$(TARGET_CC) -C $(IPMITOOL_DIR)

ipmitool-install:
	for d in $(IPMITOOL_INSTALL_DIR); do \
		cp $(IPMITOOL_DIR)/src/$(IPMITOOL_BIN) $$d; \
	done;

ipmitool: uclibc ipmitool-build ipmitool-install 
	@echo "$@ done.."

ipmitool-clean:
	for d in $(IPMITOOL_INSTALL_DIR); do \
		rm $$d/$(IPMITOOL_BIN); \
	done;
	-$(MAKE) -C $(IPMITOOL_DIR) clean

ipmitool-dirclean:
	rm -rf $(IPMITOOL_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_IPMITOOL)),y)
TARGETS+=ipmitool
endif
