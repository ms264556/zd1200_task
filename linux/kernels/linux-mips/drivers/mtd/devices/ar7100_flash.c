/*
 * This file contains glue for Atheros ar7100 spi flash interface
 * Primitives are ar7100_spi_*
 * mtd flash implements are ar7100_flash_*
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <asm/delay.h>
#include <asm/io.h>
#include <linux/math64.h>
#include <linux/init.h>

#include "ar7100.h"
#include "ar7100_flash.h"
extern unsigned char rks_is_gd42_board(void);
u32 glob_rd = 0;

#if 1
#define dPrintf3	printk
#else
#define dPrintf3(...)
#endif
#define _TRACK() dPrintf3("-->%s:%d\n", __FUNCTION__, __LINE__);

/*
 * statics
 */
static void ar7100_spi_write_enable(void);
static void ar7100_spi_poll(void);
static void ar7100_spi_write_page(uint32_t addr, uint8_t *data, int len);
static void ar7100_spi_sector_erase(uint32_t addr);

#define USE_BA24    1
#if defined(USE_BA24)
static void ar7100_spi_brrd(char *ba24);
static void ar7100_spi_brwr(char ba24);
static void ar7100_spi_brac(char ba24);
static void ar7100_spi_wrear(char ba24);
static void ar7100_spi_rdear(char *ba24);
#endif

static const char *part_probes[] __initdata = {"cmdlinepart", "RedBoot", NULL};


#define AR7100_FLASH_SIZE_2MB          (2*1024*1024)
#define AR7100_FLASH_SIZE_4MB          (4*1024*1024)
#define AR7100_FLASH_SIZE_8MB          (8*1024*1024)
#define AR7100_FLASH_SECTOR_SIZE_64KB  (64*1024)

#define AR7100_FLASH_SIZE_16MB         (16*1024*1024)   /* ST M25P128 */
#define AR7100_FLASH_SECTOR_SIZE_256KB (256*1024)

#define AR7100_FLASH_SIZE_32MB         (32*1024*1024)   /* ST M25P128 x 2 */
#define AR7100_FLASH_SIZE_48MB         (48*1024*1024)
#define AR7100_FLASH_SIZE_64MB         (64*1024*1024)

#define AR7100_FLASH_PG_SIZE_256B       256
#define AR7100_FLASH_NAME               "ar7100-nor0"
/*
 * bank geometry
 */
typedef struct ar7100_flash_geom {
    uint32_t     size;
    uint32_t     sector_size;
    uint32_t     nsectors;
    uint32_t     pgsize;
}ar7100_flash_geom_t;

#if !defined(V54_BSP)
ar7100_flash_geom_t flash_geom_tbl[AR7100_FLASH_MAX_BANKS] =
        {
            {
                .size           =   AR7100_FLASH_SIZE_8MB,
                .sector_size    =   AR7100_FLASH_SECTOR_SIZE_64KB,
                .pgsize         =   AR7100_FLASH_PG_SIZE_256B
            }
        };

static int
ar7100_flash_probe()
{
    return 0;
}
#else
#include <asm/bootinfo.h>
#if !defined(CONFIG_AR934x)
#define FLASH_SELECT_GPIO       6
#else
#include <linux/proc_fs.h>
#define FLASH_SELECT_GPIO       11
#define MODE_READ   0444
#define MODE_WRITE  0222
#define MODE_RW     0644
#define PROC_FLASH  "flash"
static struct proc_dir_entry  *proc_flash;
static struct proc_dir_entry  *flash_stats;
typedef struct flash_count_t {
    unsigned int erase;
    unsigned int read;
    unsigned int write;
} flash_count;
static flash_count flash_cnt;
#endif
extern void ar7100_gpio_out_val(int gpio, int val);
atomic_t atom;
ar7100_flash_geom_t *flash_geom_tbl=NULL;
ar7100_flash_geom_t flash_geom_tbl_4m[AR7100_FLASH_MAX_BANKS] =
        {

            {
                .size           =   AR7100_FLASH_SIZE_4MB,
                .sector_size    =   AR7100_FLASH_SECTOR_SIZE_64KB,
                .pgsize         =   AR7100_FLASH_PG_SIZE_256B
            }
        };
