
/*
 * MTD driver for the SPI Flash Memory support.
 *
 * $Id: //depot/sw/releases/linuxsrc/src/kernels/mips-linux-2.4.25/drivers/mtd/devices/spiflash.c#3 $
 *
 *
 * Copyright (c) 2005-2006 Atheros Communications Inc.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

/*===========================================================================
** !!!!  VERY IMPORTANT NOTICE !!!!  FLASH DATA STORED IN LITTLE ENDIAN FORMAT
**
** This module contains the Serial Flash access routines for the Atheros SOC.
** The Atheros SOC integrates a SPI flash controller that is used to access
** serial flash parts. The SPI flash controller executes in "Little Endian"
** mode. THEREFORE, all WRITES and READS from the MIPS CPU must be
** BYTESWAPPED! The SPI Flash controller hardware by default performs READ
** ONLY byteswapping when accessed via the SPI Flash Alias memory region
** (Physical Address 0x0800_0000 - 0x0fff_ffff). The data stored in the
** flash sectors is stored in "Little Endian" format.
**
** The spiflash_write() routine performs byteswapping on all write
** operations.
**===========================================================================*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <asm/delay.h>
#include <asm/io.h>
#include "spiflash.h"

#include "ar531xlnx.h"

// Husky
#include <asm/bootinfo.h>

/* debugging */
#if 0
#define SPIFLASH_DEBUG 1
#else
#undef SPIFLASH_DEBUG
#endif

#ifndef __BIG_ENDIAN
#error This driver currently only works with big endian CPU.
#endif

static char module_name[] = "spiflash";

#define MIN(a,b)        ((a) < (b) ? (a) : (b))
#define FALSE 	0
#define TRUE 	1

#define ROOTFS_NAME	"rootfs"

static __u32 spiflash_regread32(int reg);
static void spiflash_regwrite32(int reg, __u32 data);
static __u32 spiflash_sendcmd (int op);

int __init spiflash_init (void);
void __exit spiflash_exit (void);
static int spiflash_probe (void);
static int spiflash_erase (struct mtd_info *mtd,struct erase_info *instr);
static int spiflash_read (struct mtd_info *mtd, loff_t from,size_t len,size_t *retlen,u_char *buf);
static int spiflash_write (struct mtd_info *mtd,loff_t to,size_t len,size_t *retlen,const u_char *buf);

/* Flash configuration table */
struct flashconfig {
    __u32 byte_cnt;
    __u32 sector_cnt;
    __u32 sector_size;
    __u32 cs_addrmask;
} flashconfig_tbl[MAX_FLASH] =
    {
        { 0, 0, 0, 0},
        { STM_1MB_BYTE_COUNT, STM_1MB_SECTOR_COUNT, STM_1MB_SECTOR_SIZE, 0x0},
        { STM_2MB_BYTE_COUNT, STM_2MB_SECTOR_COUNT, STM_2MB_SECTOR_SIZE, 0x0},
        { STM_4MB_BYTE_COUNT, STM_4MB_SECTOR_COUNT, STM_4MB_SECTOR_SIZE, 0x0},
        { STM_8MB_BYTE_COUNT, STM_8MB_SECTOR_COUNT, STM_8MB_SECTOR_SIZE, 0x0},
        { STM_16MB_BYTE_COUNT, STM_16MB_SECTOR_COUNT, STM_16MB_SECTOR_SIZE, 0x0}
    };



#ifdef SLOW_SPI_FLASH
#define USE_SLOW_CLOCK 1
#else
#define USE_SLOW_CLOCK 0
#endif

#if USE_SLOW_CLOCK
#define USE_IO_REMAP 1
#else
#undef USE_IO_REMAP
#undef USE_MASK
#endif
#define FAST_RX_CNT 8       /* 8 or 4 */
/* Mapping of generic opcodes to STM serial flash opcodes */
struct opcodes {
    __u16 code;
    __s8 tx_cnt;
    __s8 rx_cnt;
} stm_opcodes[] = {
        {STM_OP_WR_ENABLE, 1, 0},
        {STM_OP_WR_DISABLE, 1, 0},
        {STM_OP_RD_STATUS, 1, 1},
        {STM_OP_WR_STATUS, 1, 0},
        {STM_OP_RD_DATA, 4, 4},
#if 0
        {STM_OP_FAST_RD_DATA, 1, 0},
#else
        {STM_OP_FAST_RD_DATA, 5, FAST_RX_CNT},
#endif
        {STM_OP_PAGE_PGRM, 8, 0},
        {STM_OP_SECTOR_ERASE, 4, 0},
        {STM_OP_BULK_ERASE, 1, 0},
        {STM_OP_DEEP_PWRDOWN, 1, 0},
        {STM_OP_RD_SIG, 4, 1}
};

