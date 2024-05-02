/*
 * File: 	mvPhy.c
 * Purpose:	Defines PHY driver routines, and standard 802.3 10/100/1000
 *		PHY interface routines.
 */


#ifndef __ECOS
#include "linux/types.h"
#include "linux/delay.h"
#endif
//#include "mvPhy.h"
#include "ag7100_phy.h"
#include "eth_diag.h"
#include "ar531x_bsp.h"

extern char *dup_str[];
extern int gd11_ethname_remap[];
extern int *ethname_remap;

#define MV_PHY0_ADDR     0x1e
#define MV_PHY1_ADDR     0x1f
/* Retriever Rev A: */
/* eth0 -> phy1 addr = 0x1f */
/* eth1 -> phy0 addr = 0x1e */
/* Retriever Rev B: eth0 -> phy0 addr = 0x1e, eth1 -> phy1 addr = 0x1f */
static uint32_t mv_phy0_addr = MV_PHY0_ADDR ;
static uint32_t mv_phy1_addr = MV_PHY1_ADDR ;
#define PHY_ADDR(unit)  ((MAC_TO_PHY(unit)) ? mv_phy1_addr : mv_phy0_addr)

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
#define MII_COPPER_CTRL1_REG    16
#define MII_COPPER_STATUS1_REG  17

#define MII_PAGE_ADDR_REG       22

#define MII_PHY_RESET           0x8000
#define MII_RESTART_AN          0x0200
#define MII_1000BT_MASK         0x0300
#define MII_DS_CNT_MASK         0x7800
#define MII_COPPER_LINK_STATUS  0x0004

#define MII_COPPER_SPEED        0xC000
#define MII_COPPER_DUPLEX       0x2000
#define MII_COPPER_RESOLVED     0x0800
#define MII_COPPER_LINK_RT      0x0400
#define MII_COPPER_SPEED_SHIFT  14

#define MII_LED_PAGE            3
#define MII_LED_FUNCTION_REG    16
#define MII_LED_POLARITY_REG    17
#define MII_LED_TIMER_REG       18

#define MII_COPPER_STATUS2_REG  19
#define MII_DEBUG_PAGE_REG      29
#define MII_DEBUG_VALUE_REG     30

#define MII_COPPER_AN_ERROR     0x8000

typedef enum {
    IDLE=0,
    AN_ERR,
    SET_WAR_REG,
} mv_war_state_type;

typedef struct {
    mv_war_state_type current_war_state;
    int chk_resolved;
    int chk_linkup;
    int chk_error;
} mv_war_struct;

mv_war_struct mv_war_info[2] = {
    {IDLE, 0, 0, 0},
    {IDLE, 0, 0, 0}
};

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

void setReg22(int unit, int page)
{
    phy_reg_write(unit, PHY_ADDR(unit), MII_PAGE_ADDR_REG, page);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("set page(0x%x)\n", phy_reg_read(unit, PHY_ADDR(unit), MII_PAGE_ADDR_REG)));
}

#if 0
void phyId(int unit)
{
    uint16_t id1, id2;
    int i;

    for (i=0; i < 11; i++) {
        DRV_PRINT(DRV_DEBUG_PHYSETUP,
                ("pag0, reg %d(0x%x)\n", i, phy_reg_read(unit, PHY_ADDR(unit), i)));
    }
    for (i=15; i < 24; i++) {
        DRV_PRINT(DRV_DEBUG_PHYSETUP,
                ("pag0, reg %d(0x%x)\n", i, phy_reg_read(unit, PHY_ADDR(unit), i)));
    }
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("pag0, reg %d(0x%x)\n", 26, phy_reg_read(unit, PHY_ADDR(unit), 26)));

    setReg22(unit, 2);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("pag2, reg %d(0x%x)\n", 16, phy_reg_read(unit, PHY_ADDR(unit), 16)));
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("pag2, reg %d(0x%x)\n", 18, phy_reg_read(unit, PHY_ADDR(unit), 18)));
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("pag2, reg %d(0x%x)\n", 19, phy_reg_read(unit, PHY_ADDR(unit), 19)));
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("pag2, reg %d(0x%x)\n", 21, phy_reg_read(unit, PHY_ADDR(unit), 21)));
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("pag2, reg %d(0x%x)\n", 24, phy_reg_read(unit, PHY_ADDR(unit), 24)));
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("pag2, reg %d(0x%x)\n", 25, phy_reg_read(unit, PHY_ADDR(unit), 25)));

    setReg22(unit, 3);
    for (i=16; i < 19; i++) {
        DRV_PRINT(DRV_DEBUG_PHYSETUP,
                ("pag3, reg %d(0x%x)\n", i, phy_reg_read(unit, PHY_ADDR(unit), i)));
    }

    setReg22(unit, 5);
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("pag5, reg %d(0x%x)\n", 20, phy_reg_read(unit, PHY_ADDR(unit), 20)));
    DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("pag5, reg %d(0x%x)\n", 21, phy_reg_read(unit, PHY_ADDR(unit), 21)));

    setReg22(unit, 0);

    return;
}
#endif

