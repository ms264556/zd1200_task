##############################
## Name    binaries.mk
## Notes   Must be called from top-level Makefile
## Description 
##   Create tarfile(s) of AP binaries for SDC.
##   dl/ - directory to store TARBALL_NAME.tgz
##   dl/binaries  - directory to store exp.tgz and exp-tools.tgz
##############################
PRODUCT_TYPE := `awk -F '=' '/^$(PROFILE)=/ {print $$2}' $(TOPDIR)/../build.config | sed -e 's/_production//g'`

TARBALL_BASENAME := $(PRODUCT_TYPE)_binaries
TARBALL_BASEVERSION = $(shell echo "$(TARBALL_BASENAME)-$(RKS_BUILD_VERSION)")
TARBALL_VERSION = $(shell echo "$(TARBALL_BASENAME)-$(BR_BUILD_VERSION)")

TARBALL_NAME = $(TARBALL_VERSION).tgz
TARBALL_FULLNAME = $(DL_DIR)/$(TARBALL_NAME)
BINARIES_DIR = $(DL_DIR)/binaries

##############################
## Export Definitions
##############################
EXPORT_DIRS = root/lib/modules/$(LINUX_HEADERS_VERSION)/net
EXPORT_TOOLS_DIRS = 

    ##  export driver
ifeq ($(strip $(BR2_PACKAGE_CHANNELFLY)),y)
EXPORT_DIRS += $(notdir $(CHANNELFLY_BUILD_DIR))
endif

ifeq ($(strip $(BR2_PACKAGE_MADWIFI)),y)
EXPORT_DIRS += $(notdir $(MADTOOLS_DIR)) 
endif

    ##  export tools
ifeq ($(strip $(BR2_PACKAGE_MESH)),y)
EXPORT_TOOLS_DIRS += $(notdir $(MESHD_BUILD_DIR))
endif

ifeq ($(strip $(BR2_PACKAGE_RFMD)),y)
EXPORT_TOOLS_DIRS += $(notdir $(RFMD_BUILD_DIR))
endif

ifeq ($(strip $(BR2_PACKAGE_SCAND)),y)
EXPORT_TOOLS_DIRS += $(notdir $(SCAND_BUILD_DIR))
endif

##############################
## Generate Binaries (.tgz)
##############################
gen-binaries : dl-binaries-dirclean prep-binaries package-binaries

package-binaries : package-binaries-exp package-binaries-tools
	@echo "TARFILE : $(TARBALL_FULLNAME)"
	$(TAR) -C $(BINARIES_DIR) -czvf $(TARBALL_FULLNAME) ./

package-binaries-exp :
	$(TAR) -C $(BUILD_DIR) -czvf $(BINARIES_DIR)/exp.tgz $(EXPORT_DIRS)

package-binaries-tools :
	@if [ ! -z "$(EXPORT_TOOLS_DIRS)" ]; then \
	  $(TAR) -C $(BUILD_DIR) -czvf $(BINARIES_DIR)/exp-tools.tgz $(EXPORT_TOOLS_DIRS);\
	fi

prep-binaries :
	@mkdir -p $(BINARIES_DIR)

##############################
## Fetch Binaries
##############################
ifneq ($(MAKE_EXPRESS),)
EXP_TARGETS += import_binaries
endif

import-binaries: dl-binaries-dirclean binaries-fetch unpack-binaries
	$(MAKE) unpack-binaries-exp
ifeq ($(MAKE_EXPRESS),2)
	$(MAKE) unpack-binaries-tools
endif

unpack-binaries : prep-binaries
	$(TAR) -C $(BINARIES_DIR) -xzvf $(TARBALL_FULLNAME)

unpack-binaries-exp :
	$(TAR) -C $(BUILD_DIR) -xzvf $(BINARIES_DIR)/exp.tgz

unpack-binaries-tools :
	$(TAR) -C $(BUILD_DIR) -xzvf $(BINARIES_DIR)/exp-tools.tgz

binaries-fetch :
	(cd $(DL_DIR); $(TOP_DIR)/../nexus fetch $(TARBALL_BASEVERSION)\* --latest --group ruckus.official.zoneflex)

##############################
## Clean Targets
##############################
dl-binaries-clean :
	$(RM) $(BINARIES_DIR)/exp.tgz
	$(RM) $(BINARIES_DIR)/exp-tools.tgz
	$(RM) $(TARBALL_FULLNAME)

dl-binaries-dirclean : dl-binaries-clean
	$(RM) -r $(BINARIES_DIR)

##############################
## Jenkins Only Targets
## Keyfile is specific to Jenkins environment
##############################
binaries-deploy :
	(cd $(DL_DIR); $(TOP_DIR)/../nexus deploy $(TARBALL_NAME) --keyfile $(HOME)/nexus.key --group ruckus.official.zoneflex --repo releases --debug)

