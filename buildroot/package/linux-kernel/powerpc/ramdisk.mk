#
# Makefile for a ramdisk image
#
#

RAMDISK_OBJ ?=  ramdisk.o
RD_LD_SCRIPT ?= ld.script
OBJDUMP ?= $(TARGET_CROSS)objdump
CROSS_LD = $(TARGET_CROSS)ld

O_FORMAT = $(shell $(OBJDUMP) -i | head -2 | grep elf32)
img = $(RAMDISK_IMG)

$(RAMDISK_OBJ): $(img)
	echo "O_FORMAT:  " $(O_FORMAT)
	$(CROSS_LD) -T $(RD_LD_SCRIPT) -b binary --oformat $(O_FORMAT) -o $@__ $(img)
	$(CROSS_LD) -m elf32ppc -r -o $@ $@__
