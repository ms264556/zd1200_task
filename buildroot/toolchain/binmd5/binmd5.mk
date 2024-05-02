######################################################################
#
# binmd5
#
######################################################################

V54_DIR = $(TOPDIR)/../video54
V54_LIBRUTIL = $(V54_DIR)/lib/libRutil

INCS = -I./ -I$(V54_DIR)/linux -I$(V54_DIR)/include 

BINMD5_SOURCE_FILE:=$(TOPDIR)/toolchain/binmd5/binmd5.c \

######################################################################
#
# Define stuff
#
######################################################################

DEFINES = -DV54_BSP -DBR2_IMG_TYPE="\"$(BR2_IMG_TYPE)\""
ifeq ($(strip $(BR2_RKS_BOARD_GD4)),y)
DEFINES += -DGD4
else
ifeq ($(strip $(BR2_RKS_BOARD_GD6-5)),y)
DEFINES += -DGD6_5
else
ifeq ($(strip $(BR2_RKS_BOARD_GD6-1)),y)
DEFINES += -DGD6_1
else
ifeq ($(strip $(BR2_RKS_BOARD_DF1)),y)
DEFINES += -DDF1
endif
endif
endif
endif

ifeq ($(strip $(BR2_mips)),y)
ifeq ($(strip $(BR2_ENDIAN_BIG)),y)
DEFINES += -DMIPS_BE
else
DEFINES += -DMIPS_LE
endif
endif

# Define the build version to override image header's fallback string
#DEFINES += -DBR_BUILD_VERSION='"$(shell echo $(BR_BUILD_VERSION))"'
DEFINES += -DBR_BUILD_VERSION="\"$(BR_BUILD_VERSION)\""

ifeq ($(strip $(BR2_PRODUCT_ROUTER)),y)
DEFINES += -DBR2_PRODUCT_ROUTER
else
ifeq ($(strip $(BR2_ENDIAN_ADAPTER)),y)
DEFINES += -DBR2_PRODUCT_ADAPTER
else
ifeq ($(strip $(BR2_ENDIAN_AP)),y)
DEFINES += -DBR2_PRODUCT_AP
endif
endif
endif

ifneq ($(strip $(BR2_CUSTOMER_ID)),)
DEFINES += -DBR2_CUSTOMER_ID="\"$(BR2_CUSTOMER_ID)\""
endif

######################################################################
#
# binmd5 host
#
######################################################################

#BINMD5_HOST:=$(STAGING_DIR)/bin/$(REAL_GNU_TARGET_NAME)-binmd5
BINMD5_HOST:=$(STAGING_DIR)/bin/binmd5

$(BINMD5_HOST): $(BINMD5_SOURCE_FILE) $(HEADER_FILES) $(PCONFIG)
	mkdir -p $(STAGING_DIR)/$(REAL_GNU_TARGET_NAME)/bin/
	mkdir -p $(STAGING_DIR)/bin
	$(HOSTCC) $(DEFINES) $(INCS) $(BINMD5_SOURCE_FILE) -o $(BINMD5_HOST)
ifdef DEPRECATED
	ln -snf ../../bin/$(REAL_GNU_TARGET_NAME)-binmd5 \
		$(STAGING_DIR)/$(REAL_GNU_TARGET_NAME)/bin/binmd5
	ln -snf $(REAL_GNU_TARGET_NAME)-binmd5 \
		$(STAGING_DIR)/bin/$(GNU_TARGET_NAME)-binmd5
endif

binmd5: binmd5_host

binmd5_host: $(BINMD5_HOST)

binmd5_host-source: $(BINMD5_SOURCE_FILE)

binmd5_host-clean:
	rm -f $(BINMD5_HOST)
	rm -f $(STAGING_DIR)/$(REAL_GNU_TARGET_NAME)/bin/binmd5
	rm -f $(STAGING_DIR)/bin/$(GNU_TARGET_NAME)-binmd5

binmd5-clean: binmd5_host-clean

binmd5_host-dirclean:
	true

######################################################################
#
# binmd5 target
#
######################################################################

BINMD5_TARGET:=$(TARGET_DIR)/usr/bin/binmd5

$(BINMD5_TARGET): $(BINMD5_SOURCE_FILE)
	$(TARGET_CC) $(TARGET_CFLAGS) $(DEFINES) $(INCS) $(BINMD5_SOURCE_FILE) -o $(BINMD5_TARGET)

binmd5_target: $(BINMD5_TARGET)

binmd5_target-source: $(BINMD5_SOURCE_FILE)

binmd5_target-clean:
	rm -f $(BINMD5_TARGET)

binmd5_target-dirclean:
	true

#############################################################
#
# Toplevel Makefile options
#
#############################################################

ifeq ($(strip $(BR2_PACKAGE_BINMD5_HOST)),y)
TARGETS+=binmd5_host
endif

ifeq ($(strip $(BR2_PACKAGE_BINMD5_TARGET)),y)
TARGETS+=binmd5_target
endif
