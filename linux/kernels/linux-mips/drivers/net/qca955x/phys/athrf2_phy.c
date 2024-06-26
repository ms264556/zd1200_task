/*
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include "athrf1_phy.h"
#include "athrs_phy.h"
#include "athrs_mac.h"
#include "athrs_phy_ctrl.h"

#define MODULE_NAME "ATHRSF2_PHY"

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
#define ATHR_PHY5_ADDR   0x5

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
    {TRUE,
     FALSE,
     ENET_UNIT_WAN,
     0,
     ATHR_PHY5_ADDR,
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
unsigned int athrf2_rd_phy(int ethUnit,unsigned int phy_addr, unsigned int reg_addr);
void athrf2_wr_phy(int ethUnit,unsigned int phy_addr, unsigned int reg_addr, unsigned int write_data);

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
    int timeout = 100;
    uint16_t phyHwStatus,phyBase,phyAddr;


    phyBase = ATHR_PHYBASE(phyUnit);
    phyAddr = ATHR_PHYADDR(phyUnit);


    if(!is_emu()) {
       phy_reg_write(phyBase,phyAddr, ATHR_PHY_CONTROL, ATHR_CTRL_AUTONEGOTIATION_ENABLE | ATHR_CTRL_SOFTWARE_RESET);
       printk("ATHR_PHY_STATUS:%X\n",phy_reg_read(phyBase,phyAddr,ATHR_PHY_SPEC_STATUS));
    }
    else {
       phy_reg_write(phyBase,phyAddr, ATHR_AUTONEG_ADVERT, ATHR_ADVERTISE_ALL);
       /* Do not advertise 1000 */
       phy_reg_write(phyBase,phyAddr, ATHR_1000BASET_CONTROL,0x0);
       phy_reg_write(phyBase,phyAddr, ATHR_PHY_CONTROL, ATHR_CTRL_AUTONEGOTIATION_ENABLE | ATHR_CTRL_SOFTWARE_RESET);
    }

   /*
     * After the phy is reset, it takes a little while before
     * it can respond properly.
     */

        printk("Wait for Autoneg to complete\n");

        phyHwStatus = phy_reg_read(phyBase,phyAddr, ATHR_PHY_STATUS);

        while(((phyHwStatus & 0x20)!=0x20) && timeout--)
	   phyHwStatus = phy_reg_read(phyBase,phyAddr, ATHR_PHY_STATUS);

   /*
     * Wait up to 3 seconds for ALL associated PHYs to finish
     * autonegotiation.  The only way we get out of here sooner is
     * if ALL PHYs are connected AND finish autonegotiation.
     */
    timeout = 20;
    for (;;) {
        phyHwStatus = phy_reg_read(phyBase,phyAddr, ATHR_PHY_CONTROL);

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

    printk(MODULE_NAME": unit %d phy addr %x ", phyBase,phyAddr);
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
athr_phy_is_link_alive(int ethUnit)
{
	uint16_t phyHwStatus;
	uint32_t phyBase;
	uint32_t phyAddr;
        int phyUnit = 0;

	phyBase = ATHR_PHYBASE(phyUnit);
	phyAddr = ATHR_PHYADDR(phyUnit);
	phyHwStatus = phy_reg_read(phyBase,phyAddr, ATHR_PHY_SPEC_STATUS);

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
athr_phy_setup(void *arg)
{
	int       phyUnit = 0;
	int       liveLinks = 0;
        athr_gmac_t *mac = (athr_gmac_t *)arg;
        int ethUnit = mac->mac_unit;

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
athr_phy_is_fdx(int ethUnit,int phyUnit)
{
    uint32_t  phyBase;
    uint32_t  phyAddr;
    uint16_t  phyHwStatus;
    int       ii = 200;

    phyUnit = 0; /* F2e has only one phyUnit */

    if (athr_phy_is_link_alive(phyUnit)) {

         phyBase = ATHR_PHYBASE(phyUnit);
         phyAddr = ATHR_PHYADDR(phyUnit);

         do {
                phyHwStatus = phy_reg_read(phyBase,phyAddr,ATHR_PHY_SPEC_STATUS);
                mdelay(10);
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
*               ATHR_PHY_SPEED_10T, ATHR_PHY_SPEED_100T;
*               ATHR_PHY_SPEED_1000T;
*/

int
athr_phy_speed(int ethUnit,int phyUnit)
{
    uint16_t  phyHwStatus;
    uint32_t  phyBase;
    uint32_t  phyAddr;
    int       ii = 200;


    phyUnit = 0; /* F2phy  has only one phyUnit */


    if (athr_phy_is_link_alive(phyUnit)) {

        phyBase = ATHR_PHYBASE(phyUnit);
        phyAddr = ATHR_PHYADDR(phyUnit);
        do {
            phyHwStatus = phy_reg_read(phyBase,phyAddr,ATHR_PHY_SPEC_STATUS);
            mdelay(10);
        } while((!(phyHwStatus & ATHR_STATUS_RESOVLED)) && --ii);

        phyHwStatus = ((phyHwStatus & ATHER_STATUS_LINK_MASK) >>
                       ATHER_STATUS_LINK_SHIFT);

        switch(phyHwStatus) {
        case 0:
            return ATHR_PHY_SPEED_10T;
        case 1:
            return ATHR_PHY_SPEED_100T;
        case 2:
            return ATHR_PHY_SPEED_1000T;
        default:
            printk("Unkown speed read!\n");
        }
    }

    //printk("athr_phy_speed: link down, returning 10t\n");
    return ATHR_PHY_SPEED_10T;
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

             phyHwStatus = phy_reg_read(phyBase,phyAddr,ATHR_PHY_SPEC_STATUS);

            /* See if we've lost link */
            if (phyHwStatus & ATHR_STATUS_LINK_PASS) { /* check realtime link */
                linkCount++;
            } else {
                phyHwStatus = phy_reg_read(phyBase,phyAddr,ATHR_PHY_STATUS);
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

                phyHwControl = phy_reg_read(phyBase,phyAddr,ATHR_PHY_CONTROL);

            if (!ATHR_RESET_DONE(phyHwControl))
                continue;

                phyHwControl = phy_reg_read(phyBase,phyAddr,ATHR_PHY_CONTROL);

            /* Check for AutoNegotiation complete */
                    phyHwStatus = phy_reg_read(phyBase,phyAddr,ATHR_PHY_STATUS);

            if ((!(phyHwControl & ATHR_CTRL_AUTONEGOTIATION_ENABLE))
                 || ATHR_AUTONEG_DONE(phyHwStatus)) {
                    phyHwStatus = phy_reg_read(phyBase,phyAddr,ATHR_PHY_SPEC_STATUS);

                    if (phyHwStatus & ATHR_STATUS_LINK_PASS) {
                        gainedLinks++;
                        linkCount++;
                        lastStatus->isPhyAlive = TRUE;
			mdelay(3000);
                   }
            }
        }
    }
    return (linkCount);

}
/* Place holders */

static int
athr_reg_init(void *arg)
{
   /* Feeding internal Clk to F2e */
   athr_reg_wr(ATHR_GMAC_XMII_CONFIG,0x0);
   return 0;
}

unsigned int athrf2_rd_phy(int ethUnit,unsigned int phy_addr, unsigned int reg_addr)
{
      return (phy_reg_read(ethUnit, phy_addr, reg_addr));
}

void athrf2_wr_phy(int ethUnit,unsigned int phy_addr, unsigned int reg_addr, unsigned int write_data)
{
      phy_reg_write(ethUnit, phy_addr, reg_addr, write_data);

}

int athrsf2_register_ops(void *arg)
{
  athr_gmac_t *mac   = (athr_gmac_t *) arg;
  athr_phy_ops_t *ops = mac->phy;

  if(!ops)
     ops = kmalloc(sizeof(athr_phy_ops_t), GFP_KERNEL);

  memset(ops,0,sizeof(athr_phy_ops_t));

  ops->mac            =  mac;
  ops->is_up          =  athr_phy_is_up;
  ops->is_alive       =  athr_phy_is_link_alive;
  ops->speed          =  athr_phy_speed;
  ops->is_fdx         =  athr_phy_is_fdx;
  ops->ioctl          =  NULL;
  ops->setup          =  athr_phy_setup;
  ops->stab_wr        =  NULL;
  ops->link_isr       =  NULL;
  ops->en_link_intrs  =  NULL;
  ops->dis_link_intrs =  NULL;
  ops->read_phy_reg   =  athrf2_rd_phy;
  ops->write_phy_reg  =  athrf2_wr_phy;
  ops->read_mac_reg   =  NULL;
  ops->write_mac_reg  =  NULL;
  ops->init           =  athr_reg_init;
  ops->port_map       =  0x1;

  mac->phy = ops;
  return 0;
}
