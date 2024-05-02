/*
 * $Id: phy.h,v 1.12 2005/01/29 00:47:45 sdk Exp $
 * $Copyright: (c) 2005 Broadcom Corp.
 * All Rights Reserved.$
 *
 * Useful constants and routines for PHY chips which
 * run with Orion (10/100/1000 Mb/s enet) and use MII, GMII,
 * or 8b/10b Serdes interfaces.
 *
 * See Also: IEEE 802.3 (1998) Local and Metropolitan Area Networks
 * Sections 22.2.4-XX
 */

#ifndef _BCM_PHY_H
#define _BCM_PHY_H

/* Standard MII Registers */

#define MII_CTRL_REG          	0x00	/* MII Control Register : r/w */
#define MII_STAT_REG          	0x01	/* MII Status Register: ro */
#define MII_PHY_ID0_REG       	0x02	/* MII PHY ID register: r/w */
#define MII_PHY_ID1_REG       	0x03	/* MII PHY ID register: r/w */
#define MII_ANA_REG     	0x04	/* MII Auto-Neg Advertisement: r/w */
#define MII_ANP_REG       	0x05	/* MII Auto-Neg Link Partner: ro */
#define MII_AN_EXP_REG        	0x06	/* MII Auto-Neg Expansion: ro */
#define	MII_GB_CTRL_REG		0x09	/* MII 1000Base-T control register */
#define	MII_GB_STAT_REG		0x0a	/* MII 1000Base-T Status register */
#define	MII_ESR_REG		0x0f	/* MII Extended Status register */

/* Non-standard MII Registers */

#define	BCM_MCR_REG		0x10	/* BCM Miscellaneous Control 	*/
#define	BCM_ASSTAT_REG		0x11	/* BCM Auxiliary Status 	*/
#define	BCM_REC_REG		0x12	/* BCM Receive Error Count 	*/
#define	BCM_FCSC_REG  		0x13	/* BCM False Carrier Sense Count */
#define	BCM_GSR_REG		0x18	/* BCM General Control/Status 	*/
#define	BCM_ASSR_REG		0x19	/* BCM Auxiliary Status Summary */
#define	BCM_RUPT_REG		0x1a	/* BCM Interrupt Register       */
#define	BCM_AUX2_REG		0x1b	/* BCM Auxiliary Mode 2 Reg     */
#define	BCM_AEGS_REG		0x1c	/* BCM Aux Error and Gen Status */
#define	BCM_AMODE2_REG		0x1d	/* BCM Auxiliary Mode 2         */
#define	BCM_AMPHY_REG		0x1e	/* BCM Auxiliary Multiple PHY   */
#define	BCM_TEST_REG		0x1f	/* BCM Test Register            */

/* MII Control Register: bit definitions */

#define MII_CTRL_SS_MSB		(1 << 6) /* Speed select, MSb */
#define MII_CTRL_CST		(1 << 7) /* Collision Signal test */
#define MII_CTRL_FD		(1 << 8) /* Full Duplex */
#define MII_CTRL_RAN		(1 << 9) /* Restart Autonegotiation */
#define MII_CTRL_IP		(1 << 10) /* Isolate Phy */
#define MII_CTRL_PD		(1 << 11) /* Power Down */
#define MII_CTRL_AE		(1 << 12) /* Autonegotiation enable */
#define MII_CTRL_SS_LSB		(1 << 13) /* Speed select, LSb */
#define MII_CTRL_LE		(1 << 14) /* Loopback enable */
#define MII_CTRL_RESET		(1 << 15) /* PHY reset */

#define	MII_CTRL_SS(_x)		((_x) & (MII_CTRL_SS_LSB|MII_CTRL_SS_MSB))
#define	MII_CTRL_SS_10		0
#define	MII_CTRL_SS_100		(MII_CTRL_SS_LSB)
#define	MII_CTRL_SS_1000	(MII_CTRL_SS_MSB)
#define	MII_CTRL_SS_INVALID	(MII_CTRL_SS_LSB | MII_CTRL_SS_MSB)
#define	MII_CTRL_SS_MASK	(MII_CTRL_SS_LSB | MII_CTRL_SS_MSB)

