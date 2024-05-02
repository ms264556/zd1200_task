/*
 * Copyright (c) 2004-2008 Ruckus Wireless, Inc.
 * All rights reserved.
 *
 */
 //Tony Chen    2008-03-04   Initial

#ifndef __NAR5520_WH627_GPIO_H__
#define __NAR5520_WH627_GPIO_H__
#include <linux/reboot.h>
#include <linux/spinlock.h>


#define LDN8    0x08
#define LDN9	0x09
#define LDNA	0x0A


#define CR07    0x07
#define CR30    0x30


#define CR2D    0x2D
#define CR20	0x20
#define CR21	0x21
#define CR2A	0x2A
#define CR2C	0x2C
#define CR24	0x24


#define CRF0	0xF0
#define CRF1	0xF1
#define CRF2	0xF2
#define CRF5    0xf5
#define CRF6    0xf6
#define CRF7    0xf7
#define CRE3	0xE3
#define CRE4	0xE4
#define CRE5	0xE5
#define CRE0    0xE0
#define CRE1    0xE1
#define CRE2    0xE2
#define CRFE    0xFE
#define CRFE    0xFE
#define CRE7    0xE7


#define W627_CONFIG     0x2E
#define W627_DATA       0x2F


#define W627_MODE       0x87
#define W627_EDHG       0x04
#define W627_END	0xAA
#define W627_DISABLE	0x00


#define W627_ID_MSB	0xA0


extern spinlock_t io_lock;

#define W627_READ_PORT(PORT, val) \
do{                               \
   outb(PORT, W627_CONFIG);  \
   val = inb(W627_DATA);     \
}while(0);

#define W627_WRITE_PORT(PORT, val)\
do{                               \
   outb(PORT, W627_CONFIG);  \
   outb(val,  W627_DATA);    \
}while(0);

#define W627_OPEN_PORT()             \
do{                                  \
   unchar val=0x00;                  \
   spin_lock_bh(&io_lock);      \
   outb(W627_MODE, W627_CONFIG);     \
   outb(W627_MODE, W627_CONFIG);     \
   W627_READ_PORT(CR20, val);        \
   if(val != W627_ID_MSB){           \
      kernel_halt();                 \
   }                                 \
}while(0);

#define W627_CLOSE_PORT()            \
do{                                  \
   outb(W627_END, W627_CONFIG);      \
   spin_unlock_bh(&io_lock); \
}while(0);

#endif
