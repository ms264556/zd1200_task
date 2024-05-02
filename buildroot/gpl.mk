##########################################################################
#
# Generate GPL release
#
##########################################################################

GPL      = ruckus_gpl


## applications

ifeq ($(strip $(BR2_PACKAGE_ARUBA_L2TPD)),y)
GPLDIRS += $(L2TPD-R_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_BONJOUR)),y)
GPLDIRS += $(BONJOUR_TOP_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_BRIDGE)),y)
GPLDIRS += $(BRIDGE_BUILD_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_BUSYBOX)),y)
GPLDIRS += $(BUSYBOX_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_DEBUG_UTILS)),y)
GPLDIRS += $(DUTIL_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_DHCP-FORWARDER)),y)
GPLDIRS += $(DHCP-FORWARDER_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_DNSMASQ)),y)
GPLDIRS += $(DNSMASQ_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_FREESWAN)),y)
GPLDIRS += $(FREESWAN_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_IPMIUTIL)),y)
GPLDIRS += $(IPMIUTIL_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_IPROUTE2)),y)
GPLDIRS += $(IPROUTE2_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_IPTABLES)),y)
GPLDIRS += $(IPTABLES_BUILD_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_LIBCCL)),y)
GPLDIRS += $(LIBCCL_DIR)
endif
## used by FreeSwan
ifeq ($(strip $(BR2_PACKAGE_LIBGMP)),y)
GPLDIRS += $(LIBGMP_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_LIBNL)),y)
GPLDIRS += $(LIBNL_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_LIBPCAP)),y)
GPLDIRS += $(LIBPCAP_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_MTD)),y)
GPLDIRS += $(MTD_HOST_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_NDISC6)),y)
GPLDIRS += $(NDISC6_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_OPENL2TP)),y)
GPLDIRS += $(OPENL2TP_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_PARPROUTED)),y)
GPLDIRS += $(PARPROUTED_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_PPPD)),y)
GPLDIRS += $(PPPD_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_RPCAPD)),y)
GPLDIRS += $(RPCAPD_BUILD_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_SNTP)),y)
GPLDIRS += $(SNTP_DIR)
endif
ifeq ($(strip $(BR2_TARGET_ROOTFS_SQUASHFS)),y)
GPLDIRS += $(SQUASHFS_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_SSMTP)),y)
GPLDIRS += $(SSMTP_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_TCPDUMP)),y)
GPLDIRS += $(TCPDUMP_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_WGET)),y)
GPLDIRS += $(WGET_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_WIRELESS_TOOLS)),y)
GPLDIRS += $(WIRELESS_TOOLS_SRC_DIR)
#### legacy wireless tools dir
ifeq ($(WIRELESS_TOOLS_SRC_DIR),)
GPLDIRS += $(WIRELESS_TOOLS_BUILD_DIR)
endif
endif


# for ZoneDirector
ifeq ($(strip $(BR2_PACKAGE_BOA)),y)
GPLDIRS += $(BOA_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_BC)),y)
GPLDIRS += $(BC_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_DIFFUTILS)),y)
GPLDIRS += $(DIFFUTILS_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_E2FSPROGS)),y)
GPLDIRS += $(E2FSPROGS_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_GKERMIT)),y)
GPLDIRS += $(GKERMIT_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_GRUB)),y)
GPLDIRS += $(GRUB_DIR)
GPL_GRUB=Y
endif
ifeq ($(strip $(BR2_PACKAGE_KEXEC)),y)
GPLDIRS += $(KEXEC_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_LIPE)),y)
GPLDIRS += $(LIPE_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_REISERFSPROGS)),y)
GPLDIRS += $(REISERFSPROGS_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_SMTPCLIENT)),y)
GPLDIRS += $(SMTPCLIENT_DIR)
endif
ifeq ($(strip $(BR2_PACKAGE_VSFTPD)),y)
GPLDIRS += $(VSFTPD_DIR)
endif

