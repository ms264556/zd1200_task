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
#include <asm/e820.h>
#include "v54_himem.h"


//#include <asm/bootinfo.h>
//#include <asm/addrspace.h>


int v54_reserve_himem(unsigned long max_pfn)
{
    int i = 0;
    unsigned long start, found;

    printk("%s:  max_pfn: 0x%0lx, MAXMEM:0x%lx\n", __FUNCTION__, max_pfn, MAXMEM);
    start = (max_pfn << PAGE_SHIFT) - v54_himem_reserved;
    if (start > MAXMEM)
        start = MAXMEM - v54_himem_reserved;
    while (i++ < 10) {
        found = find_e820_area(start, ULONG_MAX, v54_himem_reserved, 1 << PAGE_SHIFT);
        if (found == -1UL) {
            printk("failed to find_e820_area: 0x%0lx ~ 0x%0lx\n", start, start + v54_himem_reserved - 1);
            start -= v54_himem_reserved;
            continue;
        }
        printk("to early_reserve_e820: 0x%0lx ~ 0x%0lx\n", found, found + v54_himem_reserved - 1);
        found = early_reserve_e820(found, v54_himem_reserved, 1 << PAGE_SHIFT);
        if (found)
            break;
        printk("failed to early_reserve_e820\n");
        start -= v54_himem_reserved;
    }
    if (i > 10) {
        printk(KERN_INFO "%s:  failed to reserve himem\n", __FUNCTION__);
        return 0;
    } else {
        _mem_top = (unsigned long)phys_to_virt(found + v54_himem_reserved);
        printk("reserved mem: 0x%0lx ~ 0x%0lx (phy addr:0x%0lx ~ 0x%0lx) for himem\n",
            _mem_top - v54_himem_reserved, _mem_top - 1, found, found + v54_himem_reserved - 1);
        return 1;
    }
}
