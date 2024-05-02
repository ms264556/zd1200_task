#############################################################
#
# for old kernel
#
#############################################################

ifeq ($(strip $(BR2_KERNEL_HEADERS_FROM_KERNEL_SRC)),y)

KERNEL_HEADERS_DIR:=$(KERNELPATH)/include

real_kernel_headers:
	( if [ -d $(KERNEL_HEADERS_DIR) ] ; then \
	if [ -f $(LINUX_HEADERS_DIR)/.unpacked ] ; then \
		nfcnt=`find -L $(KERNEL_HEADERS_DIR)/ -type f -newer $(LINUX_HEADERS_DIR)/.unpacked 2>/dev/null | \
		grep -v /linux/ramdisk.h | grep -v /linux/compile.h | \
		grep -v config/MARKER | grep -v linux/autoconf.h | \
		grep -v asm-mips/asm-offsets.h | tee /tmp/new_headers.$$$$ | \
		wc -l` ; \
	else \
		nfcnt=1 ;\
	fi ; \
	if [ "$$nfcnt" != "0" -o ! -d $(LINUX_HEADERS_DIR)/include/linux ] ; then \
		mkdir -p $(LINUX_HEADERS_DIR)/ ;\
		rsync --copy-links -a $(KERNEL_HEADERS_DIR) $(LINUX_HEADERS_DIR)/ ; \
		touch $(LINUX_HEADERS_DIR)/.unpacked ; \
		cat /tmp/new_headers.$$$$ ; \
	fi ; \
	if [ "$$nfcnt" != "0" -a -d $(STAGING_DIR)/include/linux ] ; then \
		cp -pLRuf $(LINUX_HEADERS_DIR)/include/linux \
			$(STAGING_DIR)/include/; \
	fi ; \
	rm -f /tmp/new_headers.$$$$ ; \
	fi )

$(LINUX_HEADERS_DIR)/.patched: real_kernel_headers
	if [ ! -f $(LINUX_HEADERS_DIR)/.patched ] ; then \
	    touch $(LINUX_HEADERS_DIR)/.patched ; \
	elif [ $(LINUX_HEADERS_DIR)/.unpacked -nt $(LINUX_HEADERS_DIR)/.patched ] ; then \
		echo update LINUX_HEADERS_DIR= $(LINUX_HEADERS_DIR) ; \
		touch $(LINUX_HEADERS_DIR)/.patched ; \
	fi

else
# not BR2_KERNEL_HEADERS_FROM_KERNEL_SRC

$(LINUX_HEADERS_UNPACK_DIR)/.unpacked: $(DL_DIR)/$(LINUX_HEADERS_SOURCE)
	@echo "*** Using old sanitized kernel-headers"
	rm -rf $(LINUX_HEADERS_DIR)
	bzcat $(DL_DIR)/$(LINUX_HEADERS_SOURCE) | tar -C $(TOOL_BUILD_DIR) $(TAR_OPTIONS) -
ifneq ($(LINUX_HEADERS_UNPACK_DIR),$(LINUX_HEADERS_DIR))
	ln -fs $(LINUX_HEADERS_UNPACK_DIR) $(LINUX_HEADERS_DIR)
endif
	touch $@

$(LINUX_HEADERS_DIR)/.patched: $(LINUX_HEADERS_UNPACK_DIR)/.unpacked
	toolchain/patch-kernel.sh $(LINUX_HEADERS_DIR) toolchain/kernel-headers linux-libc-headers-$(LINUX_HEADERS_VERSION)\*.patch
ifeq ($(strip $(ARCH)),nios2)
	toolchain/patch-kernel.sh $(LINUX_HEADERS_DIR) toolchain/kernel-headers linux-libc-headers-$(LINUX_HEADERS_VERSION)-nios2nommu.patch.conditional
endif
	touch $@

endif