/* Driver private data structure */
struct spiflash_data {
	struct 	mtd_info       *mtd;
	struct 	mtd_partition  *parsed_parts;     /* parsed partitions */
	void 	*spiflash_readaddr; /* memory mapped data for read  */
	void 	*spiflash_mmraddr;  /* memory mapped register space */
};

static struct spiflash_data *spidata = NULL;

extern int parse_redboot_partitions(struct mtd_info *master, struct mtd_partition **pparts, unsigned long fis_origin);
__u32 flash_ahb_base_address = 0x1fc00000 ;

/************************************************************************************/

static __u32
spiflash_regread32(int reg)
{
	volatile __u32 *data = (__u32 *)(spidata->spiflash_mmraddr + reg);

	return (*data);
}

static void
spiflash_regwrite32(int reg, __u32 data)
{
	volatile __u32 *addr = (__u32 *)(spidata->spiflash_mmraddr + reg);

	*addr = data;
	return;
}

static __u32
spiflash_sendcmd (int op)
{
	 __u32 reg;
	 __u32 mask;
	struct opcodes *ptr_opcode;

	ptr_opcode = &stm_opcodes[op];

	do {
		reg = spiflash_regread32(SPI_FLASH_CTL);
	} while (reg & SPI_CTL_BUSY);

	spiflash_regwrite32(SPI_FLASH_OPCODE, ptr_opcode->code);

	reg = (reg & ~SPI_CTL_TX_RX_CNT_MASK) | ptr_opcode->tx_cnt |
		(ptr_opcode->rx_cnt << 4) | SPI_CTL_START;

	spiflash_regwrite32(SPI_FLASH_CTL, reg);

	if (ptr_opcode->rx_cnt > 0) {
		do {
			reg = spiflash_regread32(SPI_FLASH_CTL);
		} while (reg & SPI_CTL_BUSY);

		reg = (__u32) spiflash_regread32(SPI_FLASH_DATA);

		switch (ptr_opcode->rx_cnt) {
		case 1:
			mask = 0x000000ff;
			break;
		case 2:
			mask = 0x0000ffff;
			break;
		case 3:
			mask = 0x00ffffff;
			break;
		default:
			mask = 0xffffffff;
			break;
		}

		reg &= mask;
	}
	else {
		reg = 0;
	}

	return reg;
}

/* Probe SPI flash device
 * Function returns 0 for failure.
 * and flashconfig_tbl array index for success.
 */
static int
spiflash_probe (void)
{
	__u32 sig;
	int flash_size;

	char c = ' ', *from = saved_command_line;
	int len = 0;
//    extern char saved_command_line[];

    flash_size = 0;
	for (;;) {
		if (c == ' ' && !memcmp(from, "flash=", 6)) {
			flash_size = memparse(from + 6, &from);
            break;
		}
		c = *(from++);
		if (!c)
			break;
		if (CL_SIZE <= ++len)
			break;
	}
    printk("spiflash_probe: cmd line flash_size=0x%x\n", flash_size);
    /* husky: no RES instruction for 16MB flash */
    if (flash_size == STM_16MB_BYTE_COUNT)
        sig = STM_128MBIT_SIGNATURE;
    else
	/* Read the signature on the flash device */
	sig = spiflash_sendcmd(SPI_RD_SIG);

	switch (sig) {
	case STM_8MBIT_SIGNATURE:
		flash_size = FLASH_1MB;
		break;
        case STM_16MBIT_SIGNATURE:
		flash_size = FLASH_2MB;
		break;
        case STM_32MBIT_SIGNATURE:
		flash_size = FLASH_4MB;
		break;
        case STM_64MBIT_SIGNATURE:
		flash_size = FLASH_8MB;
                flash_ahb_base_address = 0x08000000 ;
		break;
        case STM_128MBIT_SIGNATURE:
		flash_size = FLASH_16MB;
                flash_ahb_base_address = 0x08000000 ;
		break;
        default:
		printk (KERN_WARNING "%s: Read of flash device signature failed!\n", module_name);
		return (0);
	}

	return (flash_size);
}


