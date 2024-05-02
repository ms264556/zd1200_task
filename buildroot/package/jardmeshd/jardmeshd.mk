#############################################################
#
# Jard meshd
#
#############################################################
JMESHD_BUILD_DIR      :=${BUILD_DIR}/jard-meshd
JMESHD_SRC_DIR        :=${V54_DIR}/apps/jard-meshd

MESHD_TARGET_DIR     := $(TARGET_DIR)/usr/sbin

JMESHD_MAKE_OPT := -C $(JMESHD_BUILD_DIR) -f $(JMESHD_SRC_DIR)/Makefile

MESHD_CFLAGS    = $(TARGET_CFLAGS) $(DASHDRSM) $(DASHIRSM)
MESHD_LDFLAGS  := $(DASHLRSM) -lm

JMESHD_TARGETS  := meshd
JMESHD_SCRIPTS  := gwping wired_mesh bdconnect mlogread slogread

jmeshd_make: libRutil librsm 
	@mkdir -p $(JMESHD_BUILD_DIR)
	make $(JMESHD_MAKE_OPT) CC=$(TARGET_CC) JMESHD_SRC_DIR=$(JMESHD_SRC_DIR) \
		MESHD_LDFLAGS="$(MESHD_LDFLAGS)" MESHD_CFLAGS="$(MESHD_CFLAGS)"
	install -D $(foreach f,$(JMESHD_SCRIPTS),$(JMESHD_SRC_DIR)/$f) $(JMESHD_BUILD_DIR)

jmeshd_install:
	install -D $(foreach f,$(JMESHD_TARGETS) $(JMESHD_SCRIPTS),$(JMESHD_BUILD_DIR)/$f) $(MESHD_TARGET_DIR)
#cd $(MESHD_TARGET_DIR);	ln -snf /writable/meshd meshd
	@$(STRIP) $(foreach f,$(JMESHD_TARGETS),$(MESHD_TARGET_DIR)/$f)

jardmeshd: jmeshd_make jmeshd_install

jardmeshd-clean:
	rm -f $(foreach f,$(JMESHD_TARGETS) $(notdir $(JMESHD_SCRIPTS)),$(MESHD_TARGET_DIR)/$f)
	-$(MAKE) $(JMESHD_MAKE_OPT) clean

jardmeshd-dirclean: jardmeshd-clean
	rm -rf $(JMESHD_BUILD_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_JARD_MESH)),y)
TARGETS += jardmeshd
endif
