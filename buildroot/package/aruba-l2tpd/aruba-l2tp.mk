#############################################################
#
# Aruba L2TP Tunneling Daemon
#
#############################################################

# TARGETS

L2TPD-R_VER    := 0.69
L2TPD-R_DIR    :=${BUILD_DIR}/l2tpd-${L2TPD-R_VER}
#L2TPD-R_SOURCE :=aruba/l2tpd.tgz
L2TPD-R_SOURCE :=l2tpd-0.69.tar.gz
DEST_DIR       :=$(TARGET_DIR)/usr/sbin
ETC_DIR        :=$(TARGET_DIR)/etc
LIBS_DIR       :=$(TARGET_DIR)/lib
KERNEL_DIR     :=${shell pwd}/$(strip $(subst ",, $(BR2_KERNEL_PATH)))
L2TPD_FIX_DIR  :=$(TOPDIR)/../video54/patches/aruba/l2tpd

# Rules & Targets
l2tpd_source:
	mkdir -p $(L2TPD-R_DIR)
	zcat $(DL_DIR)/$(L2TPD-R_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	rsync -cav $(L2TPD_FIX_DIR)/ $(L2TPD-R_DIR)/
ifdef NOTDEF
	cp -f -l $(DL_DIR)/aruba/Makefile.l2tpd $(L2TPD-R_DIR)/Makefile
	cp -f -l $(DL_DIR)/aruba/network.c $(L2TPD-R_DIR)/.	
#	cp -f -l $(DL_DIR)/aruba/l2tpd.c $(L2TPD-R_DIR)/.	
	cp -f -l $(DL_DIR)/aruba/call.c $(L2TPD-R_DIR)/.	
endif

l2tpd: l2tpd_source
	mkdir -p $(DEST_DIR)
	make -C $(L2TPD-R_DIR) CC=$(TARGET_CC) DEST_BIN_DIR=$(DEST_DIR) \
		TARGET_CFLAGS="$(TARGET_CFLAGS)" DEST_ETC_DIR=$(ETC_DIR) \
		DEST_LIBS_DIR=$(LIBS_DIR) KERNEL_PATH=$(KERNEL_DIR)
#	mkdir -p $(ETC_DIR)/l2tp
#	cp -f -l $(DL_DIR)/aruba/l2tpd.conf $(ETC_DIR)/rap/l2tp/.
#	cp -f -l $(DL_DIR)/aruba/pap-secrets $(ETC_DIR)/rap/ppp/.
#	cp -f -l $(DL_DIR)/aruba/options.l2tpd.client $(ETC_DIR)/rap/ppp/.

l2tpd-clean:
	-$(MAKE) -C $(L2TPD-R_DIR) DEST_BINDIR=$(DEST_DIR) DEST_ETCDIR=$(ETC_DIR) clean

l2tpd-dirclean:
	rm -rf $(L2TPD-R_DIR)


#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_ARUBA_L2TPD)),y)
TARGETS+=l2tpd
endif