static int
spiflash_erase (struct mtd_info *mtd,struct erase_info *instr)
{
	struct opcodes *ptr_opcode;
	__u32 temp, reg;
	int finished = FALSE;

#ifdef SPIFLASH_DEBUG
	printk (KERN_DEBUG "%s(addr = 0x%.8x, len = %d)\n",__FUNCTION__,instr->addr,instr->len);
#endif

	/* sanity checks */
	if (instr->addr + instr->len > mtd->size) return (-EINVAL);

	ptr_opcode = &stm_opcodes[SPI_SECTOR_ERASE];

	temp = ((__u32)instr->addr << 8) | (__u32)(ptr_opcode->code);
	spiflash_sendcmd(SPI_WRITE_ENABLE);
	do {
		reg = spiflash_regread32(SPI_FLASH_CTL);
	} while (reg & SPI_CTL_BUSY);

	spiflash_regwrite32(SPI_FLASH_OPCODE, temp);

	reg = (reg & ~SPI_CTL_TX_RX_CNT_MASK) | ptr_opcode->tx_cnt | SPI_CTL_START;
	spiflash_regwrite32(SPI_FLASH_CTL, reg);

	do {
		reg = spiflash_sendcmd(SPI_RD_STATUS);
		if (!(reg & SPI_STATUS_WIP)) {
			finished = TRUE;
		}
	} while (!finished);

	instr->state = MTD_ERASE_DONE;
	if (instr->callback) instr->callback (instr);

#ifdef SPIFLASH_DEBUG
	printk (KERN_DEBUG "%s return\n",__FUNCTION__);
#endif
	return (0);
}

#if ! defined(USE_IO_REMAP)
typedef union {
    __u8 c[4];
    __u32 l;
}       w4_t;
// no checking is done here
// assumed len < 4
static inline void
_CP_BYTES(__u8 *bp, w4_t w4, int len)
{

#ifndef __BIG_ENDIAN
#define    W4C3     w4.c[3]
#define    W4C2     w4.c[2]
#define    W4C1     w4.c[1]
#define    W4C0     w4.c[0]
#else
#define    W4C3     w4.c[0]
#define    W4C2     w4.c[1]
#define    W4C1     w4.c[2]
#define    W4C0     w4.c[3]
#endif
    switch (len) {
#if 1
    case 4:
        *(bp+3) = W4C3;
#endif
    case 3:
        *(bp+2) = W4C2;
    case 2:
        *(bp+1) = W4C1;
    case 1:
        *(bp) = W4C0;
        break;
    default:
        break;
    }
    return;
}
#endif