/*
 * MII Status Register: See 802.3, 1998 pg 544
 */
#define MII_STAT_EXT		(1 << 0) /* Extended Registers */
#define MII_STAT_JBBR		(1 << 1) /* Jabber Detected */
#define MII_STAT_LA		(1 << 2) /* Link Active */
#define MII_STAT_AN_CAP		(1 << 3) /* Autoneg capable */
#define MII_STAT_RF		(1 << 4) /* Remote Fault */
#define MII_STAT_AN_DONE	(1 << 5) /* Autoneg complete */
#define MII_STAT_MF_PS		(1 << 6) /* Preamble suppression */
#define	MII_STAT_ES		(1 << 8) /* Extended status (R15) */
#define	MII_STAT_HD_100_T2	(1 << 9) /* Half duplex 100Mb/s supported */
#define	MII_STAT_FD_100_T2	(1 << 10)/* Full duplex 100Mb/s supported */
#define	MII_STAT_HD_10		(1 << 11)/* Half duplex 10Mb/s supported */
#define	MII_STAT_FD_10		(1 << 12)/* Full duplex 10Mb/s supported */
#define	MII_STAT_HD_100		(1 << 13)/* Half duplex 100Mb/s supported */
#define	MII_STAT_FD_100		(1 << 14)/* Full duplex 100Mb/s supported */
#define	MII_STAT_100_T4		(1 << 15)/* Full duplex 100Mb/s supported */

/*
 * MII Link Advertisment
 */
#define	MII_ANA_ASF		(1 << 0)/* Advertise Selector Field */
#define	MII_ANA_HD_10		(1 << 5)/* Half duplex 10Mb/s supported */
#define	MII_ANA_FD_10		(1 << 6)/* Full duplex 10Mb/s supported */
#define	MII_ANA_HD_100		(1 << 7)/* Half duplex 100Mb/s supported */
#define	MII_ANA_FD_100		(1 << 8)/* Full duplex 100Mb/s supported */
#define	MII_ANA_T4		(1 << 9)/* T4 */
#define	MII_ANA_PAUSE		(1 << 10)/* Pause supported */
#define	MII_ANA_ASYM_PAUSE	(1 << 11)/* Asymmetric pause supported */
#define	MII_ANA_RF		(1 << 13)/* Remote fault */
#define	MII_ANA_NP		(1 << 15)/* Next Page */

#define	MII_ANA_ASF_802_3	(1)	/* 802.3 PHY */

/*
 * 1000Base-T Control Register
 */
#define	MII_GB_CTRL_MS_MAN	(1 << 12) /* Manual Master/Slave mode */
#define	MII_GB_CTRL_MS		(1 << 11) /* Master/Slave negotiation mode */
#define	MII_GB_CTRL_PT		(1 << 10) /* Port type */
#define	MII_GB_CTRL_ADV_1000FD	(1 << 9) /* Advertise 1000Base-T FD */
#define	MII_GB_CTRL_ADV_1000HD	(1 << 8) /* Advertise 1000Base-T HD */

/*
 * 1000Base-T Status Register
 */
#define MII_GB_STAT_MS_FAULT	(1 << 15) /* Master/Slave Fault */
#define MII_GB_STAT_MS		(1 << 14) /* Master/Slave, 1 == Master */
#define MII_GB_STAT_LRS		(1 << 13) /* Local receiver status */
#define	MII_GB_STAT_RRS		(1 << 12) /* Remote receiver status */
#define	MII_GB_STAT_LP_1000FD	(1 << 11) /* Link partner 1000FD capable */
#define	MII_GB_STAT_LP_1000HD	(1 << 10) /* Link partner 1000HD capable */
#define MII_GB_STAT_IDE		(0xff << 0) /* Idle error count */

