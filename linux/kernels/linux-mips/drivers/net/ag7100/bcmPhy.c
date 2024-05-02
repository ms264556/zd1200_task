/*
 * $Id: phy.c,v 1.9 2005/01/29 00:48:01 sdk Exp $
 * $Copyright: (c) 2005 Broadcom Corp.
 * All Rights Reserved.$
 *
 * File: 	bcmPhy.c
 * Purpose:	Defines PHY driver routines, and standard 802.3 10/100/1000
 *		PHY interface routines.
 */


#ifndef __ECOS
#include "linux/types.h"
#include "linux/delay.h"
#endif
#include "bcmPhy.h"
#include "ag7100_phy.h"

#define TRUE    1
#define FALSE   0

#define BCM_PHY_MAX             5
#define PHYRD_TIMEOUT           500

#define MII_CTRL_REG            0
#define MII_PHY_RESET           0x8000
#define MII_PHY_ID0_REG         2
#define MII_PHY_ID1_REG         3

#define PSEUDO_PHY_ADDR         0x1e
#define PSEUDO_REG_PAGE_NUM     0x10
#define PSEUDO_REG_ADDR         0x11
#define PSEUDO_REG_STAT         0x12
#define PSEUDO_REG_BITS_0       0x18

#define PAGE_CTRL_REGS          0
#define PORT0_TRAFFIC_CTRL_REG  0
#define IMP_PORT_CTRL_REG       0x08
#define SWITCH_MODE_REG         0x0b
#define IMP_OVERRIDE_REG        0x0e
#define LED_REFRESH_CTRL_REG    0x0f
#define LED_FUNC0_CTRL_REG      0x10
#define LED_FUNC1_CTRL_REG      0x12
#define LED_FUNC_MAP_CTRL_REG   0x14
#define LED_ENABLE_MAP_REG      0x16
#define LED_MODE_MAP0_REG       0x18
#define LED_MODE_MAP1_REG       0x1a
#define PORT_FWRD_CTRL_REG      0x21
#define UNICAST_LU_FAILED_REG   0x32
#define DISABLE_LEARN_REG       0x3c

#define PROTECTED_PORT_REG      0x24
#define WAN_PORT_SEL_REG        0x26
#define SW_RESET_CTRL_REG       0x79
#define SW_RESET                (1 << 7)

#define PAGE_STAT_REGS          1
#define LINK_STAT_REG           0
#define LINK_STAT_CHANGE_REG    2
#define PORT_SPEED_REG          4
#define DUPLEX_STATUS_REG       8
#define LINK_STAT_MASK          0x1f    /* bits[4:0]=>port[4:0] */
#define LINK_STAT_IMP_MASK      0x100   /* bit8=>IMP port */

#define PAGE_MGNT_REGS          2
#define MGNT_CONF_REG           0

#define PAGE_GPHY_MII_REGS      0x10
#define PAGE_PORT_MIB_REGS      0x20
#define IS_GPHY_MII(p) ((p>=PAGE_GPHY_MII_REGS)&&(p<PAGE_PORT_MIB_REGS)) ? 1 : 0

#define PAGE_PORT_VLAN_REGS     0x31
#define PORT_0_VLAN_CTRL_REG    0
#define PORT_1_VLAN_CTRL_REG    2
#define PORT_2_VLAN_CTRL_REG    4
#define PORT_3_VLAN_CTRL_REG    6
#define PORT_4_VLAN_CTRL_REG    8
#define PORT_VLAN_IMP_FWRD      0x0100

#define PAGE_VLAN_REGS          0x34
#define VLAN_ENABLE_REG         0
#define VLAN_CTRL1_REG          1
#define VLAN_CTRL2_REG          2
#define VLAN_CTRL3_REG          3
#define VLAN_CTRL4_REG          5
#define VLAN_CTRL5_REG          6
#define VLAN_PORT0_TAG_REG      0x10

#define VLAN_ENABLE             0x80
#define VLAN_CRC_BYPASS         0x02
#define VLAN_CRC_REGEN          0x01

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
    if (bcmPhyDebug & (FLG)) {                      \
        logMsg(X0, X1, X2, X3, X4, X5, X6);         \
    }                                               \
}

