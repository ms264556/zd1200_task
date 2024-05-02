#############################################################
#
# nuttcp
#
#############################################################
NUTTCP_SOURCE:=nuttcp-5.3.1.tar.bz2
NUTTCP_SITE:=ftp://ftp.lcp.nrl.navy.mil/pub/nuttcp/
NUTTCP_DIR:=$(BUILD_DIR)/nuttcp-5.3.1
NUTTCP_CAT:=bzcat
NUTTCP_BINARY:=nuttcp-5.3.1
NUTTCP_TARGET_BINARY:=usr/sbin/nuttcp-5.3.1



$(DL_DIR)/$(NUTTCP_SOURCE):
	 $(WGET) -P $(DL_DIR) $(NUTTCP_SITE)/$(NUTTCP_SOURCE)

$(NUTTCP_DIR)/.unpacked: $(DL_DIR)/$(NUTTCP_SOURCE)
	$(NUTTCP_CAT) $(DL_DIR)/$(NUTTCP_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	(cd $(NUTTCP_DIR); patch -p1 ) < package/nuttcp/nuttcp.patch

$(NUTTCP_DIR)/$(NUTTCP_BINARY): $(NUTTCP_DIR)/.unpacked
	$(MAKE) CC=$(TARGET_CC) AR=$(TARGET_CROSS)ar -C $(NUTTCP_DIR)
	

$(TARGET_DIR)/$(NUTTCP_TARGET_BINARY): $(NUTTCP_DIR)/$(NUTTCP_BINARY)
	cp -f $(NUTTCP_DIR)/$(NUTTCP_BINARY) $(TARGET_DIR)/$(NUTTCP_TARGET_BINARY)
	
nuttcp: uclibc $(TARGET_DIR)/$(NUTTCP_TARGET_BINARY)

nuttcp-clean:
	rm -f  $(NUTTCP_DIR)/$(NUTTCP_BINARY)
	-$(MAKE) -C $(NUTTCP_DIR) clean

nuttcp-dirclean:
	rm -rf $(NUTTCP_DIR)


#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_NUTTCP)),y)
TARGETS+=nuttcp

endif
