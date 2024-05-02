/*
 * Copyright (c) 2008, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

//#include <linux/config.h>
#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>
#include "ag7100_phy.h"
#include "ag7100.h"
#include "eth_diag.h"

#define MODULE_NAME "ATHRSF1_PHY"

#define ATHR_LAN_PORT_VLAN          1
#define ATHR_WAN_PORT_VLAN          2
#define ENET_UNIT_LAN 1
#define ENET_UNIT_WAN 0

#define TRUE    1
#define FALSE   0

#define ATHR_PHY_MAX 5
#define ATHR_PHY0_ADDR   0x0
#define ATHR_PHY1_ADDR   0x1
#define ATHR_PHY2_ADDR   0x2
#define ATHR_PHY3_ADDR   0x3
#define ATHR_PHY4_ADDR   0x4
#define ATHR_PHY6_ADDR   0x6

/*
 * Track per-PHY port information.
 */
typedef struct {
    BOOL   isEnetPort;       /* normal enet port */
    BOOL   isPhyAlive;       /* last known state of link */
    int    ethUnit;          /* MAC associated with this phy port */
    uint32_t phyBase;
    uint32_t phyAddr;          /* PHY registers associated with this phy port */
    uint32_t VLANTableSetting; /* Value to be written to VLAN table */
} athrPhyInfo_t;

/*
 * Per-PHY information, indexed by PHY unit number.
 */

static athrPhyInfo_t athrPhyInfo[] = {

    {TRUE,   /* port 1 -- LAN port 1 */
     FALSE,
     ENET_UNIT_LAN,
     0,
     ATHR_PHY6_ADDR,
     ATHR_LAN_PORT_VLAN
    },

    {TRUE,   /* port 2 -- LAN port 2 */
     FALSE,
     ENET_UNIT_LAN,
     0,
     ATHR_PHY1_ADDR,
     ATHR_LAN_PORT_VLAN
    },

    {TRUE,   /* port 3 -- LAN port 3 */
     FALSE,
     ENET_UNIT_LAN,
     0,
     ATHR_PHY2_ADDR,
     ATHR_LAN_PORT_VLAN
    },

    {TRUE,   /* port 4 --  LAN port 4 */
     FALSE,
     ENET_UNIT_LAN,
     0,
     ATHR_PHY3_ADDR,
     ATHR_LAN_PORT_VLAN   /* Send to all ports */
    },

    {TRUE,  /* port 5 -- WAN Port 5 */
     FALSE,
     ENET_UNIT_WAN,
     0,
     ATHR_PHY0_ADDR,
     ATHR_LAN_PORT_VLAN    /* Send to all ports */
    },

    {FALSE,   /* port 0 -- cpu port 0 */
     TRUE,
     ENET_UNIT_LAN,
     0,
     0x00,
     ATHR_LAN_PORT_VLAN
    },

};

#define ATHR_IS_ENET_PORT(phyUnit) (athrPhyInfo[phyUnit].isEnetPort)
#define ATHR_IS_PHY_ALIVE(phyUnit) (athrPhyInfo[phyUnit].isPhyAlive)
#define ATHR_ETHUNIT(phyUnit) (athrPhyInfo[phyUnit].ethUnit)
#define ATHR_PHYBASE(phyUnit) (athrPhyInfo[phyUnit].phyBase)
#define ATHR_PHYADDR(phyUnit) (athrPhyInfo[phyUnit].phyAddr)
#define ATHR_VLAN_TABLE_SETTING(phyUnit) (athrPhyInfo[phyUnit].VLANTableSetting)

#define ATHR_IS_ETHUNIT(phyUnit, ethUnit) \
            (ATHR_IS_ENET_PORT(phyUnit) &&        \
            ATHR_ETHUNIT(phyUnit) == (ethUnit))

#define ATHR_IS_WAN_PORT(phyUnit) (!(ATHR_ETHUNIT(phyUnit)==ENET_UNIT_LAN))

/* Forward references */
BOOL athr_phy_is_link_alive(int phyUnit);
//extern int ag7240_check_link(ag7240_mac_t *);
extern char *dup_str[];