/*
 * IEEE Extended Status Register
 */
#define	MII_ESR_1000_X_FD	(1 << 15) /* 1000Base-T FD capable */
#define	MII_ESR_1000_X_HD	(1 << 14) /* 1000Base-T HD capable */
#define	MII_ESR_1000_T_FD	(1 << 13) /* 1000Base-T FD capable */
#define	MII_ESR_1000_T_HD	(1 << 12) /* 1000Base-T FD capable */

/*
 * MII Extended Control Register (BROADCOM)
 */
#define	BCM_ECR_FE		(1 << 0) /* FIFO Elasticity */
#define	BCM_ECR_TLLM		(1 << 1) /* Three link LED mode */
#define	BCM_ECR_ET_IPG		(1 << 2) /* Extended XMIT IPG mode */
#define	BCM_ECR_FLED_OFF	(1 << 3) /* Force LED off */
#define	BCM_ECR_FLED_ON		(1 << 4) /* Force LED on */
#define	BCM_ECR_ELT		(1 << 5) /* Enable LED traffic */
#define	BCM_ECR_RS		(1 << 6) /* Reset Scrambler */
#define	BCM_ECR_BRSA		(1 << 7) /* Bypass Receive Sym. align */
#define	BCM_ECR_BMLT3		(1 << 8) /* Bypass MLT3 Encoder/Decoder */
#define	BCM_ECR_BSD		(1 << 9) /* Bypass Scramble/Descramble */
#define	BCM_ECR_B4B5B		(1 << 10) /* Bypass 4B/5B Encode/Decode */
#define	BCM_ECR_FI		(1 << 11) /* Force Interrupt */
#define	BCM_ECR_ID		(1 << 12) /* Interrupt Disable */
#define	BCM_ECR_TD		(1 << 13) /* XMIT Disable */
#define	BCM_ECR_DAMC		(1 << 14) /* DIsable Auto-MDI Crossover */
#define	BCM_ECR_10B		(1 << 15) /* 1 == 10B, 0 == GMII */

/*
 * GSR (BROADCOM)
 */
#define	BCM_GSR_FD		(1 << 0) /* Full duplex active */
#define	BCM_GSR_SI		(1 << 1) /* Speed 0-->10, 1 --> 100 */
#define	BCM_GSR_FORCE		(1 << 2) /* Force 0-->10, 1--> 100 */
#define	BCM_GSR_AN		(1 << 3) /* Autonegotiation enabled */

#define PHY_MIN_REG		0
#define PHY_MAX_REG		0x1f

#define	PHY_ADDR_MIN		0
#define	PHY_ADDR_MAX		31
#define	PHY_NUM_PORTS		(PHY_ADDR_MAX - PHY_ADDR_MIN + 1)

/* Clause 45 MIIM */
#define PHY_C45_DEV_RESERVED    0x00
#define PHY_C45_DEV_PMA_PMD     0x01
#define PHY_C45_DEV_WIS         0x02
#define PHY_C45_DEV_PCS         0x03
#define PHY_C45_DEV_PHYXS       0x04
#define PHY_C45_DEV_DTEXS       0x05

/* microsec */
#define BCM_PHY_RESET_TIMEOUT_USEC  10000

/*
 * Each PHY is identified by OUI (Organizationally Unique Identifier),
 * model number, and revision as found in IEEE registers 2 and 3.
 */

#define PHY_BRCM_OUI1	0x001018	/* Broadcom original */
#define PHY_BRCM_OUI1R	0x000818	/* Broadcom reversed */
#define PHY_BRCM_OUI2	0x000af7	/* Broadcom new */
#define PHY_ALTIMA_OUI1	0x0010a9	/* Altima */
#define PHY_ALTIMA_OUI2	0x0010a9	/* Altima */

