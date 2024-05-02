#############################################################
#
# Ruckus Network-based Positioning
#
#############################################################

rks_nbp:

rks_nbp-clean:

rks_nbp-dirclean:


#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_RKS_NBP)),y)
TARGETS+=rks_nbp
endif
