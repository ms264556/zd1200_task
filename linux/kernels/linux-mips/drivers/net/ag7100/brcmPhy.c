/*
 * $Id: phy.c,v 1.9 2005/01/29 00:48:01 sdk Exp $
 * $Copyright: (c) 2005 Broadcom Corp.
 * All Rights Reserved.$
 *
 * File: 	bcmPhy.c
 * Purpose:	Defines PHY driver routines, and standard 802.3 10/100/1000
 *		PHY interface routines.
 */


#if defined(linux)
//#include <linux/config.h>
#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>

#if !defined(CONFIG_AR7100)
#include "ar531xlnx.h"
#include "ae531x.h"
#else
#include "ag7100_phy.h"
#endif

#define dPrintf printk
#endif

#if !defined(CONFIG_AR7100)
#if defined(__ECOS)
#include "ae531xecos.h"
#endif

#include "ae531xmac.h"
#include "ae531xreg.h"

#include "bcmPhy.h"
#else
#if !defined(V54_BSP)
#include "brcmPhy.h"
#endif
#endif

#define DEBUG 0
#ifdef DEBUG
#define DRV_DEBUG 1
#define PHY_DEBUG 0
#else
#define DRV_DEBUG 0
#define PHY_DEBUG 0
#endif

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

#define DRV_PRINT(FLG, X)                           \
{                                                   \
    if (bcmPhyDebug & (FLG)) {                      \
        printk X;                                   \
    }                                               \
}

#if 1
int bcmPhyDebug = DRV_DEBUG_PHYSETUP | DRV_DEBUG_PHYCHANGE;
#else
int bcmPhyDebug = DRV_DEBUG_PHYSETUP | DRV_DEBUG_PHYCHANGE|DRV_DEBUG_PHYSTATUS;
#endif

#else /* !DRV_DEBUG */
#define DRV_LOG(DBG_SW, X0, X1, X2, X3, X4, X5, X6)
#define DRV_MSG(x,a,b,c,d,e,f)
#define DRV_PRINT(DBG_SW,X)
#endif

/*
 * Track per-PHY port information.
 */
typedef struct {
    BOOL   phyAlive;    /* last known state of link */
    UINT32 phyBase;
    UINT32 phyAddr;
} bcmPhyInfo_t;

#if 0
/*
 * There are 2 enet MACs.  Each MAC connects to
 * a PHY via its MII interface.  In both cases, the PHY
 * address is 0.
 */
#define ETH_PHY_ADDR		0
#else
// ETH_PHY_ADDR==1 for GD6-1
#define ETH_PHY_ADDR		1       /* hard coded to 1 */
#endif

/*
 * This table defines the mapping from MAC units to
 * per-PHY information.
 * On AR5311, it's a one-to-one mapping.
 *
 * Yes, we're hardcoding bits of board configuration
 * information here.  Someday, we may need a better
 * mechanism to handle the various combinations of
 * SOCs and PHYs.
 */
bcmPhyInfo_t bcmPhyInfo[] = {
    {phyAlive: FALSE,  /* PHY 0 */
     phyBase: 0, /* filled in by bcm_phySetup() */
     phyAddr: ETH_PHY_ADDR},
    {phyAlive: FALSE,  /* PHY 1 */
     phyBase: 0, /* filled in by bcm_phySetup() */
     phyAddr: ETH_PHY_ADDR},
};

/* Convert from phy unit# to (phyBase, phyAddr) pair */
#define BCM_PHYBASE(unit) (bcmPhyInfo[unit].phyBase)
#define BCM_PHYADDR(unit) (bcmPhyInfo[unit].phyAddr)

/******************************************************************************
*
* bcm_phyReset - Reset the PHY associated with
*                the specified MAC unit number.
*
*	Reset PHY and wait for it to come out of reset.
*
* RETURNS:
*    TRUE  --> associated PHY is alive
*    FALSE --> no LINKs on this ethernet unit
*/