void athr_enable_linkIntrs(int ethUnit)
{
    return;
}

void athr_disable_linkIntrs(int ethUnit)
{
    return;
}
void athr_auto_neg(int ethUnit,int phyUnit)
{
    int timeout = 0;
    uint16_t phyHwStatus;


    phy_reg_write(ethUnit, ATHR_PHYADDR(phyUnit) , ATHR_AUTONEG_ADVERT, ATHR_ADVERTISE_ALL);
    phy_reg_write(ethUnit, ATHR_PHYADDR(phyUnit) , ATHR_1000BASET_CONTROL, (ATHR_ADVERTISE_1000FULL));
    phy_reg_write(ethUnit, ATHR_PHYADDR(phyUnit) , ATHR_PHY_CONTROL, ATHR_CTRL_AUTONEGOTIATION_ENABLE | ATHR_CTRL_SOFTWARE_RESET);

    phy_reg_write(ethUnit, ATHR_PHYADDR(phyUnit) , 0x1d, 0xb);
    phyHwStatus = phy_reg_read(ethUnit, ATHR_PHYADDR(phyUnit), 0x1e);
    /* set gtx clock delay to 1.3ns */
    phy_reg_write(ethUnit, ATHR_PHYADDR(phyUnit) , 0x1e, ((phyHwStatus & 0xff9f) | 0x20));

    phy_reg_write(ethUnit, ATHR_PHYADDR(phyUnit) , 0x1d, 0x5);
    phyHwStatus = phy_reg_read(ethUnit, ATHR_PHYADDR(phyUnit), 0x1e);
    /* enable rgmii tx clock delay control */
    phy_reg_write(ethUnit, ATHR_PHYADDR(phyUnit) , 0x1e, phyHwStatus | 0x100);

   /*
     * Wait up to 3 seconds for ALL associated PHYs to finish
     * autonegotiation.  The only way we get out of here sooner is
     * if ALL PHYs are connected AND finish autonegotiation.
     */
    timeout=20;
    for (;;) {
        phyHwStatus = phy_reg_read(ethUnit, ATHR_PHYADDR(phyUnit), ATHR_PHY_CONTROL);

        if (ATHR_RESET_DONE(phyHwStatus)) {
            printk(MODULE_NAME": Port %d, Neg Success\n", phyUnit);
            break;
        }
        if (timeout == 0) {
            printk(MODULE_NAME": Port %d, Negogiation timeout\n", phyUnit);
            break;
        }
        if (--timeout == 0) {
            printk(MODULE_NAME": Port %d, Negogiation timeout\n", phyUnit);
            break;
        }

        mdelay(150);
    }

    printk(MODULE_NAME": unit %d phy addr %x ", ethUnit, ATHR_PHYADDR(phyUnit));
}

/******************************************************************************
*
* athr_phy_is_link_alive - test to see if the specified link is alive
*
* RETURNS:
*    TRUE  --> link is alive
*    FALSE --> link is down
*/
BOOL
athr_phy_is_link_alive(int phyUnit)
{
    uint16_t phyHwStatus;
    uint32_t phyBase;
    uint32_t phyAddr;

    phyBase = ATHR_PHYBASE(phyUnit);
    phyAddr = ATHR_PHYADDR(phyUnit);
    phyHwStatus = phy_reg_read(0, phyAddr, ATHR_PHY_SPEC_STATUS);

    if (phyHwStatus & ATHR_STATUS_LINK_PASS) {
        return TRUE;
    }

    return FALSE;
}

/******************************************************************************
*
* athr_phy_setup - reset and setup the PHY associated with
* the specified MAC unit number.
*
* Resets the associated PHY port.
*
* RETURNS:
*    TRUE  --> associated PHY is alive
*    FALSE --> no LINKs on this ethernet unit
*/

