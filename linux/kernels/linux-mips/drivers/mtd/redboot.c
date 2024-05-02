/*
 * Parse RedBoot-style Flash Image System (FIS) tables and
 * produce a Linux partition array to match.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/vmalloc.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>

#undef V54_DUMP_INFO
#ifdef V54_DUMP_INFO
#define dPrintf2		printk
#else
#define dPrintf2(...)
#endif
#define dPrintf		printk

#ifdef V54_FLASH_ROOTFS

static struct v54_high_mem *v54HighMem = NULL;


#define BIN_HDR_MAGIC	"RCKS"
#define MAGIC_LEN	    4
#define HDR_VERSION_LEN 15
#define RKS_BD_CN_SIZE  32		    //need to match value in
								    //../../atheros/linux/driver/v54bsp/ar531x_bsp.h
struct bin_hdr {
	u_int8_t	magic[MAGIC_LEN];
    u_int32_t   next_image;         // offset to next image ( reserved )
	u_int8_t    invalid;			// only autoboot from this image if invalid is 0
	u_int8_t	hdr_len;			// length of this header
	u_int8_t	compression[2];		// compression scheme, default l7, [l7|zl|xx]

	u_int32_t	entry_point;		// Execution entry point
	u_int32_t	bin_len;			// length of (compressed) binary image

	u_int32_t	timestamp;
	u_int8_t	signature[16];		// MD5 checksum

    u_int16_t   hdr_version;        // header version
    u_int16_t   checksum;           // header checksum
	u_int8_t	version[HDR_VERSION_LEN+1];		// FW version string

    // the following fields are for validating that we have the correct image
    // The rule is that a value of 0 means don't care
    //
    u_int8_t    product;            // product class, e.g. AP vs Adapter
    u_int8_t    architecture;       // mips, le/be, etc.
    u_int8_t    chipset;            // e.g AR531X
	u_int8_t    boardType;          // V54 board type
								    // see ../../atheros/linux/driver/v54bsp/ar531x_bsp.h

    u_int8_t   customer[32];        // customer ID, NULL ==> any customer
    u_int8_t    _pad2[32];          // pad out to 128 bytes
};

#ifdef V54_DUMP_INFO
static inline unsigned short
in_cksum(void *data, unsigned int bytes)
{
	unsigned int    running_xsum = 0;
	volatile unsigned short  *running_buffer;

	running_buffer = (volatile unsigned short *)data;
	while (bytes) {
		if (bytes == 1) {
			running_xsum += (ntohs(*running_buffer))&0xff00;
			bytes --;
		} else {
			running_xsum += ntohs(*running_buffer);
			bytes -= 2;
		}
		running_buffer++;
	}

	while (running_xsum & 0xffff0000) {
		running_xsum = (running_xsum & 0xffff) + (running_xsum >> 16);
	}

	return htons((unsigned short) running_xsum);
}

static int
BAD_HDR_CKSUM(struct bin_hdr *binhdr)
{
    unsigned short my_cksum = binhdr->checksum;
    unsigned short cksum;

    binhdr->checksum = 0;
    cksum = ~in_cksum((void *)binhdr, sizeof(struct bin_hdr));
    binhdr->checksum = my_cksum;

    return (cksum != my_cksum);
}

static void
bin_hdr_dump(struct bin_hdr *hdrp)
{
    int i;

	dPrintf("bin_hdr_dump: Magic:        ");
	for (i=0; i<(int)sizeof(hdrp->magic); i++) {
		dPrintf("%c", (char)hdrp->magic[i]);
	}
	dPrintf("\n");

	dPrintf("bin_hdr_dump: next_image:   %x\n", (unsigned int)hdrp->next_image);
	dPrintf("bin_hdr_dump: invalid:      %x\n", (unsigned int)hdrp->invalid);
	dPrintf("bin_hdr_dump: hdr_len:      %u\n", (unsigned int)hdrp->hdr_len);

	dPrintf("bin_hdr_dump: compression:  ");
	dPrintf("%c", hdrp->compression[0]);
	dPrintf("%c", hdrp->compression[1]);
	dPrintf("\n");

	dPrintf("bin_hdr_dump: entry_point:  0x%X\n", (unsigned int)hdrp->entry_point);
	dPrintf("bin_hdr_dump: timestamp:    0x%X\n", (unsigned int)hdrp->timestamp);
	dPrintf("bin_hdr_dump: binl7_len:    %lu\n",  (unsigned long)hdrp->bin_len);
	dPrintf("bin_hdr_dump: hdr_version:  0x%X\n", (unsigned int)hdrp->hdr_version);
	dPrintf("bin_hdr_dump: hdr_cksum:    0x%04X", (unsigned int)hdrp->checksum);
    if ( BAD_HDR_CKSUM(hdrp)) {
        dPrintf(" != 0x%04X - bad cksum\n", (unsigned int)hdrp->checksum);
    } else {
        dPrintf("\n");
    }

	hdrp->version[HDR_VERSION_LEN] = '\0';
	dPrintf("bin_hdr_dump: version:      %s\n", hdrp->version);

	dPrintf("bin_hdr_dump: MD5:          ");
	for (i = 0; i < (int)sizeof(hdrp->signature); i++) {
		dPrintf("%02X", hdrp->signature[i]);
	}
	dPrintf("\n");

	dPrintf("bin_hdr_dump: product:      %x\n", (unsigned int)hdrp->product);
	dPrintf("bin_hdr_dump: architecture: %x\n", (unsigned int)hdrp->architecture);
	dPrintf("bin_hdr_dump: chipset:      %x\n", (unsigned int)hdrp->chipset);
	dPrintf("bin_hdr_dump: board_type:   %x\n", (unsigned int)hdrp->boardType);

	hdrp->customer[RKS_BD_CN_SIZE-1] = '\0';
	dPrintf("bin_hdr_dump: customer:     %s\n", hdrp->customer);
    return;
}
#endif

static struct bin_hdr rks_hdr;
static int is_RKS_hdr(struct mtd_info *master, unsigned long offset)
{
    unsigned char * hdr_start = (unsigned char *)&rks_hdr;
	size_t retlen;
	int    ret;

	ret = master->read(master, offset,
			   sizeof(struct bin_hdr), &retlen, (void *)hdr_start);
	if (ret || retlen != sizeof(struct bin_hdr)) {
        dPrintf("%s: flash_read(0x%lx, %p, %d) failed\n", __FUNCTION__,
                offset, hdr_start, sizeof(struct bin_hdr));
        return 0;
	}

#if 0 && defined(V54_DUMP_INFO)
    {
        int i;
        for (i=0; i<sizeof(rks_hdr); i++) {
            if (!(i % 32))
                printk("\n");
            printk("%2x ", *(((unsigned char *)&rks_hdr)+i));
        }
        printk("\n");
    }
    bin_hdr_dump(&rks_hdr);
#endif
    if (memcmp((unsigned char *)rks_hdr.magic, BIN_HDR_MAGIC,
                    sizeof(rks_hdr.magic)) == 0) {
#if 0 && defined(V54_DUMP_INFO)
        bin_hdr_dump(&rks_hdr);
#endif
        return 1;
    }
    return 0;
}

typedef struct v54_rootfs_data {
	char *name;
	int   vaild;
	int   block;
} V54_ROOTFS_DATA;

static V54_ROOTFS_DATA rootfs_data[2] = {
  {"v54_$rootfs$_1", 0, 0},
  {"v54_$rootfs$_2", 0, 0}
};

int v54_rootfs_block = 0;
int v54_create_ramdisk = 1;
extern void * __rd_start, *__rd_end;

#define NO_EMBEDDED_ROOTFS() ((unsigned long)&__rd_start == (unsigned long)&__rd_end )

#endif

#if defined(V54_BSP)
/* max single flash size */
#define MAX_FLASH_SIZE          0x1000000
#define MAX_FLASH_SIZE_64M      0x4000000
// export symbol for v54bsp
u_int32_t flash_reserved_offset = 0;
struct mtd_partition *boarddata_part = NULL;

