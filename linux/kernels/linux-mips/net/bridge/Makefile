#
# Makefile for the IEEE 802.1d ethernet bridging layer.
#

obj-$(CONFIG_BRIDGE) += bridge.o

bridge-y	:= br.o br_device.o br_fdb.o br_forward.o br_if.o br_input.o \
			br_ioctl.o br_notify.o br_stp.o br_stp_bpdu.o \
			br_stp_if.o br_stp_timer.o br_netlink.o br_ipdb.o br_ipv6db.o br_bond.o

bridge-$(CONFIG_SYSFS) += br_sysfs_if.o br_sysfs_br.o

bridge-$(CONFIG_BRIDGE_NETFILTER) += br_netfilter.o


bridge-y += br_rks_common.o
bridge-y += br_rks_vlan.o
bridge-y += br_pif.o
obj-$(CONFIG_BRIDGE_NF_EBTABLES) += netfilter/
