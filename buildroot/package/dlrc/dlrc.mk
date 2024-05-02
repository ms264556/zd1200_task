#############################################################
#
# dlrc -- a service for downloading and running ephemeral
# code modules over the Internet, safely and securely,
# in AP/Routers
#
#############################################################

TOPDIR=${shell pwd}

DLRC_VER            :=1.0
DLRC_DIR            :=${BUILD_DIR}/dlrc
DLRC_SOURCE         :=${TOPDIR}/../video54/apps/dlrc
DLRC_TARGET_DIR     :=$(TARGET_DIR)/usr/sbin
DLRC_CP             :=cp -f -l
DLRC_BINARY         :=dlrcs

dlrcs: uclibc librsm
	echo DEBUG DLRC
	@mkdir -p $(DLRC_DIR) 
	${DLRC_CP} ${DLRC_SOURCE}/*.[ch] ${DLRC_SOURCE}/*.pem ${DLRC_SOURCE}/Makefile ${DLRC_SOURCE}/*.cnf ${DLRC_DIR}
	${DLRC_CP} -r ${DLRC_SOURCE}/downloadablecode ${DLRC_DIR}
	$(MAKE) CC=$(TARGET_CC) VIDEO54_DIR=$(V54_DIR) TARGET_CFLAGS="$(TARGET_CFLAGS)" -C $(DLRC_DIR) dlrcs
	echo DEBUG TARGET_DIR ${TARGET_DIR}
	mkdir -p ${TARGET_DIR}/etc/dlrc
	cp -f ${DLRC_DIR}/dlrcs ${TARGET_DIR}/usr/sbin
	cp -f ${DLRC_DIR}/*.pub.pem ${TARGET_DIR}/etc/dlrc
	echo DEBUG pwd `pwd`
	$(MAKE) CC=$(TARGET_CC) VIDEO54_DIR=$(V54_DIR) TARGET_CFLAGS="$(TARGET_CFLAGS)" -C $(DLRC_DIR)/downloadablecode 
	@cp -f $(DLRC_DIR)/$(DLRC_BINARY) $(DLRC_TARGET_DIR)
	(cd ${DLRC_DIR}; make dlrcc) # Make client on x86 architecture

dlrc: dlrcs

dlrc-clean:
	@rm -f $(DLRC_TARGET_DIR)/$(DLRC_BINARY)
	-$(MAKE) -C $(DLRC_DIR) TOP_DIR=$(TOPDIR) clean
	rm -f $(DLRC_TARGET_DIR)/$(DLRC_BINARY) $(DLRC_TARGET_DIR)/"-$(DLRC_BINARY)"

# refer to dlrc-clean, so it can clean out librsc
dlrc-dirclean: dlrc-clean
	rm -rf $(DLRC_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_DLRC)),y)
TARGETS+=dlrcs
endif
