#############################################################
#
# rpmkeytest
#
#############################################################
TOPDIR=${shell pwd}
RPMKEYTEST_VER     := 0.0.1
RPMKEYTEST_BINARY  := rpmkeytest
RPMKEYTEST_DIRSUFFIX := $(RPMKEYTEST_BINARY)

RPMKEYTEST_SRC_DIR:=$(TOPDIR)/../video54/apps/$(RPMKEYTEST_DIRSUFFIX)
RPMKEYTEST_DIR:=$(BUILD_DIR)/$(RPMKEYTEST_DIRSUFFIX)
RPMKEYTEST_CP:=cp -fau
RPMKEYTEST_TARGET_BINARY:=usr/sbin/$(RPMKEYTEST_BINARY)
RPMKEYTEST_CFLAGS = $(TARGET_CFLAGS)

$(RPMKEYTEST_DIR)/.unpacked: $(RPMKEYTEST_SRC_DIR)/Makefile
	-mkdir $(RPMKEYTEST_DIR)
	$(RPMKEYTEST_CP) $(RPMKEYTEST_SRC_DIR)/Makefile $(RPMKEYTEST_DIR)
	touch $@

$(RPMKEYTEST_BINARY)_make:
	$(TARGET_CONFIGURE_OPTS) $(MAKE) TOPDIR=$(TOPDIR) CC=$(TARGET_CC) DASHLRSM="$(DASHLRSM)" -C $(RPMKEYTEST_DIR) RPMKEYTEST_CFLAGS="$(RPMKEYTEST_CFLAGS)"

$(RPMKEYTEST_DIR)/$(RPMKEYTEST_BINARY): $(RPMKEYTEST_DIR)/.unpacked rpmkeytest_make
	echo

$(TARGET_DIR)/$(RPMKEYTEST_TARGET_BINARY): $(RPMKEYTEST_DIR)/$(RPMKEYTEST_BINARY)
	install -D $(RPMKEYTEST_DIR)/$(RPMKEYTEST_BINARY) $@

$(RPMKEYTEST_BINARY): uclibc librsm $(TARGET_DIR)/$(RPMKEYTEST_TARGET_BINARY)

$(RPMKEYTEST_BINARY)-clean:
	rm -f $(TARGET_DIR)/$(RPMKEYTEST_TARGET_BINARY)
	-$(MAKE) -C $(RPMKEYTEST_DIR) clean

$(RPMKEYTEST_BINARY)-dirclean:
	rm -rf $(RPMKEYTEST_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_RPMKEYTEST)),y)
TARGETS += $(RPMKEYTEST_BINARY)
endif
