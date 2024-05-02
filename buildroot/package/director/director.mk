#############################################################
#
# Ruckus Director (formally controller)
#
#############################################################

# TARGETS
DIRECTOR_DIR    :=${BUILD_DIR}/controller
DIRECTOR_SOURCE :=${TOPDIR}/../controller
DEST_DIR        :=$(TARGET_DIR)/usr/sbin

# AC dir
AC_DIR            :=${DIRECTOR_DIR}/ac
AC_COMMON_DIR     :=${DIRECTOR_DIR}/common
V54_INCLUDE_DIR   :=${DIRECTOR_DIR}/video54_include
AC_SRC_DIR        :=${DIRECTOR_SOURCE}/ac
AC_COMMON_SRC_DIR :=${DIRECTOR_SOURCE}/common
V54_INCLUDE_SRC_DIR :=${DIRECTOR_SOURCE}/../video54/include
V54_LINUX_INCLUDE_SRC_DIR :=${DIRECTOR_SOURCE}/../video54/linux

# BUILD Number
BR_BUILD_NUM=$(shell echo $(BR_BUILD_VERSION))
BUILD_NUM=`echo $(BR_BUILD_NUM) | \
		sed 's/[0-9]\+\.[0-9]\+\.[0-9]\+\.[0-9]\+\.//' | \
		sed 's/[a-z][0-9]*//'`
#CXXSUP            :=$(shell $(TARGET_CC) -print-file-name=libsupc++.a)
#CXXLIBS           :=$(CXXSUP)

DIRECTOR_CFLAGS   += -DTAC_SIG

V54_DEFS          := V54_BSP=1


ifeq ($(strip $(BR2_PACKAGE_LIBSQLITE)),y)
director: libsqlite3
endif # BR2_PACKAGE_LIBSQLITE

ifeq ($(strip $(BR2_PACKAGE_LIBRKSCRYPTO)),y)
director: librkscrypto
AC_EXTRA_LDFLAGS  += -L$(TARGET_DIR)/usr/lib -lrkscrypto
ifeq ($(strip $(BR2_PACKAGE_LIBRKSCRYPTO_AES)),y)
AC_EXTRA_CFLAGS   += -DV54_AES -I$(V54_DIR)/lib/librkscrypto/aes/gladman-aes
ifeq ($(strip $(BR2_PACKAGE_LIBRKSCRYPTO_AES_CCMP)),y)
AC_EXTRA_CFLAGS   += -DV54_AES_CCMP -I$(V54_DIR)/lib/librkscrypto/aes_ccmp
endif # BR2_PACKAGE_LIBRKSCRYPTO_AES_CCMP
endif # BR2_PACKAGE_LIBRKSCRYPTO_AES
endif # BR2_PACKAGE_LIBRKSCRYPTO

ifeq ($(strip $(BR2_PACKAGE_LIBCACHE_POOL)),y)
director: libcache_pool
AC_EXTRA_LDFLAGS  += -L$(TARGET_DIR)/usr/lib -lcache_pool
AC_EXTRA_CFLAGS   += -I$(V54_DIR)/lib/libcache_pool
endif # BR2_PACKAGE_LIBCACHE_POOL

ifeq ($(strip $(BR2_PACKAGE_C2LIB)),y)
director: c2lib
AC_EXTRA_LDFLAGS  += -L$(TARGET_DIR)/usr/lib -lc2lib
AC_EXTRA_CFLAGS   += -I$(V54_DIR)/apps/
endif # BR2_PACKAGE_C2LIB

ifeq ($(strip $(BR2_PACKAGE_LOCALIP)),y)
director: localip
AC_EXTRA_LDFLAGS  += -L$(TARGET_DIR)/usr/lib -llocalip
AC_EXTRA_CFLAGS   += -I$(V54_DIR)/apps/
endif # BR2_PACKAGE_LOCALIP

ifeq ($(strip $(BR2_PACKAGE_LIBIPC_SOCK)),y)
director: libipc_sock
AC_EXTRA_LDFLAGS  += -L$(TARGET_DIR)/usr/lib -lipc_sock
AC_EXTRA_CFLAGS   += -I$(V54_DIR)/lib/libipc_sock
endif # BR2_PACKAGE_LIBIPC_SOCK