BOOL
athr_phy_setup(int ethUnit)
{
    int       phyUnit = 0;
    int       liveLinks = 0;

    athr_auto_neg(ethUnit,phyUnit);

    if (athr_phy_is_link_alive(phyUnit)) {
        liveLinks++;
        ATHR_IS_PHY_ALIVE(phyUnit) = TRUE;
    } else {
        ATHR_IS_PHY_ALIVE(phyUnit) = FALSE;
    }
    return (liveLinks > 0);
}

/******************************************************************************
*
* athr_phy_is_fdx - Determines whether the phy ports associated with the
* specified device are FULL or HALF duplex.
*
* RETURNS:
*    1 --> FULL
*    0 --> HALF
*/
int
#if !defined(V54_BSP)
athr_phy_is_fdx(int ethUnit,int phyUnit)
{
#else
athr_phy_is_fdx(int ethUnit)
{
    int       phyUnit = 0;
#endif
    uint32_t  phyBase;
    uint32_t  phyAddr;
    uint16_t  phyHwStatus;
#if !defined(V54_BSP)
    int       ii = 200;
#else
    int       ii = 1;
#endif

    if (athr_phy_is_link_alive(phyUnit)) {

         phyBase = ATHR_PHYBASE(phyUnit);
         phyAddr = ATHR_PHYADDR(phyUnit);

         do {
                phyHwStatus = phy_reg_read(ethUnit, ATHR_PHYADDR(phyUnit),ATHR_PHY_SPEC_STATUS);
#if !defined(V54_BSP)
                mdelay(10);
#endif
          } while((!(phyHwStatus & ATHR_STATUS_RESOVLED)) && --ii);

          if (phyHwStatus & ATHER_STATUS_FULL_DUPLEX) {
                return TRUE;
          }
    }
    return FALSE;
}

/******************************************************************************
*
* athr_phy_speed - Determines the speed of phy ports associated with the
* specified device.
*
* RETURNS:
*               AG7240_PHY_SPEED_10T, AG7240_PHY_SPEED_100TX;
*               AG7240_PHY_SPEED_1000T;
*/

int
#if !defined(V54_BSP)
athr_phy_speed(int ethUnit,int phyUnit)
{
#else
athr_phy_speed(int ethUnit)
{
    int       phyUnit = 0;
#endif
    uint16_t  phyHwStatus;
    uint32_t  phyBase;
    uint32_t  phyAddr;
#if !defined(V54_BSP)
    int       ii = 200;
#else
    int       ii = 1;
#endif


    if (athr_phy_is_link_alive(phyUnit)) {

        phyBase = ATHR_PHYBASE(phyUnit);
        phyAddr = ATHR_PHYADDR(phyUnit);
        do {
            phyHwStatus = phy_reg_read(0, ATHR_PHYADDR(phyUnit),ATHR_PHY_SPEC_STATUS);
#if !defined(V54_BSP)
            mdelay(10);
#endif
        } while((!(phyHwStatus & ATHR_STATUS_RESOVLED)) && --ii);

        phyHwStatus = ((phyHwStatus & ATHER_STATUS_LINK_MASK) >>
                       ATHER_STATUS_LINK_SHIFT);

        switch(phyHwStatus) {
        case 0:
            return AG7240_PHY_SPEED_10T;
        case 1:
            return AG7240_PHY_SPEED_100TX;
        case 2:
            return AG7240_PHY_SPEED_1000T;
        default:
            printk("Unkown speed read!\n");
        }
    }

    //printk("athr_phy_speed: link down, returning 10t\n");
    return AG7240_PHY_SPEED_10T;
}

/*****************************************************************************
*
* athr_phy_is_up -- checks for significant changes in PHY state.
*
* A "significant change" is:
*     dropped link (e.g. ethernet cable unplugged) OR
*     autonegotiation completed + link (e.g. ethernet cable plugged in)
*
* When a PHY is plugged in, phyLinkGained is called.
* When a PHY is unplugged, phyLinkLost is called.
*/

int
athr_phy_is_up(int ethUnit)
{
    int           phyUnit;
    uint16_t      phyHwStatus, phyHwControl;
    athrPhyInfo_t *lastStatus;
    int           linkCount   = 0;
    int           lostLinks   = 0;
    int           gainedLinks = 0;
    uint32_t      phyBase;
    uint32_t      phyAddr;

    for (phyUnit=0; phyUnit < 1; phyUnit++) {

        phyBase = ATHR_PHYBASE(phyUnit);
        phyAddr = ATHR_PHYADDR(phyUnit);

        lastStatus = &athrPhyInfo[phyUnit];

        if (lastStatus->isPhyAlive) { /* last known link status was ALIVE */

             phyHwStatus = phy_reg_read(0, ATHR_PHYADDR(phyUnit),ATHR_PHY_SPEC_STATUS);

            /* See if we've lost link */
            if (phyHwStatus & ATHR_STATUS_LINK_PASS) { /* check realtime link */
                linkCount++;
            } else {
                phyHwStatus = phy_reg_read(0, ATHR_PHYADDR(phyUnit),ATHR_PHY_STATUS);
            /* If realtime failed check link in latch register before
         * asserting link down.
             */
                if (phyHwStatus & ATHR_LATCH_LINK_PASS)
                   linkCount++;
        else
                    lostLinks++;
                lastStatus->isPhyAlive = FALSE;
            }
        } else { /* last known link status was DEAD */

            /* Check for reset complete */

                phyHwStatus = phy_reg_read(0, ATHR_PHYADDR(phyUnit),ATHR_PHY_STATUS);

            if (!ATHR_RESET_DONE(phyHwStatus))
                continue;

                phyHwControl = phy_reg_read(0, ATHR_PHYADDR(phyUnit),ATHR_PHY_CONTROL);

            /* Check for AutoNegotiation complete */

            if ((!(phyHwControl & ATHR_CTRL_AUTONEGOTIATION_ENABLE))
                 || ATHR_AUTONEG_DONE(phyHwStatus)) {
                    phyHwStatus = phy_reg_read(0, ATHR_PHYADDR(phyUnit),ATHR_PHY_SPEC_STATUS);

                    if (phyHwStatus & ATHR_STATUS_LINK_PASS) {
                        gainedLinks++;
                        linkCount++;
                        lastStatus->isPhyAlive = TRUE;
                   }
            }
        }
    }
    return (linkCount);

}

void athr_phy_force_100M(int unit,int duplex)
{
    phy_reg_write(unit, ATHR_PHYADDR(unit), MII_CTRL_REG, (0x2000|(duplex << 8)));
}

void athr_phy_force_10M(int unit,int duplex)
{
    phy_reg_write(unit, ATHR_PHYADDR(unit), MII_CTRL_REG, (0x0000|(duplex << 8)));
}

void athr_phy_force_auto(int unit)
{
    phy_reg_write(unit, ATHR_PHYADDR(unit), MII_CTRL_REG, 0x9000);
}

int athr_phy_ioctl(uint32_t *args, int cmd)
{
    struct eth_diag *etd =(struct eth_diag *) args;
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
            etd->val = phy_reg_read(portnum, ATHR_PHYADDR(portnum),
                (phy_reg & 0xffff));
        }
    } else if(cmd  == ATHR_WR_PHY) {
        if(etd->ed_u.portnum != 0xf) {
            phy_reg_write(portnum, ATHR_PHYADDR(portnum),
            (phy_reg & 0xffff), etd->val);
        }
    }
    else if(cmd == ATHR_FORCE_PHY) {
        if(etd->val == 10) {
            printk("Forcing 10Mbps %s on port:%d \n",
                dup_str[etd->ed_u.duplex], (etd->phy_reg));
            athr_phy_force_10M(phy_reg, etd->ed_u.duplex);
        } else if(etd->val == 100) {
            printk("Forcing 100Mbps %s on port:%d \n",
                dup_str[etd->ed_u.duplex],(etd->phy_reg));
            athr_phy_force_100M(phy_reg, etd->ed_u.duplex);
        } else if(etd->val == 0) {
            printk("Enabling Auto Neg on port:%d \n",(etd->phy_reg));
            athr_phy_force_auto(phy_reg);
        } else
            return -EINVAL;
    }
    else
        return -EINVAL;

    return 0;
}
