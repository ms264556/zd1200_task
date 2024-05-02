ifeq ($(strip $(BR2_PACKAGE_TR069_4CTC)),y)
TR069_VER            := v3.0.4_4CTC
else
TR069_VER            := v3.0.4
endif
TR069_SRC            := $(V54_DIR)/apps/tr069/$(TR069_VER)
TR069_DIR            := $(BUILD_DIR)/tr069
TR069_HEADER         := $(TR069_SRC)/src/dimclient.h

TR069_GSOAP_VER      := 2.7.6c
TR069_GSOAP_SOURCE   := gsoap_$(TR069_GSOAP_VER).tar.gz
TR069_GSOAP_SITE     := http://nchc.dl.sourceforge.net/sourceforge/gsoap2
TR069_GSOAP_DIR      := $(BUILD_DIR)/gsoap-linux-$(TR069_GSOAP_VER)
TR069_GSOAP_EXE      := $(TR069_GSOAP_DIR)/soapcpp2/src/soapcpp2
TR069_GSOAP_GENSRC   := $(TR069_GSOAP_DIR)/gensrc

$(DL_DIR)/$(TR069_GSOAP_SOURCE):
	$(WGET) -P $(DL_DIR) $(TR069_GSOAP_SITE)/$(TR069_GSOAP_SOURCE)

$(TR069_GSOAP_DIR)/.unpacked: $(DL_DIR)/$(TR069_GSOAP_SOURCE)
	rm -rf $(TR069_GSOAP_DIR)
	mkdir $(TR069_GSOAP_DIR)
	tar zxvf $(DL_DIR)/$(TR069_GSOAP_SOURCE) -C $(TR069_GSOAP_DIR) --strip 1
	touch $(TR069_GSOAP_DIR)/.unpacked

$(TR069_GSOAP_DIR)/.configured: $(TR069_GSOAP_DIR)/.unpacked
	(cd $(TR069_GSOAP_DIR); ./configure)
	touch $(TR069_GSOAP_DIR)/.configured

$(TR069_GSOAP_EXE): $(TR069_GSOAP_DIR)/.configured
	make -C $(TR069_GSOAP_DIR)

tr069-gensrc: $(TR069_HEADER) $(TR069_GSOAP_EXE)
	rm -rf $(TR069_GSOAP_GENSRC)
	mkdir $(TR069_GSOAP_GENSRC)
	$(TR069_GSOAP_EXE) -c -d$(TR069_GSOAP_GENSRC) $(TR069_HEADER)

tr069-gsoap-clean: $(TR069_GSOAP_DIR)/.configured
	make -C $(TR069_GSOAP_DIR) clean

tr069-gsoap-dirclean:
	rm -rf $(TR069_GSOAP_DIR)
