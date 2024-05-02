#############################################################
#
# Chariot Endpoint
#
#############################################################

CHARIOT_DIR:=$(BUILD_DIR)/chariot
CHARIOT_TARBALL:=chariot_lexra_670.tar.gz
CHARIOT_CAT:=zcat

$(CHARIOT_DIR)/.unpacked: $(DL_DIR)/$(CHARIOT_TARBALL)
	mkdir $(CHARIOT_DIR)
	$(CHARIOT_CAT) $(DL_DIR)/$(CHARIOT_TARBALL) | tar -C $(CHARIOT_DIR) $(TAR_OPTIONS) -
	touch $(CHARIOT_DIR)/.unpacked

$(CHARIOT_DIR)/temp: $(CHARIOT_DIR)/.unpacked
	
$(TARGET_DIR)/opt/chariot: $(CHARIOT_DIR)/temp
	cp -af $(CHARIOT_DIR)/temp $(TARGET_DIR)/opt/chariot 
	cd $(TARGET_DIR)/opt/chariot;ln -s endpoint chariot
	cd $(TARGET_DIR)/sbin;ln -s ../opt/chariot/chariot

chariot: $(TARGET_DIR)/opt/chariot

chariot-clean:
	rm -rf $(TARGET_DIR)/opt/chariot
	rm -f $(TARGET_DIR)/sbin/chariot

chariot-dirclean: chariot-clean
	rm -rf $(CHARIOT_DIR)
#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_CHARIOT)),y)
TARGETS+=chariot
endif
