/*
 * $Copyright: (c) 2005-2007 Ruckus Wireless Inc.
 * All Rights Reserved.$
 *
 * File: 	proc_eth.c
 * Purpose:	Handle /proc files for the ethernet driver.
 */

#ifdef V54_BSP

#include <linux/stddef.h>
//#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <asm/byteorder.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/proc_fs.h>
#include "ag7100.h"

#define LOCAL static

LOCAL int
ethMacStats(char *buf, char **start, off_t offset, int len, int *eof, void *data)
{
    ag7100_mac_t *mac;
    struct net_device *dev;
    struct net_device_stats *devStats;
    char *buf2 = buf;
    int i;
    int max_macs = num_macs;
#if 0
    if (!data) {
        printk("Cant find the device\n");
        return 0;
    }
#endif
#if 0
    if ( num_macs > AG7100_NAMCS )  {
        printk("%s: num_macs(%d) > AG7100_NMACS(%d)\n", __FUNCTION__
                , num_macs, AG7100_NMACS);
        max_macs = AG7100_NMACS;
    }
#endif

    for ( i=0; i< max_macs; i++ ) {
        mac = ag7100_macs[i];

        dev = mac->mac_dev;
        devStats = ag7100_get_stats(dev);

        mac->sw_stats.stat_tx += ag7100_tx_reap(mac);

            /* print */
        buf2 += sprintf(buf2, "\nMAC%d: carrier %s\n", i,
                    (netif_carrier_ok(dev) ? "on" : "off") );
            buf2 += sprintf(buf2," %s %s %s %s %s %s %s\n",
                "Stats:  tx",
                "    tx_int",
                "     tx_be",
                "   tx_full",
                "        rx",
                "     rx_be",
                "   rx_full");

            buf2 += sprintf(buf2," %10u %10u %10u %10u %10u %10u %10u\n",
            mac->sw_stats.stat_tx, mac->sw_stats.stat_tx_int, mac->sw_stats.stat_tx_be,
            mac->sw_stats.stat_tx_full,
            mac->sw_stats.stat_rx, mac->sw_stats.stat_rx_be, mac->sw_stats.stat_rx_full);

        buf2 += sprintf(buf2," %s %s %s "
#if defined(CONFIG_AR7240)
                "%s "
#endif
                "%s "
                "%s\n",
                "tx_dropped",
                " rx_missed",
                "tx_timeout",
#if defined(CONFIG_AR7240)
                "   fwd_hang",
#endif
                " dma_reset",
                " rx_ff_err");

        buf2 += sprintf(buf2," %10u %10u %10u "
#if defined(CONFIG_AR7240)
            "%10u "
#endif
            "%10u "
            "%10u\n",
            (u32)mac->mac_net_stats.tx_dropped,
            (u32)mac->mac_net_stats.rx_missed_errors,
            mac->sw_stats.stat_tx_timeout,
#if defined(CONFIG_AR7240)
            mac->sw_stats.stat_fwd_hang,
#endif
            mac->sw_stats.stat_dma_reset,
            mac->sw_stats.stat_rx_ff_err);

        mac->sw_stats.stat_tx       = 0;
        mac->sw_stats.stat_tx_int   = 0;
        mac->sw_stats.stat_tx_be    = 0;
        mac->sw_stats.stat_tx_full  = 0;
        mac->sw_stats.stat_rx       = 0;
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
    ag7100_phy_speed_t  speed;
    ag7100_mac_t *mac;

    phy_up = speed = fdx = 0;

    for( i=0; i < num_macs; i++ ) {
        mac = ag7100_macs[i];
#if ETHNAMES
        if (i && ag7100_get_link_status_1) {
#if defined(CONFIG_VENETDEV)
            if (venet_enable(mac)) {
                int j;
                for (j=0; j<dev_per_mac; j++) {
                    len += sprintf(bp+len, "%d %s %d %s %s\n",
                        ethname_remap[(j+1)], ethname_array[ethname_remap[j+1]],
                        current_conn[j].up,
                        current_conn[j].up ? spd_str[current_conn[j].speed] : "",
                        current_conn[j].up ? (current_conn[j].fdx ? "full" : "half") : "");
                }
            } else
#endif
            {
                /* show first port connection if not venet_enable */
                len += sprintf(bp+len, "%d %s %d %s %s\n", ethname_remap[i], ethname_array[ethname_remap[i]],
                            current_conn[0].up,
                            current_conn[0].up ? spd_str[current_conn[0].speed] : "",
                            current_conn[0].up ? (current_conn[0].fdx ? "full" : "half") : "");
            }
        } else {
            if (rks_is_gd17_board()) {
                int port;
                #define NUM_ETH_PORTS   5
                ag7100_get_link_status(mac->mac_unit, &phy_up, &fdx, &speed);
                /* eth1, 4 10/100mbps lan ports */
                for (port=0; port < (NUM_ETH_PORTS-1); port++) {
                    len += sprintf(bp+len, "%d %s-%d %d %s %s\n", port, ethname_array[1], port,
                             ((phy_up & (1 << port)) ? 1 : 0),
                             (phy_up & (1 << port)) ? spd_str[((speed >> (port << 1)) & 0x3)] : "",
                             (phy_up & (1 << port)) ? ((fdx & (1 << port)) ? "full" : "half") : "");
                }
                /* eth0, 1 100mbps wan port */
                len += sprintf(bp+len, "%d %s %d %s %s\n", (NUM_ETH_PORTS-1), ethname_array[0],
                         ((phy_up & 0x10) ? 1 : 0),
                         (phy_up & 0x10) ? spd_str[((speed & 0x300) >> 8)] : "",
                         (phy_up & 0x10) ? ((fdx & 0x10) ? "full" : "half") : "");
            } else if (rks_is_walle_board()) {
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
            } else if (rks_is_gd26_board() || rks_is_spaniel_board() || rks_is_gd36_board()) {
                if (!i) {
                    len += sprintf(bp+len, "%d %s %d %s %s\n", i, ethname_array[ethname_remap[0]],
                             (current_conn[4].up),
                             (current_conn[4].up) ? spd_str[current_conn[4].speed] : "",
                             (current_conn[4].up) ? (current_conn[4].fdx ? "full" : "half") : "");
                } else {
                    len += sprintf(bp+len, "%d %s %d %s %s\n", i, ethname_array[ethname_remap[1]],
                             (current_conn[0].up),
                             (current_conn[0].up) ? spd_str[current_conn[0].speed] : "",
                             (current_conn[0].up) ? (current_conn[0].fdx ? "full" : "half") : "");
                }
            } else if (rks_is_gd11_board() && !i) {
                phy_up  = current_conn[2].up;
                fdx     = current_conn[2].fdx;
                speed   = current_conn[2].speed;
                len += sprintf(bp+len, "%d %s %d %s %s\n", ethname_remap[i], ethname_array[ethname_remap[i]],
                             phy_up, phy_up ? spd_str[speed] : "",
                             phy_up ? (fdx ? "full" : "half") : "");
            } else {
                phy_up  = current_conn[i].up;
                fdx     = current_conn[i].fdx;
                speed   = current_conn[i].speed;
                len += sprintf(bp+len, "%d %s %d %s %s\n", ethname_remap[i], ethname_array[ethname_remap[i]],
                             phy_up, phy_up ? spd_str[speed] : "",
                             phy_up ? (fdx ? "full" : "half") : "");
            }
        }
#else
        if (i && ag7100_get_link_status_1) {
            ag7100_get_link_status_1(mac->mac_unit, &phy_up, &fdx, &speed);
            len += sprintf(bp+len, "%d eth%d %d %s %s\n", i, i,
                         phy_up, phy_up ? spd_str[speed] : "",
                         phy_up ? (fdx ? "full" : "half") : "");
        } else {
            ag7100_get_link_status(mac->mac_unit, &phy_up, &fdx, &speed);
            len += sprintf(bp+len, "%d eth%d %d %s %s\n", i, i,
                         phy_up, phy_up ? spd_str[speed] : "",
                         phy_up ? (fdx ? "full" : "half") : "");
        }
#endif
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
    int phy_up;
    ag7100_mac_t *mac;

    for( i=0; i < num_macs; i++ ) {
        mac = ag7100_macs[i];
        if (i && ag7100_get_link_status_1) {
#if defined(CONFIG_VENETDEV)
            if (venet_enable(mac)) {
                int j;
                for (j=0; j<dev_per_mac; j++) {
                    len += sprintf(bp+len, "%d %d\n", ethname_remap[(j+1)], current_conn[j].up);
                }
            } else
#endif
            {
                len += sprintf(bp+len, "%d %d\n", i, current_conn[0].up);
            }
        } else {
            if (rks_is_walle_board()) {
                int port;
                if (!i) {
                    len += sprintf(bp+len, "%d %d\n", ethname_remap[i], current_conn[4].up);
                } else {
                    for (port=0; port < 4; port++) {
                        len += sprintf(bp+len, "%d %d\n", ethname_remap[port+1], current_conn[port].up);
                    }
                }
            } else if (rks_is_gd26_board() || rks_is_spaniel_board() || rks_is_gd36_board()) {
                if (!i) {
                    len += sprintf(bp+len, "%d %d\n", i, current_conn[4].up);
                } else {
                    len += sprintf(bp+len, "%d %d\n", i, current_conn[0].up);
                }
            } else if (rks_is_gd11_board() && !i) {
                phy_up = current_conn[2].up;
                len += sprintf(bp+len, "%d %d\n", ethname_remap[i], phy_up);
            } else {
                phy_up  = current_conn[i].up;
                len += sprintf(bp+len, "%d %d\n", ethname_remap[i], phy_up);
            }
        }
    }

    return len;
}

#define MV_PHY_DBG
#if defined(CONFIG_MV_PHY) && defined(MV_PHY_DBG)
unsigned char curr_reg_info[128];
unsigned short rdphy_unit=0, rdphy_page=0, rdphy_reg=0;
unsigned short rdphy_regVal=0xffff;

#if  !defined(CONFIG_AR7240) && !defined(CONFIG_AR934x)
LOCAL int
proc_write_rdphy(struct file *file, const char *buffer,
                unsigned long buffer_length, void *data)
{
    unsigned short len;
    int i, retval;
    char *pEnd, *pBegin;

    len = 0;

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
                    rdphy_unit = retval;
                    printk("unit(%d) ", rdphy_unit);
                    break;
                case 1:
                    rdphy_page = retval;
                    printk("page(%d) ", rdphy_page);
                    break;
                case 2:
                    rdphy_reg = retval;
                    printk("reg(%d) ", rdphy_reg);
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

    if (!mv_phyRegRead(rdphy_unit, rdphy_page, rdphy_reg, &rdphy_regVal)) {
        rdphy_regVal = 0xffff;
    }
    printk("0x%x\n", rdphy_regVal);

    return buffer_length;
}

LOCAL int
proc_read_rdphy(char* page, char ** start,
            off_t off, int count, int *eof, void *data)
{
    int len=0;

    char *bp = page;

    if (!mv_phyRegRead(rdphy_unit, rdphy_page, rdphy_reg, &rdphy_regVal)) {
        rdphy_regVal = 0xffff;
    }

    len += sprintf(bp+len, "0x%x \n", rdphy_regVal);

    return len;
}

LOCAL int
proc_write_wrphy(struct file *file, const char *buffer,
                unsigned long buffer_length, void *data)
{
    unsigned short len, unit, page, reg, val;
    int i, retval;
    char *pEnd, *pBegin;

    len = unit = page = reg = val = 0;

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
                    printk("unit(%d) ", unit);
                    break;
                case 1:
                    page = retval;
                    printk("page(%d) ", page);
                    break;
                case 2:
                    reg = retval;
                    printk("reg(%d) ", reg);
                    break;
                case 3:
                    val = retval;
                    printk("val(0x%x) ", val);
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

    mv_phyRegWrite(unit, page, reg, val);

    return buffer_length;
}

LOCAL int
proc_read_wrphy(char* page, char ** start,
            off_t off, int count, int *eof, void *data)
{
    return 0;
}
#else
LOCAL int
proc_write_rdphy(struct file *file, const char *buffer,
                unsigned long buffer_length, void *data)
{
    unsigned short len;
    int i, retval;
    char *pEnd, *pBegin;
    unsigned short addr=0, reg=0;

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
                    addr = retval;
                    printk("addr(%x) ", addr);
                    break;
                case 1:
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

#if !defined(CONFIG_AR934x)
    retval= s26_rd_phy(addr, reg);
#else
    retval= ag7100_mii_read(0, addr, reg);
#endif
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
    unsigned short addr;

    len = reg = val = addr = 0;

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
                    addr = retval;
                    printk("addr(%x) ", addr);
                    break;
                case 1:
                    reg = retval;
                    printk("reg(%x) ", reg);
                    break;
                case 2:
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

#if !defined(CONFIG_AR934x)
    s26_wr_phy(addr, reg, val);
#else
    ag7100_mii_write(0, addr, reg, val);
#endif

    return buffer_length;
}

LOCAL int
proc_read_wrphy(char* page, char ** start,
            off_t off, int count, int *eof, void *data)
{
    return 0;
}
#endif
#endif

LOCAL int
proc_read_unreap(char* page, char ** start,
            off_t off, int count, int *eof, void *data)
{
    int len=0;
    char *bp = page;

    len += sprintf(bp+len, "%d/%d\n", ag7100_num_tx_unreap, ag7100_max_tx_unreap);

    return len;
}

extern int _max_tx_unreap;
LOCAL int
proc_write_unreap(struct file *file, const char *buffer,
            unsigned long count, void*data)
{
    int val = 0 ;
    if ( count == 0 ) {
        return 0;
    }
    sscanf(buffer, "%d", &val) ;

    if( val > 0 && val < 100 ) {
        ag7100_max_tx_unreap = val;
    }

    return count;
}

LOCAL int
WalleStats(char *buf, char **start, off_t offset, int len, int *eof, void *data)
{
    char *buf2 = buf;
    int i;

    for ( i=0; i<= 0xa4; i+=0x4 ) {
        buf2 += sprintf(buf2, "\n%2x: %8x %8x %8x %8x %8x\n", i,
            athrs26_reg_read(0x20000+i),
            athrs26_reg_read(0x20100+i),
            athrs26_reg_read(0x20200+i),
            athrs26_reg_read(0x20300+i),
            athrs26_reg_read(0x20400+i));
    }
    return (buf2-buf);
}

char *counter_array[] = {
    "TR64 ", "TR127", "TR255", "TR511", "TR1K ", "TRMAX", "TRMGV",
    "RBYT ", "RPKT ", "RFCS ", "RMCA ", "RBCA ", "RXCF ", "RXPF ",
    "RXUO ", "RALN ", "RFLR ", "RCDE ", "RCSE ", "RUND ", "ROVR ",
    "RFRG ", "RJBR ", "RDRP ",
    "TBYT ", "TPKT ", "TMCA ", "TBCA ", "TXPF ", "TDFR ", "TEDF ",
    "TSCL ", "TMCL ", "TLCL ", "TXCL ", "TNCL ", "TPFH ", "TDRP ",
    "TJBR ", "TFCS ", "TXCF ", "TOVR ", "TUND ", "TFRG "
    };

LOCAL int
gecntInfo(char *buf, char **start, off_t offset, int len, int *eof, void *data)
{
    char *buf2 = buf;
    int i;

    buf2 += sprintf(buf2, "\n                  GE0      GE1\n");
    for ( i=0x80; i< 0x130; i+=4 ) {
        buf2 += sprintf(buf2, "%4x %s   %8x %8x\n",
            i, *(&counter_array[(i-0x80)>>2]),
            ar7100_reg_rd(AR7100_GE0_BASE+i),
            ar7100_reg_rd(AR7100_GE1_BASE+i));
    }
    return (buf2-buf);
}

int eth_dbg = 5;

LOCAL int
proc_read_dbg(char* page, char ** start,
            off_t off, int count, int *eof, void *data)
{
    int len=0;
    int i;
    unsigned int offset, val;
    char *bp = page;
    ag7100_mac_t *mac;

    /* for ethernet regs..... */

    len += sprintf(bp+len, "eth_dbg[%d]\noffset:\tmac0\tmac1\n", eth_dbg);
    for(offset=0; offset<=0x10; offset+=0x4) {
        len += sprintf(bp+len, "%3x:", offset);
        for( i=0; i < num_macs; i++ ) {
            mac = ag7100_macs[i];
            val = ag7100_reg_rd(mac, offset);
            len += sprintf(bp+len, "\t%8x", val);
        }
        len += sprintf(bp+len, "\n");
    }
    for(offset=0x20; offset<=0x7c; offset+=0x4) {
        len += sprintf(bp+len, "%3x:", offset);
        for( i=0; i < num_macs; i++ ) {
            mac = ag7100_macs[i];
            do {
                if (mac == ag7100_macs[1] && offset >= 0x28 && offset <= 0x34) {
                    val = 0;
                    break;
                }
                val = ag7100_reg_rd(mac, offset);
            } while(0);
            len += sprintf(bp+len, "\t%8x", val);
        }
        len += sprintf(bp+len, "\n");
    }
    for(offset=0x180; offset<=0x1d8; offset+=0x4) {
        if (offset == 0x1a0 || offset == 0x1bc) {
            continue;
        }
        len += sprintf(bp+len, "%3x:", offset);
        for( i=0; i < num_macs; i++ ) {
            mac = ag7100_macs[i];
            val = ag7100_reg_rd(mac, offset);
            len += sprintf(bp+len, "\t%8x", val);
        }
        len += sprintf(bp+len, "\n");
    }

    /* for switch regs..... */

    len += sprintf(bp+len, "\noffset:\tswitch Regs\n");
    for (offset=0; offset<=0x98; offset+=4) {
        len += sprintf(bp+len, "%5x:\t%8x\n", offset, athrs26_reg_read(offset));
    }
    for (offset=0x100; offset<=0x120; offset+=4) {
        len += sprintf(bp+len, "%5x:\t%8x\n", offset, athrs26_reg_read(offset));
    }
    for (offset=0x200; offset<=0x220; offset+=4) {
        len += sprintf(bp+len, "%5x:\t%8x\n", offset, athrs26_reg_read(offset));
    }
    for (offset=0x300; offset<=0x320; offset+=4) {
        len += sprintf(bp+len, "%5x:\t%8x\n", offset, athrs26_reg_read(offset));
    }
    for (offset=0x400; offset<=0x420; offset+=4) {
        len += sprintf(bp+len, "%5x:\t%8x\n", offset, athrs26_reg_read(offset));
    }
    for (offset=0x500; offset<=0x520; offset+=4) {
        len += sprintf(bp+len, "%5x:\t%8x\n", offset, athrs26_reg_read(offset));
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

    unsigned int addr1, val1, addr2, val2;

    buf[count] = '\0';
    sscanf(buf, "%d:%x:%x:%x:%x", &eth_dbg, &addr1, &val1, &addr2, &val2);

    printk(KERN_EMERG "dbg=%d addr1=0x%x val1=0x%x addr2=0x%x val2=0x%x\n",
        eth_dbg, addr1, val1, addr2, val2);

#define INVALID_ADDR 0x11
    if (addr1 != INVALID_ADDR) {
        ar7100_reg_wr(addr1, val1);
    }
    if (addr2 != INVALID_ADDR) {
        athrs26_reg_write(addr2, val2);
    }

    return count;
}

LOCAL int
proc_read_dma_check(char* page, char ** start,
            off_t off, int count, int *eof, void *data)
{
    int len=0;
    char *bp = page;

    len += sprintf(bp+len, "%d\n", check_dma_enabled);

    return len;
}

LOCAL int
proc_write_dma_check(struct file *file, const char *buffer,
            unsigned long count, void*data)
{
    char  buf[32];
    unsigned int val;

    if (count > 32) {
      count = 32;
    }

    if(copy_from_user(buf, buffer, count))
      return(-EFAULT);

    buf[count] = '\0';
    sscanf(buf, "%d", &val);

    if (val) {
        check_dma_enabled = 1;
    } else {
        check_dma_enabled = 0;
    }

    return count;
}

LOCAL int
proc_read_rx_fifo_check(char* page, char ** start,
            off_t off, int count, int *eof, void *data)
{
    int len=0;
    char *bp = page;

    len += sprintf(bp+len, "%d\n", check_rx_fifo_enabled);

    return len;
}

LOCAL int
proc_write_rx_fifo_check(struct file *file, const char *buffer,
            unsigned long count, void*data)
{
    char  buf[32];
    unsigned int val;

    if (count > 32) {
      count = 32;
    }

    if(copy_from_user(buf, buffer, count))
      return(-EFAULT);

    buf[count] = '\0';
    sscanf(buf, "%d", &val);

    if (val) {
        check_rx_fifo_enabled = 1;
    } else {
        check_rx_fifo_enabled = 0;
    }

    return count;
}

LOCAL int
proc_read_dma_reset(char* page, char ** start,
            off_t off, int count, int *eof, void *data)
{
    return 0;
}

LOCAL int
proc_write_dma_reset(struct file *file, const char *buffer,
            unsigned long count, void*data)
{
    char  buf[32];
    unsigned int val;

    if (count > 32) {
      count = 32;
    }

    if(copy_from_user(buf, buffer, count))
      return(-EFAULT);

    buf[count] = '\0';
    sscanf(buf, "%d", &val);

    if (val) {
        printk("reset mac1 dma\n");
        ag7100_dma_reset(ag7100_macs[1]);
    } else {
        printk("reset mac0 dma\n");
        ag7100_dma_reset(ag7100_macs[0]);
    }

    return count;
}

//------------------------------------------------------------------------
#include "macProc.h"
// total mac is in num_macs

#if 0
LOCAL struct proc_dir_entry *eth_entry;
#endif

#define MAC_FILE    "mac"
LOCAL struct proc_dir_entry *mac_file;

#define DEVICES_FILE    "devices"
LOCAL struct proc_dir_entry *devices_file;

#define PORTS_FILE    "ports"
LOCAL struct proc_dir_entry *ports_file;

#if defined(CONFIG_MV_PHY) && defined(MV_PHY_DBG)
#define RDPHY_FILE    "rdphy"
LOCAL struct proc_dir_entry *rdphy_file;

#define WRPHY_FILE    "wrphy"
LOCAL struct proc_dir_entry *wrphy_file;
#endif

#define UNREAP_FILE    "max_unreap"
LOCAL struct proc_dir_entry *unreap_file;

#define STATS_FILE      "stats"
LOCAL struct proc_dir_entry *stats_file;

#define GECNT_FILE      "ge_cnt"
LOCAL struct proc_dir_entry *gecnt_file;

#define DBG_FILE      "dbg"
LOCAL struct proc_dir_entry *dbg_file;

#define DMA_CHECK_FILE  "dma_check"
LOCAL struct proc_dir_entry *dma_check_file;

#define RX_FIFO_CHECK_FILE  "rx_fifo_check"
LOCAL struct proc_dir_entry *rx_fifo_check_file;

#define DMA_RESET_FILE  "dma_reset"
LOCAL struct proc_dir_entry *dma_reset_file;

static struct proc_dir_entry *proc_enet = NULL;

#define PROC_NET_ETH   "eth"

LOCAL void
mac_create_proc_entries(void)
{
     if ((proc_enet = proc_mkdir(PROC_NET_ETH, init_net.proc_net)) == NULL ){
        printk("%s: proc_mkdir(%s,0)\n failed\n", __FUNCTION__, PROC_NET_ETH);
        return;
    }
#if 0
   {
    int i;
    char ethMac[8];
    for ( i = 0; i< num_macs; i++ ) {
        sprintf(ethMac, "mac%d", i);
        if(CREATE_READ_PROC(proc_enet, &eth_entry, ethMac, ethMacStats) != 0){
            printk("%s: create_proc_entry(%s/%s)failed", __FUNCTION__,
                        PROC_NET_ETH, ethMac);
            continue;
        }
        eth_entry->data = ag7100_macs[i];
    }
   }
#endif

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
#if defined(CONFIG_MV_PHY) && defined(MV_PHY_DBG)
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
#endif
    if (CREATE_RDWR_PROC(proc_enet, &unreap_file, UNREAP_FILE,
                proc_read_unreap, proc_write_unreap) != 0 ) {
        printk("%s: create_proc_entry(%s/%s) failed\n", __FUNCTION__,
                PROC_NET_ETH, UNREAP_FILE);
    }

    if (rks_is_walle_board()) {
        if(CREATE_READ_PROC(proc_enet, &stats_file, STATS_FILE, WalleStats) != 0){
            printk("%s: create_proc_entry(%s/%s)failed", __FUNCTION__,
                    PROC_NET_ETH, STATS_FILE);
        }
        if(CREATE_READ_PROC(proc_enet, &gecnt_file, GECNT_FILE, gecntInfo) != 0){
            printk("%s: create_proc_entry(%s/%s)failed", __FUNCTION__,
                    PROC_NET_ETH, GECNT_FILE);
        }
        if (CREATE_RDWR_PROC(proc_enet, &dbg_file, DBG_FILE,
                    proc_read_dbg, proc_write_dbg) != 0 ) {
            printk("%s: create_proc_entry(%s/%s) failed\n", __FUNCTION__,
                    PROC_NET_ETH, DBG_FILE);
        }
        if (CREATE_RDWR_PROC(proc_enet, &dma_check_file, DMA_CHECK_FILE,
                    proc_read_dma_check, proc_write_dma_check) != 0 ) {
            printk("%s: create_proc_entry(%s/%s) failed\n", __FUNCTION__,
                    PROC_NET_ETH, DMA_CHECK_FILE);
        }
        if (CREATE_RDWR_PROC(proc_enet, &rx_fifo_check_file, RX_FIFO_CHECK_FILE,
                    proc_read_rx_fifo_check, proc_write_rx_fifo_check) != 0 ) {
            printk("%s: create_proc_entry(%s/%s) failed\n", __FUNCTION__,
                    PROC_NET_ETH, RX_FIFO_CHECK_FILE);
        }
        if (CREATE_RDWR_PROC(proc_enet, &dma_reset_file, DMA_RESET_FILE,
                    proc_read_dma_reset, proc_write_dma_reset) != 0 ) {
            printk("%s: create_proc_entry(%s/%s) failed\n", __FUNCTION__,
                    PROC_NET_ETH, DMA_RESET_FILE);
        }
    }

    return;
}

static void
mac_cleanup_proc_entries(void)
{

    if ( proc_enet == NULL ) return;

    remove_proc_entry(UNREAP_FILE, proc_enet);
    remove_proc_entry(PORTS_FILE, proc_enet);
    remove_proc_entry(DEVICES_FILE, proc_enet);
    remove_proc_entry(MAC_FILE, proc_enet);

#if 0
   {
    int i;
    char ethMac[8];
    for ( i=0; i<num_macs; i++ ) {
        sprintf(ethMac, "mac%d", i);
        remove_proc_entry(ethMac, proc_enet);
    }
   }
#endif
#if defined(CONFIG_MV_PHY) && defined(MV_PHY_DBG)
    remove_proc_entry(RDPHY_FILE, proc_enet);
    remove_proc_entry(WRPHY_FILE, proc_enet);
#endif
    if (rks_is_walle_board()) {
        remove_proc_entry(DBG_FILE, proc_enet);
        remove_proc_entry(GECNT_FILE, proc_enet);
        remove_proc_entry(STATS_FILE, proc_enet);
    }
}

#endif
