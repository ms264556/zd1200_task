/*
 * File: 	lantiq_phy.c
 * Purpose:	Defines PHY driver routines, and standard 802.3 10/100/1000
 *		PHY interface routines.
 */


#ifndef __ECOS
#include "linux/types.h"
#include "linux/delay.h"
#endif
#include "athrs_phy.h"
#include "athrs_mac.h"

extern char *dup_str[];
extern int *ethname_remap;

#define LANTIQ_PHY0_ADDR        6
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

int lantiq_phyIsUp(int unit);

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

BOOL
lantiq_phySetup(void *arg)
{
    int unit = 0; /* lantiq  has only one phyUnit */
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
lantiq_phy_is_fdx(int unit,int phyUnit)
{
    uint16_t data;

    phyUnit = 0; /* lantiq  has only one phyUnit */

    if (lantiq_phyIsUp(unit)) {
        //printk("%s: \n", __func__);
        data = phy_reg_read(unit, PHY_ADDR(unit), MII_INTF_STATUS_REG);
        DRV_PRINT(DRV_DEBUG_PHYSTATUS,
            ("IsFullDuplex(0x%x) 0x%x\n", data, (data & MII_DUPLEX_MASK)?1:0));
        return ((data & MII_DUPLEX_MASK)?1:0);
    }
    return 0;
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
lantiq_phy_speed(int unit,int phyUnit)
{
    uint16_t data;

    phyUnit = 0; /* lantiq  has only one phyUnit */

    if (lantiq_phyIsUp(unit)) {
        //printk("%s: \n", __func__);
        data = phy_reg_read(unit, PHY_ADDR(unit), MII_INTF_STATUS_REG);
        DRV_PRINT(DRV_DEBUG_PHYSTATUS,
            ("Speed(0x%x) 0x%x\n", data, (data & MII_SPEED_MASK)));
        return (data & MII_SPEED_MASK);
    }
    return 0;
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

static int
lantiq_reg_init(void *arg)
{
   return 0;
}

int lantiqPhy_ioctl(uint32_t *args, int cmd)
{
#if 0
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
#else
    return -EINVAL;
#endif
}

unsigned int lantiq_rd_phy(int ethUnit,unsigned int phy_addr, unsigned int reg_addr)
{
		/* force unit if on AR934x board */
    if (is_ar934x()) ethUnit = 0;
#if defined(V54_BSP)
    return (phy_reg_read(ethUnit, PHY_ADDR(phy_addr), reg_addr));
#else
    return (phy_reg_read(ethUnit, phy_addr, reg_addr));
#endif
}

void lantiq_wr_phy(int ethUnit,unsigned int phy_addr, unsigned int reg_addr, unsigned int write_data)
{
		/* force unit if on AR934x board */
    if (is_ar934x()) ethUnit = 0;
#if defined(V54_BSP)
    phy_reg_write(ethUnit, PHY_ADDR(phy_addr), reg_addr, write_data);
#else
    phy_reg_write(ethUnit, phy_addr, reg_addr, write_data);
#endif
}

int lantiq_register_ops(void *arg)
{
  athr_gmac_t *mac   = (athr_gmac_t *) arg;
  athr_phy_ops_t *ops = mac->phy;

  if(!ops)
     ops = kmalloc(sizeof(athr_phy_ops_t), GFP_KERNEL);

  memset(ops,0,sizeof(athr_phy_ops_t));

  ops->mac            =  mac;
  ops->is_up          =  lantiq_phyIsUp;
  ops->is_alive       =  lantiq_phyIsUp;
  ops->speed          =  lantiq_phy_speed;
  ops->is_fdx         =  lantiq_phy_is_fdx;
  ops->ioctl          =  NULL;
  ops->setup          =  lantiq_phySetup;
  ops->stab_wr        =  NULL;
  ops->link_isr       =  NULL;
  ops->en_link_intrs  =  NULL;
  ops->dis_link_intrs =  NULL;
  ops->read_phy_reg   =  lantiq_rd_phy;
  ops->write_phy_reg  =  lantiq_wr_phy;
  ops->read_mac_reg   =  NULL;
  ops->write_mac_reg  =  NULL;
  ops->init           =  lantiq_reg_init;

  ops->port_map       =  0x1;

  mac->phy = ops;

  return 0;
}
