#############################################################
#
# i2c-tools
#
#############################################################
I2C_VER      := 3.1.0
I2C_BINARY   := i2c-tools
I2C_DIR      := $(BUILD_DIR)/i2c-tools-$(I2C_VER)
I2C_SOURCE   := i2c-tools-$(I2C_VER).tar.bz2

# Fix kernel cross compile, source dir, readline include path
I2C_MAKEOPTS += CROSS_COMPILE=$(TARGET_CROSS)
I2C_MAKEOPTS += DESTDIR=$(TARGET_DIR)

$(I2C_DIR)/.unpacked:
	bzcat $(DL_DIR)/$(I2C_SOURCE) | tar -C $(BUILD_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(I2C_DIR) package/i2c-tools/ *i2c-tools.patch
	touch $@

$(I2C_DIR)/$(I2C_BINARY): $(I2C_DIR)/.unpacked
	$(MAKE) CPPFLAGS="-I$(BUILD_INCLUDE)" CC=$(TARGET_CC) -C $(I2C_DIR) $(I2C_MAKEOPTS)
	$(STRIP) $(I2C_DIR)/tools/i2cdetect
	$(STRIP) $(I2C_DIR)/tools/i2cdump
	$(STRIP) $(I2C_DIR)/tools/i2cget
	$(STRIP) $(I2C_DIR)/tools/i2cset

$(TARGET_DIR)/$(I2C_BINARY): $(I2C_DIR)/$(I2C_BINARY)
	cp $(I2C_DIR)/tools/i2cdetect   ${TARGET_DIR}/usr/sbin
	cp $(I2C_DIR)/tools/i2cdump     ${TARGET_DIR}/usr/sbin
	cp $(I2C_DIR)/tools/i2cget      ${TARGET_DIR}/usr/sbin
	cp $(I2C_DIR)/tools/i2cset      ${TARGET_DIR}/usr/sbin

i2c-tools: uclibc $(TARGET_DIR)/$(I2C_BINARY)

i2c-tools-clean:
	rm -f $(TARGET_DIR)/usr/sbin/i2cdetect
	rm -f $(TARGET_DIR)/usr/sbin/i2cdump
	rm -f $(TARGET_DIR)/usr/sbin/i2cget
	rm -f $(TARGET_DIR)/usr/sbin/i2cset
	-$(MAKE) -C $(I2C_DIR) CC=$(TARGET_CC) clean

i2c-tools-dirclean:
	if [ -h $(TARGET_CROSS)install ]; then rm -f $(TARGET_CROSS)install; fi
	rm -rf $(I2C_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_I2C_TOOLS)),y)
TARGETS+=i2c-tools
endif
