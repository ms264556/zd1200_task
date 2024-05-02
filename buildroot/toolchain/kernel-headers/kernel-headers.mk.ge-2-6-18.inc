#############################################################
#
# full kernel tarballs >= 2.6.19.1
#
#############################################################

KERNEL_ARCH:=$(shell $(SHELL) -c "echo \"$(ARCH)\" | sed -e \"s/-.*//\" \
	-e s/i.86/i386/ -e s/sun4u/sparc64/ \
	-e s/arm.*/arm/ -e s/sa110/arm/ \
	-e s/parisc64/parisc/ \
	-e s/powerpc64/powerpc/ \
	-e s/ppc.*/powerpc/ -e s/mips.*/mips/ \
	-e s/sh.*/sh/ -e s/nios2.*/nios2nommu/")

LINUX_HEADERS_DEPENDS:=

KERNEL_HEADERS_DIR:=$(KERNELPATH)/include


.PHONY: real_kernel_headers
real_kernel_headers: $(LINUX_HEADERS_UNPACK_DIR)/.unpacked
	@echo
	@echo "*** Using kernel-headers in kernel source"
	@echo
	$(MAKE) -C $(KERNEL_HEADERS_DIR)/.. ARCH=$(KERNEL_ARCH) \
		HOSTCC="$(HOSTCC)" HOSTCFLAGS="$(HOSTCFLAGS)" HOSTCXX="$(HOSTCXX)" \
		KBUILD_OUTPUT=$(KERNEL_OBJ_PATH) \
		INSTALL_HDR_PATH=$(LINUX_HEADERS_DIR) headers_install


$(LINUX_HEADERS_UNPACK_DIR)/.unpacked:
	mkdir -p $(LINUX_HEADERS_UNPACK_DIR);
	touch $@;


$(LINUX_HEADERS_DIR)/.configured: $(LINUX_HEADERS_UNPACK_DIR)/.unpacked
ifeq ($(BR2_ARCH),"cris")
	ln -s $(LINUX_HEADERS_DIR)/include/arch-v10/arch $(LINUX_HEADERS_DIR)/include/arch
	cp -a $(KERNEL_HEADERS_DIR)/include/linux/user.h $(LINUX_HEADERS_DIR)/include/linux
	$(SED) "/^#include <asm\/page\.h>/d" $(LINUX_HEADERS_DIR)/include/asm/user.h
endif
	touch $@

kernel-headers: real_kernel_headers
