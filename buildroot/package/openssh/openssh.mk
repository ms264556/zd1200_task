#############################################################
#
# openssh
#
#############################################################

OPENSSH_SITE     := ftp://ftp.openbsd.org/pub/OpenBSD/OpenSSH/portable
OPENSSH_VER      := 3.9p1
OPENSSH_DIR      := $(BUILD_DIR)/openssh-$(OPENSSH_VER)
OPENSSH_SOURCE   := openssh-$(OPENSSH_VER).tar.gz

$(DL_DIR)/$(OPENSSH_SOURCE):
	echo DEBUG openssh.mk target is OPENSSH_SOURCE $(OPENSSH_SOURCE)
	$(WGET) -P $(DL_DIR) $(OPENSSH_SITE)/$(OPENSSH_SOURCE)

$(OPENSSH_DIR)/.unpacked: $(DL_DIR)/$(OPENSSH_SOURCE) 
	echo DEBUG openssh.mk target is .unpacked
	zcat $(DL_DIR)/$(OPENSSH_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(OPENSSH_DIR) package/openssh/ openssh\*.patch
	echo DEBUG Patching again
	echo DEBUG pwd `pwd`  
	echo DEBUG OPENSSH_DIR $(OPENSSH_DIR) 
	(cd $(OPENSSH_DIR); patch -p1 ) < package/openssh/openssh.patch2
	touch  $(OPENSSH_DIR)/.unpacked

$(OPENSSH_DIR)/.configured: $(OPENSSH_DIR)/.unpacked
	echo DEBUG openssh.mk target is .configured
	(cd $(OPENSSH_DIR); rm -rf config.cache; autoconf; \
		$(TARGET_CONFIGURE_OPTS) \
		LD=$(TARGET_CROSS)gcc \
		CFLAGS="$(TARGET_CFLAGS)" \
		./configure \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
		--prefix=/usr \
		--exec-prefix=/usr \
		--bindir=/usr/bin \
		--sbindir=/usr/sbin \
		--libexecdir=/usr/sbin \
		--sysconfdir=/etc \
		--datadir=/usr/share \
		--localstatedir=/var \
		--mandir=/usr/man \
		--infodir=/usr/info \
		--includedir=$(STAGING_DIR)/include/openssl \
		--disable-lastlog --disable-utmp \
		--disable-utmpx --disable-wtmp --disable-wtmpx \
		--without-x \
		$(DISABLE_NLS) \
		$(DISABLE_LARGEFILE) \
	);
	touch  $(OPENSSH_DIR)/.configured

$(OPENSSH_DIR)/ssh: $(OPENSSH_DIR)/.configured
	echo DEBUG openssh.mk target is ssh
	mkdir -p $(OPENSSH_DIR)/openssl
	cp -f $(STAGING_DIR)/include/openssl/* $(OPENSSH_DIR)/openssl
	$(MAKE) CC=$(TARGET_CC) CFLAGS="$(TARGET_CFLAGS)" -C $(OPENSSH_DIR)
	-$(STRIP) --strip-unneeded $(OPENSSH_DIR)/scp
	-$(STRIP) --strip-unneeded $(OPENSSH_DIR)/sftp
	-$(STRIP) --strip-unneeded $(OPENSSH_DIR)/sftp-server
	-$(STRIP) --strip-unneeded $(OPENSSH_DIR)/ssh
	-$(STRIP) --strip-unneeded $(OPENSSH_DIR)/ssh-add
	-$(STRIP) --strip-unneeded $(OPENSSH_DIR)/ssh-agent
	-$(STRIP) --strip-unneeded $(OPENSSH_DIR)/ssh-keygen
	-$(STRIP) --strip-unneeded $(OPENSSH_DIR)/ssh-keyscan
	-$(STRIP) --strip-unneeded $(OPENSSH_DIR)/ssh-keysign
	-$(STRIP) --strip-unneeded $(OPENSSH_DIR)/ssh-rand-helper
	-$(STRIP) --strip-unneeded $(OPENSSH_DIR)/sshd

$(TARGET_DIR)/usr/bin/ssh: $(OPENSSH_DIR)/ssh
	echo DEBUG openssh.mk target is /usr/bin/ssh
	$(MAKE) CC=$(TARGET_CC) DESTDIR=$(TARGET_DIR) -C $(OPENSSH_DIR) install
	mkdir -p $(TARGET_DIR)/../altroot
	chmod 777 $(TARGET_DIR)/../altroot
	-mv -f $(TARGET_DIR)/usr/bin/scp  $(TARGET_DIR)/../altroot # Remove unnecessary programs to keep memory size low
	-mv -f $(TARGET_DIR)/usr/bin/sftp $(TARGET_DIR)/../altroot # but keep them around so they can be tftp'd if nec.
	-mv -f $(TARGET_DIR)/usr/bin/ssh $(TARGET_DIR)/../altroot
	-mv -f $(TARGET_DIR)/usr/bin/ssh-add $(TARGET_DIR)/../altroot
	-mv -f $(TARGET_DIR)/usr/bin/ssh-agent $(TARGET_DIR)/../altroot
	-mv -f $(TARGET_DIR)/usr/bin/ssh-keyscan $(TARGET_DIR)/../altroot
	-mv -f $(TARGET_DIR)/usr/sbin/sftp-server $(TARGET_DIR)/../altroot
	-mv -f $(TARGET_DIR)/usr/sbin/ssh-keysign $(TARGET_DIR)/../altroot
	-mv -f $(TARGET_DIR)/usr/bin/ssh-rand-helper $(TARGET_DIR)/../altroot
	mkdir -p $(TARGET_DIR)/etc/init.d/ 
	#-mkdir -p $(TARGET_DIR)/etc/openssh
	rm -rf $(TARGET_DIR)/usr/info $(TARGET_DIR)/usr/man $(TARGET_DIR)/usr/share/doc
	#mkdir -p $(TARGET_DIR)/etc/openssh
	#cp -f package/openssh/sshd_config $(TARGET_DIR)/etc/openssh/sshd_config
	#ln -snf /writable/data/openssh $(TARGET_DIR)/etc/.ssh
	#-cp -f package/openssh/banner.txt $(TARGET_DIR)/etc/openssh
	#-chmod 444 $(TARGET_DIR)/etc/ssh/banner.txt

openssh: openssl zlib $(TARGET_DIR)/usr/bin/ssh

openssh-source: $(DL_DIR)/$(OPENSSH_SOURCE)

openssh-clean: 
	$(MAKE) -C $(OPENSSH_DIR) clean

openssh-dirclean: 
	rm -rf $(OPENSSH_DIR)


#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_OPENSSH)),y)
TARGETS+=openssh
endif
