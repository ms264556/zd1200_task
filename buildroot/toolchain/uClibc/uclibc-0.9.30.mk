#############################################################
#
# uClibc (the C library)
#
#############################################################

# specifying UCLIBC_CONFIG_FILE on the command-line overrides the .config
# setting.
ifndef UCLIBC_CONFIG_FILE
UCLIBC_CONFIG_FILE=$(subst ",, $(strip $(BR2_UCLIBC_CONFIG)))
#")
endif

ifeq ($(BR2_UCLIBC_VERSION_SNAPSHOT),y)
# Be aware that this changes daily...
UCLIBC_VER:=0.9.30
UCLIBC_DIR:=$(TOOL_BUILD_DIR)/uClibc
UCLIBC_SOURCE:=uClibc-$(strip $(subst ",, $(BR2_USE_UCLIBC_SNAPSHOT))).tar.bz2
#"))
UCLIBC_SITE:=http://www.uclibc.org/downloads/snapshots
UCLIBC_PATCH_DIR:=toolchain/uClibc/
else
# releases
ifeq ($(BR2_UCLIBC_VERSION_0_9_30),y)
UCLIBC_VER:=0.9.30
endif
ifeq ($(BR2_UCLIBC_VERSION_0_9_29),y)
UCLIBC_VER:=0.9.29
endif
ifeq ($(BR2_UCLIBC_VERSION_0_9_28_3),y)
UCLIBC_VER:=0.9.28.3
endif
UCLIBC_SITE:=http://www.uclibc.org/downloads

ifeq ($(BR2_TOOLCHAIN_EXTERNAL_SOURCE),y)
UCLIBC_SITE:=$(VENDOR_SITE)
endif

UCLIBC_OFFICIAL_VERSION:=$(UCLIBC_VER)$(VENDOR_SUFFIX)$(VENDOR_UCLIBC_RELEASE)

UCLIBC_PATCH_DIR:=toolchain/uClibc/

UCLIBC_DIR:=$(TOOL_BUILD_DIR)/uClibc-$(UCLIBC_OFFICIAL_VERSION)
UCLIBC_SOURCE:=uClibc-$(UCLIBC_OFFICIAL_VERSION).tar.bz2
endif

UCLIBC_CAT:=bzcat

UCLIBC_TARGET_ARCH:=$(shell $(SHELL) -c "echo $(ARCH) | sed \
		-e 's/-.*//' \
		-e 's/i.86/i386/' \
		-e 's/sparc.*/sparc/' \
		-e 's/arm.*/arm/g' \
		-e 's/m68k.*/m68k/' \
		-e 's/ppc/powerpc/g' \
		-e 's/v850.*/v850/g' \
		-e 's/sh[234].*/sh/' \
		-e 's/mips.*/mips/' \
		-e 's/mipsel.*/mips/' \
		-e 's/cris.*/cris/' \
		-e 's/nios2.*/nios2/' \
")
# just handle the ones that can be big or little
UCLIBC_TARGET_ENDIAN:=$(shell $(SHELL) -c "echo $(ARCH) | sed \
		-e 's/armeb/BIG/' \
		-e 's/arm/LITTLE/' \
		-e 's/mipsel/LITTLE/' \
		-e 's/mips/BIG/' \
		-e 's/sh[234].*eb/BIG/' \
		-e 's/sh[234]/LITTLE/' \
		-e 's/sparc.*/BIG/' \
")

ifneq ($(UCLIBC_TARGET_ENDIAN),LITTLE)
ifneq ($(UCLIBC_TARGET_ENDIAN),BIG)
UCLIBC_TARGET_ENDIAN:=
endif
endif
ifeq ($(UCLIBC_TARGET_ENDIAN),LITTLE)
UCLIBC_NOT_TARGET_ENDIAN:=BIG
else
UCLIBC_NOT_TARGET_ENDIAN:=LITTLE
endif

UCLIBC_ARM_TYPE:=CONFIG_$(strip $(subst ",, $(BR2_ARM_TYPE)))
#"))
UCLIBC_SPARC_TYPE:=CONFIG_SPARC_$(strip $(subst ",, $(BR2_SPARC_TYPE)))
#"))

$(DL_DIR)/$(UCLIBC_SOURCE):
	$(WGET) -P $(DL_DIR) $(UCLIBC_SITE)/$(UCLIBC_SOURCE)

ifneq ($(BR2_ENABLE_LOCALE),)
UCLIBC_SITE_LOCALE:=http://www.uclibc.org/downloads
UCLIBC_SOURCE_LOCALE:=uClibc-locale-030818.tgz

$(DL_DIR)/$(UCLIBC_SOURCE_LOCALE):
	$(WGET) -P $(DL_DIR) $(UCLIBC_SITE_LOCALE)/$(UCLIBC_SOURCE_LOCALE)

UCLIBC_LOCALE_DATA:=$(DL_DIR)/$(UCLIBC_SOURCE_LOCALE)
else
UCLIBC_LOCALE_DATA=
endif

uclibc-unpacked: $(UCLIBC_DIR)/.unpacked
$(UCLIBC_DIR)/.unpacked: $(DL_DIR)/$(UCLIBC_SOURCE) $(UCLIBC_LOCALE_DATA)
	mkdir -p $(TOOL_BUILD_DIR)
	rm -rf $(UCLIBC_DIR)
	$(UCLIBC_CAT) $(DL_DIR)/$(UCLIBC_SOURCE) | tar -C $(TOOL_BUILD_DIR) $(TAR_OPTIONS) -
	touch $@

uclibc-patched: $(UCLIBC_DIR)/.patched
$(UCLIBC_DIR)/.patched: $(UCLIBC_DIR)/.unpacked
	toolchain/patch-kernel.sh $(UCLIBC_DIR) $(UCLIBC_PATCH_DIR) \*$(UCLIBC_VER)*.patch
ifneq ($(BR2_ENABLE_LOCALE),)
	cp -dpf $(DL_DIR)/$(UCLIBC_SOURCE_LOCALE) $(UCLIBC_DIR)/extra/locale/
endif
	touch $@

# Some targets may wish to provide their own UCLIBC_CONFIG_FILE...
$(UCLIBC_DIR)/.oldconfig: $(UCLIBC_DIR)/.patched $(UCLIBC_CONFIG_FILE)
	cp -f $(UCLIBC_CONFIG_FILE) $(UCLIBC_DIR)/.oldconfig
	$(SED) 's,^CROSS_COMPILER_PREFIX=.*,CROSS_COMPILER_PREFIX="$(TARGET_CROSS)",g' \
		-e 's,# TARGET_$(UCLIBC_TARGET_ARCH) is not set,TARGET_$(UCLIBC_TARGET_ARCH)=y,g' \
		-e 's,^TARGET_ARCH=".*",TARGET_ARCH=\"$(UCLIBC_TARGET_ARCH)\",g' \
		-e 's,^KERNEL_SOURCE=.*,KERNEL_SOURCE=\"$(LINUX_HEADERS_DIR)\",g' \
		-e 's,^KERNEL_HEADERS=.*,KERNEL_HEADERS=\"$(LINUX_HEADERS_DIR)/include\",g' \
		-e 's,^RUNTIME_PREFIX=.*,RUNTIME_PREFIX=\"/\",g' \
		-e 's,^DEVEL_PREFIX=.*,DEVEL_PREFIX=\"/usr/\",g' \
		-e 's,^SHARED_LIB_LOADER_PREFIX=.*,SHARED_LIB_LOADER_PREFIX=\"/lib\",g' \
		$(UCLIBC_DIR)/.oldconfig
ifeq ($(BR2_LARGEFILE),y)
	$(SED) 's,.*UCLIBC_HAS_LFS.*,UCLIBC_HAS_LFS=y,g' $(UCLIBC_DIR)/.oldconfig
else
	$(SED) 's,.*UCLIBC_HAS_LFS.*,UCLIBC_HAS_LFS=n,g' $(UCLIBC_DIR)/.oldconfig
	$(SED) '/.*UCLIBC_HAS_FOPEN_LARGEFILE_MODE.*/d' $(UCLIBC_DIR)/.oldconfig
	echo "# UCLIBC_HAS_FOPEN_LARGEFILE_MODE is not set" >> $(UCLIBC_DIR)/.oldconfig
endif
ifeq ($(BR2_SOFT_FLOAT),y)
	$(SED) 's,.*UCLIBC_HAS_FPU.*,UCLIBC_HAS_FPU=n,g' \
		-e 's,^[^_]*HAS_FPU.*,HAS_FPU=n,g' \
		-e 's,.*UCLIBC_HAS_FLOATS.*,UCLIBC_HAS_FLOATS=y,g' \
		-e 's,.*DO_C99_MATH.*,DO_C99_MATH=y,g' \
		$(UCLIBC_DIR)/.oldconfig
	#$(SED) 's,.*UCLIBC_HAS_FPU.*,UCLIBC_HAS_FPU=n\nHAS_FPU=n\nUCLIBC_HAS_FLOATS=y\nUCLIBC_HAS_SOFT_FLOAT=y,g' $(UCLIBC_DIR)/.oldconfig
else
	$(SED) '/UCLIBC_HAS_FLOATS/d' \
		-e 's,.*UCLIBC_HAS_FPU.*,UCLIBC_HAS_FPU=y\nHAS_FPU=y\nUCLIBC_HAS_FLOATS=y\n,g' \
		$(UCLIBC_DIR)/.oldconfig
endif

$(UCLIBC_DIR)/.config: $(UCLIBC_DIR)/.oldconfig
	cp -f $(UCLIBC_DIR)/.oldconfig $(UCLIBC_DIR)/.config
	mkdir -p $(TOOL_BUILD_DIR)/uClibc_dev/usr/include
	mkdir -p $(TOOL_BUILD_DIR)/uClibc_dev/usr/lib
	mkdir -p $(TOOL_BUILD_DIR)/uClibc_dev/lib
	$(MAKE1) -C $(UCLIBC_DIR) \
		PREFIX=$(TOOL_BUILD_DIR)/uClibc_dev/ \
		DEVEL_PREFIX=/usr/ \
		RUNTIME_PREFIX=$(TOOL_BUILD_DIR)/uClibc_dev/ \
		HOSTCC="$(HOSTCC)" \
		oldconfig
	touch $@

$(UCLIBC_DIR)/.configured: $(UCLIBC_DIR)/.config
	set -x && $(MAKE1) -C $(UCLIBC_DIR) \
		PREFIX=$(TOOL_BUILD_DIR)/uClibc_dev/ \
		DEVEL_PREFIX=/usr/ \
		RUNTIME_PREFIX=$(TOOL_BUILD_DIR)/uClibc_dev/ \
		HOSTCC="$(HOSTCC)" headers \
		$(if $(BR2_UCLIBC_VERSION_0_9_28_3),install_dev,install_headers)
	# Install the kernel headers to the first stage gcc include dir
	# if necessary
ifeq ($(LINUX_HEADERS_IS_KERNEL),y)
	if [ ! -f $(TOOL_BUILD_DIR)/uClibc_dev/usr/include/linux/version.h ]; then \
		cp -pLR $(LINUX_HEADERS_DIR)/include/* \
			$(TOOL_BUILD_DIR)/uClibc_dev/usr/include/; \
	fi
else
	if [ ! -f $(STAGING_DIR)/include/linux/version.h ]; then \
		cp -pLR $(LINUX_HEADERS_DIR)/include/asm \
			$(TOOL_BUILD_DIR)/uClibc_dev/usr/include/; \
		cp -pLR $(LINUX_HEADERS_DIR)/include/linux \
			$(TOOL_BUILD_DIR)/uClibc_dev/usr/include/; \
		if [ -d $(LINUX_HEADERS_DIR)/include/asm-generic ]; then \
			cp -pLR $(LINUX_HEADERS_DIR)/include/asm-generic \
				$(TOOL_BUILD_DIR)/uClibc_dev/usr/include/; \
		fi; \
	fi
endif
	touch $@

$(UCLIBC_DIR)/lib/libc.a: $(UCLIBC_DIR)/.configured
	$(MAKE1) -C $(UCLIBC_DIR) \
		PREFIX= \
		DEVEL_PREFIX=/ \
		RUNTIME_PREFIX=/ \
		HOSTCC="$(HOSTCC)" \
		all
	touch -c $@

uclibc-menuconfig: host-sed $(UCLIBC_DIR)/.config
	$(MAKE1) -C $(UCLIBC_DIR) \
		PREFIX=$(TOOL_BUILD_DIR)/uClibc_dev/ \
		DEVEL_PREFIX=/usr/ \
		RUNTIME_PREFIX=$(TOOL_BUILD_DIR)/uClibc_dev/ \
		HOSTCC="$(HOSTCC)" \
		menuconfig && \
	touch -c $(UCLIBC_DIR)/.config

$(STAGING_DIR)/lib/libc.a: $(UCLIBC_DIR)/lib/libc.a
ifneq ($(BR2_TOOLCHAIN_SYSROOT),y)
	$(MAKE1) -C $(UCLIBC_DIR) \
		PREFIX= \
		DEVEL_PREFIX=$(STAGING_DIR)/ \
		RUNTIME_PREFIX=$(STAGING_DIR)/ \
		install_runtime install_dev
else
	$(MAKE1) -C $(UCLIBC_DIR) \
		PREFIX=$(STAGING_DIR) \
		DEVEL_PREFIX=/usr/ \
		RUNTIME_PREFIX=/ \
		install_runtime install_dev
endif
	# Install the kernel headers to the staging dir if necessary
ifeq ($(LINUX_HEADERS_IS_KERNEL),y)
	if [ ! -f $(STAGING_DIR)/include/linux/version.h ]; then \
		cp -pLR $(LINUX_HEADERS_DIR)/include/* \
			$(STAGING_DIR)/include/; \
	fi
else
	if [ ! -f $(STAGING_DIR)/include/linux/version.h ]; then \
		cp -pLR $(LINUX_HEADERS_DIR)/include/asm \
			$(STAGING_DIR)/include/; \
		cp -pLR $(LINUX_HEADERS_DIR)/include/linux \
			$(STAGING_DIR)/include/; \
		if [ -d $(LINUX_HEADERS_DIR)/include/asm-generic ]; then \
			cp -pLR $(LINUX_HEADERS_DIR)/include/asm-generic \
				$(STAGING_DIR)/include/; \
		fi; \
	fi
endif
	# Build the host utils. Need to add an install target...
	$(MAKE1) -C $(UCLIBC_DIR)/utils \
		PREFIX=$(STAGING_DIR) \
		HOSTCC="$(HOSTCC)" \
		hostutils
	if [ -f $(UCLIBC_DIR)/utils/ldd.host ]; then \
		install -c $(UCLIBC_DIR)/utils/ldd.host $(STAGING_DIR)/bin/ldd; \
		ln -sf ldd $(STAGING_DIR)/bin/$(REAL_GNU_TARGET_NAME)-ldd; \
	fi
	if [ -f $(UCLIBC_DIR)/utils/ldconfig.host ]; then \
		install -c $(UCLIBC_DIR)/utils/ldconfig.host $(STAGING_DIR)/bin/ldconfig; \
		ln -sf ldconfig $(STAGING_DIR)/bin/$(REAL_GNU_TARGET_NAME)-ldconfig; \
		ln -sf ldconfig $(STAGING_DIR)/bin/$(GNU_TARGET_NAME)-ldconfig; \
	fi
	touch -c $@

ifneq ($(TARGET_DIR),)
$(TARGET_DIR)/lib/libc.so.0: $(STAGING_DIR)/lib/libc.a
	$(MAKE1) -C $(UCLIBC_DIR) \
		PREFIX=$(TARGET_DIR) \
		DEVEL_PREFIX=/usr/ \
		RUNTIME_PREFIX=/ \
		install_runtime
ifeq ($(BR2_UCLIBC_VERSION_0_9_28_3),y)
ifneq ($(BR2_PTHREAD_DEBUG),y)
	-$(STRIPCMD) $(STRIP_STRIP_UNNEEDED) $(@D)/libpthread*.so*
endif
endif
	touch -c $@

$(TARGET_DIR)/usr/bin/ldd: $(STAGING_DIR)/bin/$(REAL_GNU_TARGET_NAME)-gcc
	$(MAKE1) -C $(UCLIBC_DIR) CC=$(TARGET_CROSS)gcc \
		CPP=$(TARGET_CROSS)cpp LD=$(TARGET_CROSS)ld \
		PREFIX=$(TARGET_DIR) utils install_utils
ifeq ($(BR2_CROSS_TOOLCHAIN_TARGET_UTILS),y)
	mkdir -p $(STAGING_DIR)/usr/$(REAL_GNU_TARGET_NAME)/target_utils
	install -c $(TARGET_DIR)/usr/bin/ldd \
		$(STAGING_DIR)/usr/$(REAL_GNU_TARGET_NAME)/target_utils/ldd
endif
	touch -c $@

UCLIBC_TARGETS=$(TARGET_DIR)/lib/libc.so.0
ifeq ($(BR2_UCLIBC_INSTALL_TEST_SUITE),y)
UCLIBC_TARGETS+=uclibc-test
endif
endif

uclibc: $(STAGING_DIR)/bin/$(REAL_GNU_TARGET_NAME)-gcc $(STAGING_DIR)/lib/libc.a $(UCLIBC_TARGETS)

uclibc-source: $(DL_DIR)/$(UCLIBC_SOURCE)

uclibc-unpacked: $(UCLIBC_DIR)/.unpacked

uclibc-config: $(UCLIBC_DIR)/.config

uclibc-oldconfig: $(UCLIBC_DIR)/.oldconfig

uclibc-update: uclibc-config
	cp -f $(UCLIBC_DIR)/.config $(UCLIBC_CONFIG_FILE)

uclibc-configured: $(UCLIBC_DIR)/.configured

uclibc-configured-source: uclibc-source

uclibc-clean: uclibc-test-clean
	-$(MAKE1) -C $(UCLIBC_DIR) clean
	rm -f $(UCLIBC_DIR)/.config

uclibc-dirclean: uclibc-test-dirclean
	rm -rf $(UCLIBC_DIR)

uclibc-target-utils:
#$(TARGET_DIR)/usr/bin/ldd

uclibc-target-utils-source: $(DL_DIR)/$(UCLIBC_SOURCE)

$(UCLIBC_DIR)/test/unistd/errno:
	$(MAKE) -C $(UCLIBC_DIR)/test \
	ARCH_CFLAGS=-I$(STAGING_DIR)/include \
	UCLIBC_ONLY=1 TEST_INSTALLED_UCLIBC=1 compile

$(TARGET_DIR)/root/uClibc/test/unistd/errno: $(UCLIBC_DIR)/test/unistd/errno
	mkdir -p $(TARGET_DIR)/root/uClibc
	cp -rdpf $(UCLIBC_DIR)/test $(TARGET_DIR)/root/uClibc
	$(INSTALL) $(UCLIBC_DIR)/Rules.mak $(TARGET_DIR)/root/uClibc
	$(INSTALL) $(UCLIBC_DIR)/.config $(TARGET_DIR)/root/uClibc

uclibc-test: uclibc $(TARGET_DIR)/root/uClibc/test/unistd/errno

uclibc-test-source: uclibc-source

uclibc-test-clean:
	-$(MAKE) -C $(UCLIBC_DIR)/test clean
	rm -rf $(TARGET_DIR)/root/uClibc

uclibc-test-dirclean:
	rm -rf $(TARGET_DIR)/root/uClibc

#############################################################
#
# uClibc for the target just needs its header files
# and whatnot installed.
#
#############################################################

$(TARGET_DIR)/usr/lib/libc.a: $(STAGING_DIR)/lib/libc.a
	$(MAKE1) -C $(UCLIBC_DIR) \
		PREFIX=$(TARGET_DIR) \
		DEVEL_PREFIX=/usr/ \
		RUNTIME_PREFIX=/ \
		install_dev
	# Install the kernel headers to the target dir if necessary
ifeq ($(LINUX_HEADERS_IS_KERNEL),y)
	if [ ! -f $(TARGET_DIR)/usr/include/linux/version.h ]; then \
		cp -pLR $(LINUX_HEADERS_DIR)/include/* \
			$(TARGET_DIR)/usr/include/; \
	fi
else
	if [ ! -f $(TARGET_DIR)/usr/include/linux/version.h ]; then \
		cp -pLR $(LINUX_HEADERS_DIR)/include/asm \
			$(TARGET_DIR)/usr/include/; \
		cp -pLR $(LINUX_HEADERS_DIR)/include/linux \
			$(TARGET_DIR)/usr/include/; \
		if [ -d $(LINUX_HEADERS_DIR)/include/asm-generic ]; then \
			cp -pLR $(LINUX_HEADERS_DIR)/include/asm-generic \
				$(TARGET_DIR)/usr/include/; \
		fi; \
	fi
endif
	touch -c $@

uclibc_target: $(STAGING_DIR)/bin/$(REAL_GNU_TARGET_NAME)-gcc uclibc $(TARGET_DIR)/usr/lib/libc.a $(TARGET_DIR)/usr/bin/ldd

uclibc_target-clean: uclibc-test-clean
	rm -rf $(TARGET_DIR)/usr/include \
		$(TARGET_DIR)/usr/lib/libc.a $(TARGET_DIR)/usr/bin/ldd

uclibc_target-dirclean: uclibc-test-dirclean
	rm -rf $(TARGET_DIR)/usr/include
