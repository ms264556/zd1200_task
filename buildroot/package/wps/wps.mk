#############################################################
#
# WPS: Wi-Fi Protected Setup
#
#############################################################


WPS_VER         :=1.0.5
WPS_VENDOR      :=WscRefImpl_Intel
WPS_DIR         :=$(BUILD_DIR)/WPS_$(WPS_VENDOR)_$(WPS_VER)

# define for video54/apps build
WPS_SRC_DIR     :=$(V54_DIR)/apps/wps/$(WPS_VENDOR)/$(WPS_VER)/src
EXE_WPS         :=$(WPS_DIR)/linux/wsccmd

WPS_OPTIONS  := -DLINUX -DOPENSSL_FIPS \
				-I$(STAGING_DIR)/include \
				-I$(WPS_DIR)/common/inc \
				-I$(WPS_DIR)/common/MasterControl \
				-I$(WPS_DIR)/linux/inc \
				-L$(STAGING_DIR)/lib 

# use for video54 apps; do cp -f -l always

$(WPS_DIR)/.config $(WPS_DIR)/.unpacked: 
	@mkdir -p $(WPS_DIR)
	@cp -f -l -r $(WPS_SRC_DIR)/common $(WPS_DIR)
	@cp -f -l -r $(WPS_SRC_DIR)/linux  $(WPS_DIR)

#$(WPS_DIR)/.config: $(WPS_DIR)/.unpacked

wps: openssl $(WPS_DIR)/.config 
	$(MAKE) -C $(WPS_DIR)/linux CC="$(TARGET_CC)" CXX="$(TARGET_CPP)" \
		LD="$(TARGET_LD)" WPS_OPTIONS="$(WPS_OPTIONS)" \
		TARGET_STRIP="$(TARGET_STRIP)" BLDTYPE=release
	mkdir -p $(TARGET_DIR)/sbin
	cp -f $(EXE_WPS) $(TARGET_DIR)/sbin

wps-clean:
	rm -f $(TARGET_DIR)/sbin/$(EXE_WPS)
	-$(MAKE) -C $(WPS_DIR) clean

wps-dirclean:
	rm -f $(TARGET_DIR)/sbin/$(EXE_WPS)
	rm -rf $(WPS_DIR)


#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_WPS)),y)
TARGETS+=wps
endif