ar7100_flash_geom_t flash_geom_tbl_8m[AR7100_FLASH_MAX_BANKS] =
        {
            {
                .size           =   AR7100_FLASH_SIZE_8MB,
                .sector_size    =   AR7100_FLASH_SECTOR_SIZE_64KB,
                .pgsize         =   AR7100_FLASH_PG_SIZE_256B
            }
        };

ar7100_flash_geom_t flash_geom_tbl_16m[AR7100_FLASH_MAX_BANKS] =
        {
            {
                .size           =   AR7100_FLASH_SIZE_16MB,
                .sector_size    =   AR7100_FLASH_SECTOR_SIZE_256KB,
                .pgsize         =   AR7100_FLASH_PG_SIZE_256B
            }
        };

ar7100_flash_geom_t flash_geom_tbl_32m[AR7100_FLASH_MAX_BANKS] =
        {
            {
                .size           =   AR7100_FLASH_SIZE_32MB,
#if !defined(CONFIG_AR934x)
                .sector_size    =   AR7100_FLASH_SECTOR_SIZE_256KB,
#else
                .sector_size    =   AR7100_FLASH_SECTOR_SIZE_64KB,
#endif
                .pgsize         =   AR7100_FLASH_PG_SIZE_256B
            }
        };

ar7100_flash_geom_t flash_geom_tbl_64m[AR7100_FLASH_MAX_BANKS] =
        {
            {
                .size           =   AR7100_FLASH_SIZE_64MB,
                .sector_size    =   AR7100_FLASH_SECTOR_SIZE_256KB,
                .pgsize         =   AR7100_FLASH_PG_SIZE_256B
            }
        };

int uboot_parts=0;

/* GLOBAL FUNCTIONS */

static DECLARE_MUTEX(ar7100_flash_sem);
void
ar7100_flash_spi_down(void) {
  down(&ar7100_flash_sem);
}

void
ar7100_flash_spi_up(void) {
  up(&ar7100_flash_sem);
}

EXPORT_SYMBOL(ar7100_flash_spi_down);
EXPORT_SYMBOL(ar7100_flash_spi_up);


static int get_flash_bank(unsigned int addr)
{
    return (addr ? (addr / AR7100_FLASH_SIZE_16MB) : 0);
}

static void set_flash_bank(char num)
{
#if defined(CONFIG_AR934x)
#if !defined(USE_BA24)
    /* gd26 or spaniel 1 */
    ar7100_gpio_out_val(FLASH_SELECT_GPIO, (num?0:1));
#else
    char val;
    /* spaniel 2 */
    if(glob_rd == MICRON_VENDOR_ID)
    {
        //printk(KERN_ALERT "in Micron\n");
        ar7100_spi_wrear(num);
        ar7100_spi_rdear(&val);
    }

    else
    {
        //printk(KERN_ALERT "in Spansion\n");
        ar7100_spi_brwr(num);
        ar7100_spi_brrd(&val);
    }
    if (val != num)
        printk("%s: failed.\n", __func__);
#endif
#else
    /* zd1k */
    ar7100_gpio_out_val(FLASH_SELECT_GPIO, num);
#endif
    return;
}

static void
read_id(void)
{
    u32 rd = 0x777777;

    ar7100_reg_wr_nf(AR7100_SPI_FS, 1);
    ar7100_reg_wr_nf(AR7100_SPI_WRITE, AR7100_SPI_CS_DIS);
    ar7100_spi_bit_banger(0x9f);
    ar7100_spi_delay_8();
    ar7100_spi_delay_8();
    ar7100_spi_delay_8();
    rd = ar7100_reg_rd(AR7100_SPI_RD_STATUS);
    ar7100_spi_done();
    printk("id read %#x\n", rd);
    glob_rd = rd;
}

