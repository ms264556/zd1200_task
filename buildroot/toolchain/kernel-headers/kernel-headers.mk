#############################################################
#
# Setup the kernel headers. I include a generic package of
# kernel headers here, so you shouldn't need to include your
# own. Be aware these kernel headers _will_ get blown away
# by a 'make clean' so don't put anything sacred in here...
#
#############################################################

LINUX_HEADERS_SITE:=127.0.0.1
LINUX_HEADERS_SOURCE:=unspecified-kernel-headers
LINUX_HEADERS_UNPACK_DIR:=$(TOOL_BUILD_DIR)/linux-libc-headers-null

DEFAULT_KERNEL_HEADERS:=$(strip $(subst ",, $(BR2_DEFAULT_KERNEL_HEADERS)))

# parse linux version string
LNXVER:=$(subst ., ,$(strip $(DEFAULT_KERNEL_HEADERS)))
KH_VERSION:=$(word 1, $(LNXVER))
KH_PATCHLEVEL:=$(word 2, $(LNXVER))
KH_SUBLEVEL:=$(word 3, $(LNXVER))
KH_EXTRAVERSION:=$(word 4, $(LNXVER))
KH_LOCALVERSION:=

# should contain prepended dot
KH_EXTRAVERSION:=$(if $(KH_EXTRAVERSION),.$(KH_EXTRAVERSION),)

LINUX_HEADERS_VERSION:=$(KH_VERSION).$(KH_PATCHLEVEL).$(KH_SUBLEVEL)$(KH_EXTRAVERSION)

# assume old manually sanitized kernel-headers
LINUX_HEADERS_MAKE_HEADERS_INSTALL_SUPPORTED=n
ifeq ($(KH_VERSION),2)
	ifeq ($(KH_PATCHLEVEL),6)
# Before 2.6.13, header files comes from linux-libc-headers
# Since 2.6.13, linux-libc-headers is not provided, header files are copyed from kernel source
		ifeq ($(KH_SUBLEVEL),15)
			LINUX_HEADERS_SITE:=$(BR2_KERNEL_MIRROR)/linux/kernel/v2.6/
			LINUX_HEADERS_SOURCE:=linux-$(LINUX_HEADERS_VERSION).tar.bz2
			LINUX_HEADERS_UNPACK_DIR:=$(TOOL_BUILD_DIR)/linux-$(LINUX_HEADERS_VERSION)
# Since 2.6.18, header files comes from "make headers_install" command
		else
			LINUX_HEADERS_MAKE_HEADERS_INSTALL_SUPPORTED=y
			LINUX_HEADERS_SITE:=$(BR2_KERNEL_MIRROR)/linux/kernel/v2.6/
			LINUX_HEADERS_SOURCE:=linux-$(LINUX_HEADERS_VERSION).tar.bz2
			LINUX_HEADERS_UNPACK_DIR:=$(TOOL_BUILD_DIR)/linux-$(LINUX_HEADERS_VERSION)
		endif
	endif
endif

ifeq ($(strip $(BR2_KERNEL_HEADERS_FROM_KERNEL_SRC)),y)
LINUX_HEADERS_DIR:=$(BUILD_DIR)/linux
else
LINUX_HEADERS_DIR:=$(TOOL_BUILD_DIR)/linux
endif

$(DL_DIR)/$(LINUX_HEADERS_SOURCE):
#	$(call DOWNLOAD,$(LINUX_HEADERS_SITE),$(LINUX_HEADERS_SOURCE))
	mkdir -p $(DL_DIR)
	$(WGET) -P $(DL_DIR) $(LINUX_HEADERS_SITE)/$(LINUX_HEADERS_SOURCE)

ifeq ($(LINUX_HEADERS_MAKE_HEADERS_INSTALL_SUPPORTED),y)
include toolchain/kernel-headers/kernel-headers.mk.ge-2-6-18.inc
else
include toolchain/kernel-headers/kernel-headers.mk.l-2-6-18.inc
endif

.PHONY: kernel-headers
kernel-headers: $(LINUX_HEADERS_DIR)/.configured

.PHONY: kernel-headers-clean
kernel-headers-clean:
	rm -rf $(LINUX_HEADERS_DIR)

.PHONY: kernel-headers-dirclean
kernel-headers-dirclean:
	rm -rf $(LINUX_HEADERS_DIR)
	rm -rf $(LINUX_HEADERS_UNPACK_DIR)

.PHONY: kernel-headers-info
kernel-headers-info:
	@echo "BR2_KERNEL_HEADERS_FROM_KERNEL_SRC           = $(BR2_KERNEL_HEADERS_FROM_KERNEL_SRC)"
	@echo "LINUX_HEADERS_VERSION                        = $(LINUX_HEADERS_VERSION)"
	@echo "KERNEL_HEADERS_DIR                           = $(KERNEL_HEADERS_DIR)"
	@echo "LINUX_HEADERS_DIR                            = $(LINUX_HEADERS_DIR)"
	@echo "LINUX_HEADERS_UNPACK_DIR                     = $(LINUX_HEADERS_UNPACK_DIR)"
	@echo "DEFAULT_KERNEL_HEADERS                       = $(DEFAULT_KERNEL_HEADERS)"
	@echo "LINUX_HEADERS_SITE                           = $(LINUX_HEADERS_SITE)"
	@echo "LINUX_HEADERS_SOURCE                         = $(LINUX_HEADERS_SOURCE)"
	@echo "LINUX_HEADERS_MAKE_HEADERS_INSTALL_SUPPORTED = $(LINUX_HEADERS_MAKE_HEADERS_INSTALL_SUPPORTED)"
