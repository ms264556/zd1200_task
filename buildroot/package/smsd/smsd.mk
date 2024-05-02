#############################################################
#
# smsd
#
#############################################################

SMSD_SRC_DIR := ${V54_DIR}/apps/smsd

SMSD_BUILD_DIR:=$(BUILD_DIR)/smsd

SMSD_BUILD_BIN := $(SMSD_BUILD_DIR)/smsd

SMSD_TARGET_BIN := $(TARGET_DIR)/bin/smsd

SMSD_MAKE_OPT := -C $(SMSD_BUILD_DIR) -f $(SMSD_SRC_DIR)/Makefile

SMSD_CFLAGS = $(TARGET_CFLAGS) -Wall -Werror -MMD

smsd: uclibc ipmiutil openssl
	-mkdir $(SMSD_BUILD_DIR)
	$(MAKE) $(SMSD_MAKE_OPT) CC=$(TARGET_CC) \
		SMSD_CFLAGS="$(SMSD_CFLAGS)" SMSD_SRC_DIR="${SMSD_SRC_DIR}" \
		BUILD_DIR="$(BUILD_DIR)"
	install -D $(SMSD_BUILD_BIN) $(SMSD_TARGET_BIN);

smsd-clean:
	rm -rf $(SMSD_TARGET_BIN)
	-$(MAKE) $(SMSD_MAKE_OPT) clean

smsd-dirclean:
	rm -rf $(SMSD_BUILD_DIR)


#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_SMSD)),y)
TARGETS+=smsd
endif