static int ar7100_flash_probe(void)
{
//    extern char saved_command_line[];
	char c = ' ', *from = saved_command_line;
	unsigned long flash_size=0;
	int len = 0;

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

#ifdef CONFIG_AR9100
        if ( flash_size == 0 ) {
            flash_size = AR7100_FLASH_SIZE_16MB;
        }
#endif

    switch (flash_size) {
        case AR7100_FLASH_SIZE_4MB:
            flash_geom_tbl = flash_geom_tbl_4m;
            break;

        case AR7100_FLASH_SIZE_8MB:
            flash_geom_tbl = flash_geom_tbl_8m;
            break;

        case AR7100_FLASH_SIZE_16MB:
            /* 16MB */
            flash_geom_tbl = flash_geom_tbl_16m;
            break;

        case AR7100_FLASH_SIZE_32MB:
            /* 32MB */
            flash_geom_tbl = flash_geom_tbl_32m;
            break;

        case AR7100_FLASH_SIZE_64MB:
            /* 64MB */
            flash_geom_tbl = flash_geom_tbl_64m;
            break;

        default:
            flash_geom_tbl = flash_geom_tbl_8m;
            break;
    }

#define one_str "1"
    c = ' ';
    from = saved_command_line;
    uboot_parts = 0;
    for (;;) {
        if (c == ' ' && !memcmp(from, "uboot=", 6)) {
            if (!memcmp(from + 6, one_str, 1))
                uboot_parts = 1;
            break;
        }
        c = *(from++);
        if (!c)
            break;
        if (CL_SIZE <= ++len)
            break;
    }
    printk("uboot_parts(%d)\n", uboot_parts);
    return 0;
}
#endif

#if !defined(CONFIG_QCA955x)
static int
ar7100_flash_erase(struct mtd_info *mtd,struct erase_info *instr)
{
    int nsect, s_curr, s_last;
    uint64_t tmp;

    if (instr->addr + instr->len > mtd->size) return (-EINVAL);

#if defined(V54_BSP)
    if (!atomic_read(&atom))
        atomic_set(&atom, 1);
    else
        return (-EBUSY);

    if (instr->addr >= AR7100_FLASH_SIZE_16MB) {
        set_flash_bank(1);
#if defined(CONFIG_AR934x)
        flash_cnt.erase++;
#endif
    }
#endif

    nsect = div_u64(instr->len, mtd->erasesize);
    tmp = instr->len;
    if (do_div(tmp, mtd->erasesize))
        nsect ++;


#if defined(V54_BSP)
    if (instr->addr >= AR7100_FLASH_SIZE_16MB)
        s_curr = div_u64((instr->addr & (AR7100_FLASH_SIZE_16MB-1)), mtd->erasesize);
    else
#endif
    s_curr = div_u64(instr->addr, mtd->erasesize);
    s_last  = s_curr + nsect;

    do {
#if 0
#if defined(V54_BSP)
#if !defined(CONFIG_AR934x)
        if (mtd->size >= AR7100_FLASH_SIZE_16MB)
            ar7100_spi_sector_erase(s_curr * AR7100_FLASH_SECTOR_SIZE_256KB);
        else
#endif
#endif
        ar7100_spi_sector_erase(s_curr * AR7100_SPI_SECTOR_SIZE);
#else
        ar7100_spi_sector_erase(s_curr * mtd->erasesize);
#endif
    } while (++s_curr < s_last);

    ar7100_spi_done();

#if defined(V54_BSP)
    if (instr->addr >= AR7100_FLASH_SIZE_16MB)
        set_flash_bank(0);
    atomic_set(&atom, 0);
#endif

    if (instr->callback) {
	instr->state |= MTD_ERASE_DONE;
        instr->callback (instr);
    }

    return 0;
}