#define DRV_MSG(x,a,b,c,d,e,f)                      \
    logMsg(x,a,b,c,d,e,f)

#ifdef __ECOS
#define DRV_PRINT(FLG, X)                           \
{                                                   \
    if (bcmPhyDebug & (FLG)) {                      \
        diag_printf X;                                   \
    }                                               \
}
#else
#define DRV_PRINT(FLG, X)                           \
{                                                   \
    if (bcmPhyDebug & (FLG)) {                      \
        printk X;                                   \
    }                                               \
}
#endif

#if 1
int bcmPhyDebug = DRV_DEBUG_PHYSETUP;
#else
int bcmPhyDebug = DRV_DEBUG_PHYSETUP | DRV_DEBUG_PHYSTATUS;
#endif
#else /* !DRV_DEBUG */
#define DRV_LOG(DBG_SW, X0, X1, X2, X3, X4, X5, X6)
#define DRV_MSG(x,a,b,c,d,e,f)
#define DRV_PRINT(DBG_SW,X)
#endif

u8 *bcm_mib[(LAST_MIB_REGS/4)+1] =
{
    "TxOctets(L)            ",
    "TxOctets(H)            ",
    "TxDropPkts             ",
    "TxQosPkts              ",
    "TxBroadcastPkts        ",
    "TxMulticastPkts        ",
    "TxUnicastPkts          ",
    "TxCollisions           ",
    "TxSingleCollision      ",
    "TxMultipleCollision    ",
    "TxDeferredTransmit     ",
    "TxLateCollision        ",
    "TxExcessiveCollision   ",
    "TxFrameInDisc          ",
    "TxPausePkts            ",
    "TxQosOctets(L)         ",
    "TxQosOctets(H)         ",
    "RxOctets(L)            ",
    "RxOctets(H)            ",
    "RxUndersizePkts        ",
    "RxPausePkts            ",
    "Pkts64Octets           ",
    "Pkts65to127Octets      ",
    "Pkts128to255Octets     ",
    "Pkts256to511Octets     ",
    "Pkts512to1023ctets     ",
    "Pkts1024to1522ctets    ",
    "RxOversizePkts         ",
    "RxJabbers              ",
    "RxAlignmentErrors      ",
    "RxFCSErrors            ",
    "RxGoodOctets(L)        ",
    "RxGoodOctets(H)        ",
    "RxDropPkts             ",
    "RxUnicastPkts          ",
    "RxMulticastPkts        ",
    "RxBroadcastPkts        ",
    "RxSAChanges            ",
    "RxFragments            ",
    "RxExcessSizeDisc       ",
    "RxSymbolError          ",
    "RxQosPkts              ",
    "RxQosOctets(L)         ",
    "RxQosOctets(H)         ",
    "Pkts1523to2047         ",
    "Pkts2048to4095         ",
    "Pkts4096to8191         ",
    "Pkts8192to9728         "
};

int bcm_pseudo_read(int unit, uint8_t pg, uint8_t reg, uint16_t *data);
int bcm_pseudo_write(int unit, uint8_t pg, uint8_t reg, uint16_t *data);

