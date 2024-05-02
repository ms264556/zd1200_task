#
# Makefile to relink kernel with a ramdisk image
#
#

##### build a ramdisk object ####################
KERNEL_RAMDISK ?= ./${ARCH}
RELEASE_DIR = ${PROJECT_ROOT}/output

RD_LD_SCRIPT = ${KERNEL_RAMDISK}/ld.script
RAMDISK_OBJ = ${RELEASE_DIR}/ramdisk.o
TARGET_CROSS = ${PREFIX}/bin/${TARGET}-

C_OBJDUMP = ${TARGET_CROSS}objdump
C_LD = ${TARGET_CROSS}ld

O_FORMAT = $(shell $(C_OBJDUMP) -i | head -2 | grep elf32)

# $(RAMDISK_IMG) defined in parent makefile

$(RAMDISK_OBJ): $(RAMDISK_IMG)
	@echo "C_OBJDUMP =      $(C_OBJDUMP)"
	@echo "C_LD =           $(C_LD)"
	@echo "RAMDISK_IMG =    $(RAMDISK_IMG)"
	@echo "O_FORMAT:  " $(O_FORMAT)
	$(LD) -T $(RD_LD_SCRIPT) -b binary --oformat $(O_FORMAT) -o $@__ $(RAMDISK_IMG)
	$(LD) -m elf32btsmip -r -o $@ $@__


###  relink kernel with ramdisk object #######################################
KERNEL_OBJ_PATH ?= ${KERNEL_OBJ}

VMLINUX_OBJ = ${KERNEL_OBJ_PATH}/../kernel/mips-linux-2.6.15/vmlinux

# files from kernel build needed to re-link with ramdisk object
KERNEL1_OBJ=${KERNEL_OBJ_PATH}/kernel1.o
vmlinux-lds := ${KERNEL_OBJ_PATH}/vmlinux.lds
ldflags_vmlinux := ${KERNEL_OBJ_PATH}/ldflags_vmlinux
LDFLAGS_RD = $(shell cat ${ldflags_vmlinux})

VMLINUX_RD = ${RELEASE_DIR}/vmlinux.root

VMLINUX_BIN = ${RELEASE_DIR}/vmlinux.bin
VMLINUX_BL7 = ${RELEASE_DIR}/vmlinux.bin.l7

.PHONY: _vmlinux_rd
_vmlinux_rd : fix_release_dirs build_rootfs ${KERNEL1_OBJ} ${RAMDISK_OBJ} ${vmlinux-lds}
	${LD} ${LDFLAGS_RD} -T ${vmlinux-lds} -o ${VMLINUX_RD} ${KERNEL1_OBJ} ${RAMDISK_OBJ}

VMLINUX_FL = ${VMLINUX_OBJ}

.PHONY: _vmlinux_bl7
_vmlinux_bl7 : fix_release_dirs  ${RAMDISK_IMG} ${VMLINUX_FL}
	${OBJCOPY} -O binary -g ${VMLINUX_FL} ${VMLINUX_BIN}
	${LZMA} e ${VMLINUX_BIN} ${VMLINUX_BL7}


_copy_kernel_objs :
	-mkdir -p ${KERNEL_OBJ}
	cp -f ${KERNEL_SRC}/kernel1.o ${KERNEL_OBJ_PATH}
	cp -f ${KERNEL_SRC}/ldflags_vmlinux ${KERNEL_OBJ_PATH}
