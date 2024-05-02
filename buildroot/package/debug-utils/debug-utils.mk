#############################################################
#
# Debug-utils
#
#############################################################
TOPDIR=${shell pwd}
DUTIL_VER:=0.0.1
DUTIL_SRC_DIR:=$(TOPDIR)/../video54/apps/debug-utils
DUTIL_DIR:=$(BUILD_DIR)/debug-utils
RKS_CP:=cp -fapu
PROF_CMD := readprofile
PROF_TARGET_CMD := usr/sbin/readprofile
PROF2_CMD := readprofile2
PROF2_TARGET_CMD := usr/sbin/readprofile2
STATS_CMD := cpustats
STATS_TARGET_CMD := usr/sbin/cpustats

ALL_TARGETS =  \
		$(TARGET_DIR)/$(PROF_TARGET_CMD) \
		$(TARGET_DIR)/$(PROF2_TARGET_CMD) \
		$(TARGET_DIR)/$(STATS_TARGET_CMD)

$(DUTIL_DIR)/.unpacked: $(DUTIL_SRC_DIR)/Makefile
	-mkdir $(DUTIL_DIR)
	$(RKS_CP) $(DUTIL_SRC_DIR)/Makefile $(DUTIL_DIR)
	touch $(DUTIL_DIR)/.unpacked

Dutil_make:
	$(TARGET_CONFIGURE_OPTS) $(MAKE) CC=$(TARGET_CC) \
	TOPDIR=$(TOPDIR) DUTIL_SRC_DIR=$(DUTIL_SRC_DIR) \
	-C$(DUTIL_DIR)

#$(DUTIL_DIR)/$(DUTIL_BINARY): $(DUTIL_DIR)/.unpacked Dutil_make
#	echo

$(TARGET_DIR)/$(PROF_TARGET_CMD): $(DUTIL_DIR)/$(PROF_CMD)
	install -D $(DUTIL_DIR)/$(PROF_CMD) $(TARGET_DIR)/$(PROF_TARGET_CMD)

$(TARGET_DIR)/$(PROF2_TARGET_CMD): $(DUTIL_DIR)/$(PROF2_CMD)
	install -D $(DUTIL_DIR)/$(PROF2_CMD) $(TARGET_DIR)/$(PROF2_TARGET_CMD)

$(TARGET_DIR)/$(STATS_TARGET_CMD): $(DUTIL_DIR)/$(STATS_CMD)
	install -D $(DUTIL_DIR)/$(STATS_CMD) $(TARGET_DIR)/$(STATS_TARGET_CMD)

debug_utils: uclibc $(DUTIL_DIR)/.unpacked Dutil_make $(ALL_TARGETS)

debug_utils-clean:
	rm -f $(ALL_TARGETS)
	-$(MAKE) -C $(DUTIL_DIR) clean

debug_utils-dirclean:
	rm -rf $(DUTIL_DIR)
#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_DEBUG_UTILS)),y)
TARGETS+=debug_utils
endif
