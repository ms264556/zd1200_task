#############################################################
#
# pppd
#
#############################################################
ifdef PPPD_2_4_3
PPPD_SOURCE:=ppp-2.4.3.tar.gz
PPPD_DIR:=$(BUILD_DIR)/ppp-2.4.3
MYVERSION :=
else
PPPD_SOURCE:=ppp-2.4.4.tar.gz
PPPD_DIR:=$(BUILD_DIR)/ppp-2.4.4
MYVERSION := .2.4.4
endif
PPPD_SITE:=ftp://ftp.samba.org/pub/ppp
PPPD_CAT:=zcat
PPPD_BINARY:=pppd/pppd
PPPD_TARGET_BINARY:=usr/sbin/pppd

PPPDVERSION = $(shell awk -F '"' '/VERSION/ { print $$2; }' $(PPPD_DIR)/pppd/patchlevel.h)

$(DL_DIR)/$(PPPD_SOURCE):
	( if [ ! -f $(DL_DIR)/$(PPD_SOURCE) ] ; then\
		echo "$(DL_DIR)/$(PPD_SOURCE) not found"\
	fi\
	);
#	 $(WGET) -P $(DL_DIR) $(PPPD_SITE)/$(PPPD_SOURCE)

pppd-source: $(DL_DIR)/$(PPPD_SOURCE)

$(PPPD_DIR)/.unpacked: $(DL_DIR)/$(PPPD_SOURCE)
	$(PPPD_CAT) $(DL_DIR)/$(PPPD_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	$(SED) 's/ -DIPX_CHANGE -DHAVE_MMAP//' $(PPPD_DIR)/pppd/Makefile.linux
#	$(SED) 's/HAVE_MULTILINK=y/#HAVE_MULTILINK=y/' $(PPPD_DIR)/pppd/Makefile.linux
	$(SED) 's/FILTER=y/#FILTER=y/' $(PPPD_DIR)/pppd/Makefile.linux
	$(SED) 's/#ifdef DEBUGALL/#if 1/' $(PPPD_DIR)/pppd/pppd.h
	$(SED) 's,(INSTALL) -s,(INSTALL),' $(PPPD_DIR)/*/Makefile.linux
	$(SED) 's,(INSTALL) -s,(INSTALL),' $(PPPD_DIR)/pppd/plugins/*/Makefile.linux
	$(SED) 's/ -o root//' $(PPPD_DIR)/*/Makefile.linux
	$(SED) 's/ -g daemon//' $(PPPD_DIR)/*/Makefile.linux
	$(SED) 's/HAVE_TDB=y/#HAVE_TDB=y/' $(PPPD_DIR)/pppd/Makefile.linux
	$(SED) 's/(strlen(cmd) < 4/(strlen(cmd) < 4/' $(PPPD_DIR)/pppd/plugins/rp-pppoe/plugin.c
	$(SED) 's/padiAttempts++;/;/' $(PPPD_DIR)/pppd/plugins/rp-pppoe/discovery.c
	$(SED) 's/padrAttempts++;/;/' $(PPPD_DIR)/pppd/plugins/rp-pppoe/discovery.c
	$(SED) 's/timeout \*= 2;/;/' $(PPPD_DIR)/pppd/plugins/rp-pppoe/discovery.c
	$(SED) 's/#define PADI_TIMEOUT 5/#define PADI_TIMEOUT 2/' $(PPPD_DIR)/pppd/plugins/rp-pppoe/pppoe.h
	patch -p0 $(PPPD_DIR)/pppd/main.c < ${BASE_DIR}/package/pppd/padt$(MYVERSION).patch
	touch $(PPPD_DIR)/.unpacked

$(PPPD_DIR)/.configured: $(PPPD_DIR)/.unpacked
	(cd $(PPPD_DIR); rm -rf config.cache; \
		$(TARGET_CONFIGURE_OPTS) \
		./configure \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
		--prefix=$(TARGET_DIR)/usr \
		--sysconfdir=$(TARGET_DIR)/etc \
		--datadir=$(TARGET_DIR)/usr/share \
		--localstatedir=$(TARGET_DIR)/var \
		$(DISABLE_NLS) \
	);
	(cd $(PPPD_DIR); patch -p1 < ${BASE_DIR}/package/pppd/pppd-2.4.4-bcp.patch)
	(cd $(PPPD_DIR); /bin/cp -f $(PPPD_DIR)/pppd/bcp.h $(PPPD_DIR)/pppd/bcp.h.original)
	(cd $(PPPD_DIR); /bin/cp -f ${BASE_DIR}/package/pppd/bcp.h $(PPPD_DIR)/pppd/bcp.h)
	(cd $(PPPD_DIR); /bin/cp -f $(PPPD_DIR)/pppd/bcp.c $(PPPD_DIR)/pppd/bcp.c.original)
	(cd $(PPPD_DIR); /bin/cp -f ${BASE_DIR}/package/pppd/bcp.c $(PPPD_DIR)/pppd/bcp.c)
	(cd $(PPPD_DIR); /bin/cp -f ${PPPD_DIR}/pppd/auth.c $(PPPD_DIR)/pppd/auth.c.original)
	(cd $(PPPD_DIR); patch -p1 < ${BASE_DIR}/package/pppd/pppd-2.4.4-pidfiles.patch)
	touch  $(PPPD_DIR)/.configured

$(PPPD_DIR)/$(PPPD_BINARY): $(PPPD_DIR)/.configured
	$(MAKE) CC=$(TARGET_CC) AR=$(TARGET_CROSS)ar -C $(PPPD_DIR)/pppd
	$(MAKE) CC=$(TARGET_CC) AR=$(TARGET_CROSS)ar -C $(PPPD_DIR)/pppd/plugins/rp-pppoe

$(TARGET_DIR)/$(PPPD_TARGET_BINARY): $(PPPD_DIR)/$(PPPD_BINARY)
	$(STRIP) $(PPPD_DIR)/pppd/pppd
	$(STRIP) $(PPPD_DIR)/pppd/plugins/rp-pppoe/rp-pppoe.so
	$(MAKE) CC=$(TARGET_CC) AR=$(TARGET_CROSS)ar -C $(PPPD_DIR)/pppd install
	mkdir -p $(TARGET_DIR)/usr/lib/pppd/$(PPPDVERSION)
	cp -f $(PPPD_DIR)/pppd/plugins/rp-pppoe/rp-pppoe.so $(TARGET_DIR)/usr/lib/pppd/$(PPPDVERSION)
	ln -sf $(PPPDVERSION)/rp-pppoe.so $(TARGET_DIR)/usr/lib/pppd/
	rm -rf $(TARGET_DIR)/usr/share/locale $(TARGET_DIR)/usr/info \
		$(TARGET_DIR)/usr/man $(TARGET_DIR)/usr/share/doc \
		$(TARGET_DIR)/usr/share/man

pppd: uclibc $(TARGET_DIR)/$(PPPD_TARGET_BINARY)

pppd-clean:
	rm -f  $(TARGET_DIR)/usr/sbin/pppd
	rm -f  $(TARGET_DIR)/usr/sbin/chat
	rm -rf $(TARGET_DIR)/etc/ppp
	-$(MAKE) DESTDIR=$(TARGET_DIR)/usr CC=$(TARGET_CC) -C $(PPPD_DIR) uninstall
	-$(MAKE) -C $(PPPD_DIR) clean

pppd-dirclean:
	rm -rf $(PPPD_DIR)


#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_PPPD)),y)
TARGETS+=pppd
endif
