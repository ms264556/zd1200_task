/*
 *
 * Copyright (c) 2006 Ruckus Wireless, Inc.
 * Copyright (c) 2004,2005 Video54 Technologies, Inc.
 * All rights reserved.
 *
 */
#include <linux/version.h>



#include <linux/kernel.h>
#include "v54_himem.h"


#include <linux/string.h>
#include "nar5520_bsp.h"
#include <linux/time.h>



#ifndef NULL
#define NULL 0
#endif




#if defined(__KERNEL__)
#define DO_PRINT    printk
#define DO_PRINT2(...)
#else
#define DO_PRINT    printf
#define DO_PRINT2(...)
#endif

static struct v54_high_mem *v54HighMem = NULL;


#define HIMEM_CHECK_MINIBOOT_MAGIC(himem) \
    (himem->miniboot.magic == V54_MINIBOOT_MAGIC)

unsigned long _mem_top = 0;
unsigned long v54_himem_reserved = V54_HIMEM_RESERVED;


char * v54_mem_top(void)
{
    return (char *)_mem_top;
}

unsigned long v54_himem_buf_start(void)
{
    return _mem_top ? (_mem_top - v54_himem_reserved) : 0;
}

unsigned long v54_himem_buf_end(void)
{
    return _mem_top ? (_mem_top - MEM_TOP_OFFSET - MAX_CFDATA_SIZE) : 0;
}

unsigned long v54_himem_oops_end(void)
{

    return v54_himem_region_end(V54_HIMEM_REGION_LOWEST);
}

unsigned long v54_get_himem_reserved(void)
{
    return v54_himem_reserved;
}

unsigned int v54_himem_region_count(void)
{
  return V54_HIMEM_REGIONS;
}

unsigned long v54_himem_region_start(int i)
{
  if (i < V54_HIMEM_REGIONS ) {
    unsigned long region_size = ((v54_himem_buf_end() - v54_himem_buf_start())/V54_HIMEM_REGIONS);
    region_size &= 0xfffffffc;
    return v54_himem_buf_start() + region_size * i;
  }
  return (unsigned long)NULL;
}

unsigned long v54_himem_region_end(int i)
{
  if (i < V54_HIMEM_REGIONS-1 ) {
    unsigned long region_size = ((v54_himem_buf_end() - v54_himem_buf_start())/V54_HIMEM_REGIONS);
    region_size &= 0xfffffffc;
    return v54_himem_buf_start() + region_size * (i+1);
  }
  if (i == V54_HIMEM_REGIONS-1 )
    return (unsigned long) v54_himem_buf_end();
  return (unsigned long)NULL;
}

unsigned long v54_himem_cfdata_start(void)
{
    return _mem_top ? (_mem_top - MEM_TOP_OFFSET - MAX_CFDATA_SIZE) : 0;
}

unsigned long v54_himem_cfdata_end(void)
{
    return v54_himem_cfdata_start() + MAX_CFDATA_SIZE;
}

int v54_himem_cf_region_count(void)
{
    return V54_CFDATA_REGIONS;
}

unsigned long v54_himem_cf_region_start(int i)
{
    int region_count = v54_himem_cf_region_count();

    if (region_count > 0 && i < region_count) {
        unsigned long region_size = ((v54_himem_cfdata_end() - v54_himem_cfdata_start())/region_count);
        region_size &= 0xfffffffc;
        return v54_himem_cfdata_start() + region_size * i;
    }
    return (unsigned long)NULL;
}

unsigned long v54_himem_cf_region_end(int i)
{
    int region_count = v54_himem_cf_region_count();

    if (region_count > 0 && i < region_count - 1) {
        unsigned long region_size = ((v54_himem_cfdata_end() - v54_himem_cfdata_start())/region_count);
        region_size &= 0xfffffffc;
        return v54_himem_cfdata_start() + region_size * (i+1);
    }
    if (i == region_count-1)
        return (unsigned long)v54_himem_cfdata_end();
    return (unsigned long)NULL;
}

