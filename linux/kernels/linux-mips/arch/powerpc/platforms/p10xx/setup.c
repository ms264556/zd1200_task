//#include <linux/config.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/serial.h>
#include <linux/console.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/pci.h>

//#include <asm/reboot.h>
#include <asm/io.h>
#include <asm/time.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
//#include <asm/reboot.h>
#include <asm/system.h>
#include <asm/serial.h>
//#include <asm/traps.h>
#include <linux/serial_core.h>
#include <linux/lmb.h>

#ifdef CONFIG_AR7100_EMULATION
#define         AG7100_CONSOLE_BAUD (9600)
#else
#define         AG7100_CONSOLE_BAUD (115200)
#endif

extern void v54_set_mem_top(unsigned long memsz);

const char
*get_system_type(void)
{

    return "Freescale P1020";
}


EXPORT_SYMBOL(get_system_type);



#ifdef CONFIG_BLK_DEV_INITRD
// V54
#include <linux/initrd.h>
#include <linux/root_dev.h>
static void
v54_initrd_setup(void)
{
	extern void * __rd_start, *__rd_end;
	/* Board specific code set up initrd_start and initrd_end */
	ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
	if (&__rd_start != &__rd_end) {
		initrd_start = (unsigned long)&__rd_start;
		initrd_end = (unsigned long)&__rd_end;
	}
}
#else
#define v54_initrd_setup(...)
#endif

void __init plat_mem_setup(void)
{

#if 1 // V54
    v54_set_mem_top(lmb_phys_mem_size());
	v54_initrd_setup();
#endif

}

#if 1 || CONFIG_AR531X_WATCHDOG
#include "v54_himem.h"

typedef void (*reboot_callback_t)(u_int32_t reason);
reboot_callback_t v54_reboot_callback = NULL;
void v54_register_on_reboot(reboot_callback_t reboot_callback);

void
v54_register_on_reboot(reboot_callback_t reboot_callback)
{
    v54_reboot_callback = reboot_callback;
    return;
}

void v54_on_reboot(u_int32_t reason)
{
    static int multiple = 0;
    if ( !multiple && v54_reboot_callback != NULL ) {
        multiple++;
        v54_reboot_callback(reason);
    }
    return;
}
#endif

#if defined(CONFIG_KGDB)

u8 getDebugChar(void)
{
        return Uart16550GetPoll() ;
}

void putDebugChar(char byte)
{
	Uart16550Put(byte) ;
}

#define DEBUG_CHAR '\001';

int kgdbInterrupt(void)
{
    /*
     * Try to avoid swallowing too much input: Only consume
     * a character if nothing new has arrived.  Yes, there's
     * still a small hole here, and we may lose an input
     * character now and then.
     */
    if (UART16550_READ(OFS_LINE_STATUS) & 1) {
        return 0;
    } else {
        return UART16550_READ(OFS_RCV_BUFFER) == DEBUG_CHAR;
    }
}

#endif