ifeq ($(strip $(BR2_PACKAGE_DIRECTOR)),y)
GPLDIRS += $(DIRECTOR_DIR)/ac/usr/libsrc/emf/c2lib
GPLDIRS += $(DIRECTOR_DIR)/ac/usr/utils/libcli-1.9.4
GPLDIRS += $(DIRECTOR_DIR)/ac/usr/utils/tinylogin
GPLDIRS += $(DIRECTOR_DIR)/ac/usr/appWeb/gpl_release/appweb-src-2.4.0
endif


## toolchain
### tarballs @ buildroot/dl
GPLTOOLS += $(BINUTILS_SOURCE) $(CCACHE_SOURCE)
GPLTOOLS += $(FAKEROOT_SOURCE) $(GCC_SOURCE) $(UCLIBC_SOURCE)

### toolchain folders on patches/mk
GPLTOOLS2 += binutils ccache gcc sstrip uClibc



## this directory is chosen to be devoid of host, directory specifics,
## and user paths in the tar ball.
ifeq ($(strip $(BR2_PACKAGE_DIRECTOR)),y)
GPL_TARGET_DIR=/tmp/$(GPL)_$(RKS_BUILD_VERSION)_$(PROFILE)
else
GPL_TARGET_DIR=/tmp/$(GPL)_$(BR2_BUILD_VERSION)_$(PROFILE)
endif


define add_gpl_package
	cd $1; \
	echo $2; \
	find $2 -name "*.o" -print0 | xargs -0 rm -f; \
	find $2 -name "*.so.*" -print0 | xargs -0 rm -f; \
	find $2 -name "*.objdump" -print0 | xargs -0 rm -f; \
	find $2 -name "*.sym" -print0 | xargs -0 rm -f; \
	pwd; \
	find $2 -xtype f -print | xargs tar -vrf $(GPL_TARGET_DIR)/$(GPL)_$3.tar;
endef

define add_gpl_dir
	$(call add_gpl_package,$1,./,$2)
endef


## this is more or less a hack for now
## need to find out how bootloader is selected
ifeq ($(GPL_GRUB),)
ifeq ($(PROFILE),collie)
U_BOOT_SRC = uboot.tar.gz
U_BOOT_CODE_PATH = $(UBOOT_SRC_PATH)
else
ifeq ($(PROFILE),wbridge)
U_BOOT_SRC = uboot.tar.gz
U_BOOT_CODE_PATH = $(UBOOT_SRC_PATH)
else
ifeq ($(PROFILE),ap-11n-walle)
U_BOOT_SRC = uboot.tar.gz
U_BOOT_CODE_PATH = $(UBOOT_SRC_PATH)
else
ifeq ($(PROFILE),director-aussie)
U_BOOT_SRC = uboot.tar.gz
U_BOOT_CODE_PATH = $(UBOOT_SRC_PATH)
else
ifeq ($(PROFILE),ap-11n)
U_BOOT_SRC = uboot.tar.gz
U_BOOT_CODE_PATH = $(UBOOT_SRC_PATH)
R_BOOT_SRC = $(REDBOOT_GPL_SRC)
R_BOOT_CODE_PATH = $(BR2_PACKAGE_REDBOOT_PATH)
else
R_BOOT_SRC = $(REDBOOT_GPL_SRC)
R_BOOT_CODE_PATH = $(BR2_PACKAGE_REDBOOT_PATH)
endif
endif
endif
endif
endif
endif

BOOT_SRC = $(R_BOOT_SRC) $(U_BOOT_SRC)


## --- hard coded by looking at profile ap-11n-aussie-boot --
## this is to take care of director-aussie
## which has an extra intermediate boot step that
## runs output of profile ap-11n-aussie-boot
ifeq ($(PROFILE),director-aussie)
GPL_BOOT_LIPE_DIR  := $(subst director-aussie,ap-11n-aussie-boot,$(LIPE_DIR))
GPL_BOOT_KEXEC_DIR := $(subst director-aussie,ap-11n-aussie-boot,$(KEXEC_DIR))
GPLDIRS += $(GPL_BOOT_LIPE_DIR) $(GPL_BOOT_KEXEC_DIR)
endif


