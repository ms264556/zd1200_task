#############################################################
#
# clusterD
#
#############################################################
TOPDIR=${shell pwd}
CLUSTERD_VER:=0.0.1
CLUSTERD_SRC_DIR:=$(TOPDIR)/../video54/apps/clusterD
#CLUSTERD_DIR:=$(BUILD_DIR)/clusterD-$(CLUSTERD_VER)
CLUSTERD_DIR:=$(BUILD_DIR)/clusterD
RKS_CP:=cp -fapu

CLUSTERD_BINARY:=libcluster.so
CLUSTERD_TARGET_BINARY:=usr/lib/libcluster.so
CLUSTERD_CMD := clusterD
CLUSTERD_TARGET_CMD := bin/$(CLUSTERD_CMD)
CLUSTER_TEST := cluster_test
CLUSTERD_TARGET_TEST := bin/$(CLUSTER_TEST)

ALL_TARGETS = $(TARGET_DIR)/$(CLUSTERD_TARGET_BINARY) \
		$(TARGET_DIR)/$(CLUSTERD_TARGET_TEST) \
		$(TARGET_DIR)/$(CLUSTERD_TARGET_CMD)

$(CLUSTERD_DIR)/.unpacked: $(CLUSTERD_SRC_DIR)/Makefile
	-mkdir $(CLUSTERD_DIR)
	$(RKS_CP) $(CLUSTERD_SRC_DIR)/Makefile $(CLUSTERD_DIR)
	touch $(CLUSTERD_DIR)/.unpacked

clusterD_make:
	$(TARGET_CONFIGURE_OPTS) $(MAKE) ARCH=$(BR2_ARCH) \
	TOPDIR=$(TOPDIR) CLUSTERD_SRC_DIR=$(CLUSTERD_SRC_DIR) \
	-C$(CLUSTERD_DIR)

$(CLUSTERD_DIR)/$(CLUSTERD_BINARY): $(CLUSTERD_DIR)/.unpacked clusterD_make
	echo

$(TARGET_DIR)/$(CLUSTERD_TARGET_BINARY): $(CLUSTERD_DIR)/$(CLUSTERD_BINARY)
	install -D $(CLUSTERD_DIR)/$(CLUSTERD_BINARY) $(TARGET_DIR)/$(CLUSTERD_TARGET_BINARY)

$(TARGET_DIR)/$(CLUSTERD_TARGET_CMD): $(CLUSTERD_DIR)/$(CLUSTERD_CMD)
	install -D $(CLUSTERD_DIR)/$(CLUSTERD_CMD) $(TARGET_DIR)/$(CLUSTERD_TARGET_CMD)

$(TARGET_DIR)/$(CLUSTERD_TARGET_TEST): $(CLUSTERD_DIR)/$(CLUSTER_TEST)
	install -D $(CLUSTERD_DIR)/$(CLUSTER_TEST) $(TARGET_DIR)/$(CLUSTERD_TARGET_TEST)

clusterD: uclibc libRutil librkscrypto librkshash $(ALL_TARGETS)

clusterD-clean:
	rm -f $(ALL_TARGETS)
	-$(MAKE) -C $(CLUSTERD_DIR) clean

clusterD-dirclean:
	rm -rf $(CLUSTERD_DIR)
#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_CLUSTERD)),y)
TARGETS+=clusterD
endif