static int
__high_mem_init(char * mem_top)
{
    if( ! mem_top ) {
        DO_PRINT("high_mem_init: mem_top=0x%lx\n", (unsigned long) mem_top);
	return 0;
    }
    v54HighMem = (struct v54_high_mem *)(mem_top - MEM_TOP_OFFSET);



    (void) memset((char *)v54HighMem, 0,  sizeof(struct v54_high_mem));

    v54HighMem->magic = V54_BOOT_MAGIC_VAL;
    v54HighMem->magic2 = RUCKUS_BOOT_MAGIC_VAL;

    v54HighMem->flag_reset = 0;
    v54HighMem->flag_factory_def = 0;
    v54HighMem->flag_sw2_reset = 0;
    v54HighMem->flag_sw2_event = 0;
    v54HighMem->flag_post_fail = 0;
    v54HighMem->flag_image_type = 0;
    v54HighMem->flag_recovery_boot = 0;

    v54HighMem->boot_time = 0;

    v54HighMem->reboot_cnt = 0;
    v54HighMem->total_boot = 0;
    v54HighMem->reboot_reason = 0;

    v54HighMem->image_index = -1;

    v54HighMem->image_name[0] = '\0';
    v54HighMem->boot_version[0] = '\0';

    v54HighMem->boot.magic = 0;
    v54HighMem->boot.script[0] = '\0';

    v54HighMem->pci_wlan_reboot = 0;

    return 1;
}

static int
high_mem_miniboot_init(char * mem_top)
{
    v54HighMem = (struct v54_high_mem *)(mem_top - MEM_TOP_OFFSET);
    v54HighMem->miniboot.magic = V54_MINIBOOT_MAGIC;
    v54HighMem->miniboot.reboot_cnt = 0;

    return 1;
}

static int
high_mem_init(char * mem_top)
{
    if ( mem_top == NULL ) {
        mem_top = V54_MEM_TOP();
    }
	if( ! mem_top ) {
        DO_PRINT("high_mem_init: mem_top=0x%lx\n", (unsigned long) mem_top);
		return 0;
	}

    v54HighMem = (struct v54_high_mem *)(mem_top - MEM_TOP_OFFSET);
    if ( HIMEM_CHECK_MINIBOOT_MAGIC(v54HighMem) ) {
        DO_PRINT2("v54HighMem miniboot already initialized\n");
    } else {
        high_mem_miniboot_init(mem_top);
    }
    if ( HIMEM_CHECK_MAGIC(v54HighMem) ) {
        DO_PRINT2("v54HighMem already initialized\n");
        return 1;
    }

	return __high_mem_init(mem_top);
}

static void
__set_boot_version(char * boot_version)
{
	int len;
    len = (strlen(boot_version) > BOOT_VERSION_BUFSIZE) ?
                 BOOT_VERSION_BUFSIZE : strlen(boot_version);
    memcpy(v54HighMem->boot_version, boot_version, len);
    v54HighMem->boot_version[len] = '\0';
	return;
}


void
v54_himem_reset(char * boot_version, char* mem_top)
{
	if ( ! __high_mem_init(mem_top) ) {
		DO_PRINT("Unitialized HighMem\n");
		return;
	}

	__set_boot_version(boot_version);
	return;
}



#define REASON_UNKNOWN  "unknown"
#define REASON_HARD "reset button"
#define REASON_SOFT "user reboot"
#define REASON_APPL "application reboot"
#define REASON_OOPS  "kernel oops"
#define REASON_PANIC "kernel panic"
#define REASON_WATCHDOG  "watchdog timeout"
#define REASON_HW_WDT   "hardware watchdog timeout"
#define REASON_VM   "voltage monitor"
#define REASON_DATA_BUS  "data bus error"
#define REASON_PCI_WLAN  "pci wlan"
#define REASON_PROB_DETECT  "problem detected"
#define REASON_TARGET_FAIL_DETECT "target fail detected"
#define REASON_PWR_CYCLE_DETECT "power cycle detected"
static inline char *
_REASON_STR(u_int32_t reason)
{
    switch (reason) {
    case REBOOT_HARD:
        return REASON_HARD;
        break;
    case REBOOT_SOFT:
        return REASON_SOFT;
        break;
    case REBOOT_OOPS:
        return REASON_OOPS;
        break;
    case REBOOT_PANIC:
        return REASON_PANIC;
        break;
    case REBOOT_WATCHDOG:
        return REASON_WATCHDOG;
        break;
    case REBOOT_APPL:
        return REASON_APPL;
        break;
    case REBOOT_DATA_BUS_ERROR:
        return REASON_DATA_BUS;
        break;
    case REBOOT_HW_WDT:
        return REASON_HW_WDT;
        break;
    case REBOOT_VM:
        return REASON_VM;
        break;
    case REBOOT_PCI_WLAN_ERROR:
        return REASON_PCI_WLAN;
        break;
    case REBOOT_PROB_DETECTED:
        return REASON_PROB_DETECT;
        break;
    case REBOOT_TARGET_FAILURE:
        return REASON_TARGET_FAIL_DETECT;
        break;
    case REBOOT_PWR_CYCLE:
        return REASON_PWR_CYCLE_DETECT;
        break;
    default:
        return REASON_UNKNOWN;
        break;
    }
    return REASON_UNKNOWN;
}


