/*
 * Copyright (c) 2004-2008 Ruckus Wireless, Inc.
 * All rights reserved.
 *
 */
 //Tony Chen    2008-03-06   Initial

#ifndef __NAR5520_ICH7_GPIO_H__
#define __NAR5520_ICH7_GPIO_H__
#include <asm/io.h>
#include <linux/delay.h>

#define GP_INV_OFFSET        0x2C
#define GPE0_STS_OFFSET      0x28
#define GP_LVL_OFFSET        0x0C
#define GPIO_USE_SEL2_OFFSET 0x30
#define GP_IO_SEL2_OFFSET    0x34
#define GP_LVL2_OFFSET       0x38
#define GPI6_BYTE_CONVERT    0x02
#define GP_PORTB             0x61
#define GP_REFRESH           0x10

#define PCR_ADDR             0x0CF8
#define PCR_DATA             0x0CFC

#define GPI_ROUT_PCR_OFFSET      0x8000F8B8
#define PMBASE_PCR_OFFSET        0x8000F840
#define ACPI_CNTL_PCR_OFFSET     0x8000F844
#define GPIOBASE_PCR_OFFSET      0x8000F848
#define GPIOBASE_CNTL_PCR_OFFSET 0x8000F84C

static uint32_t GPIOBASE, PMBASE;

#define nar5520_ich7_gpio_init() \
do{ \
   uint32_t i_EAX; \
   outl(GPI_ROUT_PCR_OFFSET, PCR_ADDR);  \
   i_EAX = inl(PCR_DATA) & 0xFFFFCFFF;   \
   outl(GPI_ROUT_PCR_OFFSET, PCR_ADDR);  \
   outl(i_EAX, PCR_DATA);                \
   \
   outl(ACPI_CNTL_PCR_OFFSET, PCR_ADDR); \
   i_EAX = inl(PCR_DATA) | 0x00000080;   \
   outl(ACPI_CNTL_PCR_OFFSET, PCR_ADDR); \
   outl(i_EAX, PCR_DATA);                \
   \
   outl(PMBASE_PCR_OFFSET, PCR_ADDR);    \
   PMBASE = inl(PCR_DATA) & 0xFFFFFFF8;  \
   \
   \
   outl(GPIOBASE_CNTL_PCR_OFFSET, PCR_ADDR); \
   i_EAX = inl(PCR_DATA) | 0x00000010;       \
   outl(GPIOBASE_CNTL_PCR_OFFSET, PCR_ADDR); \
   outl(i_EAX, PCR_DATA);                    \
   \
   outl(GPIOBASE_PCR_OFFSET, PCR_ADDR);      \
   GPIOBASE = inl(PCR_DATA) & 0xFFFFFFF8;    \
}while(0);

#define nar5520_ich7_gpio_setpindest() \
do{ \
   unchar al_char; \
   al_char = inb(GPIOBASE+GPIO_USE_SEL2_OFFSET);      \
   outb(al_char|0x80, GPIOBASE+GPIO_USE_SEL2_OFFSET); \
   \
   al_char = inb(GPIOBASE+GP_IO_SEL2_OFFSET);         \
   outb(al_char&0x7F, GPIOBASE+GP_IO_SEL2_OFFSET);    \
   \
   al_char = inb(GPIOBASE+GP_LVL2_OFFSET);            \
   outb(al_char|0x80, GPIOBASE+GP_LVL2_OFFSET);       \
   \
   al_char = inb(GPIOBASE+GP_INV_OFFSET);             \
   outb(al_char&0xBF, GPIOBASE+GP_INV_OFFSET);        \
}while(0);

#define nar5520_ich7_gpio_read_GPI6(al_char) \
do{ \
   al_char = inb(PMBASE+GPE0_STS_OFFSET+GPI6_BYTE_CONVERT);             \
   inb(0x80);                                                           \
   inb(0x80);                                                           \
   outb((al_char&0x40)|0x40, PMBASE+GPE0_STS_OFFSET+GPI6_BYTE_CONVERT); \
   inb(0x80);                                                           \
   inb(0x80);                                                           \
   al_char = inb(PMBASE+GPE0_STS_OFFSET+GPI6_BYTE_CONVERT) & 0x40;      \
}while(0);

#define nar5520_ich7_gpio_delay() \
do{ \
   unchar char_ah, char_al;                 \
   char_ah = inb(GP_PORTB) & GP_REFRESH;    \
   do{                                      \
      char_al = inb(GP_PORTB) & GP_REFRESH; \
   }while(char_ah == char_al);              \
}while(0);

#define nar5520_ich7_gpio_reset_GPI6() \
do{ \
   unchar al_char;                              \
   al_char = inb(GPIOBASE+GP_LVL2_OFFSET)|0x80; \
   outb(al_char, GPIOBASE+GP_LVL2_OFFSET);      \
   nar5520_ich7_gpio_delay();                   \
   nar5520_ich7_gpio_delay();                   \
   al_char &= 0x7F;                             \
   outb(al_char, GPIOBASE+GP_LVL2_OFFSET);      \
   nar5520_ich7_gpio_delay();                   \
   nar5520_ich7_gpio_delay();                   \
   al_char |= 0x80;                             \
   outb(al_char, GPIOBASE+GP_LVL2_OFFSET);      \
   nar5520_ich7_gpio_delay();                   \
   nar5520_ich7_gpio_delay();                   \
}while(0);

#endif