static void bcmPhy_led(int unit)
{
    uint16_t data[4]={0, 0, 0, 0};

    /* check 40ms/12Hz is set */
    bcm_pseudo_read(unit, PAGE_CTRL_REGS, LED_REFRESH_CTRL_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: led refresh reg(0x%x)\n", __FUNCTION__, data[0]));

    data[0] = 0x0330;
    bcm_pseudo_write(unit, PAGE_CTRL_REGS, LED_FUNC0_CTRL_REG, data);
    bcm_pseudo_read(unit, PAGE_CTRL_REGS, LED_FUNC0_CTRL_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: led function 0 reg(0x%x)\n", __FUNCTION__, data[0]));

    /* don't care */
    bcm_pseudo_read(unit, PAGE_CTRL_REGS, LED_FUNC1_CTRL_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: led function 1 reg(0x%x)\n", __FUNCTION__, data[0]));

    data[0] = 0x0000;
    bcm_pseudo_write(unit, PAGE_CTRL_REGS, LED_FUNC_MAP_CTRL_REG, data);
    bcm_pseudo_read(unit, PAGE_CTRL_REGS, LED_FUNC_MAP_CTRL_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: led function map reg(0x%x)\n", __FUNCTION__, data[0]));

    data[0] = 0x1f;
    bcm_pseudo_write(unit, PAGE_CTRL_REGS, LED_ENABLE_MAP_REG, data);
    bcm_pseudo_read(unit, PAGE_CTRL_REGS, LED_ENABLE_MAP_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: led enable map reg(0x%x)\n", __FUNCTION__, data[0]));

    data[0] = 0x1f;
    bcm_pseudo_write(unit, PAGE_CTRL_REGS, LED_MODE_MAP0_REG, data);
    bcm_pseudo_read(unit, PAGE_CTRL_REGS, LED_MODE_MAP0_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: led mode map 0 reg(0x%x)\n", __FUNCTION__, data[0]));

    data[0] = 0x1f;
    bcm_pseudo_write(unit, PAGE_CTRL_REGS, LED_MODE_MAP1_REG, data);
    bcm_pseudo_read(unit, PAGE_CTRL_REGS, LED_MODE_MAP1_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: led mode map 1 reg(0x%x)\n", __FUNCTION__, data[0]));
}

/******************************************************************************
*
* bcm_phySetup - Setup the PHY associated with
*                the specified MAC unit number.
*
*         Initializes the associated PHY port.
*
*/

int
bcm_phySetup(int unit)
{
    uint16_t data[4]={0, 0, 0, 0};
    int port;
    int phy;

    if (unit > 0) {
        /* one gmii only  */
        INFO_PRINT("%s: invalid unit %d\n", __FUNCTION__, unit);
        return TRUE;
    }

    for (phy=0; phy<BCM_PHY_MAX; phy++) {
        phy_reg_write(unit, phy, MII_CTRL_REG, MII_PHY_RESET);
    }
    mdelay(300);

    for (port=PORT0_TRAFFIC_CTRL_REG; port<(PORT0_TRAFFIC_CTRL_REG+BCM_PHY_MAX); port++) {
        bcm_pseudo_read(unit, PAGE_CTRL_REGS, port, data);
        DRV_PRINT(DRV_DEBUG_PHYSETUP,
                ("%s: port %d traffic ctrl(0x%x)\n", __FUNCTION__, port, data[0]));
    }

    bcm_pseudo_read(unit, PAGE_CTRL_REGS, IMP_PORT_CTRL_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: imp port ctrl(0x%x)\n", __FUNCTION__, data[0]));
    data[0] = 0x001c;       /* ucst|mcst|bcst */
    bcm_pseudo_write(unit, PAGE_CTRL_REGS, IMP_PORT_CTRL_REG, data);
    bcm_pseudo_read(unit, PAGE_CTRL_REGS, IMP_PORT_CTRL_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: imp port ctrl(0x%x)\n", __FUNCTION__, data[0]));

    /* check sw_fwdg_en is set */
    bcm_pseudo_read(unit, PAGE_CTRL_REGS, SWITCH_MODE_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: switch mode reg(0x%x)\n", __FUNCTION__, data[0]));

#ifdef CONFIG_VENETDEV
    data[0] = 0x0007;       /* sw_fwdg_en|sw_fwdg_mode */
    bcm_pseudo_write(unit, PAGE_CTRL_REGS, SWITCH_MODE_REG, data);
    bcm_pseudo_read(unit, PAGE_CTRL_REGS, SWITCH_MODE_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: switch mode reg(0x%x)\n", __FUNCTION__, data[0]));

    bcm_pseudo_read(unit, PAGE_MGNT_REGS, MGNT_CONF_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: mgnt config reg(0x%x)\n", __FUNCTION__, data[0]));
    data[0] = 0x0080;       /* imp port enabled for frame management */
    bcm_pseudo_write(unit, PAGE_MGNT_REGS, MGNT_CONF_REG, data);
    bcm_pseudo_read(unit, PAGE_MGNT_REGS, MGNT_CONF_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: mgnt config reg(0x%x)\n", __FUNCTION__, data[0]));

#if 0
    bcm_pseudo_read(unit, PAGE_CTRL_REGS, WAN_PORT_SEL_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: wan port select reg(0x%x)\n", __FUNCTION__, data[0]));
    data[0] = 0x0210;
    bcm_pseudo_write(unit, PAGE_CTRL_REGS, WAN_PORT_SEL_REG, data);
    bcm_pseudo_read(unit, PAGE_CTRL_REGS, WAN_PORT_SEL_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: wan port select reg(0x%x)\n", __FUNCTION__, data[0]));

    bcm_pseudo_read(unit, PAGE_VLAN_REGS, VLAN_ENABLE_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: vlan enable reg(0x%x)\n", __FUNCTION__, data[0]));
    data[0] |= VLAN_ENABLE;
    bcm_pseudo_write(unit, PAGE_VLAN_REGS, VLAN_ENABLE_REG, data);
    bcm_pseudo_read(unit, PAGE_VLAN_REGS, VLAN_ENABLE_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: vlan enable reg(0x%x)\n", __FUNCTION__, data[0]));
#endif

    bcm_pseudo_read(unit, PAGE_VLAN_REGS, VLAN_CTRL5_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: vlan ctrl 5 reg(0x%x)\n", __FUNCTION__, data[0]));
    data[0] |= (VLAN_CRC_BYPASS | VLAN_CRC_REGEN);
    bcm_pseudo_write(unit, PAGE_VLAN_REGS, VLAN_CTRL5_REG, data);
    bcm_pseudo_read(unit, PAGE_VLAN_REGS, VLAN_CTRL5_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: vlan ctrl 5 reg(0x%x)\n", __FUNCTION__, data[0]));

#if 0
    bcm_pseudo_read(unit, PAGE_CTRL_REGS, PORT_FWRD_CTRL_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: port fwrd ctrl reg(0x%x)\n", __FUNCTION__, data[0]));
    data[0] = 0x40;
    bcm_pseudo_write(unit, PAGE_CTRL_REGS, PORT_FWRD_CTRL_REG, data);
    bcm_pseudo_read(unit, PAGE_CTRL_REGS, PORT_FWRD_CTRL_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: port fwrd ctrl reg(0x%x)\n", __FUNCTION__, data[0]));

    bcm_pseudo_read(unit, PAGE_CTRL_REGS, UNICAST_LU_FAILED_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: unicast lu failed reg(0x%x)\n", __FUNCTION__, data[0]));
    data[0] = 0x0100;
    bcm_pseudo_write(unit, PAGE_CTRL_REGS, UNICAST_LU_FAILED_REG, data);
    bcm_pseudo_read(unit, PAGE_CTRL_REGS, UNICAST_LU_FAILED_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: unicast lu failed reg(0x%x)\n", __FUNCTION__, data[0]));
#endif
#endif

    bcmPhy_led(unit);

    bcm_pseudo_read(unit, PAGE_CTRL_REGS, IMP_OVERRIDE_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: imp override reg(0x%x)\n", __FUNCTION__, data[0]));
    data[0] = 0x008b;       /* sw override, 1G, full duplex, link pass */
    bcm_pseudo_write(unit, PAGE_CTRL_REGS, IMP_OVERRIDE_REG, data);
    bcm_pseudo_read(unit, PAGE_CTRL_REGS, IMP_OVERRIDE_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: imp override reg(0x%x)\n", __FUNCTION__, data[0]));

    return TRUE;
}

/******************************************************************************
*
* bcm_phyIsDuplexFull - Determines whether the phy ports associated with the
* specified device are FULL or HALF duplex.
*
* RETURNS:
*    TRUE  --> FULL
*    FALSE --> HALF
*/
int
bcm_phyIsFullDuplex(int unit)
{
    uint16_t data[4]={0, 0, 0, 0};

    bcm_pseudo_read(unit, PAGE_STAT_REGS, DUPLEX_STATUS_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSTATUS,
            ("\nduplex state(0x%x)\n", data[0]));

    return ((data[0]&0x0100)?1:0);    /* return imp port duplex */

}

/******************************************************************************
*
* bcm_phySpeed - Determines the speed of phy ports associated with the
* specified device.
*
* RETURNS:
*    phy speed - 0:10M, 1:100M, 2:1G (ag7100_phy_speed_t)
*/
int
bcm_phySpeed(int unit)
{
    uint16_t data[4]={0, 0, 0, 0};

    bcm_pseudo_read(unit, PAGE_STAT_REGS, PORT_SPEED_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSTATUS,
            ("\nport speed(0x%x)\n", ((data[1]<<16)|data[0])));

    return (data[1]&3);     /* return imp port speed */

}

int
bcm_phyIsUp(int unit)
{
    uint16_t data[4]={0, 0, 0, 0};
    uint16_t linkStatus, linkStatusChange;
    uint16_t spd;
    int port, speed;

    bcm_pseudo_read(unit, PAGE_STAT_REGS, LINK_STAT_REG, data);
    linkStatus = data[0];
    DRV_PRINT(DRV_DEBUG_PHYSTATUS,
            ("\nlinkStatus=0x%x\n", linkStatus));

    bcm_pseudo_read(unit, PAGE_STAT_REGS, LINK_STAT_CHANGE_REG, data);
    linkStatusChange = data[0];
    DRV_PRINT(DRV_DEBUG_PHYSTATUS,
            ("\nlinkStatusChange(0x%x)\n", linkStatusChange));

    if (linkStatusChange & LINK_STAT_MASK) {
        for (port=0; port<BCM_PHY_MAX; port++) {
            if ((linkStatusChange>>port) & 1) {
                if ((linkStatus>>port) & 1) {
                    bcm_pseudo_read(unit, PAGE_STAT_REGS, PORT_SPEED_REG, data);
                    spd = (data[0]>>(port<<1)) & 0x3;
                    switch (spd) {
                        case 2:
                            speed = 1000;
                            break;
                        case 1:
                            speed = 100;
                            break;
                        default:
                            speed = 10;
                            break;
                    }
                    bcm_pseudo_read(unit, PAGE_STAT_REGS, DUPLEX_STATUS_REG, data);
                    INFO_PRINT("port-%d %dMbps %s\n",
                        port, speed, (((data[0]>>port)&0x1)?"full":"half"));

                } else {
                    INFO_PRINT("port-%d down\n", port);
                }
            }
        }
    }

    return (linkStatus & LINK_STAT_MASK);
}

unsigned int bcm_phyID(int unit)
{
    return ((phy_reg_read(unit, 0, MII_PHY_ID1_REG) << 16) |
            phy_reg_read(unit, 0, MII_PHY_ID0_REG));
}

int bcm_stats(int unit, uint8_t **stats)
{
    uint16_t data[BCM_PHY_MAX][4] = { {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0} };
    int mibReg, mibPort;
    char *buf;

    buf = *stats;

    buf += sprintf(buf, "%s\n", "Port MIB:");
    for (mibReg=0; mibReg <= LAST_MIB_REGS; mibReg+=4) {
        buf += sprintf(buf," %s", bcm_mib[mibReg/4]);
        for (mibPort=0; mibPort<BCM_PHY_MAX; mibPort++) {
            bcm_pseudo_read(unit, (PAGE_PORT_MIB_REGS+mibPort), mibReg, data[mibPort]);
            buf += sprintf(buf," %10u", (data[mibPort][0] | (data[mibPort][1]<<16)));
        }
        buf += sprintf(buf,"\n");
        if ((mibReg == 0) || (mibReg == 0x3c) || (mibReg == 0x44) ||
            (mibReg == 0x7c) || (mibReg == 0xa8) ) {
            mibReg += 4;
            buf += sprintf(buf," %s", bcm_mib[mibReg/4]);
            for (mibPort=0; mibPort<BCM_PHY_MAX; mibPort++) {
                bcm_pseudo_read(unit, (PAGE_PORT_MIB_REGS+mibPort), mibReg, data[mibPort]);
                buf += sprintf(buf," %10u", (data[mibPort][0] | (data[mibPort][1]<<16)));
            }
            buf += sprintf(buf,"\n");
        }
    }

    *stats = buf;
    return TRUE;
}

void bcm_reset_stats(int unit)
{
    uint16_t data[4]={0, 0, 0, 0};

    bcm_pseudo_read(unit, PAGE_MGNT_REGS, MGNT_CONF_REG, data);
    data[0] |= 1;       /* reset mib */
    bcm_pseudo_write(unit, PAGE_MGNT_REGS, MGNT_CONF_REG, data);
    data[0] &= ~1;      /* clear reset */
    bcm_pseudo_write(unit, PAGE_MGNT_REGS, MGNT_CONF_REG, data);
}

void bcm_port_protect(int unit, int on)
{
    uint16_t data[4]={0, 0, 0, 0};

    bcm_pseudo_read(unit, PAGE_CTRL_REGS, PROTECTED_PORT_REG, data);
    if (on)
        data[0] |= 0x000f;
    else
        data[0] &= ~0x000f;
    bcm_pseudo_write(unit, PAGE_CTRL_REGS, PROTECTED_PORT_REG, data);
}

void bcm_port_vlan(int unit, int port, uint16_t mask)
{
    uint16_t data[4]={0, 0, 0, 0};

    data[0] = PORT_VLAN_IMP_FWRD | mask;    /* always forward to imp port */
    bcm_pseudo_write(unit, PAGE_PORT_VLAN_REGS, (port<<1), data);
}

void bcm_disable_learn(int unit, uint16_t mask)
{
    uint16_t data[4]={0, 0, 0, 0};

    data[0] = mask;
    bcm_pseudo_write(unit, PAGE_CTRL_REGS, DISABLE_LEARN_REG, data);
    bcm_pseudo_read(unit, PAGE_CTRL_REGS, DISABLE_LEARN_REG, data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("%s: disable learning reg(0x%x)\n", __FUNCTION__, data[0]));
}

/*
    bcm_pseudo_read()

    pg: page number
    reg: register address
    data: pointer to 8-bytes caller-allocated memory
*/
int
bcm_pseudo_read(int unit, uint8_t pg, uint8_t reg, uint16_t *data)
{
    volatile int    rddata;
    int             i;

    if (IS_GPHY_MII(pg)) {
        INFO_PRINT("%s: invalid page number 0x%x\n", __FUNCTION__, pg);
        return FALSE;
    }

    phy_reg_write(unit, PSEUDO_PHY_ADDR, PSEUDO_REG_PAGE_NUM, ((pg<<8)|0x01));

    phy_reg_write(unit, PSEUDO_PHY_ADDR, PSEUDO_REG_ADDR, ((reg<<8)|0x02));

    i = PHYRD_TIMEOUT;
    do {
        rddata = phy_reg_read(unit, PSEUDO_PHY_ADDR, PSEUDO_REG_ADDR) & 0x03;
        i--;
    } while(rddata && i);

    if (rddata && !i) {
        INFO_PRINT("%s: check op code timeout\n", __FUNCTION__);
        return FALSE;
    }

    for (i=0; i<4; i++) {
        *(data+i) = phy_reg_read(unit, PSEUDO_PHY_ADDR, (PSEUDO_REG_BITS_0+i));
    }

    return TRUE;
}

/*
    bcm_pseudo_write()

    pg: page number
    reg: register address
    data: pointer to 8-bytes caller-allocated memory
*/
int
bcm_pseudo_write(int unit, uint8_t pg, uint8_t reg, uint16_t *data)
{
    volatile int    rddata;
    int             i;

    if (IS_GPHY_MII(pg)) {
        INFO_PRINT("%s: invalid page number 0x%x\n", __FUNCTION__, pg);
        return FALSE;
    }

    phy_reg_write(unit, PSEUDO_PHY_ADDR, PSEUDO_REG_PAGE_NUM, ((pg<<8)|0x01));

    for (i=0; i<4; i++) {
        phy_reg_write(unit, PSEUDO_PHY_ADDR, (PSEUDO_REG_BITS_0+i), *(data+i));
    }

    phy_reg_write(unit, PSEUDO_PHY_ADDR, PSEUDO_REG_ADDR, ((reg<<8)|0x01));

    i = PHYRD_TIMEOUT;
    do {
        rddata = phy_reg_read(unit, PSEUDO_PHY_ADDR, PSEUDO_REG_ADDR) & 0x03;
        i--;
    } while(rddata && i);

    if (rddata && !i) {
        INFO_PRINT("%s: check op code timeout\n", __FUNCTION__);
        return FALSE;
    }

    return TRUE;
}