$(LINUX_HEADERS_DIR)/.configured: $(LINUX_HEADERS_DIR)/.patched
	rm -f $(LINUX_HEADERS_DIR)/include/asm
	@if [ ! -f $(LINUX_HEADERS_DIR)/Makefile ] ; then \
	    echo "VERSION = $(KH_VERSION)"                                                               >  $(LINUX_HEADERS_DIR)/Makefile; \
	    echo "PATCHLEVEL = $(KH_PATCHLEVEL)"                                                         >> $(LINUX_HEADERS_DIR)/Makefile; \
	    echo "SUBLEVEL = $(KH_SUBLEVEL)"                                                             >> $(LINUX_HEADERS_DIR)/Makefile; \
	    echo "EXTRAVERSION ="                                                                        >> $(LINUX_HEADERS_DIR)/Makefile; \
	    echo "KERNELRELEASE=\$$(KH_VERSION).\$$(KH_PATCHLEVEL).\$$(KH_SUBLEVEL)\$$(KH_EXTRAVERSION)" >> $(LINUX_HEADERS_DIR)/Makefile; \
	fi
	@if [ "$(ARCH)" = "powerpc" ];then \
	    (cd $(LINUX_HEADERS_DIR)/include; ln -fs asm-ppc$(NOMMU) asm;) \
	elif [ "$(ARCH)" = "mips" ];then \
	    (cd $(LINUX_HEADERS_DIR)/include; ln -fs asm-mips$(NOMMU) asm;) \
	elif [ "$(ARCH)" = "mipsel" ];then \
	    (cd $(LINUX_HEADERS_DIR)/include; ln -fs asm-mips$(NOMMU) asm;) \
	elif [ "$(ARCH)" = "nios2" ];then \
	    (cd $(LINUX_HEADERS_DIR)/include; ln -fs asm-nios2nommu asm;) \
	elif [ "$(ARCH)" = "arm" ];then \
	    (cd $(LINUX_HEADERS_DIR)/include; ln -fs asm-arm$(NOMMU) asm; \
	     cd asm; \
	     if [ ! -L proc ] ; then \
	     ln -fs proc-armv proc; \
	     ln -fs arch-ebsa285 arch; fi); \
	elif [ "$(ARCH)" = "armeb" ];then \
	    (cd $(LINUX_HEADERS_DIR)/include; ln -fs asm-arm$(NOMMU) asm; \
	     cd asm; \
	     if [ ! -L proc ] ; then \
	     ln -fs proc-armv proc; \
	     ln -fs arch-ebsa285 arch; fi); \
	elif [ "$(ARCH)" = "cris" ];then \
	    (cd $(LINUX_HEADERS_DIR)/include; ln -fs asm-cris asm;) \
	elif [ "$(ARCH)" = "sh3" ];then \
	    (cd $(LINUX_HEADERS_DIR)/include; ln -fs asm-sh asm; \
	     cd asm; \
	     ln -s cpu-sh3 cpu) \
	elif [ "$(ARCH)" = "sh3eb" ];then \
	    (cd $(LINUX_HEADERS_DIR)/include; ln -fs asm-sh asm; \
	     cd asm; \
	     ln -s cpu-sh3 cpu) \
	elif [ "$(ARCH)" = "sh4" ];then \
	    (cd $(LINUX_HEADERS_DIR)/include; ln -fs asm-sh asm; \
	     cd asm; \
	     ln -s cpu-sh4 cpu) \
	elif [ "$(ARCH)" = "sh4eb" ];then \
	    (cd $(LINUX_HEADERS_DIR)/include; ln -fs asm-sh asm; \
	     cd asm; \
	     ln -s cpu-sh4 cpu) \
	elif [ "$(ARCH)" = "i386" -o "$(ARCH)" = "i486" -o "$(ARCH)" = "i586" -o "$(ARCH)" = "i686" ];then \
	    (cd $(LINUX_HEADERS_DIR)/include; ln -fs asm-i386$(NOMMU) asm;) \
	else \
	    (cd $(LINUX_HEADERS_DIR)/include; ln -fs asm-$(ARCH)$(NOMMU) asm;) \
	fi
	touch $(LINUX_HEADERS_DIR)/include/linux/autoconf.h
	touch $@
