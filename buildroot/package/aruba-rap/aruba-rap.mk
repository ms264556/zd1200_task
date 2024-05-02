#############################################################
#
# Aruba Remote Access Point
#
#############################################################

ARUBA_VER:=1.0.0

VPN_SCRIPTS_SRC:=$(TOPDIR)/../video54/apps/aruba_vpn

aruba-rap: pluto l2tpd aruba-provision
	rsync -ca $(VPN_SCRIPTS_SRC)/root/etc/	$(TARGET_DIR)/etc/
	rsync -ca $(VPN_SCRIPTS_SRC)/root/sbin/	$(TARGET_DIR)/usr/local/sbin/
	rsync -ca $(VPN_SCRIPTS_SRC)/root/lib/	$(TARGET_DIR)/usr/local/lib/ipsec/

aruba-rap-clean:

aruba-rap-dirclean:

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_ARUBA_REMOTE_AP)),y)
TARGETS+=aruba-rap
endif
