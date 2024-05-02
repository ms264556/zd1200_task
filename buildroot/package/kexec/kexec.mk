#############################################################
#
# kexec tools
#
#############################################################
KEXEC_SOURCE=kexec-tools-2.0.1.tar.gz
KEXEC_DIR=$(BUILD_DIR)/kexec-tools-2.0.1
KEXEC_BINARY=kexec
KEXEC_TARGET_BINARY=usr/bin/kexec

$(KEXEC_DIR)/.source: 
	zcat $(DL_DIR)/$(KEXEC_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(KEXEC_DIR) package/kexec/ kexec\*.patch
	touch $(KEXEC_DIR)/.source

$(KEXEC_DIR)/.configured: $(KEXEC_DIR)/.source
	(cd $(KEXEC_DIR); \
		$(TARGET_CONFIGURE_OPTS) \
		./configure \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
		--prefix=/usr \
		--sysconfdir=/etc \
		--without-zlib \
	);
	touch $(KEXEC_DIR)/.configured;

$(KEXEC_DIR)/$(KEXEC_BINARY): $(KEXEC_DIR)/.configured
	$(MAKE) CC=$(TARGET_CC) LD=$(TARGET_CROSS)ld \
		-C $(KEXEC_DIR) 

kexec_install: $(KEXEC_DIR)/$(KEXEC_BINARY)
	install -D $(KEXEC_DIR)/build/sbin/kexec $(TARGET_DIR)/$(KEXEC_TARGET_BINARY)

kexec: uclibc kexec_install

kexec-clean:
	rm -f $(TARGET_DIR)/$(KEXEC_TARGET_BINARY)
	-$(MAKE) -C $(KEXEC_DIR) clean

kexec-dirclean:
	rm -rf $(KEXEC_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_KEXEC)),y)
TARGETS+=kexec
endif
