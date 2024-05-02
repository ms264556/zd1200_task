#include "linux/types.h"
#include "linux/delay.h"
#include "ag7100_phy.h"
#include "rtl_types.h"
#include "rtl8363_asicdrv.h"
//#include "rtl8363Phy.h"
#include "eth_diag.h"
extern char *dup_str[];

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

#define MII_PHY_RESET           0x8000
#define MII_PHY_RESTART_AN      0x0200
#define MII_PHY_DUPLEX          0X0100
#define MII_SPD_SEL_0           0x2000
#define MII_SPD_SEL_1           0x0040
#define MII_SPD_SEL_0_SHIFT     13
#define MII_SPD_SEL_1_SHIFT     5
#define MII_LINK_STATUS         0x0004

#define RTL_MAX_PORT            2
#define PHY_ADDR(port)          (port?RTL_PHY1_ADDR:RTL_PHY0_ADDR)

#define DRV_DEBUG 0
#if DRV_DEBUG
#define DRV_DEBUG_PHYSETUP  0x00000001
#define DRV_DEBUG_PHYCHANGE 0x00000002
#define DRV_DEBUG_PHYSTATUS 0x00000004

#define DRV_PRINT(FLG, X)                           \
{                                                   \
    if (PhyDebug & (FLG)) {                         \
        printk X;                                   \
    }                                               \
}
int PhyDebug = DRV_DEBUG_PHYSETUP /* | DRV_DEBUG_PHYSTATUS */;
#else /* !DRV_DEBUG */
#define DRV_PRINT(DBG_SW,X)
#endif

/*
    phyad: 0~2
    regad: 0~31
*/
uint32_t smiRead(uint32_t phyad, uint32_t regad, uint32_t *data)
{
    *data = (uint32_t)phy_reg_read(0, phyad, regad);
    return 0;
}