static int
_spiflash_read (loff_t from,size_t len,size_t *retlen,u_char *buf)
{
#if defined(USE_IO_REMAP)
	u_char	*read_addr;
#endif

#if defined(SPIFLASH_DEBUG)
	printk (KERN_DEBUG "%s(from = 0x%.8x, len = %d)\n",__FUNCTION__,(__u32) from,(int)len);
#endif

	/* sanity checks */
	if (!len) {
        *retlen = 0;
        return (0);
    }


#if defined(USE_IO_REMAP)
	/* we always read len bytes */
	*retlen = len;

	read_addr = (u_char *)(spidata->spiflash_readaddr + from);
	memcpy(buf, read_addr, len);
#else
  {
    int done = FALSE, bytes_left;
    __u32 opcode, reg, flashAddr;
    __u32 ao_reg, cs_reg;
    __u32 xact_len;
#if defined(USE_MASK)
    __u32 mask;
#endif
    __u8 *bp = (__u8*) buf;
    struct opcodes *pOpcode;
#if 0
    static int once = 1;
    union {
        __u8 c[4];
        __u32 l;
    } w4;
#else
    w4_t    w4;
#endif

#define FAST_RD 1
#define _REG_SIZE   sizeof(__u32)
#if FAST_RD
#define RD_OPCODE   SPI_FAST_RD_DATA
#define RX_BYTES   FAST_RX_CNT
#else
#define RD_OPCODE   SPI_RD_DATA
#define RX_BYTES   pOpcode->rx_cnt
#endif
    *retlen = 0;
    pOpcode = &stm_opcodes[RD_OPCODE];
    bytes_left = len;

    ao_reg = (pOpcode->code & SPI_OPCODE_MASK);

    cs_reg = spiflash_regread32(SPI_FLASH_CTL);
    cs_reg = (cs_reg & ~SPI_CTL_TX_RX_CNT_MASK) | pOpcode->tx_cnt |
        (RX_BYTES << 4) | SPI_CTL_START;

#if defined(USE_MASK)
    switch (pOpcode->rx_cnt)
    {
             case 1:
                mask = 0x000000ff;
                break;
            case 2:
                mask = 0x0000ffff;
                break;
            case 3:
                mask = 0x00ffffff;
                break;
            default:
                mask = 0xffffffff;
                break;
    }
#endif

    flashAddr = from;
    while (done == FALSE) {

		do {
			reg = spiflash_regread32(SPI_FLASH_CTL);
		} while (reg & SPI_CTL_BUSY);

		opcode = ao_reg | ((__u32)flashAddr << 8);

		spiflash_regwrite32(SPI_FLASH_OPCODE, opcode);

		spiflash_regwrite32(SPI_FLASH_CTL, cs_reg);

        do {
			reg = spiflash_regread32(SPI_FLASH_CTL);
		} while (reg & SPI_CTL_BUSY);

        w4.l = (__u32) spiflash_regread32(SPI_FLASH_DATA);

#if defined(USE_MASK)
        w4.l &= mask;
#endif

        if ( bytes_left < _REG_SIZE ) {
            xact_len = bytes_left;
            _CP_BYTES(bp, w4, xact_len);
#define FIX_WORD_ALIGN 1
#if FIX_WORD_ALIGN
#define _NOT_WORD_ALIGN(x)  ( (((__u32)x) & ~0x3) != (__u32)x )
        } else if ( _NOT_WORD_ALIGN(bp) ) {
            // handle word alignment problem
            __u32 w = (__u32)bp;
            xact_len = (int)(w & 0x3);
            _CP_BYTES(bp, w4, xact_len);
#endif
        } else {
#if FIX_WORD_ALIGN
            *((__u32*)bp) =  __le32_to_cpu(w4.l);
#else
            _CP_BYTES(bp, w4, _REG_SIZE);
#endif
            if ( bytes_left < RX_BYTES ) {
                // we need > 3 but < 8 bytes
                if ( bytes_left == _REG_SIZE ) {
                    // exactly 4 bytes, already got it
                    xact_len = _REG_SIZE;
                } else {
                    // > 4 but less than 8 bytes
                    // get the rest from the AO register
                    w4.l = (__u32) spiflash_regread32(SPI_FLASH_OPCODE);
#if defined(USE_MASK)
                    w4.l &= mask;
#endif
                    _CP_BYTES(bp+4, w4, (bytes_left - _REG_SIZE));
                    xact_len = bytes_left;
                }
            } else {
                // we have RX_BYTES bytes to read
                w4.l = (__u32) spiflash_regread32(SPI_FLASH_OPCODE);
#if defined(USE_MASK)
                w4.l &= mask;
#endif
#if FIX_WORD_ALIGN
                *((__u32*)(bp+4)) =  __le32_to_cpu(w4.l);
#else
                _CP_BYTES(bp+4, w4, _REG_SIZE);
#endif
                xact_len = RX_BYTES;
            }
        }

		bytes_left -= xact_len;
		flashAddr += xact_len;
		bp += xact_len;

		*retlen += xact_len;

		if (bytes_left == 0) {
			done = TRUE;
		}
    }
  }
#endif

	return (0);
}

static int
spiflash_read (struct mtd_info *mtd, loff_t from,size_t len,
        size_t *retlen,u_char *buf)
{
	if (from + len > mtd->size) return (-EINVAL);
    return _spiflash_read (from, len, retlen, buf);
}

#if defined(V54_BSP)
// only used for reading boarddata info
int
v54_flash_read (loff_t from, u_char *buf, size_t len)
{
    size_t retlen;

    if ( _spiflash_read ( from, len, &retlen, buf) < 0 ) {
        return 0;
    } else {
        return retlen;
    }
}
#endif

#undef WRITE_DEBUG

