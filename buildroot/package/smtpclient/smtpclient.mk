#############################################################
#
# SMTPCLIENT: simple network time protocol client
#
#############################################################

# TARGETS
#
#
SMTPCLIENT_DIR     :=$(BUILD_DIR)/smtpclient

# define for video54/apps build
SMTPCLIENT_SRC_DIR :=$(V54_DIR)/apps/smtpclient
EXE_SMTPCLIENT     :=mail


# use for video54 apps; do cp -f -l always
$(SMTPCLIENT_DIR)/.unpacked: 
	@mkdir -p $(SMTPCLIENT_DIR)
	@cp -f -l -r $(SMTPCLIENT_SRC_DIR)/* $(SMTPCLIENT_DIR)
#	patch -d $(SMTPCLIENT_DIR) -u < $(SMTPCLIENT_DIR)/andyw.patch
	@touch $(SMTPCLIENT_DIR)/.unpacked
	@echo @done unpacking

$(SMTPCLIENT_DIR)/.config: $(SMTPCLIENT_DIR)/.unpacked
	@touch .config

#
# Use the empty CPPFLGAS to pass in target_cflags in auto-generated Makefile
#
smtpclient: $(SMTPCLIENT_DIR)/.config
	@mkdir -p $(TARGET_DIR)/bin
	make -C $(SMTPCLIENT_DIR) CC=$(TARGET_CC) CFLAGS="$(TARGET_CFLAGS)"
	cp -f $(SMTPCLIENT_DIR)/$(EXE_SMTPCLIENT) $(TARGET_DIR)/bin
	$(STRIP) $(TARGET_DIR)/bin/$(EXE_SMTPCLIENT)

smtpclient-clean:
	rm -f $(TARGET_DIR)/bin/$(EXE_SMTPCLIENT)
	-$(MAKE) -C $(SMTPCLIENT_DIR) clean

smtpclient-dirclean:
	rm -rf $(SMTPCLIENT_DIR)


#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_SMTPCLIENT)),y)
TARGETS+=smtpclient
endif