static inline int
_ar7100_flash_read(loff_t from, size_t len, size_t *retlen ,u_char *buf)
{
    uint32_t addr = from | 0xbf000000;

    if (!len) return (0);

#if defined(V54_BSP)
    if (!atomic_read(&atom))
        atomic_set(&atom, 1);
    else
        return (-EBUSY);

    if (from >= AR7100_FLASH_SIZE_16MB) {
        set_flash_bank(1);
#if defined(CONFIG_AR934x)
        flash_cnt.read++;
#endif
        addr = (from & (AR7100_FLASH_SIZE_16MB-1)) | 0xbf000000;
    }
#endif

    memcpy(buf, (uint8_t *)(addr), len);
    *retlen = len;

#if defined(V54_BSP)
    if (from >= AR7100_FLASH_SIZE_16MB)
        set_flash_bank(0);
    atomic_set(&atom, 0);
#endif

    return 0;

}

static int
ar7100_flash_read(struct mtd_info *mtd, loff_t from, size_t len,
                  size_t *retlen ,u_char *buf)
{
    if (from + len > mtd->size) return (-EINVAL);

    return _ar7100_flash_read(from, len, retlen, buf);
}

static int
ar7100_flash_write (struct mtd_info *mtd, loff_t to, size_t len,
                    size_t *retlen, const u_char *buf)
{
    int total = 0, len_this_lp, bytes_this_page;
    uint32_t addr = 0;
    u_char *mem;

#if defined(V54_BSP)
    if (!atomic_read(&atom))
        atomic_set(&atom, 1);
    else
        return (-EBUSY);

    if (to >= AR7100_FLASH_SIZE_16MB) {
        set_flash_bank(1);
#if defined(CONFIG_AR934x)
        flash_cnt.write++;
#endif
    }
#endif

    while(total < len) {
        mem = (u_char *)buf + total;
#if defined(V54_BSP)
        if (to >= AR7100_FLASH_SIZE_16MB)
            addr = (to & (AR7100_FLASH_SIZE_16MB-1)) + total;
        else
#endif
        addr             = to  + total;
        bytes_this_page  = AR7100_SPI_PAGE_SIZE - (addr % AR7100_SPI_PAGE_SIZE);
        len_this_lp      = min((len - total), (size_t)bytes_this_page);

        ar7100_spi_write_page(addr, mem, len_this_lp);
        total += len_this_lp;
    }

    ar7100_spi_done();

    *retlen = len;

#if defined(V54_BSP)
    if (to >= AR7100_FLASH_SIZE_16MB)
        set_flash_bank(0);
    atomic_set(&atom, 0);
#endif

    return 0;
}
#else
/* GD42 */
static int
ar7100_flash_erase(struct mtd_info *mtd,struct erase_info *instr)
{
    int nsect, s_curr, s_last;
    uint64_t tmp;
    int curr=0, prev=-1, i;

    if (instr->addr + instr->len > mtd->size) return (-EINVAL);

    if (!atomic_read(&atom))
        atomic_set(&atom, 1);
    else
        return (-EBUSY);

    nsect = div_u64(instr->len, mtd->erasesize);
    tmp = instr->len;
    if (do_div(tmp, mtd->erasesize))
        nsect ++;

    s_curr = div_u64(instr->addr, mtd->erasesize);
    s_last  = s_curr + nsect;

#if 0
    printk("\nfirst %d last %d sector size %d addr 0x%x len %d\n",
        s_curr, s_last, mtd->erasesize, (unsigned int)instr->addr, (unsigned int)instr->len);
#endif

    for (i = s_curr; i < s_last; i++) {
        //printk("\n%4d", i);
        curr = i / 64;  /* 16MB/256K=64sectors */
        if (curr != prev) {
            set_flash_bank(curr);
            prev = curr;
            //printk("\n(%d)", curr);
            if (curr)
                flash_cnt.erase++;
        }
        ar7100_spi_sector_erase(((i & (64-1)) * mtd->erasesize));
    }

    //printk("\n");
    ar7100_spi_done();

    set_flash_bank(0);
    atomic_set(&atom, 0);

    if (instr->callback) {
        instr->state |= MTD_ERASE_DONE;
        instr->callback (instr);
    }

    return 0;
}