static char * reboot_dt_str(unsigned long reboottime)
{
    static char buf[30];
    struct tm dt;

    time_to_tm(reboottime, 0, &dt);
    sprintf(buf, "UTC %4d-%02d-%02d %02d:%02d:%02d",
      (int)dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
    return buf;
}




#define WRITE_PAD   80
static int
_increment_check_size(char **pp, int *len, int size)
{
    int l1 = strlen(*pp);
    *len += l1;
    *pp += l1;
    return ((*len > (size - WRITE_PAD))? 1 : 0);
}


#define SPRINTF sprintf
static int
_himem_dump(char* mem_top, char *buf, int size)
{
    int len = 0;
    char *p = buf;

    if ( buf == NULL || size - WRITE_PAD < 0 )
        return 0;

	if ( ! high_mem_init(mem_top) ) {
		SPRINTF(buf, "Unitialized HighMem, mem_top=0x%08lx\n",
                 (unsigned long) mem_top);
		return (strlen(buf));
	}

    SPRINTF(p, "mem_top = 0x%08lx\n", (unsigned long) mem_top);
    if ( _increment_check_size(&p, &len, size) )
            return len;

    SPRINTF(p, "High Mem (0x%lX) size=%d\n", (unsigned long)v54HighMem
                    , sizeof(struct v54_high_mem));
    if ( _increment_check_size(&p, &len, size) )
            return len;


    SPRINTF(p, "  magic:      0x%lX 0x%lX\n",
            (unsigned long) v54HighMem->magic
            , (unsigned long) v54HighMem->magic2
            );
    if ( _increment_check_size(&p, &len, size) )
            return len;

    SPRINTF(p, "  reset:      %u", v54HighMem->flag_reset);
    if ( _increment_check_size(&p, &len, size) )
            return len;

    SPRINTF(p, "  factory:    %u", v54HighMem->flag_factory_def);
    if ( _increment_check_size(&p, &len, size) )
            return len;

    SPRINTF(p, "  sw2_reset:      %u", v54HighMem->flag_sw2_reset);
    if ( _increment_check_size(&p, &len, size) )
            return len;

    SPRINTF(p, "  sw2_event:    %u", v54HighMem->flag_sw2_event);
    if ( _increment_check_size(&p, &len, size) )
            return len;

    SPRINTF(p, "  post_fail:  %u", v54HighMem->flag_post_fail);
    if ( _increment_check_size(&p, &len, size) )
            return len;

    SPRINTF(p, "  recovery:   %u\n", v54HighMem->flag_recovery_boot);
    if ( _increment_check_size(&p, &len, size) )
            return len;

    SPRINTF(p, "  reboot_cnt: %lu", (unsigned long) v54HighMem->reboot_cnt);
    if ( _increment_check_size(&p, &len, size) )
            return len;

    SPRINTF(p, "  total_boot: %lu\n", (unsigned long) v54HighMem->total_boot);
    if ( _increment_check_size(&p, &len, size) )
            return len;

    SPRINTF(p, "  reboot_reason: %s\n", _REASON_STR(v54HighMem->reboot_reason));
    if ( _increment_check_size(&p, &len, size) )
            return len;


    SPRINTF(p, "  reboot_time: %s\n", reboot_dt_str(v54HighMem->reboot_time));
    if ( _increment_check_size(&p, &len, size) )
            return len;


    SPRINTF(p, "  type:       %u", v54HighMem->flag_image_type);
    if ( _increment_check_size(&p, &len, size) )
            return len;

    SPRINTF(p, "  index:      %d", v54HighMem->image_index);
    if ( _increment_check_size(&p, &len, size) )
            return len;


    v54HighMem->image_name[15] = '\0';
    SPRINTF(p, "  fis.image: %s\n", v54HighMem->image_name);
    if ( _increment_check_size(&p, &len, size) )
            return len;


    v54HighMem->boot_version[BOOT_VERSION_BUFSIZE] = '\0';
    SPRINTF(p, "  BootRom:\n  %s\n", v54HighMem->boot_version);
    if ( _increment_check_size(&p, &len, size) )
            return len;
    SPRINTF(p, "  pci reboot: %d\n", v54HighMem->pci_wlan_reboot);
    if ( _increment_check_size(&p, &len, size) )
            return len;
    SPRINTF(p, "  miniboot:\n  magic:      0x%X\n  reboot_cnt: %u\n",
            v54HighMem->miniboot.magic, v54HighMem->miniboot.reboot_cnt);
    if ( _increment_check_size(&p, &len, size) )
            return len;

    if ( v54HighMem->boot.magic == V54_BOOT_SCRIPT_MAGIC ) {
        v54HighMem->boot.script[V54_BOOT_SCRIPT_SIZE] = '\0';
        SPRINTF(p, "\n  Bootline:\n%s\n", v54HighMem->boot.script);
    } else {
        SPRINTF(p, "\n  No Bootline\n");
    }
    if ( _increment_check_size(&p, &len, size) )
            return len;


    return len;
}

static int
_himem_dump_seq(char* mem_top, struct seq_file *m)
{
	if ( ! high_mem_init(mem_top) ) {
		seq_printf(m, "Unitialized HighMem, mem_top=0x%08lx\n",
                 (unsigned long) mem_top);
		return 0;
	}

    seq_printf(m, "mem_top = 0x%08lx\n", (unsigned long) mem_top);



    seq_printf(m, "High Mem (0x%lX) size=%d\n", (unsigned long)v54HighMem
                    , sizeof(struct v54_high_mem));




    seq_printf(m, "  magic:      0x%lX 0x%lX\n",
            (unsigned long) v54HighMem->magic
            , (unsigned long) v54HighMem->magic2
            );


    seq_printf(m, "  reset:      %u", v54HighMem->flag_reset);



    seq_printf(m, "  factory:    %u", v54HighMem->flag_factory_def);



    seq_printf(m, "  sw2_reset:      %u", v54HighMem->flag_sw2_reset);



    seq_printf(m, "  sw2_event:    %u", v54HighMem->flag_sw2_event);



    seq_printf(m, "  post_fail:  %u", v54HighMem->flag_post_fail);



    seq_printf(m, "  recovery:   %u\n", v54HighMem->flag_recovery_boot);



    seq_printf(m, "  reboot_cnt: %lu", (unsigned long) v54HighMem->reboot_cnt);



    seq_printf(m, "  total_boot: %lu\n", (unsigned long) v54HighMem->total_boot);



    seq_printf(m, "  reboot_reason: %s\n", _REASON_STR(v54HighMem->reboot_reason));



    seq_printf(m, "  reboot_time: %s\n", reboot_dt_str(v54HighMem->reboot_time));



    seq_printf(m, "  type:       %u", v54HighMem->flag_image_type);



    seq_printf(m, "  index:      %d", v54HighMem->image_index);




    v54HighMem->image_name[15] = '\0';
    seq_printf(m, "  fis.image: %s\n", v54HighMem->image_name);





    v54HighMem->boot_version[BOOT_VERSION_BUFSIZE] = '\0';
    seq_printf(m, "  BootRom:\n  %s\n", v54HighMem->boot_version);


    seq_printf(m, "  pci reboot: %d\n", v54HighMem->pci_wlan_reboot);


    seq_printf(m, "  miniboot:\n  magic:      0x%X\n  reboot_cnt: %u\n",
            v54HighMem->miniboot.magic, v54HighMem->miniboot.reboot_cnt);



    if ( v54HighMem->boot.magic == V54_BOOT_SCRIPT_MAGIC ) {
        v54HighMem->boot.script[V54_BOOT_SCRIPT_SIZE] = '\0';
        seq_printf(m, "\n  Bootline:\n%s\n", v54HighMem->boot.script);
    } else {
        seq_printf(m, "\n  No Bootline\n");
    }

    return 0;
}

int
v54_himem_boot_version(struct seq_file *m)
{
    char* mem_top;
    int len = 0;

    mem_top = V54_MEM_TOP();
	if ( ! high_mem_init(mem_top) ) {
		DO_PRINT("Unitialized HighMem\n");
		return 0;
	}

	char buf[BOOT_VERSION_BUFSIZE];
    (void) memcpy(buf, v54HighMem->boot_version, sizeof(buf));
    buf[BOOT_VERSION_BUFSIZE - 1] = '\0';
	seq_printf(m, "%s", buf);

    return len;
}

int
v54_himem_dump_buf(struct seq_file *m)
{
    char* mem_top;

    mem_top = V54_MEM_TOP();

    return (_himem_dump_seq(mem_top, m));
}



void
v54_set_pci_reboot(char count)
{
    if ( v54HighMem == NULL ) {
        high_mem_init(NULL);
    }
    if ( v54HighMem != NULL ) {
        v54HighMem->pci_wlan_reboot = count;
    }
    return;
}

char
v54_get_pci_reboot(void)
{
    if ( v54HighMem == NULL ) {
        high_mem_init(NULL);
    }
    if ( v54HighMem != NULL ) {
        return v54HighMem->pci_wlan_reboot;
    }
    return 0;
}

void
v54_reboot_reason(u_int32_t reason)
{
    if ( v54HighMem == NULL ) {
        high_mem_init(NULL);
    }
    if ( v54HighMem != NULL ) {
        v54HighMem->reboot_reason = reason;
    }


    v54HighMem->reboot_time = get_seconds();
    if (oops_occurred)
        return;
    nar5520_save_himem();


    return;
}

int
v54_himem_reason_dump(struct seq_file *m)
{

    char* mem_top = V54_MEM_TOP();

	if ( ! high_mem_init(mem_top) ) {
		seq_printf(m, "Unitialized HighMem, mem_top=0x%08lx\n",
                 (unsigned long) mem_top);
		return 0;
	}


    seq_printf(m, "%s\nreboot_time: %s\n",  _REASON_STR(v54HighMem->reboot_reason),
      reboot_dt_str(v54HighMem->reboot_time));


    return 0;
}

#if 0
#define dPrintf printk
#else
#define dPrintf(...)
#endif
int
v54_himem_oops_dump(struct seq_file *m)
{
    struct _himem_oops * oops_hdr = (struct _himem_oops *)v54_himem_region_start(0);
    char* himem_buf;

    if ( oops_hdr->magic != HIMEM_OOPS_MAGIC ) {
        dPrintf("---> %s: No valid data @ %p [%d]: bad magic %lx != %x\n",
            __FUNCTION__, oops_hdr, MAX_OOPS_SIZE,
            oops_hdr->magic, HIMEM_OOPS_MAGIC);
        return 0;
    }

    himem_buf = (char *)(oops_hdr) + sizeof(struct _himem_oops);
    dPrintf("---> %s: himem_buf=%lx\n", __FUNCTION__,
        (unsigned long) himem_buf);

    if ( 1 ) {

        seq_printf(m, "0x%p[%lu]: Image: type=%d index=%d total_boot=%d\n"
                    , oops_hdr
                    , oops_hdr->len
                    , oops_hdr->image_type
                    , oops_hdr->image_index
                    , (int)oops_hdr->total_boot
                    );
    }

    if ( ! oops_hdr->used || (oops_hdr->len <= 0) ) {

        return 0;
    }


    if ( oops_hdr->len > MAX_OOPS_SIZE ) {

        seq_printf(m, "length=%d > %d\n", (int)oops_hdr->len, (int)MAX_OOPS_SIZE);
        return 0;
    }

    seq_write(m, himem_buf, oops_hdr->len);

    return 0;
}


void
v54_himem_oops_init(void)
{
    char * mem_top = V54_MEM_TOP();
    struct _himem_oops * oops_hdr = (struct _himem_oops *)v54_himem_region_start(0);


    if ( ! high_mem_init(mem_top) ) {
		DO_PRINT("Unitialized HighMem\n");
		return;
    }
    oops_hdr->magic = HIMEM_OOPS_MAGIC;

    oops_hdr->used = 0;
    oops_hdr->len = 0;
    oops_hdr->image_type = v54HighMem->flag_image_type;
    oops_hdr->image_index = v54HighMem->image_index;
    oops_hdr->total_boot = v54HighMem->total_boot;
    printk("--> initializing oops_hdr(0x%p)[size=%d]: total_boot=%lu Image: type=%d index=%d\n"
                    , oops_hdr
                    , MAX_OOPS_SIZE
                    , oops_hdr->total_boot
                    , oops_hdr->image_type
                    , oops_hdr->image_index
                    );
    return;
}

void
v54_himem_dump(char* mem_top)
{
#define DUMP_BUFSIZE    4096
    char  buf[DUMP_BUFSIZE];
    int len;

    len = _himem_dump(mem_top, buf, DUMP_BUFSIZE);

    DO_PRINT("%s", buf);
    return;
}


u_int32_t
v54_himem_get_reboot_cnt(char* mem_top)
{

	if ( ! high_mem_init(mem_top) ) {
		DO_PRINT("Unitialized HighMem\n");
		return 0;
	}

	return v54HighMem->reboot_cnt;

}

u_int32_t
v54_himem_set_reboot_cnt(char *mem_top, u_int32_t count)
{

    if ( mem_top == NULL ) {
        mem_top = V54_MEM_TOP();
    }



	if ( ! high_mem_init(mem_top) ) {
		DO_PRINT("Unitialized HighMem\n");
		return 0;
	}

	v54HighMem->reboot_cnt = count;
    return count;
}


u_int32_t
v54_himem_get_total_boot(void)
{
    char * mem_top = V54_MEM_TOP();


    if ( ! high_mem_init(mem_top) ) {
            DO_PRINT("Unitialized HighMem\n");
            return 0;
    }
    return v54HighMem->total_boot;
}

struct v54_high_mem *
v54_himem_data(void)
{
    if ( v54HighMem == NULL ) {
        high_mem_init(NULL);
    }
    return v54HighMem;
}

int
v54_himem_get_factory(void)
{
    return v54HighMem->flag_factory_def;
}

int
v54_himem_get_sw2_event(void)
{
    return v54HighMem->flag_sw2_event;
}



u_int32_t
v54_himem_get_image_type(void)
{
    char * mem_top = V54_MEM_TOP();


    if ( ! high_mem_init(mem_top) ) {
            DO_PRINT("Unitialized HighMem\n");
            return 0;
    }
    return v54HighMem->flag_image_type;
}


void
v54_himem_set_image_name(char* mem_top, char * name, int index)
{

	if ( ! high_mem_init(mem_top) ) {
		DO_PRINT("Unitialized HighMem\n");
		return;
	}

    if ( name != NULL ) {
        memcpy(v54HighMem->image_name, name, 16);
    } else {
        v54HighMem->image_name[0] = '\0';
    }
	v54HighMem->image_index = index;
	return ;
}
void
v54_himem_set_image_type(char* mem_top, u_int32_t image_type)
{

	if ( ! high_mem_init(mem_top) ) {
		DO_PRINT("Unitialized HighMem\n");
		return;
	}

	v54HighMem->flag_image_type = image_type & 0x3;
	return ;
}



void
v54_himem_set_recovery_boot(char* mem_top, int yes_no)
{

	if ( ! high_mem_init(mem_top) ) {
		DO_PRINT("Unitialized HighMem\n");
		return;
	}

	v54HighMem->flag_recovery_boot = (yes_no) ? 1:0;
	return;
}

void
v54_himem_set_factory(char* mem_top, int value)
{

	if ( ! high_mem_init(mem_top) ) {
		DO_PRINT("Unitialized HighMem\n");
		return;
	}

	v54HighMem->flag_factory_def = (value > 2) ? 0 :value;


	nar5520_save_himem();


	return;
}

void
v54_himem_set_sw2(char* mem_top, int value)
{

	if ( ! high_mem_init(mem_top) ) {
		DO_PRINT("Unitialized HighMem\n");
		return;
	}

	v54HighMem->flag_sw2_event = (value > 2) ? 0 :value;

	return;
}



int
v54_himem_bootline(char * mem_top, u_int32_t op, char *bootline, u_int32_t size)
{
    int len = size;


    if ( (op != V54_BOOTLINE_CLR) && (len == 0 || bootline == NULL) ) {
        DO_PRINT("%s: parameter(s) error: bootline=%p size=%d\n",
                __FUNCTION__, bootline, len);
        return -1;
    }

	if ( ! high_mem_init(mem_top) ) {
		DO_PRINT("Unitialized HighMem\n");
		return -2;
	}
    if ( len > V54_BOOT_SCRIPT_SIZE ) {
        len = V54_BOOT_SCRIPT_SIZE;
    }

    switch ( op ) {
    case V54_BOOTLINE_CLR:
        v54HighMem->boot.magic = 0;
        v54HighMem->boot.script[0] = '\0';
        break;
    case V54_BOOTLINE_GET:
        if ( v54HighMem->boot.magic == V54_BOOT_SCRIPT_MAGIC ) {
            memcpy (bootline, v54HighMem->boot.script, len);
            bootline[len] = '\0';

        } else {
            return -3;
        }
        break;
    case V54_BOOTLINE_SET:
        v54HighMem->boot.magic = V54_BOOT_SCRIPT_MAGIC;
        memcpy (v54HighMem->boot.script, bootline, len);
        v54HighMem->boot.script[len] = '\0';
        break;
    default:
        return -4;
        break;
    }

    return 0;


}
