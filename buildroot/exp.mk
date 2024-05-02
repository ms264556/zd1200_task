#############################################################
#
# Define Express Tarball Packaging / Directory Info
#
#############################################################

ifeq ($(BIN_DEPOT),)
BIN_DEPOT=/system/bigsky/vol/vol2/make-binaries
endif

BINBLD_TREE=${shell cd ..;pwd}
CODE_TREE=`if [ -d ${BINBLD_TREE}/../../release ]; \
		   then basename ${BINBLD_TREE}; \
		   else if [ -d ${BINBLD_TREE}/../../../private ]; \
		        then echo ${BINBLD_TREE} | sed 's/^.*depot\///g'; \
		        else echo mainline; \
				fi \
		   fi`

#
# ==> set this to a local path for testing
#
BINBLD_DIR = ${BIN_DEPOT}/${CODE_TREE}

BINBLD_EXPORT_TARBALL = $(PROFILE)_$(BR_BUILD_VERSION)_$(VERSION_DATE_STR).tzg
BINBLD_IMPORT_TARBALL = `if [ -r $(TOPDIR)/profiles/$(PROFILE)/build_bin ]; then cat $(TOPDIR)/profiles/$(PROFILE)/build_bin;  else echo $(PROFILE)_$(VERSION_DATE_STR).tzg ; fi`


#############################################################
#
# Exporting Express Build Packaging
#
#############################################################

#
# Export madwifi-driver stuff / meshd in exp.tzg
# Export wireless madwifi tools and hal diagnostic tools in madwifi-tool.tzg
#
EXP_OBJ  = root/lib/modules/$(LINUX_HEADERS_VERSION)/net

ifeq ($(strip $(BR2_PACKAGE_CHANNELFLY)),y)
EXP_OBJ += channelfly
endif

ifeq ($(strip $(BR2_PACKAGE_MADWIFI)),y)
EXP_OBJ += $(notdir $(MADTOOLS_DIR))
endif

ifeq ($(strip $(BR2_PACKAGE_MESH)),y)
EXP2_OBJ += $(notdir $(MESHD_BUILD_DIR))
endif

ifeq ($(strip $(BR2_PACKAGE_RFMD)),y)
EXP2_OBJ += $(notdir $(RFMD_BUILD_DIR))
endif

ifeq ($(strip $(BR2_PACKAGE_SCAND)),y)
EXP2_OBJ += $(notdir $(SCAND_BUILD_DIR))
endif


binaries: info binaries-clean
ifeq ($(strip $(BR2_PACKAGE_MADWIFI)),y)
	mkdir -p $(BINBLD_DIR)
	@echo
	@echo "Packaging modules (madwifi...) (opt. meshd) into exp.tzg"
	@echo
	(cd ${BUILD_DIR}; tar -czf ${BUILD_DIR}/../exp.tzg $(EXP_OBJ))
	(cd ${BUILD_DIR}; mkdir -p empty; tar -czf ${BUILD_DIR}/../exp2.tzg $(EXP2_OBJ) empty)
	@echo
	(cd $(BUILD_DIR)/..; tar -czvf $(BINBLD_DIR)/$(BINBLD_EXPORT_TARBALL) *.tzg)
	@echo
	@echo "${CODE_TREE},AP,,bin_build, Binary Build tarball for $(PROFILE) in \
	 BIN_DEPOT=$(BINBLD_DIR) newball=$(BINBLD_EXPORT_TARBALL)"
	@echo
else
	@echo "Nothing to do..."
endif

binaries-clean binaries-dirclean:
	@rm -rf $(BUILD_DIR)/../*.tzg


#############################################################
#
# Importing Express Build Packaging
#
#############################################################

ifneq ($(MAKE_EXPRESS),)
EXP_TARGETS += import_binaries
endif

verify_build_bin:
	@if [ ! -r $(TOP_DIR)/profiles/$(PROFILE)/build_bin ]; then \
		echo 'Missing $(TOP_DIR)/profiles/$(PROFILE)/build_bin'; \
		exit 1; \
	fi

#
# Import madwifi-driver stuff / meshd in exp.tzg
# Import wireless madwifi tools and hal diagnostic tools in madwifi-tool.tzg
#
import_binaries: verify_build_bin binaries-clean
	@echo
	@echo "Fetching $(PROFILE) bin tarball '$(BINBLD_IMPORT_TARBALL)' for from '$(BINBLD_DIR)'"
	@echo
	(cd ${BUILD_DIR}/..; tar -xvzf $(BINBLD_DIR)/$(BINBLD_IMPORT_TARBALL))
	@echo
	@echo "Unpacking exp.tzg & Installing modules (madwifi...) (opt. meshd)"
	@echo
	(cd ${BUILD_DIR}; tar -xzvf ${BUILD_DIR}/../exp.tzg)
ifeq ($(MAKE_EXPRESS),2)
	(cd ${BUILD_DIR}; tar -xzvf ${BUILD_DIR}/../exp2.tzg)
endif
	@echo