/* BCM_PHY_ID1 fields */
#define BCM_PHY_ID1_EXPECTATION         0x0143 /* OUI >> 6 */
#define MII_PHY_ID1 2

/*
 * Macros to Encode/Decode PHY_ID0/ID1 registers (MDIO register 0x02/0x03)
 */

#define PHY_ID0(oui, model, rev) \
    ((uint16)(_shr_bit_rev_by_byte_word32(oui) >> 6 & 0xffff))

#define PHY_ID1(oui, model, rev) \
    ((uint16)(_shr_bit_rev_by_byte_word32(oui) & 0x3f) << 10 | \
             ((model) & 0x3f) << 4 | \
             ((rev) & 0x0f) << 0)

#define PHY_OUI(id0, id1) \
    _shr_bit_rev_by_byte_word32((uint32)(id0) << 6 | ((id1) >> 10 & 0x3f))

#define PHY_MODEL(id0, id1) \
    ((id1) >> 4 & 0x3f)

#define PHY_REV(id0, id1) \
    ((id1) & 0xf)

/*
 * Identifiers of known PHYs
 */

#define PHY_BCM5218_OUI		PHY_BRCM_OUI1R
#define PHY_BCM5218_MODEL	0x1f

#define PHY_BCM5220_OUI		PHY_BRCM_OUI1R		/* & BCM5221 */
#define PHY_BCM5220_MODEL	0x1e

#define PHY_BCM5226_OUI		PHY_BRCM_OUI1R
#define PHY_BCM5226_MODEL	0x1c

#define PHY_BCM5228_OUI		PHY_BRCM_OUI1R
#define PHY_BCM5228_MODEL	0x1d

#define PHY_BCM5238_OUI		PHY_BRCM_OUI1R
#define PHY_BCM5238_MODEL	0x20

#define PHY_BCM5248_OUI		PHY_BRCM_OUI2
#define PHY_BCM5248_MODEL	0x00

#define PHY_BCM5400_OUI		PHY_BRCM_OUI1
#define PHY_BCM5400_MODEL	0x04

#define PHY_BCM5401_OUI		PHY_BRCM_OUI1
#define PHY_BCM5401_MODEL	0x05

#define PHY_BCM5402_OUI		PHY_BRCM_OUI1
#define PHY_BCM5402_MODEL	0x06

#define PHY_BCM5404_OUI		PHY_BRCM_OUI1
#define PHY_BCM5404_MODEL	0x08

#define PHY_BCM5411_OUI		PHY_BRCM_OUI1
#define PHY_BCM5411_MODEL	0x07

#define PHY_BCM5421_OUI		PHY_BRCM_OUI1		/* & 5421S */
#define PHY_BCM5421_MODEL	0x0e

#define PHY_BCM5424_OUI		PHY_BRCM_OUI1		/* & 5234 */
#define PHY_BCM5424_MODEL	0x0a

#define PHY_BCM5461_OUI		PHY_BRCM_OUI1		/* & 5461S */
#define PHY_BCM5461_MODEL	0x0c

#define PHY_BCM5462_OUI		PHY_BRCM_OUI1		/* & 5461D */
#define PHY_BCM5462_MODEL	0x0d

#define PHY_BCM5464_OUI		PHY_BRCM_OUI1		/* & 5464S */
#define PHY_BCM5464_MODEL	0x0b

#define PHY_BCM5466_OUI		PHY_BRCM_OUI1		/* & 5466S */
#define PHY_BCM5466_MODEL	0x3b

#define PHY_BCM8011_OUI		PHY_BRCM_OUI1R
#define PHY_BCM8011_MODEL	0x09

#define PHY_BCM8703_OUI		PHY_BRCM_OUI1
#define PHY_BCM8703_MODEL	0x03

#define PHY_BCMXGXS1_OUI	PHY_BRCM_OUI1
#define PHY_BCMXGXS1_MODEL	0x3f