#if defined(CONFIG_MACH_AR7100) /* multiple flash chip support  */
unsigned char stg_name[4][5] =
    {"stg0", "stg1", "stg2", "stg3"};
#endif

#endif

typedef struct v54_image_data {
	int type;	// 1 = main ; 2 = bkup ; 0 = others
	int good;	// 1 = good, 0 = bad
} V54_IMAGE_DATA;
#define V54_FIS_UNPADDED	 ( sizeof(struct v54_image_data) )

struct fis_image_desc {
    unsigned char name[16];      // Null terminated name
    uint32_t	  flash_base;    // Address within FLASH of image
    uint32_t	  mem_base;      // Address in memory where it executes
    uint32_t	  size;          // Length of image
    uint32_t	  entry_point;   // Execution entry point
    uint32_t	  data_length;   // Length of actual data
#ifdef V54_BSP
	V54_IMAGE_DATA	image_info;	// primary or secondary
    unsigned char _pad[256-(16+7*sizeof(uint32_t))-V54_FIS_UNPADDED];
#else
    unsigned char _pad[256-(16+7*sizeof(uint32_t))];
#endif
    uint32_t	  desc_cksum;    // Checksum over image descriptor
    uint32_t	  file_cksum;    // Checksum over image data
};

struct fis_list {
	struct fis_image_desc *img;
	struct fis_list *next;
};

static int directory = CONFIG_MTD_REDBOOT_DIRECTORY_BLOCK;
module_param(directory, int, 0);

static inline int redboot_checksum(struct fis_image_desc *img)
{
	/* RedBoot doesn't actually write the desc_cksum field yet AFAICT */
	return 1;
}

#define __DUMP16 0
#if __DUMP16
static inline
_dump16(char *bufp)
{
        char buf1[16];
        int j;
        memcpy(buf1, bufp, 16);

        for ( j=0; j< 16; j++) {
            printk("%02X ", ((int)buf1[j])&0xFF);
        }
#if 0
        printk("    ");
        for ( j=0; j< 16; j++) {
            printk("%c",  buf1[j]);
        }
#endif
        printk("\n");
}
#else
#define _dump16(...)
#endif

#if defined(CONFIG_MTD_END_RESERVED_BLOCK)
#undef CONFIG_MTD_END_RESERVED
#define CONFIG_MTD_END_RESERVED ( CONFIG_MTD_END_RESERVED_BLOCK * master->erasesize )
#endif

