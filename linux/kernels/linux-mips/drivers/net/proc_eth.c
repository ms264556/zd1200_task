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

char *spd_str[] = {"10Mbps", "100Mbps", "1000Mbps"};

char *speed_to_string(int speed)
{
    int index;
    switch (speed) {
        case 1000:
            index = 2;
            break;
        case 100:
            index = 1;
            break;
        default:
            index = 0;
            break;
    }
    return spd_str[index];
}

LOCAL int
proc_read_devices(char* page, char ** start,
            off_t off, int count, int *eof, void *data)
{
    int len=0;
    char *bp = page;
    int i;
    struct net_device *dev;
	struct gfar_private *priv;

    for (i=0; i<eth_dev_idx; i++) {
        dev = eth_dev[i];
	    priv = netdev_priv(dev);
        len += sprintf(bp+len, "%d %s %d %s %s\n", i, dev->name,
                     priv->oldlink,
                     priv->oldlink ? speed_to_string(priv->oldspeed) : "",
                     priv->oldlink ? (priv->oldduplex ? "full" : "half") : "");
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
    struct net_device *dev;
	struct gfar_private *priv;

    for (i=0; i<eth_dev_idx; i++) {
        dev = eth_dev[i];
	    priv = netdev_priv(dev);
        len += sprintf(bp+len, "%d %d\n", i, priv->oldlink);
    }
    return len;
}

LOCAL int
proc_write_rdphy(struct file *file, const char *buffer,
                unsigned long buffer_length, void *data)
{
    unsigned char curr_reg_info[64];
    unsigned short len, addr, reg;
    int i, retval;
    char *pEnd, *pBegin;
    struct net_device *dev;
	struct gfar_private *priv;

    len = addr = reg = 0;

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

    if (addr < eth_dev_idx) {
        dev = eth_dev[addr];
	    priv = netdev_priv(dev);
        retval = phy_read(priv->phydev, reg);
        printk("%x\n", retval);
    }

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
    unsigned char curr_reg_info[64];
    unsigned short len, addr, reg, val;
    int i, retval;
    char *pEnd, *pBegin;
    struct net_device *dev;
	struct gfar_private *priv;

    len = addr = reg = val = 0;

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

    if (addr < eth_dev_idx) {
        dev = eth_dev[addr];
	    priv = netdev_priv(dev);
        phy_write(priv->phydev, reg, val);
    }

    return buffer_length;
}

LOCAL int
proc_read_wrphy(char* page, char ** start,
            off_t off, int count, int *eof, void *data)
{
    return 0;
}

//------------------------------------------------------------------------
#include "macProc.h"

#define DEVICES_FILE    "devices"
LOCAL struct proc_dir_entry *devices_file;

#define PORTS_FILE    "ports"
LOCAL struct proc_dir_entry *ports_file;

#define RDPHY_FILE    "rdphy"
LOCAL struct proc_dir_entry *rdphy_file;

#define WRPHY_FILE    "wrphy"
LOCAL struct proc_dir_entry *wrphy_file;

static struct proc_dir_entry *proc_enet = NULL;

#define PROC_NET_ETH   "eth"

LOCAL void
mac_create_proc_entries(void)
{
    if ((proc_enet = proc_mkdir(PROC_NET_ETH, init_net.proc_net)) == NULL ){
        printk("%s: proc_mkdir(%s,0)\n failed\n", __FUNCTION__, PROC_NET_ETH);
        return;
    } else {
        printk("%s: proc_enet->name=%s proc_enet->parent->name=%s \n",
            __func__, proc_enet->name, proc_enet->parent->name);
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

    return;
}

static void
mac_cleanup_proc_entries(void)
{

    printk("%s: \n", __func__);
    if ( proc_enet == NULL ) return;

    remove_proc_entry(PORTS_FILE, proc_enet);
    remove_proc_entry(DEVICES_FILE, proc_enet);
}

#endif
