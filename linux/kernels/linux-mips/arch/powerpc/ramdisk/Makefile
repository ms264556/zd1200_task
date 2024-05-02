#
# Makefile for a ramdisk image
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

obj-y	:= ramdisk.o

extra-y := rd_size.o

O_FORMAT = $(shell $(OBJDUMP) -i | head -2 | grep elf32)
img = $(objtree)/$(obj)/$(CONFIG_EMBEDDED_RAMDISK_IMAGE)

$(obj)/ramdisk.o: $(subst ",,$(img)) $(srctree)/$(src)/ld.script
	echo "O_FORMAT:  " $(O_FORMAT)
	$(LD) -T $(srctree)/$(src)/ld.script -b binary --oformat $(O_FORMAT) -o $@ $(img)

# to satisfy dependency checking on initial build
MY_RD_SIZE = $(shell ls -s $(img) | cut -d' ' -f1 )

$(subst ",,$(img)):
	( if [ ! -f ${img} ]; then touch $(img) ; fi )

# also dynamically set __rd_size to store the size of the ramdisk image
# __rd_size will be used in drivers/block/rd.c to ensure that the
# maximum RAMDISK size can accomodate this.
$(obj)/rd_size.o: $(subst ",,$(img))
	echo "int __rd_size = $(MY_RD_SIZE) ;" > $(objtree)/$*.c
	$(CC) $(CFLAGS) -c -o $@ $(objtree)/$*.c

# include $(TOPDIR)/Rules.make
