#############################################################
#
# testshape
#
#############################################################
TOPDIR=${shell pwd}
TESTSHAPE_SRC_DIR:=$(TOPDIR)/../video54/common

TESTSHAPE_TARGET_BINARY:=sbin/testshape


force:


$(TARGET_DIR)/$(TESTSHAPE_TARGET_BINARY): force
	$(TARGET_CC) -I$(TOPDIR)/../video54/include $(TESTSHAPE_SRC_DIR)/ruckus_shaper.c -o testshape
	mv testshape $(TARGET_DIR)/$(TESTSHAPE_TARGET_BINARY)

testshape: uclibc  $(TARGET_DIR)/$(TESTSHAPE_TARGET_BINARY)


#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_TESTSHAPE)),y)
TARGETS+=testshape
endif
