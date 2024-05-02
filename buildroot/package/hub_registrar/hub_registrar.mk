#############################################################
#
# Ruckus HUB Registrar
#
#############################################################

HUB_REGISTRAR_BUILD_DIR       := ${BUILD_DIR}/hub_registrar
HUB_REGISTRAR_SRC_DIR         := ${TOPDIR}/../video54/apps/hub

HUB_REGISTRAR_BINARY          := hub_registrar
HUB_REGISTRAR_BUILD_BINARY   := $(HUB_REGISTRAR_BUILD_DIR)/$(HUB_REGISTRAR_BINARY)
HUB_REGISTRAR_TARGET_DIR     := $(TARGET_DIR)/usr/sbin
HUB_REGISTRAR_TARGET_BINARY  := $(HUB_REGISTRAR_TARGET_DIR)/$(HUB_REGISTRAR_BINARY)

HUB_REGISTRAR_MAKE_OPT := -C $(HUB_REGISTRAR_BUILD_DIR) -f $(HUB_REGISTRAR_SRC_DIR)/Makefile

HUB_REGISTRAR_CFLAGS += $(TARGET_CFLAGS) $(DASHIRSM) 
HUB_REGISTRAR_LDFLAGS += $(DASHLRSM)


hub_registrar: bsp libRutil build_include
	mkdir -p $(HUB_REGISTRAR_BUILD_DIR)
	make $(HUB_REGISTRAR_MAKE_OPT) CC=$(TARGET_CC) HUB_REGISTRAR_SRC_DIR=$(HUB_REGISTRAR_SRC_DIR) \
		HUB_REGISTRAR_CFLAGS="$(HUB_REGISTRAR_CFLAGS)" HUB_REGISTRAR_LDFLAGS="$(HUB_REGISTRAR_LDFLAGS)"
	install -D $(HUB_REGISTRAR_BUILD_BINARY) $(HUB_REGISTRAR_TARGET_BINARY)
	$(STRIP) $(HUB_REGISTRAR_TARGET_BINARY)

hub_registrar-clean:
	rm -f $(HUB_REGISTRAR_TARGET_BINARY)
	-$(MAKE) $(HUB_REGISTRAR_MAKE_OPT) clean

hub_registrar-dirclean:
	rm -rf $(HUB_REGISTRAR_BUILD_DIR)


#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_HUB_REGISTRAR)),y)
TARGETS+=hub_registrar
endif