BOOL
bcm_phyReset(int unit)
{
    UINT32  phyBase;
    UINT32  phyAddr;
#if !defined(V54_BSP)
    UINT16  ctrl, tmp = 0;
    int	    timeout = 0;
#else
    UINT16  ctrl;
#endif

printk("+ %s(%d)\n", __FUNCTION__, unit);
    phyBase = BCM_PHYBASE(unit);
    phyAddr = BCM_PHYADDR(unit);

    ctrl = phyRegRead( phyBase, phyAddr, MII_CTRL_REG );

    phyRegWrite(phyBase, phyAddr, MII_CTRL_REG, ctrl | MII_CTRL_RESET );

#if 0
// Skip wait for RESET completion.  Let periodic check determine link status.
    sysMsDelay(1500);

    for( timeout=BCM_PHY_RESET_TIMEOUT_USEC/150; timeout; timeout-- ) {
        /* wait for RESET to complete */
        tmp = phyRegRead( phyBase, phyAddr, MII_CTRL_REG );
        if( tmp & MII_CTRL_RESET ) {
            break;
        }

        sysMsDelay(150);
    }

    return (tmp & MII_CTRL_RESET);
#else
    return FALSE;
#endif

}


/******************************************************************************
*
* bcm_phySetup - Setup the PHY associated with
*                the specified MAC unit number.
*
*         Initializes the associated PHY port.
*
* RETURNS:
*    TRUE  --> associated PHY is alive
*    FALSE --> no LINKs on this ethernet unit
*/

#if !defined(V54_BSP)
BOOL
bcm_phySetup(int unit, UINT32 phyBase)
#else
int
bcm_phySetup(int unit)
#endif
{
#if !defined(V54_BSP)
    UINT16  phySts = 0;
    UINT16  timeout;
#else
    UINT32 phyBase = 0;
#endif
    BOOL    linkAlive = FALSE;
    UINT32  phyAddr;
    UINT16  mii_ana, mii_ctrl;
    UINT32 phyID1;

    // setup phyBase
    // single PHY
    if ( unit > 1 ) {
        printk("%s: unit=%d > 1\n", __FUNCTION__, unit);
        return FALSE;
    }
    BCM_PHYBASE(unit) = phyBase;

    for (phyAddr = 0; phyAddr< 32; phyAddr++) {
        phyID1 = 0;
        phyID1 = phyRegRead(phyBase, phyAddr, MII_PHY_ID1);
        if ( phyID1 ==  BCM_PHY_ID1_EXPECTATION ) {
            // found the matching PHY ID
            BCM_PHYADDR(unit) = phyAddr;
            DRV_PRINT(DRV_DEBUG_PHYSETUP,
                    ("\nethMac%d: phyAddr %d Phy ID=%4.4x\n", unit, phyAddr, phyID1));
            goto foundID;
        }
    }
    phyAddr = BCM_PHYADDR(unit);
foundID:

    mii_ana = MII_ANA_HD_10 | MII_ANA_FD_10 | MII_ANA_HD_100 |
	      MII_ANA_FD_100 | MII_ANA_ASF_802_3;
    mii_ctrl = MII_CTRL_FD | MII_CTRL_SS_100 | MII_CTRL_AE | MII_CTRL_RAN;

    /* Reset phy */
    phyRegWrite(phyBase, phyAddr, MII_CTRL_REG, mii_ctrl );
    phyRegWrite(phyBase, phyAddr, MII_ANA_REG, mii_ana );

#if 0
// forget the wait, just check later
    sysMsDelay(1500);

    /* Wait 6s */
    for( timeout=40; timeout; timeout-- ) {
        /* wait for AN to complete */
        phySts = phyRegRead( phyBase, phyAddr, MII_STAT_REG );
        if( MII_STAT_AN_DONE & phySts ) {
            break;
        }

        sysMsDelay(150);
    }

    if( phySts & MII_STAT_LA ) {
        linkAlive = bcmPhyInfo[unit].phyAlive = TRUE;
#if !defined(V54_BSP)
        ethSetLinkState(unit, 0, 1);
#endif
    } else {
        linkAlive = bcmPhyInfo[unit].phyAlive = FALSE;
#if !defined(V54_BSP)
        ethSetLinkState(unit, 0, 0);
#endif
    }

    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("ethMac%d: Phy Status=%4.4x\n", unit, phySts));
