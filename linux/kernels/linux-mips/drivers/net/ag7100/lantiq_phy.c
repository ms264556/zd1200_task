/*
 * File: 	lantiq_phy.c
 * Purpose:	Defines PHY driver routines, and standard 802.3 10/100/1000
 *		PHY interface routines.
 */


#ifndef __ECOS
#include "linux/types.h"
#include "linux/delay.h"
#endif
#include "ag7100_phy.h"
#include "eth_diag.h"
#include "ar531x_bsp.h"

extern char *dup_str[];
extern int *ethname_remap;

static uint32_t lantiq_phy0_addr = LANTIQ_PHY0_ADDR ;
#define PHY_ADDR(unit)  lantiq_phy0_addr

#define TRUE    1
#define FALSE   0

#if !defined(MII_CTRL_REG)
#define MII_CTRL_REG            0
#endif
#define MII_STATUS_REG          1
#define MII_ID1_REG             2
#define MII_ID2_REG             3
#define MII_AN_ADVERTISE_REG    4
#define MII_LINK_PARTNER_REG    5
#define MII_AN_EXPANSION_REG    6
#define MII_NEXT_PAGE_XMIT_REG  7
#define MII_LP_NEXT_PAGE_REG    8
#define MII_1000BT_CTRL_REG     9
#define MII_1000BT_STATUS_REG   10

#define MII_PHY_RESET           0x8000
#define MII_RESTART_AN          0x0200
#define MII_LINK_STATUS_MASK    0x04
#define MII_1000BT_MASK         0x0300

#define MII_INTF_STATUS_REG     0x18
#define MII_SPEED_MASK          0x03
#define MII_DUPLEX_MASK         0x08

#ifdef __ECOS
#define INFO_PRINT  diag_printf
#else
#define INFO_PRINT  printk
#endif

#define DRV_DEBUG 0
#if DRV_DEBUG
#define DRV_DEBUG_PHYSETUP  0x00000001
#define DRV_DEBUG_PHYCHANGE 0x00000002
#define DRV_DEBUG_PHYSTATUS 0x00000004

#define DRV_LOG(FLG, X0, X1, X2, X3, X4, X5, X6)    \
{                                                   \
    if (PhyDebug & (FLG)) {                      \
        logMsg(X0, X1, X2, X3, X4, X5, X6);         \
    }                                               \
}

#define DRV_MSG(x,a,b,c,d,e,f)                      \
    logMsg(x,a,b,c,d,e,f)

#ifdef __ECOS
#define DRV_PRINT(FLG, X)                           \
{                                                   \
    if (PhyDebug & (FLG)) {                      \
        diag_printf X;                                   \
    }                                               \
}
#else
#define DRV_PRINT(FLG, X)                           \
{                                                   \
    if (PhyDebug & (FLG)) {                      \
        printk X;                                   \
    }                                               \
}
#endif

#if 1
int PhyDebug = DRV_DEBUG_PHYSETUP;
#else
int PhyDebug = DRV_DEBUG_PHYSETUP | DRV_DEBUG_PHYSTATUS;
#endif
#else /* !DRV_DEBUG */
#define DRV_LOG(DBG_SW, X0, X1, X2, X3, X4, X5, X6)
#define DRV_MSG(x,a,b,c,d,e,f)
#define DRV_PRINT(DBG_SW,X)
#endif

int
lantiq_phyRegRead(int unit, int page, int reg, unsigned short *val)
{
    //printk("%s: unit=%d page=%d reg=%d\n",
    //    __func__, unit, page, reg);

    *val = phy_reg_read(unit, PHY_ADDR(unit), reg);
    return TRUE;
}

int
lantiq_phyRegWrite(int unit, int page, int reg, unsigned short val)
{
    //printk("%s: unit=%d page=%d reg=%d val=0x%x\n",
    //    __func__, unit, page, reg, val);

    phy_reg_write(unit, PHY_ADDR(unit), reg, val);
    return TRUE;
}

/******************************************************************************
*
* phySetup - Setup the PHY associated with
*                the specified MAC unit number.
*
*         Initializes the associated PHY port.
*
*/

int
lantiq_phySetup(int unit)
{
    printk("%s: \n", __func__);
    /* sw reset */
    phy_reg_write(unit, PHY_ADDR(unit), MII_CTRL_REG,
        (MII_PHY_RESET | phy_reg_read(unit, PHY_ADDR(unit), MII_CTRL_REG)));
#ifdef __ECOS
    sysMsDelay(500);
#else
    mdelay(500);
#endif

    return TRUE;
}