static int
spiflash_write (struct mtd_info *mtd,loff_t to,size_t len,size_t *retlen,const u_char *buf)
{
	int done = FALSE, page_offset, bytes_left, finished;
	__u32 xact_len, spi_data = 0, opcode, reg;

#if defined(SPIFLASH_DEBUG) || defined(WRITE_DEBUG)
	printk (KERN_DEBUG "%s(to = 0x%.8x, len = %d)\n",__FUNCTION__,(__u32) to,len);
#endif

	*retlen = 0;

	/* sanity checks */
	if (!len) return (0);
	if (to + len > mtd->size) return (-EINVAL);

	opcode = stm_opcodes[SPI_PAGE_PROGRAM].code;
	bytes_left = len;

	while (done == FALSE) {
		xact_len = MIN(bytes_left, sizeof(__u32));

		/* 32-bit writes cannot span across a page boundary
		 * (256 bytes). This types of writes require two page
		 * program operations to handle it correctly. The STM part
		 * will write the overflow data to the beginning of the
		 * current page as opposed to the subsequent page.
		 */
		page_offset = (to & (STM_PAGE_SIZE - 1)) + xact_len;

		if (page_offset > STM_PAGE_SIZE) {
			xact_len -= (page_offset - STM_PAGE_SIZE);
		}

		spiflash_sendcmd(SPI_WRITE_ENABLE);

		do {
			reg = spiflash_regread32(SPI_FLASH_CTL);
		} while (reg & SPI_CTL_BUSY);

		switch (xact_len) {
			case 4:
				spi_data = (buf[3] << 24) | (buf[2] << 16) |
							(buf[1] << 8) | buf[0];
				break;
			case 1:
				spi_data = *buf;
				break;
			case 2:
				spi_data = (buf[1] << 8) | buf[0];
				break;
			case 3:
				spi_data = (buf[2] << 16) | (buf[1] << 8) | buf[0];
				break;
			default:
				printk("spiflash_write: default case\n");
				break;
		}

		spiflash_regwrite32(SPI_FLASH_DATA, spi_data);
		opcode = (opcode & SPI_OPCODE_MASK) | ((__u32)to << 8);
		spiflash_regwrite32(SPI_FLASH_OPCODE, opcode);

		reg = (reg & ~SPI_CTL_TX_RX_CNT_MASK) | (xact_len + 4) | SPI_CTL_START;
		spiflash_regwrite32(SPI_FLASH_CTL, reg);
		finished = FALSE;

		do {
			udelay(1);
			reg = spiflash_sendcmd(SPI_RD_STATUS);
			if (!(reg & SPI_STATUS_WIP)) {
				finished = TRUE;
			}
		} while (!finished);

		bytes_left -= xact_len;
		to += xact_len;
		buf += xact_len;

		*retlen += xact_len;

		if (bytes_left == 0) {
			done = TRUE;
		}
	}

	return (0);
}

/* FAST_WRITE_256 defined in arch/mips/ar531x/ar531x.h */
#if FAST_WRITE_256

#if 0
extern void ar531x_gpio_ctrl_output(int gpio);
extern void ar531x_gpio_set(int gpio, int val);
#endif

static inline void
PAGE_WRITE_SELECT_SET(void)
{
#if 0
    __u32 reg;
    do {
        reg = spiflash_regread32(SPI_FLASH_CTL);
    } while (reg & SPI_CTL_BUSY);
#endif
    ar531x_gpio_ctrl_output(AR5315_FAST_WRITE_GPIO);  /* set GPIO_10 as output */
    ar531x_gpio_set(AR5315_FAST_WRITE_GPIO, 0);      /* drive low GPIO_10 */
    return;
}

static inline void
PAGE_WRITE_SELECT_UNSET(void)
{
    ar531x_gpio_set(AR5315_FAST_WRITE_GPIO, 1);
    return;
}

#ifdef PAGE_SIZE
#undef PAGE_SIZE
#endif
#define PAGE_SIZE   256
#if 0
typedef __u32  UINT32;
typedef __u8  UINT8;
#endif
#define cpu2le32    __cpu_to_le32

#define _STM_OP_PAGE_PGRM    (stm_opcodes[SPI_PAGE_PROGRAM].code)