#define MV_MAX_PAGE     7
#define MV_MAX_REG      32
typedef enum {
    REG_UNUSED=0,
    REG_RO,
    REG_RW,
} mv_phy_reg_type;
unsigned char regs_array[MV_MAX_PAGE][MV_MAX_REG];

int
mv_phyRegRead(int unit, int page, int reg, unsigned short *val)
{
    if ((unit < 0) || (unit > 1) || (page < 0) || (page >= MV_MAX_PAGE) ||
        (reg < 0) || (reg >= MV_MAX_REG) || (!regs_array[page][reg])) {
        printk(": Invalid parameters unit=%d page=%d reg=%d\n",
            unit, page, reg);
        return FALSE;
    }

    //printk("%s: unit=%d page=%d reg=%d\n",
    //    __func__, unit, page, reg);

    setReg22(unit, page);
    *val = phy_reg_read(unit, PHY_ADDR(unit), reg);
    setReg22(unit, 0);
    return TRUE;
}

int
mv_phyRegWrite(int unit, int page, int reg, unsigned short val)
{
    if ((unit < 0) || (unit > 1) || (page < 0) || (page >= MV_MAX_PAGE) ||
        (reg < 0) || (reg >= MV_MAX_REG) || (!regs_array[page][reg])) {
        printk(": invalid parameters unit=%d page=%d reg=%d val=0x%x\n",
            unit, page, reg, val);
        return FALSE;
    }

    //printk("%s: unit=%d page=%d reg=%d val=0x%x\n",
    //    __func__, unit, page, reg, val);

    setReg22(unit, page);
    phy_reg_write(unit, PHY_ADDR(unit), reg, val);
    setReg22(unit, 0);
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
mv_phySetup(int unit)
{
    if (unit > 1) {
        /* two rgmii */
        INFO_PRINT("%s: invalid unit %d\n", __FUNCTION__, unit);
    }

    setReg22(unit, 0);

    memset(regs_array, 0, (MV_MAX_PAGE*MV_MAX_REG));

    regs_array[0][1] =
    regs_array[0][2] =
    regs_array[0][3] =
    regs_array[0][5] =
    regs_array[0][6] =
    regs_array[0][8] =
    regs_array[0][10] =
    regs_array[0][15] =
    regs_array[0][17] =
    regs_array[0][19] =
    regs_array[0][21] =
    regs_array[0][23] =
    regs_array[2][19] =
    regs_array[5][16] =
    regs_array[5][17] =
    regs_array[5][18] =
    regs_array[5][19] =
    regs_array[5][20] =
    regs_array[5][21] =
    regs_array[6][17] = REG_RO;


    regs_array[0][0] =
    regs_array[0][4] =
    regs_array[0][7] =
    regs_array[0][9] =
    regs_array[0][16] =
    regs_array[0][18] =
    regs_array[0][20] =
    regs_array[0][22] =
    regs_array[0][26] =
    regs_array[0][29] =         // debug reg: page
    regs_array[0][30] =         // debug reg: value
    regs_array[2][16] =
    regs_array[2][18] =
    regs_array[2][21] =
    regs_array[2][24] =
    regs_array[2][25] =
    regs_array[3][16] =
    regs_array[3][17] =
    regs_array[3][18] =
    regs_array[5][23] =
    regs_array[5][24] =
    regs_array[5][25] =
    regs_array[5][26] =
    regs_array[5][27] =
    regs_array[5][28] =
    regs_array[6][16] =
    regs_array[6][26] = REG_RW;

#if defined(CONFIG_AR9100)
    if (rks_is_gd12_board()) {
        rks_wrphy_cb_reg(mv_phyRegWrite);
    }
#endif

#if 0
    /* if non-cat5 cable, downshift after 3 attempts */
    phy_reg_write(unit, PHY_ADDR(unit), MII_COPPER_CTRL1_REG,
        ((phy_reg_read(unit, PHY_ADDR(unit), MII_COPPER_CTRL1_REG) &
        ~MII_DS_CNT_MASK) | 0x2800));
#endif

    /* sw reset */
    phy_reg_write(unit, PHY_ADDR(unit), MII_CTRL_REG,
        (MII_PHY_RESET | phy_reg_read(unit, PHY_ADDR(unit), MII_CTRL_REG)));
#ifdef __ECOS
    sysMsDelay(500);
#else
    mdelay(500);
#endif

#if 0
    setReg22(unit, MII_LED_PAGE);
    phy_reg_write(unit, PHY_ADDR(unit), MII_LED_FUNCTION_REG, 0x1157);
    phy_reg_write(unit, PHY_ADDR(unit), MII_LED_POLARITY_REG, 0x0005);
    phy_reg_write(unit, PHY_ADDR(unit), MII_LED_TIMER_REG, 0x1205);
    setReg22(unit, 0);
#endif

    //phyId(unit);
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
mv_phyIsFullDuplex(int unit)
{
    uint16_t data;

    data = phy_reg_read(unit, PHY_ADDR(unit), MII_COPPER_STATUS1_REG);
    DRV_PRINT(DRV_DEBUG_PHYSTATUS,
        ("IsFullDuplex(0x%x) 0x%x\n", data, (data & MII_COPPER_DUPLEX)?1:0));
    return ((data & MII_COPPER_DUPLEX)?1:0);
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
mv_phySpeed(int unit)
{
    uint16_t data;

    data = phy_reg_read(unit, PHY_ADDR(unit), MII_COPPER_STATUS1_REG);
    DRV_PRINT(DRV_DEBUG_PHYSTATUS,
        ("Speed(0x%x) 0x%x\n", data, (data & MII_COPPER_SPEED) >> MII_COPPER_SPEED_SHIFT));
    return ((data & MII_COPPER_SPEED) >> MII_COPPER_SPEED_SHIFT);
}

int
mv_phyIsUp(int unit)
{
    uint16_t data;
    static int setled0;
    static int setled1;
    unsigned short temp;
    int val;
    extern unsigned char rks_is_gd10_board(void);

    if (!unit && (setled0 < 2)) {
        setled0++;
        if (setled0 == 2) {
            setReg22(unit, MII_LED_PAGE);

            phy_reg_write(unit, PHY_ADDR(unit), MII_LED_FUNCTION_REG, 0x180f);
            phy_reg_write(unit, PHY_ADDR(unit), MII_LED_POLARITY_REG, 0x8815);
            phy_reg_write(unit, PHY_ADDR(unit), MII_LED_TIMER_REG, 0x1205);

            DRV_PRINT(DRV_DEBUG_PHYSETUP,
                ("unit%d MII_LED_FUNCTION_REG 0x%x\n", unit,
                phy_reg_read(unit, PHY_ADDR(unit), MII_LED_FUNCTION_REG)));
            DRV_PRINT(DRV_DEBUG_PHYSETUP,
                ("MII_LED_POLARITY_REG 0x%x\n",
                phy_reg_read(unit, PHY_ADDR(unit), MII_LED_POLARITY_REG)));
            DRV_PRINT(DRV_DEBUG_PHYSETUP,
                ("MII_LED_TIMER_REG 0x%x\n",
                phy_reg_read(unit, PHY_ADDR(unit), MII_LED_TIMER_REG)));
            setReg22(unit, 0);
        }
    }

    if (unit && (setled1 < 2)) {
        setled1++;
        if (setled1 == 2) {
            setReg22(unit, MII_LED_PAGE);

            phy_reg_write(unit, PHY_ADDR(unit), MII_LED_FUNCTION_REG, 0x180f);
            phy_reg_write(unit, PHY_ADDR(unit), MII_LED_POLARITY_REG, 0x8815);
            phy_reg_write(unit, PHY_ADDR(unit), MII_LED_TIMER_REG, 0x1205);

            DRV_PRINT(DRV_DEBUG_PHYSETUP,
                ("unit%d MII_LED_FUNCTION_REG 0x%x\n", unit,
                phy_reg_read(unit, PHY_ADDR(unit), MII_LED_FUNCTION_REG)));
            DRV_PRINT(DRV_DEBUG_PHYSETUP,
                ("MII_LED_POLARITY_REG 0x%x\n",
                phy_reg_read(unit, PHY_ADDR(unit), MII_LED_POLARITY_REG)));
            DRV_PRINT(DRV_DEBUG_PHYSETUP,
                ("MII_LED_TIMER_REG 0x%x\n",
                phy_reg_read(unit, PHY_ADDR(unit), MII_LED_TIMER_REG)));
            setReg22(unit, 0);
        }

        if (rks_is_gd10_board()) {
            temp = phy_reg_read(unit, PHY_ADDR(unit), MII_1000BT_CTRL_REG);
            if ((temp & MII_1000BT_MASK) == MII_1000BT_MASK) {
                temp &= ~MII_1000BT_MASK;
                phy_reg_write(unit, PHY_ADDR(unit), MII_1000BT_CTRL_REG, temp);
                /* restart auto-negotiation */
                phy_reg_write(unit, PHY_ADDR(unit), MII_CTRL_REG,
                    (MII_RESTART_AN | phy_reg_read(unit, PHY_ADDR(unit), MII_CTRL_REG)));
            }
        }
    }

    data = phy_reg_read(unit, PHY_ADDR(unit), MII_COPPER_STATUS1_REG);

    if (!(data & MII_COPPER_LINK_RT) || !(data & MII_COPPER_RESOLVED)) {
        /* workaround 100mbps auto-negotiation error */
        if (mv_war_info[unit].current_war_state != IDLE)
            printk(" phy%d war(%d)\n", unit, mv_war_info[unit].current_war_state);
        val = FALSE;
        switch (mv_war_info[unit].current_war_state) {
            case AN_ERR:
                if (data & MII_COPPER_RESOLVED) {
                    mv_war_info[unit].chk_resolved = 0;
                    mv_war_info[unit].current_war_state = SET_WAR_REG;
                    phy_reg_write(unit, PHY_ADDR(unit), MII_DEBUG_PAGE_REG, 0);
                    phy_reg_write(unit, PHY_ADDR(unit), MII_DEBUG_VALUE_REG, 0x0180);
                } else {
                    mv_war_info[unit].chk_resolved++;
                    if (mv_war_info[unit].chk_resolved == 2) {
                        mv_war_info[unit].chk_resolved = 0;
                        mv_war_info[unit].current_war_state = IDLE;
                    }
                }
                break;
            case SET_WAR_REG:
                if (data & MII_COPPER_LINK_RT) {
                    mv_war_info[unit].chk_linkup = 0;
                    mv_war_info[unit].current_war_state = IDLE;
                    phy_reg_write(unit, PHY_ADDR(unit), MII_DEBUG_PAGE_REG, 0);
                    phy_reg_write(unit, PHY_ADDR(unit), MII_DEBUG_VALUE_REG, 0);
                    val = TRUE;
                } else {
                    mv_war_info[unit].chk_linkup++;
                    if (mv_war_info[unit].chk_linkup == 2) {
                        mv_war_info[unit].chk_linkup = 0;
                        mv_war_info[unit].current_war_state = IDLE;
                        phy_reg_write(unit, PHY_ADDR(unit), MII_DEBUG_PAGE_REG, 0);
                        phy_reg_write(unit, PHY_ADDR(unit), MII_DEBUG_VALUE_REG, 0);
                    }
                }
                break;
            case IDLE:
            default:
                temp = phy_reg_read(unit, PHY_ADDR(unit), MII_COPPER_STATUS2_REG);
                if (temp & MII_COPPER_AN_ERROR) {
                    printk(" phy%d ane_bit cs2(0x%x) cs1(0x%x)\n", unit, temp, data);
                    mv_war_info[unit].current_war_state = AN_ERR;
                    phy_reg_write(unit, PHY_ADDR(unit), MII_CTRL_REG,
                        (MII_RESTART_AN | phy_reg_read(unit, PHY_ADDR(unit), MII_CTRL_REG)));
                }
                break;
        }
    } else {
        // reset workaround counters
        mv_war_info[unit].current_war_state = IDLE;
        mv_war_info[unit].chk_resolved = 0;
        mv_war_info[unit].chk_linkup = 0;
        mv_war_info[unit].chk_error = 0;

        phy_reg_write(unit, PHY_ADDR(unit), MII_DEBUG_PAGE_REG, 0);
        phy_reg_write(unit, PHY_ADDR(unit), MII_DEBUG_VALUE_REG, 0);

        val = TRUE;
    }

    return val;
}

void mvPhy_force_100M(int unit,int duplex)
{
    mv_phyRegWrite(unit, 0, MII_CTRL_REG, (0xa000|(duplex << 8)));
}

void mvPhy_force_10M(int unit,int duplex)
{
    mv_phyRegWrite(unit, 0, MII_CTRL_REG, (0x8000|(duplex << 8)));
}

void mvPhy_force_auto(int unit, int max_100)
{
    if ((max_100 == 1) || (rks_is_gd10_board() && unit)) {
        mv_phyRegWrite(unit, 0, MII_1000BT_CTRL_REG, 0);
    mv_phyRegWrite(unit, 0, MII_CTRL_REG, 0x9000);
    } else {
        mv_phyRegWrite(unit, 0, MII_1000BT_CTRL_REG, MII_1000BT_MASK);
        mv_phyRegWrite(unit, 0, MII_CTRL_REG, 0x9000);
    }
}

int mvPhy_ioctl(uint32_t *args, int cmd)
{
    struct eth_diag *etd =(struct eth_diag *) args;
    unsigned short val;
    uint16_t portnum=etd->ed_u.portnum;
    uint32_t phy_reg=etd->phy_reg;

    if (rks_is_gd11_board() && (ethname_remap == gd11_ethname_remap)) {
        if(cmd == ATHR_FORCE_PHY) {
            /* phy_reg: force command port number */
            phy_reg = 0;
        } else if (portnum > 1) {
            /* portnum: r/w command port number */
            portnum = 0;
        }
    }

    if(cmd  == ATHR_RD_PHY) {
        etd->val = 0;
        if(etd->ed_u.portnum != 0xf) {
            if (mv_phyRegRead(portnum, ((phy_reg >> 16) & 0xffff),
                (phy_reg & 0xffff), &val)) {
                etd->val = val;
            }
        }
    } else if(cmd  == ATHR_WR_PHY) {
        if(etd->ed_u.portnum != 0xf) {
            mv_phyRegWrite(portnum, ((phy_reg >> 16) & 0xffff),
            (phy_reg & 0xffff), etd->val);
        }
    }
    else if(cmd == ATHR_FORCE_PHY) {
        if(phy_reg < 3) {
            if(etd->val == 10) {
                printk("Forcing 10Mbps %s on port:%d \n",
                    dup_str[etd->ed_u.duplex], (phy_reg));
                mvPhy_force_10M(phy_reg, etd->ed_u.duplex);
            } else if(etd->val == 100) {
                printk("Forcing 100Mbps %s on port:%d \n",
                    dup_str[etd->ed_u.duplex],(phy_reg));
                mvPhy_force_100M(phy_reg, etd->ed_u.duplex);
            } else if(etd->val == 0) {
                printk("Enabling Auto Neg (1000Mbps) on port:%d \n",(phy_reg));
                mvPhy_force_auto(phy_reg, 0);
            } else if(etd->val == 1) {
                printk("Enabling Auto Neg (100Mbps) on port:%d \n",(phy_reg));
                mvPhy_force_auto(phy_reg, 1);
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