uint32_t smiWrite(uint32_t phyad, uint32_t regad, uint32_t data)
{
    phy_reg_write(0, phyad, regad, (uint16_t)data);
    return 0;
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
rtl8363_phySetup(int unit)
{
    phyCfg_t cfg0, cfg1;

    uint16_t data;
    uint8_t val;
    enum ChipID id;
    enum RLNum rlnum;
    enum VerNum ver;
    FwdSpecialMac_t  fwdcfg = {RTL8363_FWD_TRAPCPU, 0, 0, 0};
    int8_t ret;
    ret = rtl8363_getAsicChipID(&id, &rlnum, &ver);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
        ("ret=%d rtl id %d num %d ver %d\n", ret, id, rlnum, ver));

    rtl8363_setAsicInit();

    /* cpu port 2 speed is set to 100Mb by hardware */
    rtl8363_getAsicReg(2, 0, MACPAG, 0, &data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
        ("rtl phy2 reg0=%x\n", data));

    /* cpu port 2 tagged */
#if defined(CONFIG_VENETDEV)
    rtl8363_setAsicCPUPort(2, 1);
#else
    rtl8363_setAsicCPUPort(2, 0);
#endif

    cfg0.AutoNegotiation    = 1;
    cfg0.Speed              = PHY_1000M;
    cfg0.Fullduplex         = 1;
    cfg0.Cap_10Half         = 1;
    cfg0.Cap_10Full         = 1;
    cfg0.Cap_100Half        = 1;
    cfg0.Cap_100Full        = 1;
    cfg0.Cap_1000Full       = 0;
    cfg0.FC                 = 0;
    cfg0.AsyFC              = 0;

    cfg1.AutoNegotiation    = 1;
    cfg1.Speed              = PHY_1000M;
    cfg1.Fullduplex         = 1;
    cfg1.Cap_10Half         = 1;
    cfg1.Cap_10Full         = 1;
    cfg1.Cap_100Half        = 1;
    cfg1.Cap_100Full        = 1;
    cfg1.Cap_1000Full       = 0;
    cfg1.FC                 = 0;
    cfg1.AsyFC              = 0;

    rtl8363_setAsicEthernetPHY(0, cfg0);
    rtl8363_setAsicEthernetPHY(1, cfg1);

    rtl8363_getAsicReg(0, PHY_CONTROL_REG, UTPPAG, 0, &data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
        ("rtl phy0 reg0=%x\n", data));

    rtl8363_getAsicReg(1, PHY_CONTROL_REG, UTPPAG, 0, &data);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
        ("rtl phy1 reg0=%x\n", data));

    rtl8363_setAsicEnableAcl(1);
    rtl8363_getAsicEnableAcl(&val);
    if (val)
        printk("rtl acl enabled\n");

    rtl8363_setAsicSpecialMACFWD(RSV_00, fwdcfg);
    rtl8363_setAsicSpecialMACFWD(RSV_01, fwdcfg);
    rtl8363_setAsicSpecialMACFWD(RSV_02, fwdcfg);
    rtl8363_setAsicSpecialMACFWD(RSV_03, fwdcfg);
    rtl8363_setAsicSpecialMACFWD(RSV_04, fwdcfg);

    /*
        0x0800 	IP Internet Protocol (IPv4)
        0x0806 	Address Resolution Protocol (ARP)
        0x8035 	Reverse Address Resolution Protocol (RARP)
        0x809b 	AppleTalk (Ethertalk)
        0x80f3 	Appletalk Address Resolution Protocol (AARP)
        0x8100 	(identifies IEEE 802.1Q tag)
        0x8137 	Novell IPX (alt)
        0x8138 	Novell
        0x86DD 	Internet Protocol, Version 6 (IPv6)
        0x8847 	MPLS unicast
        0x8848 	MPLS multicast
        0x8863 	PPPoE Discovery Stage
        0x8864 	PPPoE Session Stage
    */

    if (!rtl8363_setAsicAclEntry(0, RTL8363_ACL_PORT01, RTL8363_ACT_TRAPCPU, RTL8363_ACL_ETHER, 0x0800, RTL8363_PRIO0))
        DRV_PRINT(DRV_DEBUG_PHYSETUP, ("rtl acl 0 ok\n"));

    if (!rtl8363_setAsicAclEntry(1, RTL8363_ACL_PORT01, RTL8363_ACT_TRAPCPU, RTL8363_ACL_ETHER, 0x0806, RTL8363_PRIO0))
        DRV_PRINT(DRV_DEBUG_PHYSETUP, ("rtl acl 1 ok\n"));

    if (!rtl8363_setAsicAclEntry(2, RTL8363_ACL_PORT01, RTL8363_ACT_TRAPCPU, RTL8363_ACL_ETHER, 0x8035, RTL8363_PRIO0))
        DRV_PRINT(DRV_DEBUG_PHYSETUP, ("rtl acl 2 ok\n"));

    if (!rtl8363_setAsicAclEntry(3, RTL8363_ACL_PORT01, RTL8363_ACT_TRAPCPU, RTL8363_ACL_ETHER, 0x809b, RTL8363_PRIO0))
        DRV_PRINT(DRV_DEBUG_PHYSETUP, ("rtl acl 3 ok\n"));

    if (!rtl8363_setAsicAclEntry(4, RTL8363_ACL_PORT01, RTL8363_ACT_TRAPCPU, RTL8363_ACL_ETHER, 0x80f3, RTL8363_PRIO0))
        DRV_PRINT(DRV_DEBUG_PHYSETUP, ("rtl acl 4 ok\n"));

    if (!rtl8363_setAsicAclEntry(5, RTL8363_ACL_PORT01, RTL8363_ACT_TRAPCPU, RTL8363_ACL_ETHER, 0x8100, RTL8363_PRIO0))
        DRV_PRINT(DRV_DEBUG_PHYSETUP, ("rtl acl 5 ok\n"));

    if (!rtl8363_setAsicAclEntry(6, RTL8363_ACL_PORT01, RTL8363_ACT_TRAPCPU, RTL8363_ACL_ETHER, 0x8137, RTL8363_PRIO0))
        DRV_PRINT(DRV_DEBUG_PHYSETUP, ("rtl acl 6 ok\n"));

    if (!rtl8363_setAsicAclEntry(7, RTL8363_ACL_PORT01, RTL8363_ACT_TRAPCPU, RTL8363_ACL_ETHER, 0x8138, RTL8363_PRIO0))
        DRV_PRINT(DRV_DEBUG_PHYSETUP, ("rtl acl 7 ok\n"));

    if (!rtl8363_setAsicAclEntry(8, RTL8363_ACL_PORT01, RTL8363_ACT_TRAPCPU, RTL8363_ACL_ETHER, 0x86dd, RTL8363_PRIO0))
        DRV_PRINT(DRV_DEBUG_PHYSETUP, ("rtl acl 8 ok\n"));

    if (!rtl8363_setAsicAclEntry(9, RTL8363_ACL_PORT01, RTL8363_ACT_TRAPCPU, RTL8363_ACL_ETHER, 0x8847, RTL8363_PRIO0))
        DRV_PRINT(DRV_DEBUG_PHYSETUP, ("rtl acl 9 ok\n"));

    if (!rtl8363_setAsicAclEntry(10, RTL8363_ACL_PORT01, RTL8363_ACT_TRAPCPU, RTL8363_ACL_ETHER, 0x8848, RTL8363_PRIO0))
        DRV_PRINT(DRV_DEBUG_PHYSETUP, ("rtl acl 10 ok\n"));

    if (!rtl8363_setAsicAclEntry(11, RTL8363_ACL_PORT01, RTL8363_ACT_TRAPCPU, RTL8363_ACL_ETHER, 0x8863, RTL8363_PRIO0))
        DRV_PRINT(DRV_DEBUG_PHYSETUP, ("rtl acl 11 ok\n"));

    if (!rtl8363_setAsicAclEntry(12, RTL8363_ACL_PORT01, RTL8363_ACT_TRAPCPU, RTL8363_ACL_ETHER, 0x8864, RTL8363_PRIO0))
        DRV_PRINT(DRV_DEBUG_PHYSETUP, ("rtl acl 12 ok\n"));

    if (!rtl8363_setAsicAclEntry(13, RTL8363_ACL_PORT01, RTL8363_ACT_TRAPCPU, RTL8363_ACL_ETHER, 0x88bb, RTL8363_PRIO0))
        DRV_PRINT(DRV_DEBUG_PHYSETUP, ("rtl acl 13 ok\n"));

    if (!rtl8363_setAsicAclEntry(14, RTL8363_ACL_PORT01, RTL8363_ACT_TRAPCPU, RTL8363_ACL_ETHER, 0x9100, RTL8363_PRIO0))
        DRV_PRINT(DRV_DEBUG_PHYSETUP, ("rtl acl 14 ok\n"));

    if (!rtl8363_setAsicAclEntry(15, RTL8363_ACL_PORT01, RTL8363_ACT_TRAPCPU, RTL8363_ACL_ETHER, 0x9200, RTL8363_PRIO0))
        DRV_PRINT(DRV_DEBUG_PHYSETUP, ("rtl acl 15 ok\n"));

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
rtl8363_phyIsFullDuplex(int unit)
{
    phyCfg_t cfg0, cfg1;
    rtl8363_getAsicEthernetPHY(0, &cfg0);
    rtl8363_getAsicEthernetPHY(1, &cfg1);
    DRV_PRINT(DRV_DEBUG_PHYSTATUS,
        ("rtl8363_phyIsFullDuplex %d\n", (cfg0.Fullduplex | (cfg1.Fullduplex << 1))));
    return  (cfg0.Fullduplex | (cfg1.Fullduplex << 1));
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
rtl8363_phySpeed(int unit)
{
    phyCfg_t cfg0, cfg1;
    rtl8363_getAsicEthernetPHY(0, &cfg0);
    rtl8363_getAsicEthernetPHY(1, &cfg1);
    DRV_PRINT(DRV_DEBUG_PHYSTATUS,
        ("rtl8363_phySpeed %d\n", (cfg0.Speed | (cfg1.Speed << 2))));
    return  (cfg0.Speed | (cfg1.Speed << 2));
}

int
rtl8363_phyIsUp(int unit)
{
    uint8_t link_0, link_1;
    rtl8363_getAsicPHYLinkStatus(0, &link_0);
    rtl8363_getAsicPHYLinkStatus(1, &link_1);
    DRV_PRINT(DRV_DEBUG_PHYSTATUS,
        ("rtl8363_phyIsUp %d\n", ((link_1 << 1)|link_0)));
    return ((link_1 << 1) | link_0);
}

void rtl8363_force_100M(int unit,int duplex)
{
    rtl8363_setAsicReg(unit, 0, UTPPAG, 0, (0xa000|(duplex << 8)));
}

void rtl8363_force_10M(int unit,int duplex)
{
    rtl8363_setAsicReg(unit, 0, UTPPAG, 0, (0x8000|(duplex << 8)));
}

void rtl8363_force_auto(int unit)
{
    rtl8363_setAsicReg(unit, 0, UTPPAG, 0, 0x9000);
}

int rtl8363_ioctl(uint32_t *args, int cmd)
{
    struct eth_diag *etd =(struct eth_diag *) args;
    unsigned short val;

    //printk("%s: cmd=0x%x portnum=0x%x phy_reg=0x%x etd->val=0x%x\n",
    //    __func__, cmd, etd->ed_u.portnum, etd->phy_reg, etd->val);
    if(cmd  == ATHR_RD_PHY) {
        etd->val = 0;
        if(etd->ed_u.portnum != 0xf) {
            if (!rtl8363_getAsicReg((etd->ed_u.portnum ^ 1),
                (etd->phy_reg & 0xff),          // reg addr
                ((etd->phy_reg >> 8) & 0xff),   // reg page type
                ((etd->phy_reg >> 16) & 0xff),  // reg page number
                &val)) {
                etd->val = val;
            }
        }
    } else if(cmd  == ATHR_WR_PHY) {
        if(etd->ed_u.portnum != 0xf) {
            rtl8363_setAsicReg((etd->ed_u.portnum ^ 1),
                (etd->phy_reg & 0xff),          // reg addr
                ((etd->phy_reg >> 8) & 0xff),   // reg page type
                ((etd->phy_reg >> 16) & 0xff),  // reg page number
                etd->val);
        }
    }
    else if(cmd == ATHR_FORCE_PHY) {
        if(etd->phy_reg < 2) {
            if(etd->val == 10) {
	            printk("Forcing 10Mbps %s on port:%d \n",
                    dup_str[etd->ed_u.duplex], (etd->phy_reg));
                rtl8363_force_10M((etd->phy_reg ^ 1), etd->ed_u.duplex);
            } else if(etd->val == 100) {
	            printk("Forcing 100Mbps %s on port:%d \n",
                    dup_str[etd->ed_u.duplex],(etd->phy_reg));
                rtl8363_force_100M((etd->phy_reg ^ 1), etd->ed_u.duplex);
            } else if((etd->val == 0) || (etd->val == 1)) {
	            printk("Enabling Auto Neg on port:%d \n",(etd->phy_reg));
                rtl8363_force_auto(etd->phy_reg ^ 1);
            } else
                return -EINVAL;
        } else {
            return -EINVAL;
        }
    }
    else
        return -EINVAL;

    return 0;
}