static int
spiflash_page_write (struct mtd_info *mtd,loff_t to,size_t len,size_t *retlen,
        const u_char *buf)
{
    int bytes_left;
    UINT8 *cb = (UINT8 *)buf;

    UINT32 reg, opcode;
    int xact_len;
    UINT32 flashAddr;
    UINT8 *cur;
    UINT32  spi_data = 0, spi_data_swapped;
    UINT32  pagesToWrite, lastByteInPage, i, j;
    UINT32  beginPage, beginOffset, beginRemain, endPage, endOffset;

#if defined(SPIFLASH_DEBUG)
	printk (KERN_DEBUG "%s(to = 0x%.8x, len = %d)\n",__FUNCTION__,(__u32) to,len);
#endif
	*retlen = 0;
    bytes_left = len;

	/* sanity checks */
	if (!len) return (0);
	if (to + len > mtd->size) return (-EINVAL);

#if 1
	opcode = stm_opcodes[SPI_PAGE_PROGRAM].code;
#endif

    flashAddr = (UINT32) to;

    lastByteInPage= PAGE_SIZE -1;

    beginPage= ((unsigned int)flashAddr) / PAGE_SIZE;
    beginOffset= ((unsigned int)flashAddr) % PAGE_SIZE;
    beginRemain= PAGE_SIZE - beginOffset;
    endPage= ((unsigned int)flashAddr + len - 1) / PAGE_SIZE;
    endOffset= ((unsigned int)flashAddr + len - 1) % PAGE_SIZE;
    pagesToWrite= endPage - beginPage + 1;
    if( beginOffset || (len < PAGE_SIZE) )
    {
        /* write from the middle of a page */
        xact_len = MIN(len, beginRemain);
        spiflash_write(mtd, (loff_t)flashAddr, xact_len, retlen, cb);
        flashAddr += xact_len;
        cb += xact_len;
        pagesToWrite --;
        bytes_left -= xact_len;
    }
    cur = cb;
    xact_len = 4;

    /*********** 256-byte Page program *************/
    if( pagesToWrite )
    {
        for(i=0; i<pagesToWrite; i++)   /* write page(256 bytes) by page */
        {
            // check for partial last page
            if ( bytes_left < PAGE_SIZE ) {
                break;
            }
            spiflash_sendcmd(SPI_WRITE_ENABLE);

            do {
                reg = spiflash_regread32(SPI_FLASH_CTL);
            } while (reg & SPI_CTL_BUSY);
            PAGE_WRITE_SELECT_SET();

            spi_data = (UINT32) *((UINT32 *) cur);
            spi_data = cpu2le32(spi_data);
            spiflash_regwrite32(SPI_FLASH_DATA, spi_data);
            opcode = (opcode & SPI_OPCODE_MASK) | ((UINT32) flashAddr << 8);
            spiflash_regwrite32(SPI_FLASH_OPCODE, opcode);
            reg = (reg & ~SPI_CTL_TX_RX_CNT_MASK) | 0x8 | SPI_CTL_START;
            spiflash_regwrite32(SPI_FLASH_CTL, reg);
            cur += 4;
            for(j=0; j<31; j++)   /* 32 loops, each loop writes 4 bytes */
            {
                do {
                    reg = spiflash_regread32(SPI_FLASH_CTL);
                } while (reg & SPI_CTL_BUSY);
                spi_data = (UINT32) *((UINT32 *) cur);
                spi_data = cpu2le32(spi_data);
                spi_data_swapped = (((spi_data >> 8) & 0xff) << 24) |
                    (((spi_data >> 24) & 0xff) << 8) | (spi_data & 0x00ff00ff);
                spiflash_regwrite32(SPI_FLASH_OPCODE, spi_data_swapped);
                cur += 4;
                spi_data = (UINT32) *((UINT32 *) cur);
                spi_data = cpu2le32(spi_data);
                spiflash_regwrite32(SPI_FLASH_DATA, spi_data);
                reg = (reg & ~SPI_CTL_TX_RX_CNT_MASK) | 0x8 | SPI_CTL_START;
                spiflash_regwrite32(SPI_FLASH_CTL, reg);
                cur += 4;
            }
            do {
                reg = spiflash_regread32(SPI_FLASH_CTL);
            } while (reg & SPI_CTL_BUSY);
            spi_data = (UINT32) *((UINT32 *) cur);
            spi_data = cpu2le32(spi_data);
            spi_data_swapped = (((spi_data >> 8) & 0xff) << 24) |
                 (((spi_data >> 24) & 0xff) << 8) | (spi_data & 0x00ff00ff);
            spiflash_regwrite32(SPI_FLASH_OPCODE, spi_data_swapped);
            reg = (reg & ~SPI_CTL_TX_RX_CNT_MASK) | 0x4 | SPI_CTL_START;
            spiflash_regwrite32(SPI_FLASH_CTL, reg);
            cur += 4;
            do {
                reg = spiflash_regread32(SPI_FLASH_CTL);
            } while (reg & SPI_CTL_BUSY);

            PAGE_WRITE_SELECT_UNSET();

            do {
                udelay(1);
                reg = spiflash_sendcmd(SPI_RD_STATUS);
                if (!(reg & SPI_STATUS_WIP)) {
                    break;
                }
            } while (TRUE);

            flashAddr += PAGE_SIZE;
            bytes_left -= PAGE_SIZE;
            *retlen += PAGE_SIZE;
        }
    }
    /************* 256-byte Page program ***************/

    if( (beginPage != endPage) && (endOffset != lastByteInPage) ) {
        /* write to middle of a page */
        size_t retlen1;
        spiflash_write(mtd, (loff_t)flashAddr, (endOffset + 1), &retlen1, cur);
        bytes_left -= retlen1;
        *retlen += retlen1;
#if defined(WRITE_DEBUG) || defined(SPIFLASH_DEBUG)
	printk (KERN_DEBUG "%s(to = 0x%.8x, len = %d)\n",__FUNCTION__,(__u32) to,len);
	printk (KERN_DEBUG "%s pagesToWrite = %u page[%x - %x] offset[%u - %u]\n",
                __FUNCTION__,
                pagesToWrite, beginPage, endPage, beginOffset, endOffset );
#endif
    }

    if ( bytes_left != 0 || *retlen != len ) {
        printk("%s: bytes_left = %d *retlen(%d) != len(%d)\n", __FUNCTION__,
                    bytes_left, *retlen, len );
    }
    return 0;
}