ifeq ($(strip $(BR2_PACKAGE_LIBRKSHASH)),y)
director: librkshash
AC_EXTRA_LDFLAGS  += -L$(TARGET_DIR)/usr/lib -lrkshash
AC_EXTRA_CFLAGS   += -I$(V54_DIR)/lib/librkshash/sha1
AC_EXTRA_CFLAGS   += -I$(V54_DIR)/lib/librkshash/hmac_sha1
AC_EXTRA_CFLAGS   += -I$(V54_DIR)/lib/librkshash/pbkdf2_sha1
AC_EXTRA_CFLAGS   += -I$(V54_DIR)/lib/librkshash/md5
AC_EXTRA_CFLAGS   += -I$(V54_DIR)/lib/librkshash/hmac_md5
endif # BR2_PACKAGE_LIBRKSHASH

AC_DEFS           = BR2_BUILD=yes BR2_OFFICIAL=$(BR_BUILD_OFFICIAL)
AC_DEFS          += BR2_ARCH=$(BR2_ARCH) BR2_PROFILE=$(PROFILE)
AC_DEFS          += KBUILD_OUTPUT=$(KERNEL_OBJ_PATH) KERNELPATH=$(KERNELPATH_ABS)
AC_DEFS          += BR2_PACKAGE_BONJOUR=$(BR2_PACKAGE_BONJOUR)
AC_DEFS          += BR2_4SIM_AP=$(BR2_PACKAGE_DIRECTOR_WITH_SIM_AP)
AC_DEFS          += TARGET_CROSS=$(TARGET_CROSS)
AC_DEFS          += LINUX_KERNEL_SRC_DIR=$(KERNELPATH_ABS)
AC_DEFS          += DIRECTOR_CFLAGS=-DV54_CUSTOM_PROFILE=1
AC_DEFS          += JAVA_HOME=$(JAVA_HOME)

ifeq ($(strip $(BR2_PACKAGE_RKS_TUNNEL_EVAL)),y)
AC_DEFS += RKS_TUNNEL_EVAL=1
endif

ifeq ($(strip $(BR2_PACKAGE_RADIUS_DM)),y)
AC_DEFS += RKS_RADIUS_DYNAMIC_AUTHORIZATION=1
endif

ifeq ($(strip $(BR2_PACKAGE_RADIUS_CUI)),y)
AC_DEFS += RKS_RADIUS_CUI=1
endif

ifeq ($(strip $(BR2_PACKAGE_SUPPORT_ENTITLEMENT)),y)
AC_DEFS += RKS_SUPPORT_ENTITLEMENT=1
endif

ifeq ($(strip $(BR2_PACKAGE_RKS_SUPPORT_REPORT)),y)
AC_DEFS += RKS_SUPPORT_REPORT=1
endif

ifeq ($(strip $(BR2_RKS_BOARD_NAR5520)),y)
AC_DEFS += BR2_PLATFORM=nar5520
ifeq ($(strip $(BR2_TARGET_RESTORETOOL_KIT)),y)
AC_DEFS += BR2_RESTORETOOL_DIR=$(BR2_RESTORETOOL_DIR)
endif
else
ifeq ($(strip $(PROFILE)),director-aussie)
AC_DEFS += BR2_PLATFORM=ar7161
else
ifeq ($(strip $(PROFILE)),zd1200)
AC_DEFS += BR2_PLATFORM=zd1200
else
AC_DEFS += BR2_PLATFORM=ar7100
endif
endif
endif
ifeq ($(strip $(BR2_PACKAGE_DMALLOC)),y)
AC_DEFS += DMALLOC_CFLAGS="$(DMALLOC_CFLAGS)"
AC_DEFS += DMALLOC_LIB="$(DMALLOC_LIB)"
DMALLOC_PKG = dmalloc
endif

ifeq ($(strip $(BR2_PACKAGE_ACGPS)),y)
AC_DEFS += V54_GPS=1
endif

AC_DEFS += VERSION_BUILD_NUM=$(RKS_BUILD_VERSION) BUILD_NUM=$(BUILD_NUM)