static inline int
_ar7100_flash_read(loff_t from, size_t len, size_t *retlen ,u_char *buf)
{
    uint32_t addr = from;
    uint32_t boundary;
    int newlen=len, count, bank, prev=-1;
    u_char *ptr=buf;

    if (!len) return (0);

    if (!atomic_read(&atom))
        atomic_set(&atom, 1);
    else
        return (-EBUSY);

    while (newlen > 0) {
        boundary = (addr + AR7100_FLASH_SIZE_16MB) & ~(AR7100_FLASH_SIZE_16MB - 1);
        if ((addr + newlen) > boundary)
            count = boundary - addr;
        else
            count = newlen;
        bank = get_flash_bank(addr);
        if (bank != prev) {
#if 0
        printk("%s: bank(%d)\n", __func__, bank);
#endif
            set_flash_bank(bank);
            prev = bank;
            if (bank)
                flash_cnt.read++;
        }
#if 0
        printk("%s: addr(0x%x) boundary(0x%x) count(0x%x) bank(%d)\n", __func__, addr, boundary, count, bank);
#endif
#if 1 // workaround cache issue
        if (!(addr & (AR7100_FLASH_SIZE_16MB - 1)) && (count >= 32)) {
            memcpy(ptr, (uint8_t *)(0xbf000000), count);
        }
#endif
        memcpy(ptr, (uint8_t *)((addr & (AR7100_FLASH_SIZE_16MB - 1)) | 0xbf000000), count);
        ptr += count;
        addr += count;
        newlen -= count;
    }

    *retlen = len;

    set_flash_bank(0);
    atomic_set(&atom, 0);

    return 0;

}

static int
ar7100_flash_read(struct mtd_info *mtd, loff_t from, size_t len,
                  size_t *retlen ,u_char *buf)
{
    if (from + len > mtd->size) return (-EINVAL);

    return _ar7100_flash_read(from, len, retlen, buf);
}

static int
ar7100_flash_write (struct mtd_info *mtd, loff_t to, size_t len,
                    size_t *retlen, const u_char *buf)
{
    int total = 0, len_this_lp, bytes_this_page;
    uint32_t addr = 0;
    u_char *mem;
    int bank, prev=-1;

    if (!atomic_read(&atom))
        atomic_set(&atom, 1);
    else
        return (-EBUSY);

    while(total < len) {
        mem = (u_char *)buf + total;
        addr             = to  + total;
        bank = get_flash_bank(addr);
        if (bank != prev) {
            set_flash_bank(bank);
            prev = bank;
            if (bank)
                flash_cnt.write++;
        }
        bytes_this_page  = AR7100_SPI_PAGE_SIZE - (addr % AR7100_SPI_PAGE_SIZE);
        len_this_lp      = min((len - total), (size_t)bytes_this_page);

#if 0
        printk("%s: addr(0x%x) len_this_lp(0x%x) bank(%d)\n", __func__, addr, len_this_lp, bank);
#endif
        ar7100_spi_write_page((addr & (AR7100_FLASH_SIZE_16MB - 1)), mem, len_this_lp);
        total += len_this_lp;
    }

    ar7100_spi_done();

    *retlen = len;

    set_flash_bank(0);
    atomic_set(&atom, 0);

    return 0;
}
#endif

#if defined(V54_BSP)
// only used for reading boarddata info
int
v54_flash_read (loff_t from, u_char *buf, size_t len)
{
    size_t retlen = 0;

    if ( _ar7100_flash_read ( from, len, &retlen, buf) < 0 ) {
        return 0;
    } else {
        return retlen;
    }
}