#define PHY_BCMXGXS2_OUI	PHY_BRCM_OUI1
#define PHY_BCMXGXS2_MODEL	0x3a

#define PHY_AL101_OUI		PHY_ALTIMA_OUI
#define PHY_AL101_MODEL		0x21

#define PHY_AL101L_OUI		PHY_ALTIMA_OUI
#define PHY_AL101L_MODEL	0x12

#define PHY_AL104_OUI		PHY_ALTIMA_OUI
#define PHY_AL104_MODEL		0x14

#define PHY_BCMXGXS5_OUI	PHY_BRCM_OUI2
#define PHY_BCMXGXS5_MODEL	0x19

/*
 * Enumeration of known PHYs
 */

typedef enum soc_known_phy_e {
    _phy_id_unknown = 0,
    _phy_id_BCM5218,
    _phy_id_BCM5220,
    _phy_id_BCM5226,
    _phy_id_BCM5228,
    _phy_id_BCM5238,
    _phy_id_BCM5248,
    _phy_id_BCM5400,
    _phy_id_BCM5401,
    _phy_id_BCM5402,
    _phy_id_BCM5404,
    _phy_id_BCM5411,
    _phy_id_BCM5421,
    _phy_id_BCM5424,
    _phy_id_BCM5461,
    _phy_id_BCM5464,
    _phy_id_BCM5466,
    _phy_id_BCM8011,
    _phy_id_BCM8703,
    _phy_id_BCMXGXS1,
    _phy_id_BCMXGXS2,
    _phy_id_BCMXGXS5,
    _phy_id_SERDES,
    _phy_id_NULL,
    _phy_id_SIMUL,
    _phy_id_numberKnown			/* Always last, please */
} soc_known_phy_t;

#define PHY_FLAGS_COPPER	0x01	/* copper medium */
#define PHY_FLAGS_FIBER		0x02	/* fiber medium */
#define PHY_FLAGS_PASSTHRU	0x04	/* serdes passthru (5690) */
#define PHY_FLAGS_10B		0x08	/* ten bit interface (TBI) */
#define PHY_FLAGS_5421S		0x10	/* True if PHY is a 5421S */
#define PHY_FLAGS_DISABLE       0x20	/* True if PHY is disabled */
#define	PHY_FLAGS_C45		0x40	/* True if PHY uses clause 45 MIIM */

#if defined(CONFIG_AR7100)
#define UINT8   u8
#define UINT16  u16
#define UINT32  u32
#define TRUE    1
#define FALSE   0
#define BOOL    int

#define phyRegRead  ag7100_mii_read
#define phyRegWrite ag7100_mii_write

#define sysMsDelay  mdelay

#define ethSetLinkState
#define phyLinkGained
#define phyLinkLost
#endif

/* Put extra PHY information for each port here. */
typedef struct soc_phy_info_s {
    UINT16	phy_id0;	/* Identifier for PHY, from MII register 2. */
    UINT16	phy_id1;	/* Identifier for PHY, from MII register 3. */
    UINT8	phy_addr;	/* PHY address to use for MII ops */
    UINT8	phy_addr_int;	/* Internal PHY address if applicable */
    char	*phy_name;	/* Pointer to static string name of PHY */
    UINT32	phy_flags;	/* Logical OR among PHY_FLAGS_xxx */
} soc_phy_info_t;


/* Forward def */
typedef struct soc_phy_table_s soc_phy_table_t;

#if !defined(V54_BSP)
BOOL    bcm_phySetup(int ethUnit, UINT32 phyBase);
#else
int     bcm_phySetup(int ethUnit);
#endif
void    bcm_phyCheckStatusChange(int ethUnit);
BOOL    bcm_phyIsSpeed100(int ethUnit);
int     bcm_phyIsFullDuplex(int ethUnit);
BOOL    bcm_ethIsLinkUp(int ethUnit);

#endif	/* !_BCM_PHY_H */