$(AC_DIR)/.unpacked:
	rm -rf $(AC_DIR)
	rm -rf $(AC_COMMONG_DIR)
	@mkdir -p $(AC_DIR)
	@mkdir -p $(AC_COMMON_DIR)
	@mkdir -p $(V54_INCLUDE_DIR) 
	cp -l -f -r $(AC_SRC_DIR)/* $(AC_DIR)
	cp -l -f -r $(AC_COMMON_SRC_DIR)/* $(AC_COMMON_DIR)
	cp -l -f -r $(V54_INCLUDE_SRC_DIR)/* $(V54_INCLUDE_DIR)
	cp -l -f -r $(V54_LINUX_INCLUDE_SRC_DIR)/* $(V54_INCLUDE_DIR)
	touch $(AC_DIR)/.unpacked

director-link:
	cp -l -f -r $(AC_SRC_DIR)/* $(AC_DIR)
	cp -l -f -r $(AC_COMMON_SRC_DIR)/* $(AC_COMMON_DIR)

director:  linux-kernel openssl dropbear clusterD zing $(DMALLOC_PKG) libsqlite3 librkscrypto $(AC_DIR)/.unpacked regdomain regdmn_chann
	$(MAKE) $(AC_DEFS) OPENSSL_DIR=$(OPENSSL_DIR) -C $(AC_DIR) $(V54_DEFS) \
		AC_EXTRA_CFLAGS="$(AC_EXTRA_CFLAGS)" AC_EXTRA_LDFLAGS="$(AC_EXTRA_LDFLAGS)" \
		BUILD_NUM=$(BUILD_NUM) all package


director-clean:
	rm -f $(AC_DIR)/.unpacked
	-$(MAKE) -C $(DIRECTOR_DIR)/ac clean

director-dirclean:
	rm -rf $(DIRECTOR_DIR)


# RELEASE_DIR set by package/release/release.mk
RELEASE_OPTS = BUILD_NUM=$(BUILD_NUM) BLD_RELEASE_DIR=$(RELEASE_DIR) 

# For zd1k build, need to fix_up the rootfs to get more space
# DIRECTOR_FIXUP is used in package/rlease/release.mk
ifeq ($(strip $(BR2_TARGET_ROOTFS_SQUASHFS)),y)
DIRECTOR_FIXUP=director-fixup
else
DIRECTOR_FIXUP=
endif

.PHONY: director-fixup

# relocate some files before building rootfs - space limitation for ZD1K
director-fixup: director squashfs
	cd $(AC_DIR) ;  env TOOLPATH=$(TOOLPATH) ./rootfs_fix.sh

# need dependency on rks_release before calling controller/ac/build_fd_pkg.sh
director-package: rks_release
ifeq ($(strip $(BR2_RKS_BOARD_ZD1200)),y)
	mkdir -p $(AC_DIR)
	cp $(BASE_DIR)/package/director/build_pkg.sh $(AC_DIR)
	chmod +x $(AC_DIR)/build_pkg.sh
	env BLD_RELEASE_DIR=$(BLD_RELEASE_DIR) $(AC_DEFS) $(AC_DIR)/build_pkg.sh
else	
	$(MAKE) $(AC_DEFS) -C $(AC_DIR) $(V54_DEFS) $(RELEASE_OPTS) upgrade
endif	

#director-upgrade: 
#	$(MAKE) $(AC_DEFS) -C $(AC_DIR)/etc $(V54_DEFS) $(RELEASE_OPTS) package
#	$(MAKE) KEEP_TARGET_DIR=yes squashfsroot kernel-rootfs-flash
#	$(MAKE) $(AC_DEFS) -C $(AC_DIR) $(V54_DEFS) $(RELEASE_OPTS) upgrade

#
# rules for debugging
#
debug-print-%:
	@echo "Variable: $(subst debug-print-,,$@)"
	@echo "Origin:   $(origin $(subst debug-print-,,$@))"
	@echo -n  "Value:    " ; if [ "$(origin $(subst debug-print-,,$@))" != "undefined" ] ; then echo $($(subst debug-print-,,$@)) ; else  echo "undefined" ; fi

#
# rules for debugging
#
director-print-%:
	make $(AC_DEFS) $(V54_DEFS) OPENSSL_DIR=$(OPENSSL_DIR) -C $(AC_DIR) BUILD_NUM=$(BUILD_NUM) $(subst director-,,$@)

#
# rules for subdirectory build
#
director-usr-%:
	make $(AC_DEFS) $(V54_DEFS) OPENSSL_DIR=$(OPENSSL_DIR) -C $(AC_DIR)/usr/$(subst director-usr-,,$@) all package
director-clean-%:
	make $(AC_DEFS) $(V54_DEFS) OPENSSL_DIR=$(OPENSSL_DIR) -C $(AC_DIR)/usr/$(subst director-clean-,,$@) clean
director-%:
	make $(AC_DEFS) $(V54_DEFS) OPENSSL_DIR=$(OPENSSL_DIR) -C $(AC_DIR)/$(subst director-,,$@) all package

director-enable-nfs:
	sudo ln -s $(TARGET_DIR) /nfs
	sudo /usr/sbin/exportfs -o rw,no_root_squash *:/nfs

director-disable-nfs:
	sudo /usr/sbin/exportfs -u *:/nfs
	sudo rm -f /nfs


#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_DIRECTOR)),y)
TARGETS+=director 
POST_TARGETS+=director-package
endif