#if defined(CONFIG_AR9100)
#if 0
struct fis_image_desc {
    unsigned char name[16];      // Null terminated name
    uint32_t flash_base;    // Address within FLASH of image
    uint32_t mem_base;      // Address in memory where it executes
    uint32_t size;          // Length of image
    uint32_t entry_point;   // Execution entry point
    uint32_t data_length;   // Length of actual data
#ifdef V54_BSP
	V54_IMAGE_DATA	image_info;	// primary or secondary
    unsigned char _pad[256-(16+7*sizeof(uint32_t))-V54_FIS_UNPADDED];
#else
    unsigned char _pad[256-(16+7*sizeof(uint32_t))];
#endif
    uint32_t desc_cksum;    // Checksum over image descriptor
    uint32_t file_cksum;    // Checksum over image data
};
#endif

// fake a partition table for uboot
struct fis_image_desc uboot_fake[] = {
{"u-boot",         0x00000000, 0xA8000000, 0x00040000, 0x00000000, 0x00040000,{0,1},{0},0,0},
{"rcks_wlan.main", 0x00040000, 0x80041000, 0x006C0000, 0x80570000, 0x006C0000,{1,1},{0},0,0},
{"rcks_wlan.bkup", 0x00700000, 0x80041000, 0x006C0000, 0x80570000, 0x006C0000,{2,1},{0},0,0},
{"datafs",         0x00DC0000, 0xA8DC0000, 0x00180000, 0x00000000, 0x00180000,{0,1},{0},0,0},
{"u-boot-env",     0x00F40000, 0xA8F40000, 0x00040000, 0x00000000, 0x00040000,{0,1},{0},0,0},
{{0xff,},          0,          0,          0,          0,          0,         {0,0},{0},0,0},
};
#elif defined(CONFIG_AR934x)
// fake a partition table for uboot
struct fis_image_desc uboot_fake_32m[] = {
{"u-boot",         0x00000000, 0xA8000000, 0x00040000, 0x00000000, 0x00040000,{0,1},{0},0,0},
{"rcks_wlan.main", 0x00040000, 0x80041000, 0x00D00000, 0x80570000, 0x00D00000,{1,1},{0},0,0},
{"datafs",         0x00D40000, 0xA8DC0000, 0x00200000, 0x00000000, 0x00200000,{0,1},{0},0,0},
{"u-boot-env",     0x00F40000, 0xA8F40000, 0x00040000, 0x00000000, 0x00040000,{0,1},{0},0,0},
{"Board Data",     0x00F80000, 0xA8F40000, 0x00080000, 0x00000000, 0x00080000,{0,1},{0},0,0},
{"rcks_wlan.bkup", 0x01000000, 0x80041000, 0x00D00000, 0x80570000, 0x00D00000,{2,1},{0},0,0},
{"datafs2",        0x01D00000, 0x00000000, 0x00300000, 0x00000000, 0x00300000,{0,1},{0},0,0},
{{0xff,},          0,          0,          0,          0,          0,         {0,0},{0},0,0},
};
int uboot_fake_32m_slots = sizeof(uboot_fake_32m)/sizeof(struct fis_image_desc);

// fake a partition table for uboot
struct fis_image_desc uboot_fake_64m[] = {
{"u-boot",         0x00000000, 0xA8000000, 0x00100000, 0x00000000, 0x00100000,{0,1},{0},0,0},
{"u-boot-env",     0x00100000, 0xA8F40000, 0x00040000, 0x00000000, 0x00040000,{0,1},{0},0,0},
{"Board Data",     0x00140000, 0xA8F40000, 0x00080000, 0x00000000, 0x00080000,{0,1},{0},0,0},
{"datafs",         0x001c0000, 0xA8DC0000, 0x00640000, 0x00000000, 0x00640000,{0,1},{0},0,0},
{"rcks_wlan.main", 0x00800000, 0x80041000, 0x01800000, 0x80570000, 0x01800000,{1,1},{0},0,0},
{"rcks_wlan.bkup", 0x02000000, 0x80041000, 0x01800000, 0x80570000, 0x01800000,{2,1},{0},0,0},
{"datafs2",        0x03800000, 0x00000000, 0x00300000, 0x00000000, 0x00300000,{0,1},{0},0,0},
{{0xff,},          0,          0,          0,          0,          0,         {0,0},{0},0,0},
};

int uboot_fake_64m_slots = sizeof(uboot_fake_64m)/sizeof(struct fis_image_desc);

