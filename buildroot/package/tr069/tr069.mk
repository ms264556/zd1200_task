#############################################################
#
# tr069
#
#############################################################

ifeq ($(strip $(BR2_PACKAGE_TR069_4CTC)),y)
TR069_VER           := v3.0.4_4CTC
else
TR069_VER           := v3.0.4
endif
TR069_SRC           := $(V54_DIR)/apps/tr069/$(TR069_VER)
TR069_DIR           := $(BUILD_DIR)/tr069
TR069_BINARY        := tr069d
TR069_TARGET_BINARY := $(TARGET_DIR)/usr/sbin/$(TR069_BINARY)

TR069_MAKE_OPT := -C $(TR069_DIR) -f $(TR069_SRC)/Makefile.br

TR069_LDFLAGS     += $(DASHLRSM)  
TR069_CFLAGS      += $(TARGET_CFLAGS) $(DASHDRSM) $(DASHIRSM)
TR069_CFLAGS      += -DTR069_ENABLE_SAVE_ALLRPMKEYS
TR069_CFLAGS      += -D_BR_BUILD_VERSION='\"$(BR_BUILD_VERSION)\"'
TR069_CFLAGS      += -D_BR_BUILD_SUBVERSION='\"$(BR_BUILD_SUBVERSION)\"'
TR069_CFLAGS      += -D_DOTPROFILE='\"$(DOTPROFILE)\"'
#TR069_CFLAGS      += -DTR069_TURN

ifeq ($(strip $(BR2_PACKAGE_TR069_DEVICE)), y)
TR069_CFLAGS += -DTR_111_DEVICE
endif

ifeq ($(strip $(BR2_PACKAGE_TR069_INTERNETGATEWAY)),y)
TR069_CFLAGS += -DTR_111_GATEWAY
endif

ifeq ($(strip $(BR2_PACKAGE_OPENSSL)),y)
TR069_CFLAGS  += -DWITH_SSLAUTH -DWITH_OPENSSL -I$(SSLINC) -DOPENSSL_NO_MD5
TR069_LDFLAGS += -lssl -lcrypto
endif

ifeq ($(strip $(BR2_PACKAGE_MOCANA_SSL)),y)
TR069_CFLAGS += -DWITH_SSLAUTH -DWITH_MOCANA -I$(MOCANASSL_SRC_DIR)/src
TR069_CFLAGS += $(MOCANASSL_CFLAGS)
TR069_LDFLAGS += -lssl -lcrypto
endif

ifeq ($(strip $(BR2_PACKAGE_TR069_4CTC)),y)
TR069_CFLAGS += -DV54_4CTC
endif

$(TR069_DIR)/.configure:
	mkdir -p $(TR069_DIR) $(TR069_DIR)/../root/usr/include
	@touch $(TR069_DIR)/.configure

tr069: librsm $(TR069_DIR)/.configure
	@mkdir -p $(TARGET_DIR)/etc/tr069
	make $(TR069_MAKE_OPT) TR069_SRC_DIR=$(TR069_SRC) CC="$(TARGET_CC)" \
		TR069_CFLAGS="$(TR069_CFLAGS)" TR069_LDFLAGS="$(TR069_LDFLAGS)" 
	cp ${TR069_DIR}/${TR069_BINARY} ${TR069_DIR}/tr069d.notstripped
	$(STRIP) $(TR069_DIR)/$(TR069_BINARY)
	install -D $(TR069_DIR)/$(TR069_BINARY) $(TR069_TARGET_BINARY)
ifeq ($(TR069_VER),v3.0.4)
	cp -f $(TR069_SRC)/dps.param          $(TARGET_DIR)/etc/tr069
	cp -f $(TR069_SRC)/dps.param.videowan $(TARGET_DIR)/etc/tr069
else
ifeq ($(strip $(BR2_PACKAGE_TR069_4CTC)),y)
	cp -f $(TR069_SRC)/dps.param.4ctc     $(TARGET_DIR)/etc/tr069/dps.param
else
	cp -f $(TR069_SRC)/dps.param          $(TARGET_DIR)/etc/tr069/dps.init
endif
endif

tr069-clean:
	-make $(TR069_MAKE_OPT) clean
	(cd $(TR069_DIR); \
	 rm -f $(TR069_DIR)/.configure $(TR069_BINARY))

tr069-dirclean: tr069-clean
	rm -rf $(TR069_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_BUILD_MULTI_IMAGES)),y)
TR069_PACKAGE_EXTRAS=$(BR2_PACKAGE_EXTRAS_TR069)
else
TR069_PACKAGE_EXTRAS=
endif

ifeq ($(strip $(BR2_PACKAGE_TR069)),y)
ifeq ($(strip $(TR069_PACKAGE_EXTRAS)),y)
EXTRAS_TARGETS+=tr069
else
TARGETS+=tr069
endif
endif
