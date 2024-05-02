#############################################################
#
# csm
#
#############################################################

TOPDIR=${shell pwd}

CSM_VER            :=1.01
CSM_DIR            :=${BUILD_DIR}/csm

CSM_SOURCE         :=${TOPDIR}/../video54/apps/csm

CSM_TARGET_DIR     :=$(TARGET_DIR)/usr/bin
CSM_TARGET_LIB_DIR :=$(TARGET_DIR)/usr/lib

CSM_CP             :=cp -f -l
CSM_BINARY         :=csm

RSM_CFLAGS =-I$(TOPDIR)/../video54/lib/librsm/include -I$(TOPDIR)/../video54/include


#csm: uclibc  librsm busybox
csm: wireless-tools librsm 
	@mkdir -p $(CSM_DIR) ${CSM_DIR}/../root/usr/lib ${CSM_DIR}/../root/usr/include
	${CSM_CP} ${CSM_SOURCE}/*.[ch] ${CSM_SOURCE}/Makefile ${CSM_DIR}
	$(MAKE) CC=$(TARGET_CC) TOPDIR=$(TOPDIR) TARGET_CFLAGS="$(TARGET_CFLAGS)"\
	-C $(CSM_DIR) 
#	@cp -f $(CSM_DIR)/libcsm.so            $(CSM_TARGET_LIB_DIR)
	@cp -f $(CSM_DIR)/$(CSM_BINARY)        $(CSM_TARGET_DIR)
#	@ln -f $(CSM_TARGET_DIR)/$(CSM_BINARY) $(CSM_TARGET_DIR)/"-$(CSM_BINARY)"


csm-clean:
	@-$(MAKE) -C $(CSM_DIR) clean
	rm -f $(CSM_TARGET_DIR)/$(CSM_BINARY) $(CSM_TARGET_DIR)/"-$(CSM_BINARY)"

csm-dirclean:
	rm -rf $(CSM_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_CSM)),y)
TARGETS+=csm
endif
