obj-m       += hello.o
TOOLCHAIN_DIR ?= /opt/2.6.15_gcc4.2.4
KERNEL_SRC    := ../../linux/kernel/mips-linux-2.6.15
CROSS_COMPILE := ${TOOLCHAIN_DIR}/bin/mips-linux-
 
all:
	$(MAKE) -C ${KERNEL_SRC} CROSS_COMPILE=${CROSS_COMPILE} M=$(shell echo `pwd`) modules
	@cp -v hello.ko ../../linux/rootfs/lib/modules/2.6.15/kernel

clean:
	$(MAKE) -C ${KERNEL_SRC} CROSS_COMPILE=${CROSS_COMPILE} M=$(shell echo `pwd`) clean
