/*
 * $Copyright: (c) 2005-2007 Ruckus Wireless Inc.
 * All Rights Reserved.$
 *
 * File: 	proc_eth.c
 * Purpose:	Handle /proc files for the ethernet driver.
 */

#ifdef V54_BSP

#include <linux/stddef.h>
#include <linux/module.h>
#include <linux/types.h>
#include <asm/byteorder.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/proc_fs.h>

#define LOCAL static

LOCAL int
ethMacStats(char *buf, char **start, off_t offset, int len, int *eof, void *data)
{
    athr_gmac_t *mac;
    struct net_device *dev;
    char *buf2 = buf;
    int i;
    int max_macs = num_macs;

    for ( i=0; i< max_macs; i++ ) {
        mac = athr_gmacs[i];

        dev = mac->mac_dev;

        mac->sw_stats.stat_tx += athr_gmac_tx_reap(mac, 0); //temp

            /* print */
        buf2 += sprintf(buf2, "\nMAC%d: carrier %s\n", i,
                    (netif_carrier_ok(dev) ? "on" : "off") );
            buf2 += sprintf(buf2," %s %s %s %s %s %s %s %s\n",
                "Stats:  tx",
                "    tx_int",
                "     tx_be",
                "   tx_full",
                "        rx",
                "    rx_ovf",
                "     rx_be",
                "   rx_full");

            buf2 += sprintf(buf2," %10u %10u %10u %10u %10u %10u %10u %10u\n",
            mac->sw_stats.stat_tx, mac->sw_stats.stat_tx_int, mac->sw_stats.stat_tx_be,
            mac->sw_stats.stat_tx_full,
            mac->sw_stats.stat_rx, mac->sw_stats.stat_rx_ovf, mac->sw_stats.stat_rx_be, mac->sw_stats.stat_rx_full);

        buf2 += sprintf(buf2," %s %s %s "
                "%s %s\n",
                "tx_dropped",
                " rx_missed",
                "tx_timeout",
                " dma_reset",
                " dma_owned");

        buf2 += sprintf(buf2," %10u %10u %10u "
            "%10u %10u\n",
            (u32)mac->mac_net_stats.tx_dropped,
            (u32)mac->mac_net_stats.rx_missed_errors,
            mac->sw_stats.stat_tx_timeout,
            mac->sw_stats.stat_dma_reset,
            mac->sw_stats.stat_dma_owned);

        mac->sw_stats.stat_tx       = 0;
        mac->sw_stats.stat_tx_int   = 0;
        mac->sw_stats.stat_tx_be    = 0;
        mac->sw_stats.stat_tx_full  = 0;
        mac->sw_stats.stat_rx       = 0;
        mac->sw_stats.stat_rx_ovf   = 0;
        mac->sw_stats.stat_rx_be    = 0;
        mac->sw_stats.stat_rx_full  = 0;

        mac->mac_net_stats.tx_dropped       = 0;
        mac->mac_net_stats.rx_missed_errors = 0;
    }
    return (buf2-buf);
}

LOCAL int
proc_read_devices(char* page, char ** start,
            off_t off, int count, int *eof, void *data)
{
    int len=0;
    char *bp = page;
    int i;
    int fdx, phy_up;
    athr_phy_speed_t  speed;
    athr_gmac_t *mac;

    phy_up = speed = fdx = 0;
    for( i=0; i < num_macs; i++ ) {
        mac = athr_gmacs[i];

        if (rks_is_gd26_board() || rks_is_spaniel_board() || rks_is_gd36_board()) {
            if (!i) {
                len += sprintf(bp+len, "%d %s %d %s %s\n", ethname_remap[i], ethname_array[ethname_remap[0]],
                        (current_conn[4].up),
                        (current_conn[4].up) ? spd_str[current_conn[4].speed] : "",
                        (current_conn[4].up) ? (current_conn[4].fdx ? "full" : "half") : "");
            } else {
                len += sprintf(bp+len, "%d %s %d %s %s\n", ethname_remap[i], ethname_array[ethname_remap[1]],
                        (current_conn[0].up),
                        (current_conn[0].up) ? spd_str[current_conn[0].speed] : "",
                        (current_conn[0].up) ? (current_conn[0].fdx ? "full" : "half") : "");
            }
        }
        else if(rks_is_walle2_board()) {
#define WALLE_ETH_PORTS   5
            if (!i) {
                len += sprintf(bp+len, "%d %s %d %s %s\n", (WALLE_ETH_PORTS-1), ethname_array[ethname_remap[0]],
                        (current_conn[WALLE_ETH_PORTS-1].up),
                        (current_conn[WALLE_ETH_PORTS-1].up) ? spd_str[current_conn[WALLE_ETH_PORTS-1].speed] : "",
                        (current_conn[WALLE_ETH_PORTS-1].up) ? (current_conn[WALLE_ETH_PORTS-1].fdx ? "full" : "half") : "");
            } else {
                /* eth1-eth4, 4 10/100mbps lan ports */
                int port;
                for (port=0; port < (WALLE_ETH_PORTS-1); port++) {
                    len += sprintf(bp+len, "%d %s %d %s %s\n", ethname_remap[port+1], ethname_array[ethname_remap[port+1]],
                            (current_conn[port].up),
                            (current_conn[port].up) ? spd_str[current_conn[port].speed] : "",
                            (current_conn[port].up) ? (current_conn[port].fdx ? "full" : "half") : "");
                }
            }
        }
    }
    return len;
}

