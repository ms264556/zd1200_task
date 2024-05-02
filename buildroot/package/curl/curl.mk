#############################################################
#
# cURL/libcurl: cURL - curl is a command line tool for 
#               transferring data with URL syntax, supporting 
#               DICT, FILE, FTP, FTPS, GOPHER, HTTP, HTTPS, 
#               IMAP, IMAPS, LDAP, LDAPS, POP3, POP3S, RTMP, 
#               RTSP, SCP, SFTP, SMTP, SMTPS, TELNET and TFTP. 
#               curl supports SSL certificates, HTTP POST, 
#               HTTP PUT, FTP uploading, HTTP form based upload, 
#               proxies, cookies, user+password authentication 
#               (Basic, Digest, NTLM, Negotiate, kerberos...), 
#               file transfer resume, proxy tunneling and more
#
#############################################################
CURL_VER     :=7.21.1
CURL_SOURCE:=curl-$(CURL_VER).tar.bz2
CURL_SITE:=http://curl.haxx.se/download/
CURL_DIR:=$(BUILD_DIR)/curl-$(CURL_VER)
CURL_CAT:=zcat
CURL_BINARY:=src/curl
CURL_TARGET_BINARY:=bin/curl

CURL_PREREQ:=openssl uclibc

# use for video54 apps; do cp -f -l always
#$(CURL_PREREQ)
$(CURL_DIR)/.unpacked: $(DL_DIR)/$(CURL_SOURCE) 
	(cd $(BUILD_DIR);tar xjvf $(DL_DIR)/$(CURL_SOURCE))
	@touch $(CURL_DIR)/.unpacked
	@echo @done unpacking


$(CURL_DIR)/curl-config: $(CURL_DIR)/.unpacked 
	( \
                cd $(CURL_DIR); \
                rm -rf config.cache; \
                ac_cv_linux_vers=$(BR2_DEFAULT_KERNEL_HEADERS) \
                BUILD_CC=$(TARGET_CC) HOSTCC=$(HOSTCC) \
                $(TARGET_CONFIGURE_OPTS) \
                CFLAGS="$(TARGET_CFLAGS)" \
                ./configure \
                --target=$(GNU_TARGET_NAME) \
                --host=$(GNU_TARGET_NAME) \
                --build=$(GNU_HOST_NAME) \
                --with-build-cc=$(HOSTCC) \
                --disable-yydebug \
                --enable-http \
                --enable-ftp \
                --enable-ipv6 \
                --disable-file \
                --disable-ldap \
                --disable-ldaps \
                --disable-rtsp \
                --disable-dict \
                --disable-tftp \
                --disable-pop3 \
                --disable-imap \
                --disable-smtp \
                --disable-manual \
                --disable-sspi \
                --disable-telnet \
                --prefix=/usr \
                --with-ssl=$(STAGING_DIR)/usr \
				--with-random=/dev/urandom \
                --with-ca-path=/tmp/ssl/ca-certs \
				--disable-static \
    )
	@echo @done configure

$(CURL_DIR)/src/.libs/curl: $(CURL_DIR)/curl-config 
	$(MAKE) -C $(CURL_DIR) CC=$(TARGET_CC) AR="$(TARGET_CROSS)ar"
	$(STRIP) $(CURL_DIR)/src/.libs/curl

$(TARGET_DIR)/$(CURL_TARGET_BINARY): $(CURL_DIR)/src/.libs/curl
	install -D $(CURL_DIR)/src/.libs/curl $(TARGET_DIR)/$(CURL_TARGET_BINARY)
	cp -af $(CURL_DIR)/lib/.libs/libcurl.so* $(STAGING_DIR)/lib/
	cp -af $(CURL_DIR)/lib/.libs/libcurl.so* $(TARGET_DIR)/lib/
#	install -D  $(CURL_DIR)/include/curl $(STAGING_DIR)/include/

curl: uclibc openssl $(TARGET_DIR)/$(CURL_TARGET_BINARY)

curl-clean:
	rm -f $(TARGET_DIR)/bin/$(EXE_CURL)
	rm -f $(TARGET_DIR)/lib/$(LIB_CURL)*
	$(MAKE) -C $(CURL_DIR) clean

curl-dirclean:
	rm -rf $(CURL_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_CURL)),y)
TARGETS+=curl
endif