gplinfo:
	@echo
	@echo GPL apps: $(sort $(notdir $(GPLDIRS))); echo
	@echo GPL kernel folder: $(BR2_KERNEL_PATH); echo
	@echo GPL toolchain pkg: $(sort $(GPLTOOLS)); echo
	@echo GPL toolchain folder: $(sort $(GPLTOOLS2)); echo
	@echo GPL package folder: $(GPL_TARGET_DIR); echo
ifneq ($(R_BOOT_SRC),)
	@echo \*\* Note \*\*
	@echo Manually prepare \"$(R_BOOT_SRC)\" from $(R_BOOT_CODE_PATH)
	@echo   and put in $(DL_DIR)
	@echo this file \"$(R_BOOT_SRC)\" will be added to GPL apps; echo
endif
ifneq ($(U_BOOT_SRC),)
	@echo \*\* Note \*\*
	@echo Manually prepare \"$(U_BOOT_SRC)\" from $(U_BOOT_CODE_PATH)
	@echo   and put in $(DL_DIR)
	@echo this file \"$(U_BOOT_SRC)\" will be added to GPL apps; echo
endif
ifeq ($(PROFILE),director-aussie)
	@echo \*\* Note \*\*
	@echo 'director-aussie' GPL also requires from \"ap-11n-aussie-boot\" profile:
	@echo -e \\t- $(GPL_BOOT_LIPE_DIR)
	@echo -e \\t- $(GPL_BOOT_KEXEC_DIR); echo
endif


## redboot source is sometimes not branched, so we instead
## refer to a manually added redboot src tarball

gplrelease: gplinfo
	@rm -f ruckus_gpl.tar.bz2;
ifneq ($(R_BOOT_SRC),)
	[ -e $(DL_DIR)/$(R_BOOT_SRC) ]
endif
ifneq ($(U_BOOT_SRC),)
	[ -e $(DL_DIR)/$(U_BOOT_SRC) ]
endif
ifeq ($(PROFILE),director-aussie)
	[ -e $(GPL_BOOT_LIPE_DIR)  ]
	[ -e $(GPL_BOOT_KEXEC_DIR) ]
endif
	@rm -f $(BR2_KERNEL_PATH)/System.map 
	@rm -f $(BR2_KERNEL_PATH)/vmlinux 
	@rm -f $(BUILD_DIR)/$(GPL)*.tar	
	@rm -rf $(GPL_TARGET_DIR)
ifeq ($(strip $(BR2_PACKAGE_DIRECTOR)),y)
	chmod +w $(BUSYBOX_DIR)/shell/ash.c $(BUSYBOX_DIR)/shell/tac_cli.h
	sed -i "/tac_cli_hidden/d" $(BUSYBOX_DIR)/shell/ash.c
	sed -i "/CMD_HIDDEN_STR/d" $(BUSYBOX_DIR)/shell/tac_cli.h
	chmod -w $(BUSYBOX_DIR)/shell/ash.c $(BUSYBOX_DIR)/shell/tac_cli.h
endif
	mkdir $(GPL_TARGET_DIR)
	@cd $(DL_DIR); tar -vrf $(GPL_TARGET_DIR)/$(GPL)_apps.tar $(BOOT_SRC);
	@pwd
	$(call add_gpl_dir,$(BR2_KERNEL_PATH),kernel)
	$(foreach i,$(GPLTOOLS),$(call add_gpl_package,$(DL_DIR),$i,toolchain))
	$(foreach i,$(GPLTOOLS2),$(call add_gpl_package,toolchain,$i,toolchain))
	$(foreach i,$(GPLDIRS),$(call add_gpl_package,$(dir $i),$(notdir $i),apps))
	tar -cvf $(GPL_TARGET_DIR)/$(GPL).tar $(GPL_TARGET_DIR)/$(GPL)_*.tar
	@echo "Compressing Toolchain, Kernel, and Apps tar balls";
	bzip2 $(GPL_TARGET_DIR)/$(GPL).tar;
	@echo $(PROFILE) > $(GPL_TARGET_DIR)/profile
	@echo; echo tarball is ready @ $(GPL_TARGET_DIR)/$(GPL).tar.bz2; echo



