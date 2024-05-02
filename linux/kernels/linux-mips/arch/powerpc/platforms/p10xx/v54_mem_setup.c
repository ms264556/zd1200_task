/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright 2008 Ruckus Wireless Inc,  All Rights Reserved.
 */

/*
 * Memory setup adjustment functions.
 */

#include <linux/init.h>
//#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/bootmem.h>
#include "v54_himem.h"

//#include <asm/bootinfo.h>
//#include <asm/addrspace.h>


void v54_set_mem_top(unsigned long memsz)
{
    _mem_top = V54_MEM_START + memsz;

    printk("---> %s: mem_size: %lx @ [%lx,%lx] reserved=%lx\n", __FUNCTION__,
                    memsz, V54_MEM_START, _mem_top, v54_himem_reserved);
    return;
}
