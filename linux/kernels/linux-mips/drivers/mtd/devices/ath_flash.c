/*
 * This file contains glue for Atheros ath spi flash interface
 * Primitives are ath_spi_*
 * mtd flash implements are ath_flash_*
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <asm/semaphore.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <asm/delay.h>
#include <asm/io.h>
#include <asm/div64.h>

#include "ar7100.h"
#include "ath_flash.h"

/* this is passed in as a boot parameter by bootloader */
extern int __ath_flash_size;

/*
 * statics
 */
#if !defined(V54_BSP)
static void ath_spi_write_enable(void);
static void ath_spi_poll(void);
#endif
#if !defined(ATH_SST_FLASH)
static void ath_spi_write_page(uint32_t addr, uint8_t * data, int len);
#endif
static void ath_spi_sector_erase(uint32_t addr);
static void ath_spi_read_page(uint32_t addr, u_char *data, int len);

static const char *part_probes[] __initdata = { "cmdlinepart", "RedBoot", NULL };

static DECLARE_MUTEX(ath_flash_sem);

/* GLOBAL FUNCTIONS */
void
ath_flash_spi_down(void)
{
	down(&ath_flash_sem);
}

void
ath_flash_spi_up(void)
{
	up(&ath_flash_sem);
}

EXPORT_SYMBOL(ath_flash_spi_down);
EXPORT_SYMBOL(ath_flash_spi_up);

#define ATH_FLASH_SIZE_2MB          (2*1024*1024)
#define ATH_FLASH_SIZE_4MB          (4*1024*1024)
#define ATH_FLASH_SIZE_8MB          (8*1024*1024)
#if defined(V54_BSP)
#define ATH_FLASH_SIZE_16MB         (16*1024*1024)
#define ATH_FLASH_SIZE_32MB         (32*1024*1024)
#endif
#define ATH_FLASH_SECTOR_SIZE_64KB  (64*1024)
#define ATH_FLASH_PG_SIZE_256B       256
#if !defined(V54_BSP)
#define ATH_FLASH_NAME               "ath-nor0"
#define ATH_FLASH_NAME_1             "ath-nor1"
#else
#define ATH_FLASH_NAME               "ar7100-nor0"
#define ATH_FLASH_NAME_1             "ar7100-nor1"
#endif
/*
 * bank geometry
 */
typedef struct ath_flash_geom {
	uint32_t size;
	uint32_t sector_size;
	uint32_t nsectors;
	uint32_t pgsize;
} ath_flash_geom_t;

#if !defined(V54_BSP)
ath_flash_geom_t flash_geom_tbl[ATH_FLASH_MAX_BANKS] = {
	{
		.size		= ATH_FLASH_SIZE_8MB,
		.sector_size	= ATH_FLASH_SECTOR_SIZE_64KB,
		.pgsize		= ATH_FLASH_PG_SIZE_256B
	}
};

static int
ath_flash_probe(void)
{
	return 0;
}
#else
ath_flash_geom_t flash_geom_tbl[ATH_FLASH_MAX_BANKS] = {
	{
		.size		= ATH_FLASH_SIZE_32MB,
		.sector_size	= ATH_FLASH_SECTOR_SIZE_64KB,
		.pgsize		= ATH_FLASH_PG_SIZE_256B
	}
};

static int
ath_flash_probe(void)
{
	return 0;
}
#endif

#if defined(ATH_SST_FLASH)
void
ath_spi_flash_unblock(void)
{
	ath_spi_write_enable();
	ath_spi_bit_banger(ATH_SPI_CMD_WRITE_SR);
	ath_spi_bit_banger(0x0);
	ath_spi_go();
	ath_spi_poll();
}
#endif

static int
ath_flash_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	int nsect, s_curr, s_last;
	uint64_t  res;

	if (instr->addr + instr->len > mtd->size)
		return (-EINVAL);

	ath_flash_spi_down();

	res = instr->len;
	do_div(res, mtd->erasesize);
	nsect = res;

	if (((uint32_t)instr->len) % mtd->erasesize)
		nsect ++;

	res = instr->addr;
	do_div(res,mtd->erasesize);
	s_curr = res;

	s_last  = s_curr + nsect;

	do {
		ath_spi_sector_erase(s_curr * ATH_SPI_SECTOR_SIZE);
	} while (++s_curr < s_last);

	ath_spi_done();

	ath_flash_spi_up();

	if (instr->callback) {
		instr->state |= MTD_ERASE_DONE;
		instr->callback(instr);
	}

	return 0;
}

static int
ath_flash_read(struct mtd_info *mtd, loff_t from, size_t len,
		  size_t *retlen, u_char *buf)
{
	uint32_t addr = from ; //| 0xbf000000;

	if (!len)
		return (0);
	if (from + len > mtd->size)
		return (-EINVAL);

	ath_flash_spi_down();

	ath_spi_read_page(addr, buf, len); //memcpy(buf, (uint8_t *)(addr), len);
	*retlen = len;

	ath_flash_spi_up();

	return 0;
}

