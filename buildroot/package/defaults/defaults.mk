#############################################################
#
# Profile defaults
#
#############################################################
TOPDIR=${shell pwd}
DEF_SRC_DIR:=$(TOPDIR)/../video54/defaults/
DEF_TARGET_DIR:=$(TARGET_DIR)/defaults

ifneq (,$(filter $(strip $(PROFILE)),director-aussie director directorx86 sim-zd vmva))

ZD_DEF_TARGET_DIR=${BUILD_DIR}/controller/ac/etc/default
COUNTRYCODE_DIR=${BUILD_DIR}/controller/ac/etc/country_codes

ROMFSINST=${BUILD_DIR}/controller/ac/config/romfs-inst.sh
ROMFSDIR=$(TARGET_DIR)

export ROMFSDIR
export COUNTRYCODE_DIR

COUNTRY_LIST_XML="$(ZD_DEF_TARGET_DIR)/country-list.xml"
REGDOMAIN_BIN=$(COUNTRYCODE_DIR)/regdomain
ZD_TARGET=director  $(COUNTRY_LIST_XML)

$(COUNTRY_LIST_XML):
	if [ -f "$(REGDOMAIN_BIN)" ]; then \
		python $(COUNTRYCODE_DIR)/countrycode_csv.py $(COUNTRYCODE_DIR) > $(COUNTRYCODE_DIR)/country-codes-d.csv; \
	fi; \
	python $(COUNTRYCODE_DIR)/toxml.py > $(ZD_DEF_TARGET_DIR)/country-list.xml; \
	$(ROMFSINST) -p 755 -d $(ZD_DEF_TARGET_DIR)/country-list.xml /etc/airespider-default/country-list.xml;

endif

$(DEF_TARGET_DIR):
	if [ -d $(DEF_SRC_DIR)data/$(PROFILE) ] ; then \
		echo RPM defaults @ video54/defaults/data/$(PROFILE); \
		make -C $(DEF_SRC_DIR) RPM_PROFILE=$(PROFILE) DEF_TARGET_DIR=$(DEF_TARGET_DIR) MY_BUILD_VERSION=$(BR2_BUILD_VERSION); \
	fi;

defaults: $(DEF_TARGET_DIR) $(ZD_TARGET)

defaults-dirclean defaults-clean:
	rm -rf $(DEF_TARGET_DIR)
	-rm -f $(COUNTRY_LIST_XML)
#############################################################
#
# Toplevel Makefile options
#
#############################################################
#ifeq ($(strip $(BR2_PACKAGE_DEFAULTS)),y)
TARGETS+=defaults
#endif