LOCAL int
proc_read_ports(char* page, char ** start,
        off_t off, int count, int *eof, void *data)
{
    int len=0;
    char *bp = page;
    int i;
    athr_gmac_t *mac;

    for( i=0; i < num_macs; i++ ) {
        mac = athr_gmacs[i];
        if (rks_is_gd26_board() || rks_is_spaniel_board() || rks_is_gd36_board()) {
            if (!i) {
                len += sprintf(bp+len, "%d %d\n", ethname_remap[i], current_conn[4].up);
            } else {
                len += sprintf(bp+len, "%d %d\n", ethname_remap[i], current_conn[0].up);
            }
        }
        else if(rks_is_walle2_board()) {
            int port;
            if (!i) {
                len += sprintf(bp+len, "%d %d\n", ethname_remap[i], current_conn[4].up);
            } else {
                for (port=0; port < 4; port++) {
                    len += sprintf(bp+len, "%d %d\n", ethname_remap[port+1], current_conn[port].up);
                }
            }
        }
    }
    return len;
}

unsigned char curr_reg_info[128];
unsigned short rdphy_unit=0, rdphy_page=0, rdphy_reg=0;
unsigned short rdphy_regVal=0xffff;

LOCAL int
proc_write_rdphy(struct file *file, const char *buffer,
                unsigned long buffer_length, void *data)
{
    unsigned short len;
    int i, retval;
    char *pEnd, *pBegin;
    unsigned short unit=0, addr=0, reg=0;

    len = 0;

    if( buffer_length > (sizeof(curr_reg_info)-1) ) {
        len = sizeof(curr_reg_info)-1;
    } else {
        len = buffer_length - 1; /* extra 0xa0 in buffer ?? */
    }

    if( copy_from_user(&curr_reg_info[0], buffer, len)) {
        printk("rdphy exit: copy_from_user error\n");
        return -EFAULT;
    }

    curr_reg_info[len] = '\0';

    pBegin = &curr_reg_info[0];
    i = 0;
    do {
        retval = simple_strtoul( pBegin, &pEnd, 0 );
        if (pBegin != pEnd) {
            switch (i) {
                case 0:
                    unit = retval;
                    printk("unit(%x) ", unit);
                    break;
                case 1:
                    addr = retval;
                    printk("addr(%x) ", addr);
                    break;
                case 2:
                    reg = retval;
                    printk("reg(%x) ", reg);
                    break;
                default:
                    break;
            }
            i++;
        } else
            break;

        pBegin = pEnd;
        if ((*pBegin == 0x20) || (*pBegin == '\0'))
            pBegin++;
    } while ((unsigned int)pBegin < (unsigned int)&curr_reg_info[len]);
    printk("\n");

    if (!unit)
        retval= athr_gmac_mii_read(unit, addr, reg);
    else
        retval= s27_rd_phy(unit, addr, reg);
    printk("%x\n", retval);

    return buffer_length;
}

LOCAL int
proc_read_rdphy(char* page, char ** start,
            off_t off, int count, int *eof, void *data)
{
    int len=0;

    return len;
}

