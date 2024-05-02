#############################################################
#
# irqbalance
#
#############################################################
IRQBALANCE_VER:=0.55
IRQBALANCE_SOURCE:=irqbalance-$(IRQBALANCE_VER).tar.gz
IRQBALANCE_SITE:=http://www.irqbalance.org/releases/
IRQBALANCE_DIR:=$(BUILD_DIR)/irqbalance-$(IRQBALANCE_VER)
IRQBALANCE_CAT:=zcat
IRQBALANCE_INSTALL_DIR:=/usr/sbin
IRQBALANCE_EXTRA_DIR:=$(strip $(subst ",,$(BR2_PACKAGE_IRQBALANCE_COPYTO)))
GLIB_DIR=glib-$(strip $(BR2_PACKAGE_LIBGLIB12_VER))

$(DL_DIR)/$(IRQBALANCE_SOURCE):
	 $(WGET) -P $(DL_DIR) $(IRQBALANCE_SITE)/$(IRQBALANCE_SOURCE)

irqbalance-source: $(DL_DIR)/$(IRQBALANCE_SOURCE)

$(IRQBALANCE_DIR)/.unpacked: $(DL_DIR)/$(IRQBALANCE_SOURCE)
	$(IRQBALANCE_CAT) $(DL_DIR)/$(IRQBALANCE_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(IRQBALANCE_DIR) package/irqbalance/ irqbalance\*.patch
	touch $(IRQBALANCE_DIR)/.unpacked

irqbalance-build: $(IRQBALANCE_DIR)/.unpacked
#	$(MAKE) CC=$(TARGET_CC) CFLAGS="-I$(BUILD_DIR)/$(GLIB_DIR)" LDFLAGS="--static $(BUILD_DIR)/$(GLIB_DIR)/.libs/libglib.a" -C $(IRQBALANCE_DIR) 
	$(MAKE) CC=$(TARGET_CC) CFLAGS="-I$(BUILD_DIR)/$(GLIB_DIR)" LDFLAGS="-L$(BUILD_DIR)/$(GLIB_DIR)/.libs -lglib" -C $(IRQBALANCE_DIR)

irqbalance-install:
	$(MAKE) DESTDIR=$(TARGET_DIR)/usr/sbin CC=$(TARGET_CC) -C $(IRQBALANCE_DIR) install-exec 

irqbalance-dep: uclibc libglib12 irqbalance-build irqbalance-install
	@echo "$@ done.."

irqbalance-clean:
	$(MAKE) -C $(IRQBALANCE_DIR) clean

irqbalance-distclean:
	-rm -rf $(IRQBALANCE_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_IRQBALANCE)),y)
TARGETS+=irqbalance-dep
endif