/******************************************************************************
*
* phyIsDuplexFull - Determines whether the phy ports associated with the
* specified device are FULL or HALF duplex.
*
* RETURNS:
*    TRUE  --> FULL
*    FALSE --> HALF
*/
int
lantiq_phyIsFullDuplex(int unit)
{
    uint16_t data;

    //printk("%s: \n", __func__);
    data = phy_reg_read(unit, PHY_ADDR(unit), MII_INTF_STATUS_REG);
    DRV_PRINT(DRV_DEBUG_PHYSTATUS,
        ("IsFullDuplex(0x%x) 0x%x\n", data, (data & MII_DUPLEX_MASK)?1:0));
    return ((data & MII_DUPLEX_MASK)?1:0);
}

/******************************************************************************
*
* phySpeed - Determines the speed of phy ports associated with the
* specified device.
*
* RETURNS:
*    phy speed - (ag7100_phy_speed_t)
*/
int
lantiq_phySpeed(int unit)
{
    uint16_t data;

    //printk("%s: \n", __func__);
    data = phy_reg_read(unit, PHY_ADDR(unit), MII_INTF_STATUS_REG);
    DRV_PRINT(DRV_DEBUG_PHYSTATUS,
        ("Speed(0x%x) 0x%x\n", data, (data & MII_SPEED_MASK)));
    return (data & MII_SPEED_MASK);
}

int
lantiq_phyIsUp(int unit)
{
    uint16_t data1, data2;
    int val;

    //printk("%s: \n", __func__);
    data1 = phy_reg_read(unit, PHY_ADDR(unit), MII_STATUS_REG);
    data2 = phy_reg_read(unit, PHY_ADDR(unit), MII_STATUS_REG);

    if (!(data2 & MII_LINK_STATUS_MASK)) {
        val = FALSE;
    } else {
        val = TRUE;
    }

    return val;
}

void lantiqPhy_force_100M(int unit,int duplex)
{
    lantiq_phyRegWrite(unit, 0, MII_CTRL_REG, (0x2000|(duplex << 8)));
}

void lantiqPhy_force_10M(int unit,int duplex)
{
    lantiq_phyRegWrite(unit, 0, MII_CTRL_REG, (0x0000|(duplex << 8)));
}

void lantiqPhy_force_auto(int unit)
{
    lantiq_phyRegWrite(unit, 0, MII_CTRL_REG, 0x9000);
}

int lantiqPhy_ioctl(uint32_t *args, int cmd)
{
    struct eth_diag *etd =(struct eth_diag *) args;
    unsigned short val;
    uint16_t portnum=etd->ed_u.portnum;
    uint32_t phy_reg=etd->phy_reg;

    if(cmd == ATHR_FORCE_PHY) {
        /* phy_reg: force command port number */
        phy_reg = 0;
    } else if (portnum > 1) {
        /* portnum: r/w command port number */
        portnum = 0;
    }

    if(cmd  == ATHR_RD_PHY) {
        etd->val = 0;
        if(etd->ed_u.portnum != 0xf) {
            if (lantiq_phyRegRead(portnum, ((phy_reg >> 16) & 0xffff),
                (phy_reg & 0xffff), &val)) {
                etd->val = val;
            }
        }
    } else if(cmd  == ATHR_WR_PHY) {
        if(etd->ed_u.portnum != 0xf) {
            lantiq_phyRegWrite(portnum, ((phy_reg >> 16) & 0xffff),
            (phy_reg & 0xffff), etd->val);
        }
    }
    else if(cmd == ATHR_FORCE_PHY) {
        if(etd->val == 10) {
            printk("Forcing 10Mbps %s on port:%d \n",
                dup_str[etd->ed_u.duplex], (etd->phy_reg));
            lantiqPhy_force_10M(phy_reg, etd->ed_u.duplex);
        } else if(etd->val == 100) {
            printk("Forcing 100Mbps %s on port:%d \n",
                dup_str[etd->ed_u.duplex],(etd->phy_reg));
            lantiqPhy_force_100M(phy_reg, etd->ed_u.duplex);
        } else if(etd->val == 0) {
            printk("Enabling Auto Neg on port:%d \n",(etd->phy_reg));
            lantiqPhy_force_auto(phy_reg);
        } else
            return -EINVAL;
    }
    else
        return -EINVAL;

    return 0;
}