struct fis_image_desc *uboot_fake = uboot_fake_32m;
#define BOARDDATA_END_64M   0x001c0000
#elif defined(CONFIG_AR7100) && !defined(CONFIG_AR9100) && !defined(CONFIG_AR934x)
// fake a partition table for uboot
struct fis_image_desc uboot_fake[] = {
{"u-boot",         0x00000000, 0xA8000000, 0x00040000, 0x00000000, 0x00040000,{0,1},{0},0,0},
{"rcks_wlan.main", 0x00040000, 0x80041000, 0x00700000, 0x80570000, 0x00700000,{1,1},{0},0,0},
{"rcks_wlan.bkup", 0x00740000, 0x80041000, 0x00700000, 0x80570000, 0x00700000,{2,1},{0},0,0},
{"datafs",         0x00E40000, 0xA8E40000, 0x00140000, 0x00000000, 0x00140000,{0,1},{0},0,0},
{"u-boot-env",     0x00F80000, 0xA8F80000, 0x00040000, 0x00000000, 0x00040000,{0,1},{0},0,0},
{{0xff,},          0,          0,          0,          0,          0,         {0,0},{0},0,0},
};
#elif defined(CONFIG_P10XX)
// fake a partition table for uboot
struct fis_image_desc uboot_fake_32m[] = {
{"u-boot",         0x00000000, 0x00000000, 0x00080000, 0x00000000, 0x00080000,{0,1},{0},0,0},
{"rcks_wlan.main", 0x00080000, 0x00000000, 0x00D00000, 0x00000000, 0x00D00000,{1,1},{0},0,0},
{"datafs",         0x00D80000, 0x00000000, 0x00260000, 0x00000000, 0x00260000,{0,1},{0},0,0},
{"u-boot-env",     0x00FE0000, 0x00000000, 0x00010000, 0x00000000, 0x00010000,{0,1},{0},0,0},
{"Board Data",     0x00FF0000, 0x00000000, 0x00010000, 0x00000000, 0x00010000,{0,1},{0},0,0},
{"rcks_wlan.bkup", 0x01000000, 0x00000000, 0x00D00000, 0x00000000, 0x00D00000,{2,1},{0},0,0},
{"datafs2",        0x01D00000, 0x00000000, 0x00300000, 0x00000000, 0x00300000,{0,1},{0},0,0},
{{0xff,},          0,          0,          0,          0,          0,         {0,0},{0},0,0},
};
int uboot_fake_32m_slots = sizeof(uboot_fake_32m)/sizeof(struct fis_image_desc);

struct fis_image_desc uboot_fake_64m[] = {
{"u-boot",         0x00000000, 0x00000000, 0x00100000, 0x00000000, 0x00100000,{0,1},{0},0,0},
{"rcks_wlan.main", 0x00100000, 0x00000000, 0x01800000, 0x00000000, 0x01800000,{1,1},{0},0,0},
{"datafs",         0x01900000, 0x00000000, 0x00680000, 0x00000000, 0x00680000,{0,1},{0},0,0},
{"u-boot-env",     0x01f80000, 0x00000000, 0x00040000, 0x00000000, 0x00040000,{0,1},{0},0,0},
{"Board Data",     0x01fc0000, 0x00000000, 0x00040000, 0x00000000, 0x00040000,{0,1},{0},0,0},
{"rcks_wlan.bkup", 0x02000000, 0x00000000, 0x01800000, 0x00000000, 0x01800000,{2,1},{0},0,0},
{"datafs2",        0x03800000, 0x00000000, 0x00800000, 0x00000000, 0x00800000,{0,1},{0},0,0},
{{0xff,},          0,          0,          0,          0,          0,         {0,0},{0},0,0},
};
int uboot_fake_64m_slots = sizeof(uboot_fake_64m)/sizeof(struct fis_image_desc);

struct fis_image_desc *uboot_fake = uboot_fake_32m;
#else
struct fix_image_desc *uboot_fake = NULL;
#endif

