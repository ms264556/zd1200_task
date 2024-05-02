#############################################################
#
# regdmn_chann
#
#############################################################
STAMGR_DIR := $(AC_DIR)/usr/stamgr/

REGDMN_CHANN_H := regdmn_chan.h

REGDMN_SRC_DIR := ${V54_DIR}/apps/regdmn_chann

REGDMN_BUILD_DIR:=$(BUILD_DIR)/regdmn_chann

REGDMN_HEADER := $(REGDMN_BUILD_DIR)/$(REGDMN_CHANN_H)

REGDMN_MAKE_OPT := -C $(REGDMN_BUILD_DIR) -f $(REGDMN_SRC_DIR)/Makefile

REGDMN_CFLAGS = $(TARGET_CFLAGS) -Wall -Werror -MMD

regdmn_chann:
	-mkdir $(REGDMN_BUILD_DIR)
	$(MAKE) $(REGDMN_MAKE_OPT) REGDMN_SRC_DIR=$(REGDMN_SRC_DIR) \
	REGDMN_BUILD_DIR=$(REGDMN_BUILD_DIR)
	install -D $(REGDMN_HEADER) $(STAMGR_DIR);

regdmn-clean:
	rm -rf $(STAMGR_DIR)/$(REGDMN_CHANN_H)
	-$(MAKE) $(REGDMN_MAKE_OPT) clean

regdmn-dirclean:
	rm -rf $(REGDMN_BUILD_DIR)


#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_REGDMN_CHANN)),y)
TARGETS+=regdmn_chann
endif