#if defined(CONFIG_AR934x)
static int ar7100_flash_read_proc (char* page, char ** start,
            off_t off, int count, int *eof, void *data)
{
    int len=0;

    len += sprintf(page+len, "flash%d:\n", 1);
    len += sprintf(page+len, "%s%s%s\n",
        "Stats: erase",
        "        read",
        "       write");
    len += sprintf(page+len, "  %10u  %10u  %10u\n", flash_cnt.erase, flash_cnt.read, flash_cnt.write);
    memset(&flash_cnt, 0, sizeof(flash_cnt));

    return len;
}
#endif
#endif

/*
 * sets up flash_info and returns size of FLASH (bytes)
 */
static int __init ar7100_flash_init (void)
{
    int i, np;
    ar7100_flash_geom_t *geom;
    struct mtd_info *mtd;
    struct mtd_partition *mtd_parts;
    uint8_t index;

#if defined(V54_BSP)
    atomic_set(&atom, 0);
    read_id();
#endif

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
#if defined(CONFIG_QCA955x)
    spiclk = 0x46;
#endif
    printk("spi(0x%x)\n", (spiclk | 0x200));
    ar7100_reg_wr_nf(AR7100_SPI_CLOCK, (spiclk | 0x200));
#else
    printk("spi(0x%x)\n", spiclk);
    ar7100_reg_wr_nf(AR7100_SPI_CLOCK, spiclk);
#endif
  }
#else
    ar7100_reg_wr_nf(AR7100_SPI_CLOCK, 0x43);
#endif
    for(i = 0; i < AR7100_FLASH_MAX_BANKS; i++) {

        index = ar7100_flash_probe();
        geom  = &flash_geom_tbl[index];
        mtd         =  kmalloc(sizeof(struct mtd_info), GFP_KERNEL);
        if (!mtd) {
            printk("Cant allocate mtd stuff\n");
            return -1;
        }
        memset(mtd, 0, sizeof(struct mtd_info));

        mtd->name               =   AR7100_FLASH_NAME;
        mtd->type               =   MTD_NORFLASH;
        mtd->flags              =   (MTD_CAP_NORFLASH|MTD_WRITEABLE);
        mtd->size               =   geom->size;
        mtd->erasesize          =   geom->sector_size;
        mtd->numeraseregions    =   0;
        mtd->eraseregions       =   NULL;
        mtd->owner              =   THIS_MODULE;
        mtd->erase              =   ar7100_flash_erase;
        mtd->read               =   ar7100_flash_read;
        mtd->write              =   ar7100_flash_write;
        mtd->writesize = 1;

#if 1
        // create a partition for the entire flash as /dev/mtd0
        add_mtd_device(mtd);
#endif
        np = parse_mtd_partitions(mtd, part_probes, &mtd_parts, 0);
        if (np > 0) {
            add_mtd_partitions(mtd, mtd_parts, np);
        }
        else {
            printk("No partitions found on flash bank %d\n", i);
		}
    }

#if defined(CONFIG_AR934x)
    if ((proc_flash=proc_mkdir(PROC_FLASH, NULL)) != NULL) {
        if ((flash_stats=create_proc_entry("stats", MODE_READ, proc_flash))!= NULL) {
            flash_stats->read_proc = ar7100_flash_read_proc;
            flash_stats->data = NULL;
            memset(&flash_cnt, 0, sizeof(flash_cnt));
        } else {
            printk(": failed to create /proc/flash/stats");
        }
    } else {
        printk(": failed to create /proc/flash\n");
    }
#endif

    return 0;
}

static void __exit ar7100_flash_exit(void)
{
    /*
     * nothing to do
     */
}


/*
 * Primitives to implement flash operations
 */
#if defined(USE_BA24)
static void ar7100_spi_brrd(char *ba24)
{
    ar7100_reg_wr_nf(AR7100_SPI_FS, 1);                     // set bitbang mode
    ar7100_reg_wr_nf(AR7100_SPI_WRITE, AR7100_SPI_CS_DIS);  // disable all cs#
    ar7100_spi_bit_banger(AR7100_SPI_CMD_BRRD);             // toggle cs0, clk and data out
    ar7100_spi_delay_8();
    *ba24 = ar7100_reg_rd(AR7100_SPI_RD_STATUS);            // read data
    ar7100_spi_done();                                      // exit bitbang mode
    return;
}

