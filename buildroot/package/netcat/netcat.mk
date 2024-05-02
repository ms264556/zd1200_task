#############################################################
#
# netcat tcp networking tool
#
#############################################################

NETCAT_VER:=0.7.1
NETCAT_SOURCE:=netcat-$(NETCAT_VER).tar.gz
NETCAT_SITE:=http://osdn.dl.sourceforge.net/sourceforge/netcat
NETCAT_DIR:=$(BUILD_DIR)/netcat-$(NETCAT_VER)
NETCAT_BINARY:=src/netcat
NETCAT_TARGET_BINARY:=sbin/netcat

$(DL_DIR)/$(NETCAT_SOURCE):
	$(WGET) -P $(DL_DIR) $(NETCAT_SITE)/$(NETCAT_SOURCE)

netcat-source: $(DL_DIR)/$(NETCAT_SOURCE)

$(NETCAT_DIR)/.unpacked: $(DL_DIR)/$(NETCAT_SOURCE)
	tar xzf $(DL_DIR)/$(NETCAT_SOURCE) -C $(BUILD_DIR)
	touch $(NETCAT_DIR)/.unpacked


$(NETCAT_DIR)/.configured: $(NETCAT_DIR)/.unpacked
	(cd $(NETCAT_DIR); rm -rf config.cache; \
	$(TARGET_CONFIGURE_OPTS) CC_FOR_BUILD="$(HOSTCC)" \
	CFLAGS="$(TARGET_CFLAGS)" \
	./configure \
	--target=$(GNU_TARGET_NAME) \
	--host=$(GNU_TARGET_NAME) \
	--build=$(GNU_HOST_NAME) \
	--prefix=/ \
	);
	touch $(NETCAT_DIR)/.configured

$(NETCAT_DIR)/$(NETCAT_BINARY): $(NETCAT_DIR)/.configured
	$(TARGET_CONFIGURE_OPTS) $(MAKE) CC=$(TARGET_CC) -C $(NETCAT_DIR)

$(TARGET_DIR)/$(NETCAT_TARGET_BINARY): $(NETCAT_DIR)/$(NETCAT_BINARY)
	install -D $(NETCAT_DIR)/$(NETCAT_BINARY) $(TARGET_DIR)/$(NETCAT_TARGET_BINARY)

netcat: uclibc $(TARGET_DIR)/$(NETCAT_TARGET_BINARY)

netcat-clean:
	rm -f $(TARGET_DIR)/$(NETCAT_TARGET_BINARY)
	-$(MAKE) -C $(NETCAT_DIR) clean

netcat-dirclean:
	rm -rf $(NETCAT_DIR)
#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_NETCAT)),y)
TARGETS=netcat
endif