#if defined(ATH_SST_FLASH)
static int
ath_flash_write(struct mtd_info *mtd, loff_t dst, size_t len,
		   size_t * retlen, const u_char * src)
{
	uint32_t val;

	//printk("write len: %lu dst: 0x%x src: %p\n", len, dst, src);

	*retlen = len;

	for (; len; len--, dst++, src++) {
		ath_spi_write_enable();	// dont move this above 'for'
		ath_spi_bit_banger(ATH_SPI_CMD_PAGE_PROG);
		ath_spi_send_addr(dst);

		val = *src & 0xff;
		ath_spi_bit_banger(val);

		ath_spi_go();
		ath_spi_poll();
	}
	/*
	 * Disable the Function Select
	 * Without this we can't re-read the written data
	 */
	ath_reg_wr(ATH_SPI_FS, 0);

	if (len) {
		*retlen -= len;
		return -EIO;
	}
	return 0;
}
#else
static int
ath_flash_write(struct mtd_info *mtd, loff_t to, size_t len,
		   size_t *retlen, const u_char *buf)
{
	int total = 0, len_this_lp, bytes_this_page;
	uint32_t addr = 0;
	u_char *mem;

	ath_flash_spi_down();

	while (total < len) {
		mem = (u_char *) (buf + total);
		addr = to + total;
		bytes_this_page =
		    ATH_SPI_PAGE_SIZE - (addr % ATH_SPI_PAGE_SIZE);
		len_this_lp = min(((int)len - total), bytes_this_page);

		ath_spi_write_page(addr, mem, len_this_lp);
		total += len_this_lp;
	}

	ath_spi_done();

	ath_flash_spi_up();

	*retlen = len;
	return 0;
}
#endif

#if defined(V54_BSP)
// only used for reading boarddata info
int
v54_flash_read (loff_t from, u_char *buf, size_t len)
{
	uint32_t addr = from ; //| 0xbf000000;

	if (!len)
		return (0);

	ath_flash_spi_down();

	ath_spi_read_page(addr, buf, len); //memcpy(buf, (uint8_t *)(addr), len);

	ath_flash_spi_up();

	return len;
}
#endif

/*
 * sets up flash_info and returns size of FLASH (bytes)
 */
static int __init ath_flash_init(void)
{
	int i, np;
	ath_flash_geom_t *geom;
	struct mtd_info *mtd;
	struct mtd_partition *mtd_parts;
	uint8_t index;

	init_MUTEX(&ath_flash_sem);

#if defined(V54_BSP)
    {
        /*
            Set SPI CLK <= 20MHz to workaround flash read issue
            spi clk = ahb clk / ((spi clk divider+1)*2)
        */
        extern uint32_t ar7100_ahb_freq;
        unsigned char spiclk=0x43;
        if ((ar7100_ahb_freq/1000000/8) > 20) {
            spiclk = 0x44;
            if ((ar7100_ahb_freq/1000000/10) > 20) {
                spiclk = 0x45;
            }
        }
#if defined(CONFIG_AR934x)
        printk("ar934x spi(0x%x)\n", (spiclk | 0x200));
        ath_reg_wr_nf(AR7100_SPI_CLOCK, (spiclk | 0x200));
#else
        printk("spi(0x%x)\n", spiclk);
        ath_reg_wr_nf(AR7100_SPI_CLOCK, spiclk);
#endif
    }
#else
#if defined(ATH_SST_FLASH)
	ath_reg_wr_nf(ATH_SPI_CLOCK, 0x3);
	ath_spi_flash_unblock();
	ath_reg_wr(ATH_SPI_FS, 0);
#else
#ifndef CONFIG_MACH_AR934x
	ath_reg_wr_nf(ATH_SPI_CLOCK, 0x43);
#endif
#endif
#endif
	for (i = 0; i < ATH_FLASH_MAX_BANKS; i++) {

		index = ath_flash_probe();
		//geom = &flash_geom_tbl[index];
		geom = &flash_geom_tbl[i];

#if !defined(V54_BSP)
		/* set flash size to value from bootloader if it passed valid value */
		/* otherwise use the default 4MB.                                   */
		if (__ath_flash_size >= 4 && __ath_flash_size <= 16)
			geom->size = __ath_flash_size * 1024 * 1024;
#endif

		mtd = kmalloc(sizeof(struct mtd_info), GFP_KERNEL);
		if (!mtd) {
			printk("Cant allocate mtd stuff\n");
			return -1;
		}
		memset(mtd, 0, sizeof(struct mtd_info));

		mtd->name		= (i == 0) ? ATH_FLASH_NAME : ATH_FLASH_NAME_1;
		mtd->type		= MTD_NORFLASH;
		mtd->flags		= MTD_CAP_NORFLASH | MTD_WRITEABLE;
		mtd->size		= geom->size;
		mtd->erasesize		= geom->sector_size;
		mtd->numeraseregions	= 0;
		mtd->eraseregions	= NULL;
		mtd->owner		= THIS_MODULE;
		mtd->erase		= ath_flash_erase;
		mtd->read		= ath_flash_read;
		mtd->write		= ath_flash_write;
#if !defined(V54_BSP)
		mtd->writesize		= 1;
#endif

                if (i == 1) {
                   mtd_parts->offset = 0x01000000;
                   mtd->size = 0x02000000;
                   mtd->erasesize = 0x00010000;
                }

#if defined(V54_BSP)
        // create a partition for the entire flash as /dev/mtd0
        add_mtd_device(mtd);
#endif
		np = parse_mtd_partitions(mtd, part_probes, &mtd_parts, 0);
		if (np > 0) {

                        if (i == 1 ) mtd_parts->offset = 0x01000000;

			add_mtd_partitions(mtd, mtd_parts, np);
		} else {
			printk("No partitions found on flash bank %d\n", i);
		}
	}

	return 0;
}

