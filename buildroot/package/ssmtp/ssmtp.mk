#############################################################
#
# ssmtp
#
#############################################################
SSMTP_VER:=2.63
SSMTP_SOURCE:=ssmtp_$(SSMTP_VER).orig.tar.gz
SSMTP_SITE:=ftp://ftp.debian.org/debian/pool/main/s/ssmtp/
SSMTP_DIR:=$(BUILD_DIR)/ssmtp-$(SSMTP_VER)
SSMTP_CAT:=zcat
SSMTP_BINARY:=ssmtp
SSMTP_TARGET_BINARY:=bin/ssmtp

SSMTP_PATCH_1=ssmtp_2.63-1.diff.gz

$(DL_DIR)/$(SSMTP_SOURCE):
	 $(WGET) -P $(DL_DIR) $(SSMTP_SITE)/$(SSMTP_SOURCE)
	 $(WGET) -P $(DL_DIR) $(SSMTP_SITE)/$(SSMTP_PATCH_1)

$(SSMTP_DIR)/.unpacked: $(DL_DIR)/$(SSMTP_SOURCE)
	mkdir $(SSMTP_DIR); cd $(SSMTP_DIR)
	$(SSMTP_CAT) $(DL_DIR)/$(SSMTP_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	$(SSMTP_CAT) $(DL_DIR)/$(SSMTP_PATCH_1) | patch -p1 -
	# Use the Ruckus modified version
	mv $(SSMTP_DIR)/ssmtp.c $(SSMTP_DIR)/ssmtp.c.orig
	cp $(BASE_DIR)/package/ssmtp/ssmtp.c $(SSMTP_DIR)/ssmtp.c
	touch $(SSMTP_DIR)/.unpacked

$(SSMTP_DIR)/.configured: $(SSMTP_DIR)/.unpacked
	(cd $(SSMTP_DIR); rm -rf config.cache; \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		./configure \
		--target=$(GNU_TARGET_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--build=$(GNU_HOST_NAME) \
		--prefix= \
		--sysconfdir=/tmp \
		--enable-ssl \
		--enable-md5auth \
		--enable-logfile \
		--enable-rewrite-domain \
		--enable-inet6\
	);
	touch  $(SSMTP_DIR)/.configured

$(SSMTP_DIR)/$(SSMTP_BINARY): $(SSMTP_DIR)/.configured
	$(MAKE) -C $(SSMTP_DIR)

# This stuff is needed to work around GNU make deficiencies
$(TARGET_DIR)/$(SSMTP_TARGET_BINARY): $(SSMTP_DIR)/$(SSMTP_BINARY)
	@if [ -L $(TARGET_DIR)/$(SSMTP_TARGET_BINARY) ] ; then \
		rm -f $(TARGET_DIR)/$(SSMTP_TARGET_BINARY); fi;
	@if [ ! -f $(SSMTP_DIR)/$(SSMTP_BINARY) -o $(TARGET_DIR)/$(SSMTP_TARGET_BINARY) \
	-ot $(SSMTP_DIR)/$(SSMTP_BINARY) ] ; then \
	    set -x; \
	    rm -f $(TARGET_DIR)/$(SSMTP_TARGET_BINARY); \
	    cp -f $(SSMTP_DIR)/$(SSMTP_BINARY) $(TARGET_DIR)/$(SSMTP_TARGET_BINARY); fi ;

ssmtp: uclibc $(TARGET_DIR)/$(SSMTP_TARGET_BINARY)

ssmtp-clean:
	$(MAKE) DESTDIR=$(TARGET_DIR) -C $(SSMTP_DIR) uninstall
	-$(MAKE) -C $(SSMTP_DIR) clean

ssmtp-dirclean:
	rm -rf $(SSMTP_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_SSMTP)),y)
TARGETS+=ssmtp
endif
