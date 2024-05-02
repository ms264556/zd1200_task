#RUCKUS AP SDK make file v 1.0

include config.in

BUILD_KERNEL = build_kernel build_modules
ALL	= build_kernel build_apps build_rootfs build_images

all: info ${ALL}
	@echo "=== Done ==="

kernel: ${BUILD_KERNEL}
	@echo "=== Rebuilt Kernel ==="

# this re-compile the kernel
# universe > world > all
universe: info kernel ${ALL}

world: info ${ALL}
	@echo "=== build World ==="

help:
	@echo "Usages:"
	@echo "	Build kernel_config:    make kernel_config"
	@echo "	Build kernel:make       build_kernel build_modules"
	@echo "	Build rootfs:make       build_rootfs"
	@echo "	Build images:make       build_images"
	@echo "	Check parameters:       make info"

info:
	@echo "ARCH	= ${ARCH}"
	@echo "TARGET	= ${TARGET}" 
	@echo "HOST	= ${HOST}"
	@echo "PREFIX	= ${PREFIX}"
	@echo "PATH	= ${PATH}"
	@echo "AS	= ${AS}"
	@echo "AR	= ${AR}"
	@echo "CC	= ${CC}"
	@echo "CPP	= ${CPP}"
	@echo "LD	= ${LD}"
	@echo "NM	= ${NM}"
	@echo "OBJCOPY	= ${OBJCOPY}"
	@echo "OBJDUMP	= ${OBJDUMP}"
	@echo "RANLIB	= ${RANLIB}"
	@echo "READELF	= ${READELF}"
	@echo "SIZE	= ${SIZE}"
	@echo "STRINGS	= ${STRINGS}"
	@echo "STRIP	= ${STRIP}"

make_kernel :
	@echo "=== Make Kernel ==="
	cp -n ${KERNE_CONFIG} ${KERNEL_SRC}/.config
	make -C ${KERNEL_SRC} V=1 EXTRA_DEFS="${CFLAGS_KERNEL}" oldconfig vmlinux build_ldflags_vmlinux build_kernel1_obj

kernel_config :
	@echo "=== Make Kernel_config ==="
	cp -n ${KERNE_CONFIG} ${KERNEL_SRC}/.config
	make -C ${KERNEL_SRC} V=1 EXTRA_DEFS="${CFLAGS_KERNEL}" oldconfig menuconfig

# _copy_kernel_objs target in mips/release.mk
build_kernel: make_kernel _copy_kernel_objs
	@echo "=== Build Kernel ==="

INSTALL_MOD_PATH = ${PROJECT_ROOT}/output/module.temp

fix_release_dirs :
	@-mkdir -p ${INSTALL_MOD_PATH}

build_modules: fix_release_dirs build_kernel
	@echo "=== Build Kernel Module ==="
	make -C ${KERNEL_SRC} V=1 EXTRA_DEFS="${CFLAGS_KERNEL}" INSTALL_MOD_PATH=$(INSTALL_MOD_PATH) modules modules_install
	rsync -av $(INSTALL_MOD_PATH)/lib/modules/ $(ROOTFS_DIR)/lib/modules/

RAMDISK_IMG = ${PROJECT_ROOT}/output/${ROOTFS_NAME}

build_rootfs: build_apps
	@echo "=== Build Rootfs ==="
	${PREFIX}/usr/bin/fakeroot -l ${FAKEROOT_LIB} -f ${FAKEROOT_BIN} -- /bin/sh -c " \
	chown -R root:root ${ROOTFS_DIR} 2>/dev/null; \
	${PREFIX}/bin/makedevs -d ${DEV_TABLE} ${ROOTFS_DIR}; \
	${PREFIX}/bin/mksquashfs ${ROOTFS_DIR} ${RAMDISK_IMG} -noappend -be"

#--- build loadable image -------------------------------------------------
# ${VMLINUX_RD} ${VMLINUX_BL7} targets defined in release.mk
KERNEL_RAMDISK = ./${ARCH}
include  ${KERNEL_RAMDISK}/release.mk

IMG_VERSION = ${PROJECT_VERSION}.${PROJECT_BUILD}
KERNEL_ENTRY_POINT = `${OBJDUMP} -f $(VMLINUX_FL) |grep start|cut -f3 -d' '`
KERNEL_MEMSTART = `${OBJDUMP} -p $(VMLINUX_FL)|grep LOAD|awk '{if ( NR == 1 ) print $$5;}'`

BINMD5_OPTS = -d -v"${IMG_VERSION}" -e${KERNEL_ENTRY_POINT} -a${KERNEL_MEMSTART}
BINMD5_OPTS += -p${PROJECT} -cap71
BINMD5_OPTS +=  -r${RAMDISK_IMG}

IMAGE_DEPEND = ${VMLINUX_RD} ${VMLINUX_BL7}

build_images : _vmlinux_rd _vmlinux_bl7
	@echo "=== Build Image ==="
	@echo "=== Network boot image is ready  ==="
	cp -f ${VMLINUX_RD} ${OUTPUT_DIR}/${NET_BOOT_IMG}
	@echo "=== Firmware image is ready  ==="
	${BINMD5} $(BINMD5_OPTS) -i${VMLINUX_BL7} -o${OUTPUT_DIR}/${AP_FLASH_IMG}
	@echo "=== Content of ap-sdk folder ==="
	ls -l "${OUTPUT_DIR}"

#----------------------------------------------------------------------

build_kernel_module: 
	@echo "=== Build Hello kernel module  ==="
	@make -C ${PROJECT_ROOT}/examples/module

build_apps: build_kernel_module
	@echo "=== Build Hello Apps ==="
	@make -C ${PROJECT_ROOT}/examples/app

clean:
	-rm -rf ${OUTPUT_DIR}/*
	@make -C ${PROJECT_ROOT}/examples/app clean
	@make -C ${PROJECT_ROOT}/examples/module clean

.PHONY : build_kernel make_kernel fix_release_dirs build_image
