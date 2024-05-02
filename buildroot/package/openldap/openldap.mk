#############################################################
#
# openldap
#
#############################################################
OPENLDAP_VER:=2.3.32
OPENLDAP_SOURCE:=openldap-stable-20070110.tgz
OPENLDAP_SITE:=http://www.openldap.org/software/download/OpenLDAP/openldap-stable/
OPENLDAP_DIR:=$(BUILD_DIR)/openldap-$(OPENLDAP_VER)
OPENLDAP_CAT:=zcat
OPENLDAP_BINARY:=clients/tools/ldapsearch
OPENLDAP_TARGET_BINARY:=bin/ldapsearch

#Director related changes
ifeq ($(BR2_PACKAGE_DIRECTOR),y)
DIRECTOR_OPTIONS=--disable-slapd --disable-slurpd \
		--without-yielding-select --without-tls
endif

$(DL_DIR)/$(OPENLDAP_SOURCE):
	 $(WGET) -P $(DL_DIR) $(OPENLDAP_SITE)/$(OPENLDAP_SOURCE)

openldapd-source: $(DL_DIR)/$(OPENLDAP_SOURCE)

$(OPENLDAP_DIR)/.unpacked: $(DL_DIR)/$(OPENLDAP_SOURCE)
	$(OPENLDAP_CAT) $(DL_DIR)/$(OPENLDAP_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(OPENLDAP_DIR) package/openldap/ openldapd\*.patch
	touch $(OPENLDAP_DIR)/.unpacked

$(OPENLDAP_DIR)/.configured: $(OPENLDAP_DIR)/.unpacked
	(cd $(OPENLDAP_DIR); rm -rf config.cache; \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(filter-out -MMD,$(TARGET_CFLAGS))" \
		./configure \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
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
		$(DIRECTOR_OPTIONS) \
	);
	touch  $(OPENLDAP_DIR)/.configured

$(OPENLDAP_DIR)/$(OPENLDAP_BINARY): $(OPENLDAP_DIR)/.configured
	$(MAKE) -C $(OPENLDAP_DIR)

# This stuff is needed to work around GNU make deficiencies
$(TARGET_DIR)/$(OPENLDAP_TARGET_BINARY): $(OPENLDAP_DIR)/$(OPENLDAP_BINARY)
	@if [ -L $(TARGET_DIR)/$(OPENLDAP_TARGET_BINARY) ] ; then \
		rm -f $(TARGET_DIR)/$(OPENLDAP_TARGET_BINARY); fi;
	@if [ ! -f $(OPENLDAP_DIR)/$(OPENLDAP_BINARY) -o $(TARGET_DIR)/$(OPENLDAP_TARGET_BINARY) \
	-ot $(OPENLDAP_DIR)/$(OPENLDAP_BINARY) ] ; then \
	    set -x; \
	    rm -f $(TARGET_DIR)/$(OPENLDAP_TARGET_BINARY); \
	    cp -a $(OPENLDAP_DIR)/$(OPENLDAP_BINARY) $(TARGET_DIR)/$(OPENLDAP_TARGET_BINARY); fi ;

openldap: uclibc $(TARGET_DIR)/$(OPENLDAP_TARGET_BINARY)

openldap-clean:
	$(MAKE) DESTDIR=$(TARGET_DIR) -C $(OPENLDAP_DIR) uninstall
	-$(MAKE) -C $(OPENLDAP_DIR) clean

openldap-dirclean:
	rm -rf $(OPENLDAP_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_OPENLDAP)),y)
TARGETS+=openldap
endif
