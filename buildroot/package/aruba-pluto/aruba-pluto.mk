#############################################################
#
# Aruba VPN Client Application
#
#############################################################

# TARGETS
CP=cp -f -l

# @@@ the following hack just helps to get us going
FREESWAN_DIR   :=$(BUILD_DIR)/freeswan-1.95
PLUTO-R_DIR    :=$(FREESWAN_DIR)/pluto
PLUTO-R_SOURCE :=aruba/pluto.tgz
DESTLIB_DIR    :=$(TARGET_DIR)/usr/local/lib/ipsec
ARUBA_PATCH_DIR  := $(TOPDIR)/../video54/aruba

ifeq ($(strip $(BR2_PACKAGE_ARUBA_PLUTO)),y)
PLUTO_SOURCE := pluto_source
else
PLUTO_SOURCE =
endif

# Rules & Targets
pluto_source:
	rm -rf $(PLUTO-R_DIR)
	mkdir -p $(PLUTO-R_DIR)
	zcat $(DL_DIR)/$(PLUTO-R_SOURCE) | tar -C $(FREESWAN_DIR) $(TAR_OPTIONS) -
	rsync -av $(ARUBA_PATCH_DIR)/pluto/ $(PLUTO-R_DIR)/
	rm -rf $(PLUTO-R_DIR)/des*.*
	ln -s $(PLUTO-R_DIR)/../libdes/des_enc.c  $(PLUTO-R_DIR)
	ln -s $(PLUTO-R_DIR)/../libdes/des_locl.h $(PLUTO-R_DIR)
	ln -s $(PLUTO-R_DIR)/../libdes/des.h      $(PLUTO-R_DIR)
	rm -rf $(PLUTO-R_DIR)/*.o
#	${CP} -f -l $(DL_DIR)/aruba/Makefile.pluto $(PLUTO-R_DIR)/Makefile
#	${CP} -f -l $(DL_DIR)/aruba/ipsec_doi.c $(PLUTO-R_DIR)/.
#	${CP} -f -l $(DL_DIR)/aruba/spdb.c $(PLUTO-R_DIR)/.
#	${CP} -f -l $(DL_DIR)/aruba/preshared.c $(PLUTO-R_DIR)/.

pluto: libgmp freeswan $(PLUTO_SOURCE)
	mkdir -p $(DESTLIB_DIR)
	make -C $(PLUTO-R_DIR) CC=$(TARGET_CC) AR=$(TARGET_AR) DEST_DIR=$(DESTLIB_DIR) BUILD_DIR=$(FREESWAN_DIR) \
	TARGET_CFLAGS="$(TARGET_CFLAGS)" install

pluto-clean:
	-$(MAKE) -C $(PLUTO-R_DIR) DEST_DIR=$(DESTLIB_DIR) clean

pluto-dirclean:
	rm -rf $(PLUTO-R_DIR)


#############################################################
#
# Toplevel Makefile options
#
#############################################################
#ifeq ($(strip $(BR2_PACKAGE_ARUBA_PLUTO)),y)
#TARGETS+=pluto
#endif