#endif

    return linkAlive;
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
BOOL
bcm_phyIsFullDuplex(int unit)
{
    UINT16  phyGsr;
    UINT32  phyBase;
    UINT32  phyAddr;

    phyBase = BCM_PHYBASE(unit);
    phyAddr = BCM_PHYADDR(unit);

    phyGsr = phyRegRead( phyBase, phyAddr, BCM_GSR_REG );

    if( phyGsr & BCM_GSR_FD ) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/******************************************************************************
*
* bcm_phyIsSpeed100 - Determines the speed of phy ports associated with the
* specified device.
*
* RETURNS:
*    TRUE --> 100Mbit
*    FALSE --> 10Mbit
*/
BOOL
bcm_phyIsSpeed100(int unit)
{
    UINT16  phyGsr;
    UINT32  phyBase;
    UINT32  phyAddr;

    phyBase = BCM_PHYBASE(unit);
    phyAddr = BCM_PHYADDR(unit);

    phyGsr = phyRegRead( phyBase, phyAddr, BCM_GSR_REG );

    if( phyGsr & BCM_GSR_SI ) {
        return TRUE;
    } else {
        return FALSE;
    }
}

void bcm_phyCheckStatusChange(int unit);
/******************************************************************************
*
* bcm_ethIsLinkUp - test to see if the specified link is alive
*
* RETURNS:
*    TRUE  --> link is alive
*    FALSE --> link is down
*/
BOOL
bcm_ethIsLinkUp(int unit)
{
    bcmPhyInfo_t     *lastStatus = &bcmPhyInfo[unit];

    bcm_phyCheckStatusChange(unit);

    return lastStatus->phyAlive;
}

#if 1 || AE_POLL_ACTIVITIES
/*****************************************************************************
*
* bcm_phyCheckStatusChange -- checks for significant changes in PHY state.
*
* A "significant change" is:
*     dropped link (e.g. ethernet cable unplugged) OR
*     autonegotiation completed + link (e.g. ethernet cable plugged in)
*
* On AR5311, there is a 1-to-1 mapping of ethernet units to PHYs.
* When a PHY is plugged in, phyLinkGained is called.
* When a PHY is unplugged, phyLinkLost is called.
*/
void
bcm_phyCheckStatusChange(int unit)
{
    UINT16          phyHwStatus;
    bcmPhyInfo_t     *lastStatus = &bcmPhyInfo[unit];
    UINT32          phyBase;
    UINT32          phyAddr;

    phyBase = BCM_PHYBASE(unit);
    phyAddr = BCM_PHYADDR(unit);

    phyHwStatus = phyRegRead( phyBase, phyAddr, MII_STAT_REG );

    DRV_PRINT(DRV_DEBUG_PHYSTATUS,
            ("ethMac%d: Phy Status=%4.4x\n", unit, phyHwStatus));

    if( lastStatus->phyAlive ) { /* last known status was ALIVE */
        /* See if we've lost link */
        if( !(phyHwStatus & MII_STAT_LA) ) {
            DRV_PRINT(DRV_DEBUG_PHYCHANGE,("\nethMac%d link down\n", unit));
            lastStatus->phyAlive = FALSE;
#if !defined(V54_BSP)
            phyLinkLost(unit);
            ethSetLinkState(unit, 0, 0);
#endif
        }
    } else { /* last known status was DEAD */
        /* Check for AN complete */
        if( phyHwStatus & MII_STAT_AN_DONE ) {
            DRV_PRINT(DRV_DEBUG_PHYCHANGE,("\nethMac%d link up, %s %s\n", unit
                            , bcm_phyIsSpeed100(unit) ? "100" : "10"
                            , bcm_phyIsFullDuplex(unit) ? "Full" : "Half"
                        ));
            lastStatus->phyAlive = TRUE;
#if !defined(V54_BSP)
            phyLinkGained(unit);
            ethSetLinkState(unit, 0, 1);
#endif
        }
    }
}
#endif

#if PHY_DEBUG
// phyShow command not used

/* Define the PHY registers of interest for a phyShow command */
struct bcmRegisterTable_s {
    UINT32 regNum;
    char  *regIdString;
} bcmRegisterTable[] =
{
    {MII_CTRL_REG,    "Basic Mode Control (MII_CTRL_REG)    	"},
    {MII_STAT_REG,    "Basic Mode Status (MII_STAT_REG)     	"},
    {MII_PHY_ID0_REG, "PHY Identifier 1 (PHY_id_hi)    		"},
    {MII_PHY_ID1_REG, "PHY Identifier 2 (PHY_id_lo)    		"},
    {MII_ANA_REG,     "Auto-Neg Advertisement (AN_adv) 		"},
    {MII_ANP_REG,     "Auto-Neg Link Partner Ability   		"},
    {MII_AN_EXP_REG,  "Auto-Neg Expansion              		"},
    {BCM_MCR_REG,     "Broadcom Micellaneous Control   		"},
    {BCM_ASSTAT_REG,  "Broadcom Auxiliary Status       		"},
    {BCM_REC_REG,     "Broadcom Receive Error Count    		"},
    {BCM_FCSC_REG,    "Broadcom False Carrier Sense Count	"},
    {BCM_GSR_REG,     "Broadcom General Status Register 	"},
    {BCM_ASSR_REG,    "Broadcom Auxiliary Status Summary	"},
    {BCM_RUPT_REG,    "Broadcom Interrupt Register      	"},
    {BCM_AUX2_REG,    "Broadcom Auxiliary Mode 2        	"},
    {BCM_AEGS_REG,    "Broadcom Aux Error and General Status	"},
    {BCM_AMODE2_REG,  "Broadcom Auxiliary Mode 2 Register   	"},
    {BCM_AMPHY_REG,   "Broadcom Auxiliary Multiple PHY      	"},
    {BCM_TEST_REG,    "Broadcom Test Register               	"},
};

int bcmNumRegs = sizeof(bcmRegisterTable) / sizeof(bcmRegisterTable[0]);

/*
 * Dump the state of a PHY.
 */
void
bcm_phyShow(int unit)
{
    int i;
    UINT16  value;
    UINT32  phyBase;
    UINT32  phyAddr;

    dPrintf("bcm Eth: Link state: %s %s\n",
                bcm_phyIsSpeed100(unit) ? "100" : "10" ,
                bcm_phyIsFullDuplex(unit) ? "Full" : "Half"
            );

    phyBase = BCM_PHYBASE(unit);
    phyAddr = BCM_PHYADDR(unit);

    dPrintf("PHY state for PHY%d (eth%d, phyBase 0x%8x, phyAddr 0x%x)\n",
           unit,
           unit,
           BCM_PHYBASE(unit),
           BCM_PHYADDR(unit) );

    for (i=0; i<bcmNumRegs; i++) {

        value = phyRegRead(phyBase, phyAddr, bcmRegisterTable[i].regNum);

        dPrintf("Reg %02d (0x%02x) %s = 0x%08x\n",
            bcmRegisterTable[i].regNum, bcmRegisterTable[i].regNum,
            bcmRegisterTable[i].regIdString, value);
    }
}

/*
 * Modify the value of a PHY register.
 * This makes it a bit easier to modify PHY values during debug.
 */
void
bcm_phySet(int unit, UINT32 regnum, UINT32 value)
{
    UINT32  phyBase;
    UINT32  phyAddr;

    phyBase = BCM_PHYBASE(unit);
    phyAddr = BCM_PHYADDR(unit);

    phyRegWrite(phyBase, phyAddr, regnum, value);
}
#endif