LOCAL int
proc_write_wrphy(struct file *file, const char *buffer,
                unsigned long buffer_length, void *data)
{
    unsigned short len, reg, val;
    int i, retval;
    char *pEnd, *pBegin;
    unsigned short unit, addr;

    len = reg = val = unit = addr = 0;

    if( buffer_length > (sizeof(curr_reg_info)-1) ) {
        len = sizeof(curr_reg_info)-1;
    } else {
        len = buffer_length - 1; /* extra 0xa0 in buffer ?? */
    }

    if( copy_from_user(&curr_reg_info[0], buffer, len)) {
        printk("wrphy exit: copy_from_user error\n");
        return -EFAULT;
    }

    curr_reg_info[len] = '\0';

    pBegin = &curr_reg_info[0];
    i = 0;
    do {
        retval = simple_strtoul( pBegin, &pEnd, 0 );
        if (pBegin != pEnd) {
            switch (i) {
                case 0:
                    unit = retval;
                    printk("unit(%x) ", unit);
                    break;
                case 1:
                    addr = retval;
                    printk("addr(%x) ", addr);
                    break;
                case 2:
                    reg = retval;
                    printk("reg(%x) ", reg);
                    break;
                case 3:
                    val = retval;
                    printk("val(%x) ", val);
                    break;
                default:
                    break;
            }
            i++;
        } else
            break;

        pBegin = pEnd;
        if ((*pBegin == 0x20) || (*pBegin == '\0'))
            pBegin++;
    } while ((unsigned int)pBegin < (unsigned int)&curr_reg_info[len]);
    printk("\n");

    if (!unit)
        athr_gmac_mii_write(unit, addr, reg, val);
    else
        s27_wr_phy(unit, addr, reg, val);
    return buffer_length;
}

LOCAL int
proc_read_wrphy(char* page, char ** start,
            off_t off, int count, int *eof, void *data)
{
    return 0;
}

LOCAL int
proc_read_dbg(char* page, char ** start,
            off_t off, int count, int *eof, void *data)
{
    int len=0;
    int i;
    unsigned int offset, val;
    char *bp = page;
    athr_gmac_t *mac;

    /* for ethernet regs..... */

    len += sprintf(bp+len, "offset:     mac0            mac1\n");
    for(offset=0; offset<=0x10; offset+=0x4) {
        len += sprintf(bp+len, "%3x:", offset);
        for( i=0; i < num_macs; i++ ) {
            mac = athr_gmacs[i];
            val = athr_gmac_reg_rd(mac, offset);
            len += sprintf(bp+len, "\t%8x", val);
        }
        len += sprintf(bp+len, "\n");
    }
    for(offset=0x20; offset<=0x5c; offset+=0x4) {
        len += sprintf(bp+len, "%3x:", offset);
        for( i=0; i < num_macs; i++ ) {
            mac = athr_gmacs[i];
            val = athr_gmac_reg_rd(mac, offset);
            len += sprintf(bp+len, "\t%8x", val);
        }
        len += sprintf(bp+len, "\n");
    }
    for(offset=0x80; offset<=0x12c; offset+=0x4) {
        len += sprintf(bp+len, "%3x:", offset);
        for( i=0; i < num_macs; i++ ) {
            mac = athr_gmacs[i];
            val = athr_gmac_reg_rd(mac, offset);
            len += sprintf(bp+len, "\t%8x", val);
        }
        len += sprintf(bp+len, "\n");
    }

    /* for switch regs..... */

    if (num_macs < 2)
        return len;

    len += sprintf(bp+len, "\noffset: switch regs\n");
    for (offset=0; offset<=0x98; offset+=4) {
        len += sprintf(bp+len, "%5x:\t%8x\n", offset, athrs27_reg_read(offset));
    }
    for (offset=0x100; offset<=0x120; offset+=4) {
        len += sprintf(bp+len, "%5x:\t%8x\n", offset, athrs27_reg_read(offset));
    }
    for (offset=0x200; offset<=0x220; offset+=4) {
        len += sprintf(bp+len, "%5x:\t%8x\n", offset, athrs27_reg_read(offset));
    }
    for (offset=0x300; offset<=0x320; offset+=4) {
        len += sprintf(bp+len, "%5x:\t%8x\n", offset, athrs27_reg_read(offset));
    }
    for (offset=0x400; offset<=0x420; offset+=4) {
        len += sprintf(bp+len, "%5x:\t%8x\n", offset, athrs27_reg_read(offset));
    }
    for (offset=0x500; offset<=0x520; offset+=4) {
        len += sprintf(bp+len, "%5x:\t%8x\n", offset, athrs27_reg_read(offset));
    }

    return len;
}

