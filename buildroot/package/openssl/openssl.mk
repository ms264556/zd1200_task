#############################################################
#
# openssl
#
#############################################################

ifneq ($(subst ",,$(strip $(BR2_PACKAGE_OPENSSL_VERSION))),)
OPENSSL_VER       :=$(subst ",,$(strip $(BR2_PACKAGE_OPENSSL_VERSION)))
endif

OPENSSL_MAJ_VER   := $(shell echo $(OPENSSL_VER) | tr -d [:alpha:])

OPENSSL_SITE      :=http://www.openssl.org/source
OPENSSL_SOURCE    :=openssl-$(OPENSSL_VER).tar.gz
OPENSSL_DIR       :=$(BUILD_DIR)/openssl-$(OPENSSL_VER)
OPENSSL_INCLUDE   :=$(OPENSSL_DIR)/include


ifneq ($(strip $(BR2_PACKAGE_COOVACHILLI)),y)
ifneq ($(COOVACHILLI_VERSION),$(COOVACHILLI_VERSION_2))
OPENSSL_NO_LIST += no-engine # why does coovachilli 1.2.5 needs openssl engine?
endif
endif


ifneq ($(strip $(BR2_PACKAGE_CURL)),y)
OPENSSL_NO_LIST += no-ssl2 # needed for curl
endif


ifneq ($(strip $(BR2_PACKAGE_DIRECTOR)),y)
OPENSSL_NO_LIST += no-threads # needed by ZD appWeb server
endif


ifneq ($(strip $(BR2_PACKAGE_OPENSSL_PKCS12)),y)
OPENSSL_NO_LIST += no-rc2 # pkcs12 file format support requires rc2
endif


#
# Hint: see openssl's util for these options
#
# Also:
# # find ./ -name "*.c" | xargs grep "\#if"  |  grep OPENSSL_NO_ |  grep -v defined  | cut -d ' ' -f2 | sort -ui 
#

# Note 1: md4 is necessary for mschapv2 (stamgr, coovachilli)
# Note 2: rc4-sha1 combo is default for gmail https (best practice) (see beast attack)
# Note 3: des is needed by curl & netsnmp
# Note 4: no-ssl3 to be investigated; does not seem to take effect; needed by tr069 today

#
# openssl's Makefile is broken as the following no-xxx or -D.._.NO_yyy options 
# when present will result in compile failure. 
#
# This problem, simply described, is due to the inconsistent handling among Makefile,
# src files, and header files.
#
#OPENSSL_NO_LIST += no-sock

OPENSSL_NO_LIST += no-hw no-asm
OPENSSL_NO_LIST += no-err
OPENSSL_NO_LIST += no-cms
OPENSSL_NO_LIST += no-idea no-bf no-cast
OPENSSL_NO_LIST += no-ripemd no-rc5
OPENSSL_NO_LIST += no-ec no-ecdsa no-ecdh no-krb4 no-krb5 
OPENSSL_NO_LIST += no-md2 no-mdc2 
OPENSSL_NO_LIST += no-ans1 no-dsh no-dso

OPENSSL_NO_FLAG += -DOPENSSL_NO_SHA0 -DOPENSSL_NO_SPEED
OPENSSL_NO_FLAG += -DOPENSSL_NO_RMD160
OPENSSL_NO_FLAG += -DSSL_FORBID_ENULL


OPENSSL_NO_LIST += no-camellia no-seed no-jpake
OPENSSL_NO_LIST += no-capieng no-gost no-ans1

OPENSSL_NO_FLAG += -DOPENSSL_NO_COMP
#OPENSSL_NO_FLAG += -DOPENSSL_NO_OCSP # inconsistency issue per comment above


$(DL_DIR)/$(OPENSSL_SOURCE):
	$(WGET) -P $(DL_DIR) $(OPENSSL_SITE)/$(OPENSSL_SOURCE)

$(OPENSSL_DIR)/.unpacked: $(DL_DIR)/$(OPENSSL_SOURCE)
	gunzip -c $(DL_DIR)/$(OPENSSL_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	# GCC 4.2.4 deprecated "mcpu="; use "march=" instead
	(cd $(OPENSSL_DIR); sed -i.orig 's/mcpu=/march=/' Configure)
	touch $(OPENSSL_DIR)/.unpacked

#
# --openssldir is to educate openssl to look for /etc/openssl.cnf on AP & ZD
# -DDEFAULT_HOME is to educate openssl to find .rnd random seed file
#
$(OPENSSL_DIR)/Makefile: $(OPENSSL_DIR)/.unpacked
ifneq ($(findstring 1.0.0, $(OPENSSL_VER)),)
	(cd $(OPENSSL_DIR); \
	mv $(STAGING_DIR)/lib/libz.so.1 $(STAGING_DIR)/lib/libz.so.2; \
	./Configure linux-generic32 \
		--openssldir=/etc --prefix=/ \
		--cross-compile-prefix=$(TARGET_CROSS) \
		-I$(STAGING_DIR)/include \
		-L$(STAGING_DIR)/lib -ldl \
		-Os -DDEFAULT_HOME='"/writable/etc/system"' \
		$(OPENSSL_NO_FLAG) $(OPENSSL_NO_LIST) \
		shared; \
	make depend)
endif
	@echo
	@echo

#
# where did this libz (wrong architecture?) come from? TBD...!
#
$(OPENSSL_DIR)/apps/openssl: $(OPENSSL_DIR)/Makefile
	$(MAKE1) CC=$(TARGET_CC) AR="$(TARGET_AR) r" RANLIB=$(TARGET_RANLIB) -C $(OPENSSL_DIR) all build-shared
	# Work around openssl build bug to link libssl.so with libcrypto.so.
	-rm $(OPENSSL_DIR)/libssl.so.*.*.*
	$(MAKE1) CC=$(TARGET_CC) AR="$(TARGET_AR) r" RANLIB=$(TARGET_RANLIB) -C $(OPENSSL_DIR) do_linux-shared
	# Work around openssl build bug to link to libz.so.1 - change work around back while completing openssl compiler
	-mv $(STAGING_DIR)/lib/libz.so.2 $(STAGING_DIR)/lib/libz.so.1

$(STAGING_DIR)/lib/libcrypto.a: $(OPENSSL_DIR)/apps/openssl
	$(MAKE) -C $(OPENSSL_DIR) CC=$(TARGET_CC) INSTALL_PREFIX=$(STAGING_DIR)
	cp -fa $(OPENSSL_DIR)/libcrypto.so* $(STAGING_DIR)/lib/
	chmod a-x $(STAGING_DIR)/lib/libcrypto.so.$(OPENSSL_MAJ_VER)
	(cd $(STAGING_DIR)/lib; ln -fs libcrypto.so.$(OPENSSL_MAJ_VER) libcrypto.so)
	(cd $(STAGING_DIR)/lib; ln -fs libcrypto.so.$(OPENSSL_MAJ_VER) libcrypto.so.0)
	cp -fa $(OPENSSL_DIR)/libssl.so* $(STAGING_DIR)/lib/
	chmod a-x $(STAGING_DIR)/lib/libssl.so.$(OPENSSL_MAJ_VER)
	(cd $(STAGING_DIR)/lib; ln -fs libssl.so.$(OPENSSL_MAJ_VER) libssl.so)
	(cd $(STAGING_DIR)/lib; ln -fs libssl.so.$(OPENSSL_MAJ_VER) libssl.so.0)

define install_ssl_so
	mkdir -p $1; \
	cp -fa $(STAGING_DIR)/lib/libcrypto.so.$(OPENSSL_MAJ_VER) $1; \
	cp -fa $(STAGING_DIR)/lib/libssl.so.$(OPENSSL_MAJ_VER) $1; \
	$(STRIP) --strip-unneeded $1/libcrypto.so.$(OPENSSL_MAJ_VER); \
	$(STRIP) --strip-unneeded $1/libssl.so.$(OPENSSL_MAJ_VER); \
	cd $1; \
	ln -sf libcrypto.so.$(OPENSSL_MAJ_VER) libcrypto.so; \
	ln -sf libcrypto.so.$(OPENSSL_MAJ_VER) libcrypto.so.0; \
	ln -sf libssl.so.$(OPENSSL_MAJ_VER) libssl.so; \
	ln -sf libssl.so.$(OPENSSL_MAJ_VER) libssl.so.0;
endef

$(TARGET_DIR)/usr/lib/libcrypto.so.$(OPENSSL_MAJ_VER): $(STAGING_DIR)/lib/libcrypto.a
	$(call install_ssl_so,$(TARGET_DIR)/usr/lib)
ifeq ($(strip $(BR2_TARGET_RESTORETOOL_KIT)),y)
	$(call install_ssl_so,$(BR2_RESTORETOOL_DIR)/lib)
endif
	mkdir -p $(STAGING_DIR)/include/openssl
	rm -f $(STAGING_DIR)/include/openssl/*
	find -L $(OPENSSL_DIR)/include/openssl/ -type f | xargs cp -pft $(STAGING_DIR)/include/openssl

$(TARGET_DIR)/usr/lib/libssl.a: $(STAGING_DIR)/lib/libcrypto.a
	mkdir -p $(TARGET_DIR)/usr/include 
	cp -a $(STAGING_DIR)/include/openssl $(TARGET_DIR)/usr/include/
	cp -dpf $(OPENSSL_DIR)/libssl.a $(OPENSSL_DIR)/libcrypto.a $(STAGING_DIR)/lib
	cp -dpf $(STAGING_DIR)/lib/libssl.a $(TARGET_DIR)/usr/lib/
	cp -dpf $(STAGING_DIR)/lib/libcrypto.a $(TARGET_DIR)/usr/lib/
	touch -c $(TARGET_DIR)/usr/lib/libssl.a

openssl-headers: $(TARGET_DIR)/usr/lib/libssl.a
	mkdir -p $(STAGING_DIR)/include/openssl
	find -L $(OPENSSL_DIR)/include/openssl/ -type f | xargs cp -pft $(STAGING_DIR)/include/openssl

openssl: uclibc $(OPENSSL_DIR)/apps/openssl $(TARGET_DIR)/usr/lib/libcrypto.so.$(OPENSSL_MAJ_VER) 
	cp -f $(OPENSSL_DIR)/apps/openssl $(TARGET_DIR)/usr/bin
	ln -sf /writable/etc/system/.rnd $(TARGET_DIR)/
	-$(STRIP) --strip-unneeded $(TARGET_DIR)/usr/bin/openssl

openssl-clean:
	rm -f $(TARGET_DIR)/usr/bin/openssl
	rm -f $(STAGING_DIR)/bin/openssl
	rm -f $(STAGING_DIR)/lib/libcrypto.so* $(TARGET_DIR)/usr/lib/libcrypto.so*
	rm -f $(STAGING_DIR)/lib/libssl.so* $(TARGET_DIR)/usr/lib/libssl.so*
	rm -rf $(STAGING_DIR)/include/openssl
	-$(MAKE) -C $(OPENSSL_DIR) clean
	rm -f $(OPENSSL_DIR)/Makefile # clean up so Configure will be re-run

openssl-dirclean:
	rm -f $(TARGET_DIR)/usr/bin/openssl 
	rm -f $(TARGET_DIR)/usr/lib/libcrypto.so*
	rm -f $(TARGET_DIR)/usr/lib/libssl.so*
	rm -rf $(OPENSSL_DIR)


#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_OPENSSL)),y)
TARGETS+=openssl
endif
ifeq ($(strip $(BR2_PACKAGE_OPENSSL_TARGET_HEADERS)),y)
TARGETS+=openssl-headers
endif