static void __exit ath_flash_exit(void)
{
	/*
	 * nothing to do
	 */
}

/*
 * Primitives to implement flash operations
 */
#if !defined(V54_BSP)
static void
ath_spi_write_enable()
{
	ath_reg_wr_nf(ATH_SPI_FS, 1);
	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
	ath_spi_bit_banger(ATH_SPI_CMD_WREN);
	ath_spi_go();
}
#endif

static void
ath_spi_write_enable_idx(int idx)
{
        ath_reg_wr_nf(ATH_SPI_FS, 1);
        ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
        ath_spi_bit_banger_idx(idx, ATH_SPI_CMD_WREN);
        ath_spi_go_idx(idx);
}


#if !defined(V54_BSP)
static void
ath_spi_poll()
{
	int rd;

	do {
		ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
		ath_spi_bit_banger(ATH_SPI_CMD_RD_STATUS);
		ath_spi_delay_8();
		rd = (ath_reg_rd(ATH_SPI_RD_STATUS) & 1);
	} while (rd);
}
#endif

static void
ath_spi_poll_idx(int idx)
{
        int rd;

        do {
                ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
                ath_spi_bit_banger_idx(idx, ATH_SPI_CMD_RD_STATUS);
                ath_spi_delay_8_idx(idx);
                rd = (ath_reg_rd(ATH_SPI_RD_STATUS) & 1);
        } while (rd);
}


static void
ath_spi_write_page(uint32_t addr, uint8_t *data, int len)
{
	int i;
	uint8_t ch;
        uint32_t idx;

	idx = (addr < 0x1000000) ? 0 : 1;
	if(idx==1)
	   addr -= 0x1000000;

	ath_spi_write_enable_idx(idx);
	ath_spi_bit_banger_idx(idx, ATH_SPI_CMD_PAGE_PROG);
	ath_spi_send_addr_idx(idx, addr);

	for (i = 0; i < len; i++) {
		ch = *(data + i);
		ath_spi_bit_banger_idx(idx, ch);
	}

	ath_spi_go_idx(idx);
	ath_spi_poll_idx(idx);
}

static void
ath_spi_sector_erase(uint32_t addr)
{
	uint32_t idx;
	idx = (addr < 0x1000000) ? 0 : 1;

	if(idx==1)
           addr -= 0x1000000;

	ath_spi_write_enable_idx(idx);
	ath_spi_bit_banger_idx(idx, ATH_SPI_CMD_SECTOR_ERASE);
	ath_spi_send_addr_idx(idx, addr);
	ath_spi_go_idx(idx);
#if 0
	/*
	 * Do not touch the GPIO's unnecessarily. Might conflict
	 * with customer's settings.
	 */
	display(0x7d);
#endif
	ath_spi_poll_idx(idx);
}

static void
ath_spi_read_page(uint32_t addr, u_char *data, int len)
{
	int i;
	uint32_t idx;

	idx = (addr < 0x1000000) ? 0 : 1;
	if(idx==1)
	   addr -= 0x1000000;

	ath_reg_wr_nf(ATH_SPI_FS, 1);
	ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);
	ath_spi_bit_banger_idx(idx, 0x03);
	ath_spi_send_addr_idx(idx, addr);
	for(i = 0; i < len; i++) {
            ath_spi_delay_8_idx(idx);
            *(data + i) = (ath_reg_rd(ATH_SPI_RD_STATUS)) & 0xff;
	}
	ath_spi_go_idx(idx);
	ath_spi_done();
}


module_init(ath_flash_init);
module_exit(ath_flash_exit);
