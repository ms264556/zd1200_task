#############################################################
#
# FreeS/WAN
#
#############################################################
FREESWAN_VER:=1.95
# jsoung: create freeswan-1.95 directory for tar file and the changed file
FREESWAN_SOURCE:=freeswan-$(FREESWAN_VER).tar.gz
FREESWAN_PATCH_DIR:=$(TOPDIR)/../video54/patches/freeswan-$(FREESWAN_VER)-rap
FREESWAN_SITE:=http://download.freeswan.ca/old/
FREESWAN_DIR:=$(BUILD_DIR)/freeswan-$(FREESWAN_VER)
FREESWAN_CAT:=zcat
FREESWAN_BINARY:=src/xx
FREESWAN_TARGET_BINARY:=bin/xxx
KERNEL_IPSEC:=$(BR2_KERNEL_PATH)/net/ipsec

#$(DL_DIR)/$(FREESWAN_SOURCE):
#	 $(WGET) -P $(DL_DIR) $(FREESWAN_SITE)/$(FREESWAN_SOURCE)

freeswan-source: $(DL_DIR)/$(FREESWAN_SOURCE)

$(FREESWAN_DIR)/.unpacked: $(DL_DIR)/$(FREESWAN_SOURCE)
	$(FREESWAN_CAT) $(DL_DIR)/$(FREESWAN_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	touch $(FREESWAN_DIR)/.unpacked

$(FREESWAN_DIR)/.configured: $(FREESWAN_DIR)/.unpacked
	(cd $(FREESWAN_DIR); rm -rf config.cache; \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
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
		$(DISABLE_NLS) \
		$(DISABLE_LARGEFILE) \
	);
	touch  $(FREESWAN_DIR)/.configured

$(FREESWAN_DIR)/$(FREESWAN_BINARY): $(FREESWAN_DIR)/.configured
	$(MAKE) -C $(FREESWAN_DIR)

# This stuff is needed to work around GNU make deficiencies
freeswan-target_binary: $(FREESWAN_DIR)/$(FREESWAN_BINARY)
	@if [ -L $(TARGET_DIR)/$(FREESWAN_TARGET_BINARY) ] ; then \
		rm -f $(TARGET_DIR)/$(FREESWAN_TARGET_BINARY); fi;
	@if [ ! -f $(FREESWAN_DIR)/$(FREESWAN_BINARY) -o $(TARGET_DIR)/$(FREESWAN_TARGET_BINARY) \
	-ot $(FREESWAN_DIR)/$(FREESWAN_BINARY) ] ; then \
	    set -x; \
	    rm -f $(TARGET_DIR)/$(FREESWAN_TARGET_BINARY); \
	    cp -a $(FREESWAN_DIR)/$(FREESWAN_BINARY) $(TARGET_DIR)/$(FREESWAN_TARGET_BINARY); fi ;

freeswan: uclibc libgmp $(FREESWAN_DIR)/.unpacked 
	rsync -cav --force $(FREESWAN_PATCH_DIR)/ $(FREESWAN_DIR)/
ifdef NOTDEF
	rm -rf $(KERNEL_IPSEC)
	mkdir -p $(KERNEL_IPSEC)/libdes/asm
	mkdir -p $(KERNEL_IPSEC)/libfreeswan
	mkdir -p $(KERNEL_IPSEC)/zlib
	ln -s $(FREESWAN_DIR)/klips/net/ipsec/Makefile $(KERNEL_IPSEC)
	ln -s $(FREESWAN_DIR)/Makefile.inc $(KERNEL_IPSEC)
	ln -s $(FREESWAN_DIR)/Makefile.ver $(KERNEL_IPSEC) 
	ln -s $(FREESWAN_DIR)/klips/net/ipsec/Config.in $(KERNEL_IPSEC)
	ln -s $(FREESWAN_DIR)/klips/net/ipsec/defconfig $(KERNEL_IPSEC)
	ln -s $(FREESWAN_DIR)/klips/net/ipsec/*.[ch] $(KERNEL_IPSEC)
	ln -s $(FREESWAN_DIR)/lib/Makefile.kernel $(KERNEL_IPSEC)/libfreeswan/Makefile
	ln -s $(FREESWAN_DIR)/lib/*.[ch] $(KERNEL_IPSEC)/libfreeswan
	ln -s $(FREESWAN_DIR)/libdes/Makefile $(KERNEL_IPSEC)/libdes
	ln -s $(FREESWAN_DIR)/libdes/*.[ch] $(KERNEL_IPSEC)/libdes
	ln -s $(FREESWAN_DIR)/libdes/asm/*.pl $(KERNEL_IPSEC)/libdes/asm
	ln -s $(FREESWAN_DIR)/libdes/libdes/asm/perlasm $(KERNEL_IPSEC)/libdes/asm
	ln -s $(FREESWAN_DIR)/zlib/Makefile $(KERNEL_IPSEC)/zlib
	ln -s $(FREESWAN_DIR)/zlib/*.[chS] $(KERNEL_IPSEC)/zlib
endif
#@@@ freeswan: uclibc libgmp freeswan-target_binary
	export FREESWAN_DIR

freeswan-clean:
	$(MAKE) DESTDIR=$(TARGET_DIR) -C $(FREESWAN_DIR) uninstall
	-$(MAKE) -C $(FREESWAN_DIR) clean

freeswan-dirclean:
	rm -rf $(FREESWAN_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_FREESWAN)),y)
TARGETS+=freeswan
endif
