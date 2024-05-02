#ifndef __V54_HIMEM_H__
#define __V54_HIMEM_H__


#include <linux/kernel.h>


#ifdef __KERNEL__
#include <linux/seq_file.h>
#endif

#ifndef NULL
#define NULL 0
#endif

extern unsigned long _mem_top;
extern unsigned long v54_himem_reserved;



extern char * v54_mem_top(void);
#define V54_MEM_TOP v54_mem_top




extern unsigned int v54_himem_region_count(void);
extern unsigned long v54_himem_region_start(int);
extern unsigned long v54_himem_region_end(int);



#define V54_HIMEM_RESERVED 0x10000
#define V54_HIMEM_REGIONS 1


#define V54_HIMEM_REGION_LOWEST 0
#define MEM_TOP_OFFSET  0x400


#define MAX_CFDATA_SIZE     0
#define V54_CFDATA_REGIONS  0


#define MAX_OOPS_SIZE   0x1400




struct v54_high_mem {
    u_int32_t   magic;
#define V54_BOOT_MAGIC_VAL	0x54545454

    u_int32_t   magic2;
#define RUCKUS_BOOT_MAGIC_VAL	0x52434B53
    u_int32_t   flag_reset:1;
#define FACTORY_DEFAULTS_RESET  1
#define FACTORY_DEFAULTS_UPDATE 2
    u_int32_t   flag_factory_def:2;
    u_int32_t   flag_recovery_boot:1;
    u_int32_t   flag_post_fail:1;
#define V54_IMG_MAIN    1
#define V54_IMG_BKUP    2
#define V54_IMG_OTHER   0
    u_int32_t   flag_image_type:2;
    u_int32_t   flag_sw2_reset:1;
#define SW2_BUTTON_PRESSED  1
#define SW2_BUTTON_HELD     2
    u_int32_t   flag_sw2_event:2;
    u_int32_t   reboot_cnt;
    u_int32_t   total_boot;
	u_int32_t	boot_time;
	int			image_index;


#define REBOOT_UNKNOWN  0
#define REBOOT_WATCHDOG 1
#define REBOOT_OOPS     2
#define REBOOT_PANIC    4
#define REBOOT_HARD     8
#define REBOOT_HW_WDT   10
#define REBOOT_VM       11
#define REBOOT_PROB_DETECTED 12
#define REBOOT_TARGET_FAILURE 13
#define REBOOT_SOFT    16
#define REBOOT_APPL    17
#define REBOOT_DATA_BUS_ERROR 18
#define REBOOT_PCI_WLAN_ERROR 19
#define REBOOT_PWR_CYCLE      20

    u_int32_t   reboot_reason;

	char		image_name[16];
#define BOOT_VERSION_BUFSIZE    63
    char        boot_version[BOOT_VERSION_BUFSIZE+1];

    char        pci_wlan_reboot;
    char        pad[15];




#define V54_BOOT_SCRIPT_MAGIC   0x424F4F54
#define V54_BOOT_SCRIPT_SIZE    247
#define V54_BOOTLINE_CLR  0
#define V54_BOOTLINE_GET  1
#define V54_BOOTLINE_SET  2
    struct {
        u_int32_t magic;
        u_int32_t crc;
        char    script[V54_BOOT_SCRIPT_SIZE+1];
    } boot;

#define V54_MINIBOOT_MAGIC   0x6d696e69
    struct {
        u_int32_t magic;
        u_int32_t reboot_cnt;
    } miniboot;

    unsigned long     reboot_time;

};




enum {
    HIMEM_REBOOT_CNT = 1,
    HIMEM_TOTAL_BOOT = 2,
    HIMEM_REBOOT_REASON = 3,
};

#define HIMEM_CHECK_MAGIC(himem) \
    ((himem)->magic == V54_BOOT_MAGIC_VAL && (himem)->magic2 == RUCKUS_BOOT_MAGIC_VAL)


extern void v54_himem_set_boot(char *boot_version, char *mem_top);
extern void v54_himem_dump(char *mem_top);
extern u_int32_t v54_himem_get_reboot_cnt(char* mem_top);
extern u_int32_t v54_himem_set_reboot_cnt(char* mem_top, u_int32_t count);


u_int32_t v54_himem_get_image_type(void);


extern u_int32_t v54_himem_get_total_boot(void);

#ifdef __KERNEL__
extern int v54_himem_dump_buf(struct seq_file *m);
extern int v54_himem_reason_dump(struct seq_file *m);
#endif
extern void v54_himem_set_image_name(char* mem_top, char * name, int indx);
extern void v54_himem_set_image_type(char* mem_top, u_int32_t image_type);
extern void v54_himem_set_recovery_boot(char* mem_top, int yes_no);

extern struct v54_high_mem *v54_himem_data(void);
#ifdef __KERNEL__
extern int v54_himem_dump_buf(struct seq_file *m);
extern int v54_himem_oops_dump(struct seq_file *m);
extern int v54_himem_boot_version(struct seq_file *m);
#endif
extern int v54_himem_get_factory(void);

extern void v54_himem_oops_init(void);

extern void v54_himem_set_factory(char * mem_top, int value);
#define V54_HIMEM_SET_FACTORY(value)   v54_himem_set_factory(NULL, value)

extern int  v54_himem_get_sw2_event(void);
extern void v54_himem_set_sw2(char * mem_top, int value);
#define V54_HIMEM_SET_SW2(value)   v54_himem_set_sw2(NULL, value)

extern void v54_himem_reset(char * boot_version, char* mem_top);

extern int v54_himem_bootline(char * mem_top, u_int32_t op, char *bootline, u_int32_t size);


extern void v54_reboot_reason(u_int32_t reason);
extern void v54_set_pci_reboot(char count);
extern char v54_get_pci_reboot(void);


#endif
