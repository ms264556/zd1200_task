#############################################################
#
# ipmiutil
#
#############################################################
IPMIUTIL_VER:=2.7.2
IPMIUTIL_SOURCE:=ipmiutil-$(IPMIUTIL_VER).tar.gz
IPMIUTIL_SITE:=http://prdownloads.sourceforge.net/ipmiutil/
IPMIUTIL_DIR:=$(BUILD_DIR)/ipmiutil-$(IPMIUTIL_VER)
IPMIUTIL_CAT:=zcat
IPMIUTIL_INSTALL_DIR:=$(TARGET_DIR)/usr/sbin
IPMIUTIL_LIB_INSTALL_DIR:=$(TARGET_DIR)/usr/lib
IPMIUTIL_BIN:=ipmiutil
IPMIUTIL_LIB:=libipmiutil.so


$(DL_DIR)/$(IPMIUTIL_SOURCE):
	 $(WGET) -P $(DL_DIR) $(IPMIUTIL_SITE)/$(IPMIUTIL_SOURCE)

ipmiutil-source: $(DL_DIR)/$(IPMIUTIL_SOURCE)

$(IPMIUTIL_DIR)/.unpacked: $(DL_DIR)/$(IPMIUTIL_SOURCE)
	$(IPMIUTIL_CAT) $(DL_DIR)/$(IPMIUTIL_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(IPMIUTIL_DIR) package/ipmiutil/ ipmiutil\*.patch
	sed -i 's/ALLOW_GPL/ALLOW_GNU/' $(IPMIUTIL_DIR)/configure
	touch $(IPMIUTIL_DIR)/.unpacked

#
# Note: 6/2012, we stop building broken MD2 hash into openssl
# Here, we use '--enable-gpl' so ipmiutil will bundle a bit of GPL (util/md2.h)
# This changes its license from BSD to GPL.
# buildroot/gpl.mk is updated to reflect this.
#
# Note the 'sed' above, as configure script is broken (defining wrong symbol)
#
$(IPMIUTIL_DIR)/.configured: $(IPMIUTIL_DIR)/.unpacked
	(cp -af $(BASE_DIR)/package/ipmiutil/util/Makefile.in $(IPMIUTIL_DIR)/util/; \
		cd $(IPMIUTIL_DIR); rm -rf config.cache; \
		cp -af configure configure.old; \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		./configure \
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
		--enable-gpl \
	);
	touch  $(IPMIUTIL_DIR)/.configured

ipmiutil-build: $(IPMIUTIL_DIR)/.configured
	$(MAKE) CC=$(TARGET_CC) -C $(IPMIUTIL_DIR)

ipmiutil-install:
	for d in $(IPMIUTIL_INSTALL_DIR); do \
		cp $(IPMIUTIL_DIR)/util/$(IPMIUTIL_BIN) $$d; \
	done;
	cp $(IPMIUTIL_DIR)/util/$(IPMIUTIL_LIB) $(IPMIUTIL_LIB_INSTALL_DIR)

ipmiutil: uclibc ipmiutil-build ipmiutil-install 
	@echo "$@ done.."

ipmiutil-clean:
	for d in $(IPMIUTIL_INSTALL_DIR); do \
		rm -f $$d/$(IPMIUTIL_BIN); \
	done;
	for d in $(IPMIUTIL_LIB_INSTALL_DIR); do \
		rm -f $$d/$(IPMIUTIL_LIB); \
	done;
	-$(MAKE) -C $(IPMIUTIL_DIR) clean

ipmiutil-dirclean:
	rm -rf $(IPMIUTIL_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_IPMIUTIL)),y)
TARGETS+=ipmiutil
endif
