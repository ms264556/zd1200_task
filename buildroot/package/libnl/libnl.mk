LIBNL_VER_MAJOR:=1
LIBNL_VER:=1.1
LIBNL_BASE_DIR:=${BASE_DIR}/package/libnl
LIBNL_DIR:=$(BUILD_DIR)/libnl-$(LIBNL_VER)
LIBNL_SITE:=http://ftp.de.debian.org/debian/pool/main/libn/libnl
LIBNL_SOURCE:=libnl_$(LIBNL_VER).orig.tar.gz
LIBNL_SOURCE_PATCH_01:=libnl-1.1.01-missing_declaration.patch
LIBNL_SOURCE_PATCH_02:=libnl-1.1.02-hz-txtime-cal.patch
LIBNL_SOURCE_PATCH_03:=libnl-1.1.02-tc_ratespec_declaration.patch
LIBNL_CAT:=zcat

$(DL_DIR)/$(LIBNL_SOURCE):
	 $(WGET) -P $(DL_DIR) $(LIBNL_SITE)/$(LIBNL_SOURCE)

libnl-source: $(DL_DIR)/$(LIBNL_SOURCE)

$(LIBNL_DIR)/.unpacked: $(DL_DIR)/$(LIBNL_SOURCE)
	$(LIBNL_CAT) $(DL_DIR)/$(LIBNL_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	mkdir -p $(LIBNL_DIR)
	(cd $(LIBNL_DIR); patch -p1 < ${LIBNL_BASE_DIR}/$(LIBNL_SOURCE_PATCH_01);)
	(cd $(LIBNL_DIR); patch -p1 < ${LIBNL_BASE_DIR}/$(LIBNL_SOURCE_PATCH_02);)
#for kernel version great than 2.6.32
	if [ "$(VERSION)" -ge "2" -a "$(PATCHLEVEL)" -ge "6" -a "$(SUBLEVEL)" -ge "32" ]; then\
	  (cd $(LIBNL_DIR); patch -p1 < ${LIBNL_BASE_DIR}/$(LIBNL_SOURCE_PATCH_03);)\
	fi
	touch $(LIBNL_DIR)/.unpacked

$(LIBNL_DIR)/.configured: $(LIBNL_DIR)/.unpacked
	( \
		cd $(LIBNL_DIR) ; \
		CC=$(TARGET_CC) \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		./configure \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
		--prefix=/usr \
	)
	( \
		cd $(LIBNL_DIR) ; \
		mv Makefile Makefile.orig; cp ${LIBNL_BASE_DIR}/Makefile ./Makefile; \
		mv lib/Makefile lib/Makefile.orig; cp ${LIBNL_BASE_DIR}/Makefile.lib lib/Makefile; \
		mv include/Makefile include/Makefile.orig; cp ${LIBNL_BASE_DIR}/Makefile.include include/Makefile; \
	) 
	touch $(LIBNL_DIR)/.configured

$(LIBNL_DIR)/libnl.a: $(LIBNL_DIR)/.configured
	$(MAKE) \
		AR="$(TARGET_CROSS)ar" \
		-C $(LIBNL_DIR)

$(STAGING_DIR)/lib/libnl.a: $(LIBNL_DIR)/libnl.a
	$(MAKE) \
		-C $(LIBNL_DIR) \
		prefix=$(STAGING_DIR) \
		exec_prefix=$(STAGING_DIR) \
		bindir=$(STAGING_DIR)/bin \
		datadir=$(STAGING_DIR)/share \
		install

$(STAGING_DIR)/lib/libnl.so.$(LIBNL_VER): $(STAGING_DIR)/lib/libnl.a
	$(TARGET_STRIP) $(STAGING_DIR)/lib/libnl.so.$(LIBNL_VER)

$(TARGET_DIR)/usr/lib/libnl.so.$(LIBNL _VER_MAJOR): $(STAGING_DIR)/lib/libnl.so.$(LIBNL_VER)
	mkdir -p $(TARGET_DIR)/usr/lib
	(cd $(TARGET_DIR)/usr/lib; cp $(STAGING_DIR)/lib/libnl.so.$(LIBNL_VER) .)
	(cd $(TARGET_DIR)/usr/lib; ln -sf libnl.so.$(LIBNL_VER) libnl.so.$(LIBNL_VER_MAJOR)) 

libnl: uclibc $(TARGET_DIR)/usr/lib/libnl.so.$(LIBNL _VER_MAJOR)

libnl-clean:
	rm -f $(STAGING_DIR)/include/nl*.h $(STAGING_DIR)/lib/libnl*
	rm -f $(TARGET_DIR)/usr/lib/libnl*
	-$(MAKE) -C $(LIBNL_DIR) clean

libnl-dirclean:
	rm -rf $(LIBNL_DIR)
	rm -f $(TARGET_DIR)/usr/lib/libnl*

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_LIBNL)),y)
TARGETS+=libnl
endif