int parse_redboot_partitions(struct mtd_info *master,
                             struct mtd_partition **pparts,
                             unsigned long fis_origin)
{
	int nrparts = 0;
	struct fis_image_desc *buf;
	struct mtd_partition *parts;
	struct fis_list *fl = NULL, *tmp_fl;
	int ret, i;
	size_t retlen;
	char *names;
	char *nullname;
	int namelen = 0;
	int nulllen = 0;
	int numslots;
	unsigned long offset = 0;
#ifdef CONFIG_MTD_REDBOOT_PARTS_UNALLOCATED
	static char nullstring[] = "unallocated";
#endif
#ifdef V54_BSP
    int numParts=0;     /* number of partitions in 2nd flash */
    int totalParts;     /* number of total partitions*/
#endif
#ifdef V54_FLASH_ROOTFS
    int k, load_type = 0;
    struct fis_image_desc desc[2];
#ifdef V54_MOUNT_FLASH_ROOTFS
    struct fis_image_desc *main_desc = NULL, *bkup_desc = NULL;
    struct fis_image_desc *current_desc = NULL;
    unsigned long boot_part_size = 0;
    v54_create_ramdisk = 0;
#endif

#endif

#if defined(V54_BSP) && defined(CONFIG_MACH_AR7100) && !defined(CONFIG_AR934x)
    if (master->size > MAX_FLASH_SIZE) {
        numParts = CONFIG_MTD_2ND_FLASH_PARTS;
        if ((numParts != 1) && (numParts != 2) && (numParts != 4))
            numParts = 1;
    }
#endif

	buf = vmalloc(master->erasesize);

	if (!buf)
		return -ENOMEM;

#if (!defined(CONFIG_MACH_AR7100) && !defined(CONFIG_P1020RDB) && !defined(CONFIG_P10XX)) || (defined(CONFIG_AR9100) || defined(CONFIG_AR934x))
	if ( directory < 0 ) {
#if defined(CONFIG_AR934x)
        if (master->size > MAX_FLASH_SIZE) {
            if (master->size == MAX_FLASH_SIZE_64M) {
                offset = BOARDDATA_END_64M + (-1*master->erasesize);
            } else {
		        offset = MAX_FLASH_SIZE + directory*master->erasesize;
            }
        } else
#endif
		offset = master->size + directory*master->erasesize;
#if defined(CONFIG_MTD_END_RESERVED)
        if (master->size == MAX_FLASH_SIZE_64M) {
            offset -= (2*master->erasesize);
        } else {
            offset -= CONFIG_MTD_END_RESERVED;
        }
#endif
		while (master->block_isbad && 
		       master->block_isbad(master, offset)) {
			if (!offset) {
			nogood:
				printk(KERN_NOTICE "Failed to find a non-bad block to check for RedBoot partition table\n");
				return -EIO;
			}
			offset -= master->erasesize;
		}
	} else {
		offset = directory * master->erasesize;
		while (master->block_isbad && 
		       master->block_isbad(master, offset)) {
			offset += master->erasesize;
			if (offset == master->size)
				goto nogood;
		}
	}
#else /* CONFIG_MACH_AR7100 */
	if ( directory < 0 ) {
        if (master->size > MAX_FLASH_SIZE) {
            if (master->size == MAX_FLASH_SIZE_64M) {
		        offset = 2*MAX_FLASH_SIZE + directory*master->erasesize;
            } else {
		        offset = MAX_FLASH_SIZE + directory*master->erasesize;
            }
        } else {
		offset = master->size + directory*master->erasesize;
        }
	} else {
		offset = directory*master->erasesize;
    }
#endif /* CONFIG_MACH_AR7100 */

printk("master:{size=%lx erasesize=%lx}, directory=%d offset=%lx\n",
			(unsigned long)master->size, (unsigned long)master->erasesize,
			directory, offset);

#if defined(V54_BSP)
        // offset points to start of redboot/fis directory
        // Sizeof redboot/fis directory is master->erasesize
#if defined(CONFIG_AR934x)
        if (master->erasesize == (64*1024))
            flash_reserved_offset = offset + (master->erasesize * 4) ;
        else
#endif
            flash_reserved_offset = offset + master->erasesize ;

        printk("offset=%lx flash_reserved_offset=%lx\n",
			offset, (unsigned long)flash_reserved_offset);
#endif
	printk(KERN_NOTICE "Searching for RedBoot partition table in %s at offset 0x%lx\n",
	       master->name, offset);

	ret = master->read(master, offset,
			   master->erasesize, &retlen, (void *)buf);

	if (ret)
		goto out;

	if (retlen != master->erasesize) {
		ret = -EIO;
		goto out;
	}

	numslots = (master->erasesize / sizeof(struct fis_image_desc));
	for (i = 0; i < numslots; i++) {
		if (!memcmp(buf[i].name, "FIS directory", 14)) {
			/* This is apparently the FIS directory entry for the
			 * FIS directory itself.  The FIS directory size is
			 * one erase block; if the buf[i].size field is
			 * swab32(erasesize) then we know we are looking at
			 * a byte swapped FIS directory - swap all the entries!
			 * (NOTE: this is 'size' not 'data_length'; size is
			 * the full size of the entry.)
			 */

			/* RedBoot can combine the FIS directory and
			   config partitions into a single eraseblock;
			   we assume wrong-endian if either the swapped
			   'size' matches the eraseblock size precisely,
			   or if the swapped size actually fits in an
			   eraseblock while the unswapped size doesn't. */
			if (swab32(buf[i].size) == master->erasesize ||
			    (buf[i].size > master->erasesize
			     && swab32(buf[i].size) < master->erasesize)) {
				int j;
				/* Update numslots based on actual FIS directory size */
				numslots = swab32(buf[i].size) / sizeof (struct fis_image_desc);
				for (j = 0; j < numslots; ++j) {

					/* A single 0xff denotes a deleted entry.
					 * Two of them in a row is the end of the table.
					 */
					if (buf[j].name[0] == 0xff) {
				  		if (buf[j].name[1] == 0xff) {
							break;
						} else {
							continue;
						}
					}

					/* The unsigned long fields were written with the
					 * wrong byte sex, name and pad have no byte sex.
					 */
					swab32s(&buf[j].flash_base);
					swab32s(&buf[j].mem_base);
					swab32s(&buf[j].size);
					swab32s(&buf[j].entry_point);
					swab32s(&buf[j].data_length);
					swab32s(&buf[j].desc_cksum);
					swab32s(&buf[j].file_cksum);
				}
			} else if (buf[i].size < master->erasesize) {
				/* Update numslots based on actual FIS directory size */
				numslots = buf[i].size / sizeof(struct fis_image_desc);
			}
			break;
		}
	}
	if (i == numslots) {
		/* Didn't find it */
		printk(KERN_NOTICE "No RedBoot partition table detected in %s\n",
		       master->name);
		ret = 0;
#if defined(V54_BSP) && (defined(CONFIG_AR7100) || defined(CONFIG_P10XX))
#if !defined(CONFIG_AR9100) && !defined(CONFIG_AR934x) && !defined(CONFIG_P10XX)
        extern int uboot_parts;
        if ( (uboot_fake != NULL) && uboot_parts)
#endif
        {
#if defined(CONFIG_P10XX)
            int n = uboot_fake_32m_slots;
            if (master->size == MAX_FLASH_SIZE_64M) {
                n = uboot_fake_64m_slots;
                uboot_fake = uboot_fake_64m;
            }
            memcpy(buf, uboot_fake, sizeof(struct fis_image_desc)*n);
            numslots = n;
#else
#if defined(CONFIG_AR934x)
            int n = uboot_fake_32m_slots;
            if (master->size == MAX_FLASH_SIZE_64M) {
                n = uboot_fake_64m_slots;
                uboot_fake = uboot_fake_64m;
            }
            memcpy(buf, uboot_fake, sizeof(struct fis_image_desc)*n);
            numslots = n;
#else
            int n = sizeof(uboot_fake)/sizeof(struct fis_image_desc);
            memcpy(buf, uboot_fake, sizeof(struct fis_image_desc)*n);
            numslots = n;
#endif
#endif
        }
#else
		goto out;
#endif
	}

#ifdef CONFIG_MTD_SPI_FLASH
#include "ar531x.h"
#else
#undef FAST_WRITE_256
#endif
#if defined(FAST_WRITE_256)
#include "ar531xlnx.h"
    {
        // try to detect PageWrite circuit, by setting GPIO to 0 and read again
        // i is the slot with RedBoot
        extern void spi_flashPageWrite_set(int enable);
        struct fis_image_desc *buf1;
        int rc;

        ar531x_gpio_ctrl_output(AR5315_FAST_WRITE_GPIO);
        ar531x_gpio_set(AR5315_FAST_WRITE_GPIO, 0); // set to 0 for testing
        buf1 = kmalloc(PAGE_SIZE, GFP_KERNEL);
        if ( ! buf1 ) {
            goto skip_check;
        }

        rc = master->read(master, offset,
			   PAGE_SIZE, &retlen, (void *)buf1);

        if ( rc || retlen != PAGE_SIZE ) {
            // read failed
            printk("%s: Test read failed\n", __FUNCTION__);
        } else {
#if __DUMP16
            int k;
            printk("i=%d\n", i);
            for (k=0; k<=i; k++) {
                _dump16(buf1[k].name);
            }
#endif
            if (memcmp(buf1, buf, PAGE_SIZE)) {
                // comparison failed, we have pageWrite circuit
                printk("flash page write circuit present !!!\n");
                spi_flashPageWrite_set(1);
                goto finish_check;
            }
        }
        printk("NO flash page write circuit!\n");
finish_check:
        if ( buf1 )
            kfree(buf1);
skip_check:
        ar531x_gpio_set(AR5315_FAST_WRITE_GPIO, 1); // set to 0 for testing
        ar531x_gpio_ctrl_input(AR5315_FAST_WRITE_GPIO);
        dPrintf2("%s: setting gpio %d to input\n", __FUNCTION__,
                    AR5315_FAST_WRITE_GPIO);
    }
#endif // FAST_WRITE_256

	for (i = 0; i < numslots; i++) {
		struct fis_list *new_fl, **prev;

		if (buf[i].name[0] == 0xff) {
			if (buf[i].name[1] == 0xff) {
				break;
			} else {
				continue;
			}
		}
		if (!redboot_checksum(&buf[i]))
			break;

		namelen += strlen(buf[i].name)+1;

#ifdef V54_FLASH_ROOTFS
#ifdef V54_MOUNT_FLASH_ROOTFS
        if (!memcmp(buf[i].name, "rcks_wlan.main", 15)) {
            main_desc = &buf[i];
            dPrintf("%s: main image: address = %08x, size = %08x.\n", __FUNCTION__,
                  main_desc->flash_base, main_desc->size);
        }
        else if (!memcmp(buf[i].name, "rcks_wlan.bkup", 15)) {
            bkup_desc = &buf[i];
            dPrintf("%s: bkup image: address = %08x, size = %08x.\n", __FUNCTION__,
                  bkup_desc->flash_base, bkup_desc->size);
        }
#endif

        if (fis_origin) {
            buf[i].flash_base -= fis_origin;
        } else {
#if !defined(CONFIG_MACH_AR7100)
            buf[i].flash_base &= master->size-1;
#else
#if !defined(CONFIG_AR934x)
            if (master->size > MAX_FLASH_SIZE) {
                buf[i].flash_base &= MAX_FLASH_SIZE-1;
            } else {
                buf[i].flash_base &= master->size-1;
            }
#endif
#endif
        }

		dPrintf2("%s: <%d> search FIS name <%s>\n",  __FUNCTION__, i, buf[i].name);

		dPrintf2(": is_RKS_hdr=0x%x\n",  is_RKS_hdr(master, buf[i].flash_base));
		dPrintf2(": NO_EMBEDDED_ROOTFS=%d\n",  NO_EMBEDDED_ROOTFS());
		dPrintf2(": rks_hdr.next_image=0x%x\n",  rks_hdr.next_image);
		dPrintf2(": buf[i].size=0x%x rks_hdr.bin_len=0x%x\n",  buf[i].size, rks_hdr.bin_len);
		dPrintf2(": buf[i].image_info.type=0x%x\n", buf[i].image_info.type);
		dPrintf2(": load_type=0x%x\n", load_type);

        if (is_RKS_hdr(master, buf[i].flash_base) &&
            NO_EMBEDDED_ROOTFS() &&
            rks_hdr.next_image > 0 && (rks_hdr.next_image & 0xffff) == 0 &&
            buf[i].size > rks_hdr.bin_len &&
           (buf[i].image_info.type == 1 || buf[i].image_info.type == 2) &&
           (load_type == 0 || load_type == buf[i].image_info.type))
        {
		int    index = buf[i].image_info.type-1;
		struct fis_image_desc *img = &desc[index];

#ifdef V54_MOUNT_FLASH_ROOTFS
            current_desc = &buf[i];
            boot_part_size = rks_hdr.next_image;
#endif
		load_type = buf[i].image_info.type;

		strcpy(img->name, rootfs_data[index].name);
			namelen += strlen(img->name)+1;

		img->flash_base = buf[i].flash_base + rks_hdr.next_image;
		img->size       = buf[i].size - rks_hdr.next_image;

		dPrintf2("%s: Update rootfs: name=<%s>, base=0x%lx, size=0x%lx\n",  __FUNCTION__,
		        img->name, img->flash_base, img->size);

#if 0 && defined(V54_MOUNT_FLASH_ROOTFS)
		buf[i].size = rks_hdr.next_image;
#endif
		    dPrintf2("%s: Modify images: name=<%s>, base=0x%lx, size=0x%lx\n",  __FUNCTION__,
		            buf[i].name, buf[i].flash_base, buf[i].size);

			new_fl = kmalloc(sizeof(struct fis_list), GFP_KERNEL);
			if (!new_fl) {
				ret = -ENOMEM;
				goto out;
			}
			new_fl->img = img;

			prev = &fl;
			while(*prev && (*prev)->img->flash_base < new_fl->img->flash_base)
				prev = &(*prev)->next;
			new_fl->next = *prev;
			*prev = new_fl;

			nrparts++;
        }
#ifdef V54_DUMP_INFO
        else {
            dPrintf2("%s:  %s rks_hdr next_image=0x%x size=%d bin_len=%d itype=%d, load_type=%d\n",
                __FUNCTION__,
                is_RKS_hdr(master, buf[i].flash_base) ? "" : "not" ,
                rks_hdr.next_image,
                buf[i].size, rks_hdr.bin_len,
                buf[i].image_info.type ,
                load_type );
        }
#endif
#endif

		new_fl = kmalloc(sizeof(struct fis_list), GFP_KERNEL);
		if (!new_fl) {
			ret = -ENOMEM;
			goto out;
		}
		new_fl->img = &buf[i];
#ifndef V54_FLASH_ROOTFS
                if (fis_origin) {
                        buf[i].flash_base -= fis_origin;
                } else {
#if !defined(CONFIG_MACH_AR7100)
                        buf[i].flash_base &= master->size-1;
#else
#if !defined(CONFIG_AR934x)
                    if (master->size > MAX_FLASH_SIZE) {
                        buf[i].flash_base &= MAX_FLASH_SIZE-1;
                    } else {
                        buf[i].flash_base &= master->size-1;
                    }
#endif
#endif
                }
#endif

		/* I'm sure the JFFS2 code has done me permanent damage.
		 * I now think the following is _normal_
		 */
		prev = &fl;
		while(*prev && (*prev)->img->flash_base < new_fl->img->flash_base)
			prev = &(*prev)->next;
		new_fl->next = *prev;
		*prev = new_fl;

		nrparts++;
	}

#ifdef V54_FLASH_ROOTFS
#ifdef V54_MOUNT_FLASH_ROOTFS
#if !defined(CONFIG_AR934x)
    if (!main_desc || !bkup_desc || main_desc->size != bkup_desc->size)
    {
        v54_create_ramdisk = 1;
    } else if ( current_desc )
#else
    if (!main_desc)
    {
        v54_create_ramdisk = 1;
    } else if ( current_desc )
#endif
    {
        current_desc->size = boot_part_size;
    }
#endif
#endif

#ifdef CONFIG_MTD_REDBOOT_PARTS_UNALLOCATED
	if (fl->img->flash_base) {
		nrparts++;
		nulllen = sizeof(nullstring);
	}

	for (tmp_fl = fl; tmp_fl->next; tmp_fl = tmp_fl->next) {
		if (tmp_fl->img->flash_base + tmp_fl->img->size + master->erasesize <= tmp_fl->next->img->flash_base) {
			nrparts++;
			nulllen = sizeof(nullstring);
		}
	}
#endif

#ifdef V54_BSP
    totalParts = nrparts + numParts;

#if (defined(CONFIG_MTD_END_RESERVED) || defined(CONFIG_MACH_AR7100)) && !defined(CONFIG_P10XX)
#if !defined(CONFIG_AR934x)
	// add a partition for reserved space
#define RESERVED_NAME	"Board Data"
	namelen += strlen(RESERVED_NAME)+1;
	totalParts++;
#endif
#endif

    dPrintf2("%s: Total partition=%d\n", __FUNCTION__, totalParts);

	parts = kmalloc(sizeof(*parts)*totalParts + nulllen + namelen, GFP_KERNEL);
#else
	parts = kzalloc(sizeof(*parts)*nrparts + nulllen + namelen, GFP_KERNEL);
#endif
	if (!parts) {
		ret = -ENOMEM;
		goto out;
	}

#ifdef V54_BSP
	memset(parts, 0, sizeof(*parts)*totalParts + nulllen + namelen);
    nullname = (char *)&parts[totalParts];
#else
	nullname = (char *)&parts[nrparts];
#endif

#ifdef CONFIG_MTD_REDBOOT_PARTS_UNALLOCATED
	if (nulllen > 0) {
		strcpy(nullname, nullstring);
	}
#endif
	names = nullname + nulllen;

	i=0;

#ifdef CONFIG_MTD_REDBOOT_PARTS_UNALLOCATED
	if (fl->img->flash_base) {
	       parts[0].name = nullname;
	       parts[0].size = fl->img->flash_base;
	       parts[0].offset = 0;
		i++;
	}
#endif
	for ( ; i<nrparts; i++) {
		parts[i].size = fl->img->size;
		parts[i].offset = fl->img->flash_base;
		parts[i].name = names;

		strcpy(names, fl->img->name);
#ifdef V54_FLASH_ROOTFS
        for (k = 0; k < 2; k++)
        {
            if (memcmp(names, rootfs_data[k].name, strlen(rootfs_data[k].name)) == 0)
            {
		    rootfs_data[k].vaild = 1;
		    rootfs_data[k].block = i + 1;
#ifdef V54_MOUNT_FLASH_ROOTFS
                if (v54_create_ramdisk)
#endif
                {
                    strcpy(names, "v54_$ramdisk$");
                }
		    dPrintf("%s: rootfs partition, name=%s, type=%d, block=%d\n",
		            __FUNCTION__, names, k+1, rootfs_data[k].block);
			    parts[i].mask_flags = MTD_WRITEABLE;
			    break;
		}
        }
#endif
#ifdef CONFIG_MTD_REDBOOT_PARTS_READONLY
		if (!memcmp(names, "RedBoot", 8) ||
				!memcmp(names, "RedBoot config", 15) ||
				!memcmp(names, "FIS directory", 14)) {
			parts[i].mask_flags = MTD_WRITEABLE;
		}
#endif
		names += strlen(names)+1;

#ifdef CONFIG_MTD_REDBOOT_PARTS_UNALLOCATED
		if(fl->next && fl->img->flash_base + fl->img->size + master->erasesize <= fl->next->img->flash_base) {
			i++;
			parts[i].offset = parts[i-1].size + parts[i-1].offset;
			parts[i].size = fl->next->img->flash_base - parts[i].offset;
			parts[i].name = nullname;
		}
#endif
		tmp_fl = fl;
		fl = fl->next;
		kfree(tmp_fl);
	}

#if defined(V54_BSP) && (defined(CONFIG_MTD_END_RESERVED) || defined(CONFIG_MACH_AR7100)) && !defined(CONFIG_AR934x) && !defined(CONFIG_P10XX)
	// add a partition for reserved space
		strcpy(names, RESERVED_NAME);
#if !defined(CONFIG_MACH_AR7100) || defined(CONFIG_AR9100)
	{
		parts[nrparts].size = CONFIG_MTD_END_RESERVED ;
		parts[nrparts].offset = master->size - CONFIG_MTD_END_RESERVED;
		parts[nrparts].name = names;
        boarddata_part = &parts[nrparts];
    }
#else
    {
        parts[nrparts].size = master->erasesize ;
        parts[nrparts].offset = master->size - master->erasesize;
        parts[nrparts].name = names;;
        boarddata_part = &parts[nrparts];
        if (master->size > MAX_FLASH_SIZE) {
            int i;
            unsigned int partSize;

            /* board data offset */
            parts[nrparts].offset = MAX_FLASH_SIZE - master->erasesize;

            /* partitions in 2nd flash */
            partSize = MAX_FLASH_SIZE / numParts;
            for (i=0; i<numParts; i++) {
		nrparts++;
                parts[nrparts].size = partSize ;
                parts[nrparts].offset = MAX_FLASH_SIZE+(i * partSize);
                parts[nrparts].name = &stg_name[i][0];
            }
        }
    }
#endif
#endif

#ifdef V54_FLASH_ROOTFS
    v54_rootfs_block = 0;
    if (load_type > 0 && rootfs_data[load_type-1].vaild)
    {
        v54_rootfs_block = rootfs_data[load_type-1].block;
    }

    dPrintf("%s: flash root fs block = %d\n",  __FUNCTION__, v54_rootfs_block);
#endif

#ifdef V54_BSP
	ret = totalParts;
#else
	ret = nrparts;
#endif
	*pparts = parts;
 out:
	while (fl) {
		struct fis_list *old = fl;
		fl = fl->next;
		kfree(old);
	}
	vfree(buf);
	return ret;
}

static struct mtd_part_parser redboot_parser = {
	.owner = THIS_MODULE,
	.parse_fn = parse_redboot_partitions,
	.name = "RedBoot",
};

static int __init redboot_parser_init(void)
{
	return register_mtd_parser(&redboot_parser);
}

static void __exit redboot_parser_exit(void)
{
	deregister_mtd_parser(&redboot_parser);
}

module_init(redboot_parser_init);
module_exit(redboot_parser_exit);

#if defined(V54_BSP)
EXPORT_SYMBOL(flash_reserved_offset);
/* only for serial flash */

#ifdef CONFIG_AR5315
extern void spi_flashPageWrite_set(int enable);
extern int spi_flashPageWrite_get(void);

EXPORT_SYMBOL(spi_flashPageWrite_set);
EXPORT_SYMBOL(spi_flashPageWrite_get);
#endif
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Woodhouse <dwmw2@infradead.org>");
MODULE_DESCRIPTION("Parsing code for RedBoot Flash Image System (FIS) tables");
