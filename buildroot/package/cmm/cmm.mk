#############################################################
#
# Cloud Module
#
#############################################################
TOPDIR=${shell pwd}
CMM_SRC_DIR:=$(TOPDIR)/../video54/apps/cmm/
CMM_TARGET_DIR:=$(TARGET_DIR)/opt/cmm/
CMM_CERT_DIR:=$(TARGET_DIR)/opt/cmm/cert/xcloud/

cmm:
	echo "cmm make" ; \
	if [ -d $(CMM_SRC_DIR) ] ; then \
		make -C $(CMM_SRC_DIR) CMM_TARGET_DIR=$(CMM_TARGET_DIR) CMM_CERT_DIR=$(CMM_CERT_DIR); \
	fi;

cmm-dirclean cmm-clean:
	rm -rf $(CMM_TARGET_DIR)
#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_CMM)),y)
TARGETS+=cmm
endif
