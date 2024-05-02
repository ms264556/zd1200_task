#############################################################
#
# cares
#
#############################################################

# TARGETS
CARES_VER       :=1.7.1
CARES_SITE      :=http://c-ares.haxx.se/
CARES_SOURCE    :=c-ares-$(CARES_VER).tar.gz
CARES_DIR       :=$(BUILD_DIR)/c-ares-$(CARES_VER)




ifeq ($(BR2_i386),y)
CARES_TARGET_ARCH:=i386-linux
else
ifeq ($(BR2_powerpc),y)
CARES_TARGET_ARCH:=powerpc-linux
else
CARES_TARGET_ARCH:=mips-linux-uclibc
endif
endif




$(DL_DIR)/$(CARES_SOURCE):
	$(WGET) -P $(DL_DIR) $(CARES_SITE)/$(CARES_SOURCE)

$(CARES_DIR)/.unpacked: $(DL_DIR)/$(CARES_SOURCE)
	gunzip -c $(DL_DIR)/$(CARES_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	touch $(CARES_DIR)/.unpacked


$(CARES_DIR)/Makefile: $(CARES_DIR)/.unpacked
	(cd $(CARES_DIR); \
	CFLAGS="-O2 $(NO_LIST) $(TARGET_CFLAGS)" PATH=$(TARGET_PATH) ./configure --host=$(CARES_TARGET_ARCH) --enable-shared)


$(CARES_DIR)/apps/cares: $(CARES_DIR)/Makefile
	$(MAKE) CC=$(TARGET_CC) -C $(CARES_DIR)


cares:  $(CARES_DIR)/apps/cares
	cp -f $(CARES_DIR)/.libs/libcares.so.2.0.0 $(TARGET_DIR)/usr/lib/
	ln -sf libcares.so.2.0.0  $(TARGET_DIR)/usr/lib/libcares.so
	ln -sf libcares.so.2.0.0  $(TARGET_DIR)/usr/lib/libcares.so.2
cares-source: $(DL_DIR)/$(CARES_SOURCE)

cares-clean: 
	rm -f  $(TARGET_DIR)/usr/lib/libcares.so.*
	-$(MAKE) -C $(CARES_DIR) clean

cares-dirclean: 
	rm -f $(TARGET_DIR)/user/lib/libcares.so.*
	rm -rf $(CARES_DIR)


#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_CARES)),y)
TARGETS+=cares
endif
