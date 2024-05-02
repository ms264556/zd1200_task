#
# Copyright (c) 2006 Ruckus Wireless, Inc.
# All rights reserved.
#

#
# Makefile for the board_data pseudo driver
#
#



ifneq ($(findstring 2.6,$(KERNELVERSION)),)
KMODSUF		:= ko
else
export-objs	:= board_data.o
list-multi	:= v54_bsp.o
O_TARGET := v54_bsp.o
KMODSUF		:= o
endif

obj?= .


v54_bsp-objs := board_data.o v54_timer.o

v54_bsp-objs += nar5520_bsp.o nar5520_cf.o nar5520_reset.o nar5520_superio_gpio.o \
                nar5520_watchdog.o nar5520_led.o v54_himem.o nar5520_hwck.o \
                t5520ur_watchdog.o t5520ur_hwck.o





obj-y	:=  $(v54_bsp-objs)


INCS+= -Ittt -I${obj}/ -I./ -Ixxx
INCS+= -I$(KERNELPATH_ABS)/arch/mips/ar531x/
INCS+= -I$(KERNELPATH_ABS)/net/RKS_NL/

EXTRA_CFLAGS+= -DLINUX -DV54_BSP=1 ${INCS} ${COPTS} -Wno-unused






clean:
	$(MAKE) -C $(KERNELPATH_ABS) SUBDIRS=$(shell pwd) clean
	-rm -f *~ *.o *.ko *.mod.c
	-rm -f .depend .version .*.o.flags .*.o.d .*.o.cmd .*.ko.cmd