static int flashPageWrite_capable = 0;
static void
_set_page_write(struct mtd_info *mtd)
{

    if ( mtd == NULL ) {
        return;
    }
    if ( flashPageWrite_capable ) {
        mtd->write = spiflash_page_write;
    } else {
        mtd->write = spiflash_write;
    }
    return;
}

void
spi_flashPageWrite_set(int enable)
{
    if ( enable ) {
        flashPageWrite_capable = 1;
    } else {
        flashPageWrite_capable = 0;
    }
    if ( spidata && spidata->mtd )  {
        _set_page_write(spidata->mtd);
    } else {
        printk("%s: !(spidata && spidata->mtd)\n", __FUNCTION__);
    }
}

__u32
flash_write_addr(void)
{
    if ( ! spidata || !spidata->mtd ) {
        return 0;
    }
    if ( spidata->mtd->write == spiflash_page_write ) {
        printk("spidata->mtd->write = spiflash_page_write\n");
    } else if ( spidata->mtd->write == spiflash_write ) {
        printk("spidata->mtd->write = spiflash_write\n");
    } else {
        printk("spidata->mtd->write (0x%08x) unknown function\n",
                (__u32)spidata->mtd->write);
    }
    return (__u32) spidata->mtd->write;
}

int
spi_flashPageWrite_get(void)
{
    return flashPageWrite_capable;
}
#endif

static void
setSpiSpeed(void)
{
    __u32 reg;

    reg = spiflash_regread32(SPI_FLASH_CTL);

#if 1
    reg = (reg & ~(3<<19)) | 0x00100000;    // force 24-bit addressing
#endif
#if USE_SLOW_CLOCK
    /* set to 12MHz.. 25MHz causes too many read errors */
    // clock/16
    spiflash_regwrite32(SPI_FLASH_CTL, (reg|(3<<24)) & ~(2<<24));
#else
    // clock/8
    spiflash_regwrite32(SPI_FLASH_CTL, (reg&~(3<<24)));
#endif
    reg = spiflash_regread32(SPI_FLASH_CTL);
    printk("%s: Reg: 0x%08x\n", __FUNCTION__, reg);
}