LOCAL int
proc_write_dbg(struct file *file, const char *buffer,
            unsigned long count, void*data)
{
    char  buf[255];

    if (count > 254) {
      count = 254;
    }

    if(copy_from_user(buf, buffer, count))
      return(-EFAULT);

    unsigned int addr, val;

    buf[count] = '\0';
    sscanf(buf, "%x %x", &addr, &val);

    printk(KERN_EMERG "switch addr=0x%x val=0x%x\n", addr, val);

    athrs27_reg_write(addr, val);

    return count;
}
//------------------------------------------------------------------------
#include "macProc.h"

#define MAC_FILE    "mac"
LOCAL struct proc_dir_entry *mac_file;

#define DEVICES_FILE    "devices"
LOCAL struct proc_dir_entry *devices_file;

#define PORTS_FILE    "ports"
LOCAL struct proc_dir_entry *ports_file;

#define RDPHY_FILE    "rdphy"
LOCAL struct proc_dir_entry *rdphy_file;

#define WRPHY_FILE    "wrphy"
LOCAL struct proc_dir_entry *wrphy_file;

#define DBG_FILE      "dbg"
LOCAL struct proc_dir_entry *dbg_file;

static struct proc_dir_entry *proc_enet = NULL;

#define PROC_NET_ETH   "eth"

LOCAL void
mac_create_proc_entries(void)
{
    if ((proc_enet = proc_mkdir(PROC_NET_ETH, init_net.proc_net)) == NULL ){
        printk("%s: proc_mkdir(%s,0)\n failed\n", __FUNCTION__, PROC_NET_ETH);
        return;
    }
    if(CREATE_READ_PROC(proc_enet, &mac_file, MAC_FILE, ethMacStats) != 0){
        printk("%s: create_proc_entry(%s/%s)failed", __FUNCTION__,
                PROC_NET_ETH, MAC_FILE);
    }
    if (CREATE_READ_PROC(proc_enet, &devices_file, DEVICES_FILE,
                proc_read_devices) != 0 ) {
        printk("%s: create_proc_entry(%s/%s) failed\n", __FUNCTION__,
                PROC_NET_ETH, DEVICES_FILE);
    }

    if (CREATE_READ_PROC(proc_enet, &ports_file, PORTS_FILE,
                proc_read_ports) != 0 ) {
        printk("%s: create_proc_entry(%s/%s) failed\n", __FUNCTION__,
                PROC_NET_ETH, PORTS_FILE);
    }
    if (CREATE_RDWR_PROC(proc_enet, &rdphy_file, RDPHY_FILE,
		        proc_read_rdphy, proc_write_rdphy) != 0 ) {
        printk("%s: create_proc_entry(%s/%s) failed\n", __FUNCTION__,
                PROC_NET_ETH, RDPHY_FILE);
    }
    if (CREATE_RDWR_PROC(proc_enet, &wrphy_file, WRPHY_FILE,
		        proc_read_wrphy, proc_write_wrphy) != 0 ) {
        printk("%s: create_proc_entry(%s/%s) failed\n", __FUNCTION__,
                PROC_NET_ETH, WRPHY_FILE);
    }
    if (CREATE_RDWR_PROC(proc_enet, &dbg_file, DBG_FILE,
                proc_read_dbg, proc_write_dbg) != 0 ) {
        printk("%s: create_proc_entry(%s/%s) failed\n", __FUNCTION__,
                PROC_NET_ETH, DBG_FILE);
    }
    return;
}

static void
mac_cleanup_proc_entries(void)
{

    if ( proc_enet == NULL ) return;

    remove_proc_entry(PORTS_FILE, proc_enet);
    remove_proc_entry(DEVICES_FILE, proc_enet);
    remove_proc_entry(MAC_FILE, proc_enet);
    remove_proc_entry(RDPHY_FILE, proc_enet);
    remove_proc_entry(WRPHY_FILE, proc_enet);
    remove_proc_entry(DBG_FILE, proc_enet);
}
#endif