static void ar7100_spi_brwr(char ba24)
{
    ar7100_reg_wr_nf(AR7100_SPI_FS, 1);
    ar7100_reg_wr_nf(AR7100_SPI_WRITE, AR7100_SPI_CS_DIS);
    ar7100_spi_bit_banger(AR7100_SPI_CMD_BRWR);
    ar7100_spi_bit_banger(ba24);
    ar7100_spi_done();
    return;
}

static void ar7100_spi_brac(char ba24)
{
    ar7100_reg_wr_nf(AR7100_SPI_FS, 1);
    ar7100_reg_wr_nf(AR7100_SPI_WRITE, AR7100_SPI_CS_DIS);
    ar7100_spi_bit_banger(AR7100_SPI_CMD_BRAC);
    ar7100_spi_bit_banger(AR7100_SPI_CMD_WRITE_SR);
    ar7100_spi_bit_banger(ba24);
    ar7100_spi_done();
    return;
}

static void ar7100_spi_rdear(char *ba24)
{
    ar7100_reg_wr_nf(AR7100_SPI_FS, 1);                     // set bitbang mode
    ar7100_reg_wr_nf(AR7100_SPI_WRITE, AR7100_SPI_CS_DIS);  // disable all cs#
    ar7100_spi_bit_banger(AR7100_MICRON_SPI_CMD_RDEAR);     // toggle cs0, clk and data out
    ar7100_spi_delay_8();
    *ba24 = ar7100_reg_rd(AR7100_SPI_RD_STATUS);            // read data
    ar7100_spi_done();                                      // exit bitbang mode
    return;
}

static void ar7100_spi_wrear(char ba24)
{
    ar7100_reg_wr_nf(AR7100_SPI_FS, 1);
    ar7100_reg_wr_nf(AR7100_SPI_WRITE, AR7100_SPI_CS_DIS);
    ar7100_spi_bit_banger(AR7100_MICRON_SPI_CMD_WREAR);
    ar7100_spi_bit_banger(ba24);
    ar7100_spi_done();
    return;
}
#endif

static void
ar7100_spi_write_enable()
{
    ar7100_reg_wr_nf(AR7100_SPI_FS, 1);
    ar7100_reg_wr_nf(AR7100_SPI_WRITE, AR7100_SPI_CS_DIS);
    ar7100_spi_bit_banger(AR7100_SPI_CMD_WREN);
    ar7100_spi_go();
}

static void
ar7100_spi_poll()
{
    int rd;

    do {
        ar7100_reg_wr_nf(AR7100_SPI_WRITE, AR7100_SPI_CS_DIS);
        ar7100_spi_bit_banger(AR7100_SPI_CMD_RD_STATUS);
        ar7100_spi_delay_8();
        rd = (ar7100_reg_rd(AR7100_SPI_RD_STATUS) & 1);
    }while(rd);
}

static void
ar7100_spi_write_page(uint32_t addr, uint8_t *data, int len)
{
    int i;
    uint8_t ch;

    ar7100_spi_write_enable();
    ar7100_spi_bit_banger(AR7100_SPI_CMD_PAGE_PROG);
    ar7100_spi_send_addr(addr);

    for(i = 0; i < len; i++) {
        ch = *(data + i);
        ar7100_spi_bit_banger(ch);
    }

    ar7100_spi_go();
    ar7100_spi_poll();
}

static void
ar7100_spi_sector_erase(uint32_t addr)
{
    ar7100_spi_write_enable();
    ar7100_spi_bit_banger(AR7100_SPI_CMD_SECTOR_ERASE);
    ar7100_spi_send_addr(addr);
    ar7100_spi_go();
#if !defined(V54_BSP)
    display(0x7d);
#endif
    ar7100_spi_poll();
}

module_init(ar7100_flash_init);
module_exit(ar7100_flash_exit);