int __init
spiflash_init (void)
{
	int result;
	int index, num_parts;
	struct mtd_info *mtd;
#ifndef CONFIG_BLK_DEV_INITRD
    int i;
	struct  mtd_partition  *mtd_parts;
#endif
//        __u32 reg;

	spidata = kmalloc(sizeof(struct spiflash_data), GFP_KERNEL);
	if (!spidata)
		return (-ENXIO);

	spidata->spiflash_mmraddr = ioremap_nocache(SPI_FLASH_MMR, SPI_FLASH_MMR_SIZE);
	if (!spidata->spiflash_mmraddr) {
		printk (KERN_WARNING "%s: Failed to map flash device\n", module_name);
		kfree(spidata);
		return (-ENXIO);
	}

	mtd = kmalloc(sizeof(struct mtd_info), GFP_KERNEL);
	if (!mtd) {
		kfree(spidata);
		return (-ENXIO);
	}

	memset (mtd,0,sizeof (*mtd));

	printk ("MTD driver for SPI flash.\n");
	printk ("%s: Probing for Serial flash ...\n", module_name);
	if (!(index = spiflash_probe ())) {
		printk (KERN_WARNING "%s: Found no serial flash device\n", module_name);
		kfree(mtd);
		kfree(spidata);
		return (-ENXIO);
	}
	printk ("%s: Found SPI serial Flash.\n", module_name);
	printk ("%d: size\n", flashconfig_tbl[index].byte_cnt);

	spidata->spiflash_readaddr = ioremap_nocache(SPI_FLASH_READ, flashconfig_tbl[index].byte_cnt);
	if (!spidata->spiflash_readaddr) {
		printk (KERN_WARNING "%s: Failed to map flash device\n", module_name);
		kfree(mtd);
		kfree(spidata);
		return (-ENXIO);
	}

	mtd->name = module_name;
	mtd->type = MTD_NORFLASH;
	mtd->flags = (MTD_CAP_NORFLASH|MTD_WRITEABLE);
	mtd->size = flashconfig_tbl[index].byte_cnt;
	mtd->erasesize = flashconfig_tbl[index].sector_size;
	mtd->numeraseregions = 0;
	mtd->eraseregions = NULL;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	// 2.4
	mtd->module = THIS_MODULE;
#else
	// 2.6
	mtd->owner = THIS_MODULE;
#endif
	mtd->erase = spiflash_erase;
	mtd->read = spiflash_read;
	mtd->write = spiflash_write;

#ifdef SPIFLASH_DEBUG
	printk (KERN_DEBUG
		   "mtd->name = %s\n"
		   "mtd->size = 0x%.8x (%uM)\n"
		   "mtd->erasesize = 0x%.8x (%uK)\n"
		   "mtd->numeraseregions = %d\n",
		   mtd->name,
		   mtd->size, mtd->size / (1024*1024),
		   mtd->erasesize, mtd->erasesize / 1024,
		   mtd->numeraseregions);

	if (mtd->numeraseregions) {
		for (result = 0; result < mtd->numeraseregions; result++) {
			printk (KERN_DEBUG
			   "\n\n"
			   "mtd->eraseregions[%d].offset = 0x%.8x\n"
			   "mtd->eraseregions[%d].erasesize = 0x%.8x (%uK)\n"
			   "mtd->eraseregions[%d].numblocks = %d\n",
			   result,mtd->eraseregions[result].offset,
			   result,mtd->eraseregions[result].erasesize,mtd->eraseregions[result].erasesize / 1024,
			   result,mtd->eraseregions[result].numblocks);
		}
	}
#endif

    setSpiSpeed();

#if 1
    // create a partition for the entire flash as /dev/mtd0
    add_mtd_device(mtd);
#endif
    spidata->mtd = NULL;    // spidata->mtd is uninitialzed at this time
	/* parse redboot partitions */
	num_parts = parse_redboot_partitions(mtd, &spidata->parsed_parts, 0);

#ifdef SPIFLASH_DEBUG
	printk (KERN_DEBUG "Found %d redboot partitions\n", num_parts);
#endif

	if (num_parts) {
		result = add_mtd_partitions(mtd, spidata->parsed_parts, num_parts);
#ifndef CONFIG_BLK_DEV_INITRD
		/* Find root partition */
		mtd_parts = spidata->parsed_parts;
		for (i=0; i < num_parts; i++) {
			if (!strcmp(mtd_parts[i].name, ROOTFS_NAME)) {
				/* Create root device */
				ROOT_DEV = MKDEV(MTD_BLOCK_MAJOR, i);
				break;
			}
		}
#endif
	} else {
#ifdef SPIFLASH_DEBUG
	printk (KERN_DEBUG "Did not find any redboot partitions\n");
#endif
		kfree(mtd);
		kfree(spidata);
		spidata = NULL;
		return (-ENXIO);
	}

	spidata->mtd = mtd;
#if FAST_WRITE_256
    _set_page_write(spidata->mtd);
    (void) flash_write_addr();
#endif

	return (result);
}

void __exit
spiflash_exit (void)
{
printk("%s ...\n", __FUNCTION__);
	if (spidata && spidata->parsed_parts) {
		del_mtd_partitions (spidata->mtd);
		kfree(spidata->mtd);
		kfree(spidata);
	}
}

module_init (spiflash_init);
module_exit (spiflash_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Atheros Communications Inc");
MODULE_DESCRIPTION("MTD driver for SPI Flash on Atheros SOC");
