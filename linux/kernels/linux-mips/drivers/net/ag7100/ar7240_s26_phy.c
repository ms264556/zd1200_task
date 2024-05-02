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
#if !defined(V54_BSP)
#include "ag7240_phy.h"
#include "ag7240.h"
#include "ar7240_s26_phy.h"
#include "eth_diag.h"
#else
#include "ag7100_phy.h"
#include "ar7240_s26_phy.h"
#include "eth_diag.h"
#endif


/* PHY selections and access functions */

typedef enum {
    PHY_SRCPORT_INFO,
    PHY_PORTINFO_SIZE,
} PHY_CAP_TYPE;

typedef enum {
    PHY_SRCPORT_NONE,
    PHY_SRCPORT_VLANTAG,
    PHY_SRCPORT_TRAILER,
} PHY_SRCPORT_TYPE;

#define DRV_DEBUG 0
#if DRV_DEBUG
#define DRV_DEBUG_PHYSETUP  0x00000001
#define DRV_DEBUG_PHYCHANGE 0x00000002
#define DRV_DEBUG_PHYSTATUS 0x00000004

int PhyDebug = DRV_DEBUG_PHYSETUP;

#define DRV_PRINT(FLG, X)                           \
{                                                   \
    if (PhyDebug & (FLG)) {                      \
        printk X;                                   \
    }                                               \
}
#else
#define DRV_LOG(DBG_SW, X0, X1, X2, X3, X4, X5, X6)
#define DRV_MSG(x,a,b,c,d,e,f)
#define DRV_PRINT(DBG_SW,X)
#endif

#define ATHR_LAN_PORT_VLAN          1
#define ATHR_WAN_PORT_VLAN          2
#define ENET_UNIT_LAN 1
#define ENET_UNIT_WAN 0

#define TRUE    1
#define FALSE   0

#define ATHR_PHY0_ADDR   0x0
#define ATHR_PHY1_ADDR   0x1
#define ATHR_PHY2_ADDR   0x2
#define ATHR_PHY3_ADDR   0x3
#define ATHR_PHY4_ADDR   0x4

#define MODULE_NAME "ATHRS26"
MODULE_LICENSE("Dual BSD/GPL");

//#define S26_PHY_DEBUG   1
#ifdef S26_PHY_DEBUG
#define DPRINTF(_fmt,...) do {         \
                printk(MODULE_NAME":"_fmt, __VA_ARGS__);      \
} while (0)
#else
#define DPRINTF(_fmt,...)
#endif

#ifdef ETH_SOFT_LED
extern ATH_LED_CONTROL    PLedCtrl;
extern atomic_t Ledstatus;
#endif
extern int phy_in_reset;
extern char *dup_str[];

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
     ATHR_PHY0_ADDR,
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
     ATHR_PHY4_ADDR,
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

static uint8_t athr26_init_flag = 0,athr26_init_flag1 = 0;
static DECLARE_WAIT_QUEUE_HEAD (hd_conf_wait);
static ag7240_mac_t *ag7240_macs[2];

#define ATHR_GLOBALREGBASE    0

#define ATHR_PHY_MAX 5

/* Range of valid PHY IDs is [MIN..MAX] */
#define ATHR_ID_MIN 0
#define ATHR_ID_MAX (ATHR_PHY_MAX-1)

/* Convenience macros to access myPhyInfo */
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
BOOL athrs26_phy_is_link_alive(int phyUnit);
uint32_t athrs26_reg_read(uint32_t reg_addr);
void athrs26_reg_write(uint32_t reg_addr, uint32_t reg_val);
unsigned int s26_rd_phy(unsigned int phy_addr, unsigned int reg_addr);
void s26_wr_phy(unsigned int phy_addr, unsigned int reg_addr, unsigned int write_data);
#if !defined(V54_BSP)
extern int ag7240_check_link(ag7240_mac_t *mac,int phyUnit);
#else
extern int ag7100_check_link(ag7240_mac_t *mac,int phyUnit);
static int function_ctrl=0x862;
static int debug_port_2=0x36c4;
#endif

#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
static uint16_t port_def_vid[5] = {1, 1, 1, 1, 1};
static uint8_t cpu_egress_tagged_flag = 0;

inline uint8_t is_cpu_egress_tagged(void)
{
    return cpu_egress_tagged_flag;
}

void set_cpu_egress_tagged(uint8_t is_tagged)
{
    cpu_egress_tagged_flag = is_tagged;
}

inline uint16_t athrs26_defvid_get(uint32_t port_id)
{
    if ((port_id == 0) || (port_id > 5))
        return 0;

    return port_def_vid[port_id - 1];
}

BOOL athrs26_defvid_set(uint32_t port_id, uint16_t def_vid)
{
    if ((def_vid == 0) || (def_vid > 4094))
        return FALSE;

    if ((port_id == 0) || (port_id > 5))
        return FALSE;

    port_def_vid[port_id - 1] = def_vid;
    return TRUE;
}
#endif

void athrs26_powersave_off(int phy_addr)
{
    s26_wr_phy(phy_addr,ATHR_DEBUG_PORT_ADDRESS,0x29);
    s26_wr_phy(phy_addr,ATHR_DEBUG_PORT_DATA,0x36c0);

}

void athrs26_sleep_off(int phy_addr)
{
    s26_wr_phy(phy_addr,ATHR_DEBUG_PORT_ADDRESS,0xb);
    s26_wr_phy(phy_addr,ATHR_DEBUG_PORT_DATA,0x3c00);
}

void athrs26_enable_linkIntrs(int ethUnit)
{
     int phyUnit = 0, phyAddr = 0;

    /* Enable global PHY link status interrupt */
    athrs26_reg_write(S26_GLOBAL_INTR_MASK_REG,PHY_LINK_CHANGE_REG);
#ifdef CONFIG_S26_SWITCH_ONLY_MODE
     if (is_ar7241()) {
         if (ethUnit == 1) {
             for (phyUnit = 0; phyUnit < ATHR_PHY_MAX; phyUnit++) {
                 phyAddr = ATHR_PHYADDR(phyUnit);
                 s26_wr_phy(phyAddr,ATHR_PHY_INTR_ENABLE,PHY_LINK_INTRS);
             }
         }
         return ;
      }
#endif

     if (ethUnit == ENET_UNIT_WAN) {
         s26_wr_phy(ATHR_PHY4_ADDR,ATHR_PHY_INTR_ENABLE,PHY_LINK_INTRS);
     }
     else {
         for (phyUnit = 0; phyUnit < ATHR_PHY_MAX - 1; phyUnit++) {
             phyAddr = ATHR_PHYADDR(phyUnit);
             s26_wr_phy(phyAddr,ATHR_PHY_INTR_ENABLE,PHY_LINK_INTRS);
         }
     }
}

void athrs26_disable_linkIntrs(int ethUnit)
{
     int phyUnit = 0, phyAddr = 0;

#ifdef CONFIG_S26_SWITCH_ONLY_MODE
     if (is_ar7241()) {
         if (ethUnit == 1) {
             for (phyUnit = 0; phyUnit < ATHR_PHY_MAX; phyUnit++) {
                 phyAddr = ATHR_PHYADDR(phyUnit);
                 s26_wr_phy(phyAddr,ATHR_PHY_INTR_ENABLE,0x0);
             }
         }
         return ;
      }
#endif

     if (ethUnit == ENET_UNIT_WAN) {
         s26_wr_phy(ATHR_PHY4_ADDR,ATHR_PHY_INTR_ENABLE,0x0);
     }
     else {
         for (phyUnit = 0; phyUnit < ATHR_PHY_MAX - 1; phyUnit++) {
             phyAddr = ATHR_PHYADDR(phyUnit);
             s26_wr_phy(phyAddr,ATHR_PHY_INTR_ENABLE,0x0);
         }
     }
}

#if defined(V54_BSP)
void athrs26_force_auto(int phyAddr,int duplex)
{
    uint32_t ar7240_revid;

    phy_in_reset = 1;

    ar7240_revid = ar7100_reg_rd(AR7100_REV_ID) & AR7100_REV_ID_MASK;

    if(ar7240_revid == AR7240_REV_1_0) {
        s26_wr_phy(phyAddr,ATHR_DEBUG_PORT_ADDRESS,0x0);
        s26_wr_phy(phyAddr,ATHR_DEBUG_PORT_DATA,0x2ee);
        s26_wr_phy(phyAddr,ATHR_DEBUG_PORT_ADDRESS,0x3);
        s26_wr_phy(phyAddr,ATHR_DEBUG_PORT_DATA,0x3a11);
    }

    if (is_ar934x()) {
        /* s27 */
        s26_wr_phy(phyAddr,0x1d,0x29);
        s26_wr_phy(phyAddr,0x1e,debug_port_2);
        s26_wr_phy(phyAddr,0x10,function_ctrl);
    }

    s26_wr_phy(phyAddr,ATHR_PHY_CONTROL,0x9000);
    phy_in_reset = 0;
}
#endif

void athrs26_force_100M(int phyAddr,int duplex)
{
    uint32_t ar7240_revid;

#ifdef ETH_SOFT_LED
    /* Resetting PHY will disable MDIO access needed by soft LED task.
     * Hence, Do not reset PHY until Soft LED task get completed.
     */
    while(atomic_read(&Ledstatus) == 1);
    PLedCtrl.ledlink[phyAddr] = 0;
#endif
    phy_in_reset = 1;
#if !defined(V54_BSP)
#error "mdi, mdx"
   /*
    *  Force MDI and MDX to alternate ports
    *  Phy 0,2 and 4 -- MDI
    *  Phy 1 and 3 -- MDX
    */
    if(phyAddr%2) {
         s26_wr_phy(phyAddr,ATHR_PHY_FUNC_CONTROL,0x820);
    }
    else {
        s26_wr_phy(phyAddr,ATHR_PHY_FUNC_CONTROL,0x800);
    }
#endif

#if !defined(V54_BSP)
    ar7240_revid = ar7240_reg_rd(AR7240_REV_ID) & AR7240_REV_ID_MASK;
#else
    ar7240_revid = ar7100_reg_rd(AR7100_REV_ID) & AR7100_REV_ID_MASK;
#endif

    if(ar7240_revid == AR7240_REV_1_0) {
        s26_wr_phy(phyAddr,ATHR_DEBUG_PORT_ADDRESS,0x0);
        s26_wr_phy(phyAddr,ATHR_DEBUG_PORT_DATA,0x2ee);
        s26_wr_phy(phyAddr,ATHR_DEBUG_PORT_ADDRESS,0x3);
        s26_wr_phy(phyAddr,ATHR_DEBUG_PORT_DATA,0x3a11);
    }

    if (is_ar934x()) {
        /* s27 */
        s26_wr_phy(phyAddr,0x1d,0x29);
        s26_wr_phy(phyAddr,0x1e,0x0);
        s26_wr_phy(phyAddr,0x10,0xc60);
    }

    s26_wr_phy(phyAddr,ATHR_PHY_CONTROL,(0xa000|(duplex << 8)));
    phy_in_reset = 0;
}

void athrs26_force_10M(int phyAddr,int duplex)
{
    uint32_t ar7240_revid;
#ifdef ETH_SOFT_LED
    /* Resetting PHY will disable MDIO access needed by soft LED task.
     * Hence, Do not reset PHY until Soft LED task get completed.
     */
    while(atomic_read(&Ledstatus) == 1);
    PLedCtrl.ledlink[phyAddr] = 0;
#endif
    phy_in_reset = 1;

    athrs26_powersave_off(phyAddr);
    athrs26_sleep_off(phyAddr);

#if !defined(V54_BSP)
    ar7240_revid = ar7240_reg_rd(AR7240_REV_ID) & AR7240_REV_ID_MASK;
#else
    ar7240_revid = ar7100_reg_rd(AR7100_REV_ID) & AR7100_REV_ID_MASK;
#endif

    if(ar7240_revid == AR7240_REV_1_0) {
        s26_wr_phy(phyAddr,ATHR_PHY_FUNC_CONTROL,0x800);
        s26_wr_phy(phyAddr,ATHR_PHY_CONTROL,0x8100);
        s26_wr_phy(phyAddr,ATHR_DEBUG_PORT_ADDRESS,0x0);
        s26_wr_phy(phyAddr,ATHR_DEBUG_PORT_DATA,0x12ee);
        s26_wr_phy(phyAddr,ATHR_DEBUG_PORT_ADDRESS,0x3);
        s26_wr_phy(phyAddr,ATHR_DEBUG_PORT_DATA,0x3af0);
    }
    s26_wr_phy(phyAddr,ATHR_PHY_CONTROL,(0x8000 |(duplex << 8)));
    phy_in_reset = 0;
}

void athrs26_reg_init(void)
{
    uint32_t ar7240_revid;
    uint32_t rd_data;

    /* if using header for register configuration, we have to     */
    /* configure s26 register after frame transmission is enabled */
    if (athr26_init_flag)
        return;

    if (is_ar934x())
        athrs27_reg_rmw(0x8,(1<<28));  /* Set WAN port is connected to GE0 */

#if !defined(V54_BSP)
    ar7240_revid = ar7240_reg_rd(AR7240_REV_ID) & AR7240_REV_ID_MASK;
#else
    ar7240_revid = ar7100_reg_rd(AR7100_REV_ID) & AR7100_REV_ID_MASK;
#endif

    if(ar7240_revid == AR7240_REV_1_0) {
#ifdef S26_FORCE_100M
        athrs26_force_100M(ATHR_PHY4_ADDR,1);
#endif
#ifdef S26_FORCE_10M
        athrs26_force_10M(ATHR_PHY4_ADDR,1);
#endif
    } else {
        s26_wr_phy(ATHR_PHY4_ADDR,ATHR_PHY_CONTROL,0x9000);

        if (!is_ar934x()) {
            /* Class A setting for 100M */
            s26_wr_phy(ATHR_PHY4_ADDR, 29, 5);
            s26_wr_phy(ATHR_PHY4_ADDR, 30, (s26_rd_phy(ATHR_PHY4_ADDR, 30)&((~2)&0xffff)));
        }
    }

    if (!is_ar934x())
        /* Enable flow control on CPU port */
        athrs26_reg_write(PORT_STATUS_REGISTER0,
                          (athrs26_reg_read(PORT_STATUS_REGISTER0) | 0x30));

    /* Disable WAN mac inside S26 */
    athrs26_reg_write(PORT_STATUS_REGISTER5,0x0);

#ifdef CONFIG_S26_SWITCH_ONLY_MODE
    /* Enable WAN mac inside S26 */
    if (is_ar7241())
        athrs26_reg_write(PORT_STATUS_REGISTER5,0x200);
#endif

    /* Enable MDIO Access if PHY is Powered-down */
    s26_wr_phy(ATHR_PHY4_ADDR,ATHR_DEBUG_PORT_ADDRESS,0x3);
    rd_data = s26_rd_phy(ATHR_PHY4_ADDR,ATHR_DEBUG_PORT_DATA);
    s26_wr_phy(ATHR_PHY4_ADDR,ATHR_DEBUG_PORT_ADDRESS,0x3);
    s26_wr_phy(ATHR_PHY4_ADDR,ATHR_DEBUG_PORT_DATA,(rd_data & 0xfffffeff) );

    if (!is_ar934x())
        /* header on, no learn */
        athrs26_reg_write(PORT_CONTROL_REGISTER0, 0x0804);
    else
        athrs26_reg_write(PORT_CONTROL_REGISTER0, 0x4004);

    athr26_init_flag = 1;
}

#if defined(V54_BSP)
void athrs26_reg_init_lan(int vlan)
#else
void athrs26_reg_init_lan(void)
#endif
{
    int i = 60;
    int       phyUnit;
    uint32_t  phyBase = 0;
    BOOL      foundPhy = FALSE;
    uint32_t  phyAddr = 0;
    uint32_t  queue_ctrl_reg = 0;
    uint32_t  ar7240_revid;
    uint32_t rd_data;

    /* if using header for register configuration, we have to     */
    /* configure s26 register after frame transmission is enabled */
    if (athr26_init_flag1)
        return;

    /* reset switch */
    printk(MODULE_NAME ": resetting s26\n");
    athrs26_reg_write(0x0, athrs26_reg_read(0x0)|0x80000000);

    while(i--) {
	mdelay(100);
	if(!(athrs26_reg_read(0x0)&0x80000000))
		break;
    }
    printk(MODULE_NAME ": s26 reset done\n");

    if (is_ar934x()) {
        athrs26_reg_write(PORT_STATUS_REGISTER0,0x4e);

        /* Enable flow control on CPU port */
        athrs26_reg_write(PORT_STATUS_REGISTER0,
                          (athrs26_reg_read(PORT_STATUS_REGISTER0) | 0x30));

        athrs27_reg_rmw(OPERATIONAL_MODE_REG0,(1<<6));  /* Set GMII mode */

        athrs27_reg_rmw(0x2c,((1<<26)| (1<<16) | 0x1)); /* FiX ME: EBU debug */
    } else {

       /* 100M Half Duplex Throughput setting */
        athrs26_reg_write(0xe4,0x079c1040);

       /* Change HOL settings
        * 25: PORT_QUEUE_CTRL_ENABLE.
        * 24: PRI_QUEUE_CTRL_EN.
        */
        athrs26_reg_write(0x118,0x0032b5555);
    }

    for (phyUnit=0; phyUnit < ATHR_PHY_MAX; phyUnit++) {

#ifndef CONFIG_S26_SWITCH_ONLY_MODE
        if (ATHR_ETHUNIT(phyUnit) == ENET_UNIT_WAN)
            continue;
#else
        if (!is_ar7241()) {
            if (ATHR_ETHUNIT(phyUnit) == ENET_UNIT_WAN)
                continue;

        }
#endif
        foundPhy = TRUE;
        phyBase = ATHR_PHYBASE(phyUnit);
        phyAddr = ATHR_PHYADDR(phyUnit);

#if !defined(V54_BSP)
        ar7240_revid = ar7240_reg_rd(AR7240_REV_ID) & AR7240_REV_ID_MASK;
#else
        ar7240_revid = ar7100_reg_rd(AR7100_REV_ID) & AR7100_REV_ID_MASK;
#endif

        if(ar7240_revid == AR7240_REV_1_0) {
#ifdef S26_FORCE_100M
            athrs26_force_100M(phyAddr,1);
#endif
#ifdef S26_FORCE_10M
            athrs26_force_10M(phyAddr,1);
#endif
        }
        else {
            s26_wr_phy(phyAddr,ATHR_PHY_CONTROL,0x9000);

           /* Class A setting for 100M */
            s26_wr_phy(phyAddr, 29, 5);
            s26_wr_phy(phyAddr, 30, (s26_rd_phy(phyAddr, 30)&((~2)&0xffff)));
        }

        /* Enable MDIO Access if PHY is Powered-down */
        s26_wr_phy(phyAddr,ATHR_DEBUG_PORT_ADDRESS,0x3);
        rd_data = s26_rd_phy(phyAddr,ATHR_DEBUG_PORT_DATA);
        s26_wr_phy(phyAddr,ATHR_DEBUG_PORT_ADDRESS,0x3);
        s26_wr_phy(phyAddr,ATHR_DEBUG_PORT_DATA,(rd_data & 0xfffffeff) );

        /* Change HOL settings
         * 25: PORT_QUEUE_CTRL_ENABLE.
         * 24: PRI_QUEUE_CTRL_EN.
         */
        queue_ctrl_reg = (0x100 * phyUnit) + 0x218 ;
        athrs26_reg_write(queue_ctrl_reg,0x032b5555);

        DPRINTF("S26 ATHR_PHY_FUNC_CONTROL (%d):%x\n",phyAddr,
            s26_rd_phy(phyAddr,ATHR_PHY_FUNC_CONTROL));
        DPRINTF("S26 PHY ID  (%d) :%x\n",phyAddr,
            s26_rd_phy(phyAddr,ATHR_PHY_ID1));
        DPRINTF("S26 PHY CTRL  (%d) :%x\n",phyAddr,
            s26_rd_phy(phyAddr,ATHR_PHY_CONTROL));
        DPRINTF("S26 ATHR PHY STATUS  (%d) :%x\n",phyAddr,
            s26_rd_phy(phyAddr,ATHR_PHY_STATUS));
    }

    /*
     * CPU port Enable
     */

    /*
     * status[1:0]=2'h2;   - (0x10 - 1000 Mbps , 0x01 - 100Mbps, 0x0 - 10 Mbps)
     * status[2]=1'h1;     - Tx Mac En
     * status[3]=1'h1;     - Rx Mac En
     * status[4]=1'h1;     - Tx Flow Ctrl En
     * status[5]=1'h1;     - Rx Flow Ctrl En
     * status[6]=1'h1;     - Duplex Mode
     */
#ifdef CONFIG_AR7240_EMULATION
    athrs26_reg_write(PORT_STATUS_REGISTER0, 0x7e);  /* CPU Port */
    athrs26_reg_write(PORT_STATUS_REGISTER1, 0x3c);
    athrs26_reg_write(PORT_STATUS_REGISTER2, 0x3c);
    athrs26_reg_write(PORT_STATUS_REGISTER3, 0x3c);
    athrs26_reg_write(PORT_STATUS_REGISTER4, 0x3c);
#else
    if (is_ar934x()) {
        athrs26_reg_write(PORT_STATUS_REGISTER1, S27_PORT_STATUS_DEFAULT);  /* LAN - 1 */
        athrs26_reg_write(PORT_STATUS_REGISTER2, S27_PORT_STATUS_DEFAULT);  /* LAN - 2 */
        athrs26_reg_write(PORT_STATUS_REGISTER3, S27_PORT_STATUS_DEFAULT);  /* LAN - 3 */
        athrs26_reg_write(PORT_STATUS_REGISTER4, S27_PORT_STATUS_DEFAULT);  /* LAN - 4 */
    } else {
        athrs26_reg_write(PORT_STATUS_REGISTER1, 0x200);  /* LAN - 1 */
        athrs26_reg_write(PORT_STATUS_REGISTER2, 0x200);  /* LAN - 2 */
        athrs26_reg_write(PORT_STATUS_REGISTER3, 0x200);  /* LAN - 3 */
        athrs26_reg_write(PORT_STATUS_REGISTER4, 0x200);  /* LAN - 4 */
    }
#endif

    if (!is_ar934x())
        /* QM Control */
        athrs26_reg_write(0x38, 0xc000050e);

    /*
     * status[11]=1'h0;    - CPU Disable
     * status[7] = 1'b1;   - Learn One Lock
     * status[14] = 1'b0;  - Learn Enable
     */
#ifdef CONFIG_AR7240_EMULATION
    athrs26_reg_write(PORT_CONTROL_REGISTER0, 0x04);
    athrs26_reg_write(PORT_CONTROL_REGISTER1, 0x4004);
#else
#if !defined(V54_BSP)
   /* Atheros Header Disable */
    athrs26_reg_write(PORT_CONTROL_REGISTER0, 0x4004);
#else
    if (vlan) {
        /* [9:8] - eg_vlan_mode, [2:0] - port_state */
        athrs26_reg_write(PORT_CONTROL_REGISTER0, 0x0804);
        athrs26_reg_write(PORT_CONTROL_REGISTER1, 0x0004);
        athrs26_reg_write(PORT_CONTROL_REGISTER2, 0x0004);
        athrs26_reg_write(PORT_CONTROL_REGISTER3, 0x0004);
        athrs26_reg_write(PORT_CONTROL_REGISTER4, 0x0004);
    } else {
        athrs26_reg_write(PORT_CONTROL_REGISTER0, 0x4004);
    }
#endif
#endif

   /* Tag Priority Mapping */
    athrs26_reg_write(0x70, 0xfa50);

    if (!is_ar934x())
        /* Enable ARP packets to CPU port */
        athrs26_reg_write(S26_ARL_TBL_CTRL_REG,(athrs26_reg_read(S26_ARL_TBL_CTRL_REG) | 0x100000));

    if (is_ar934x()) {
        athrs26_reg_write(S26_FLD_MASK_REG,(athrs26_reg_read(S26_FLD_MASK_REG) |
            S27_ENABLE_CPU_BCAST_FWD));
    } else {
       /* Enable Broadcast packets to CPU port */
        athrs26_reg_write(S26_FLD_MASK_REG,(athrs26_reg_read(S26_FLD_MASK_REG) |
            S26_ENABLE_CPU_BROADCAST));
    }

#if defined(V54_BSP)
    /* max_frame_size */
    DPRINTF("S26_GLOBAL_CTRL_REG :%x\n", athrs26_reg_read ( S26_GLOBAL_CTRL_REG ));
    athrs26_reg_write(S26_GLOBAL_CTRL_REG,((athrs26_reg_read(S26_GLOBAL_CTRL_REG) & ~S26_MAX_FRAME_SIZE_MASK) |
    (0x600) ));
    DPRINTF("S26_GLOBAL_CTRL_REG :%x\n", athrs26_reg_read ( S26_GLOBAL_CTRL_REG ));
#endif

   /* Turn on back pressure */
    athrs26_reg_write(PORT_STATUS_REGISTER0,
        (athrs26_reg_read(PORT_STATUS_REGISTER0) | 0x80));
    athrs26_reg_write(PORT_STATUS_REGISTER1,
        (athrs26_reg_read(PORT_STATUS_REGISTER1) | 0x80));
    athrs26_reg_write(PORT_STATUS_REGISTER2,
        (athrs26_reg_read(PORT_STATUS_REGISTER2) | 0x80));
    athrs26_reg_write(PORT_STATUS_REGISTER3,
        (athrs26_reg_read(PORT_STATUS_REGISTER3) | 0x80));
    athrs26_reg_write(PORT_STATUS_REGISTER4,
        (athrs26_reg_read(PORT_STATUS_REGISTER4) | 0x80));

    DPRINTF("S26 CPU_PORT_REGISTER :%x\n", athrs26_reg_read ( CPU_PORT_REGISTER ));
    DPRINTF("S26 PORT_STATUS_REGISTER0  :%x\n", athrs26_reg_read ( PORT_STATUS_REGISTER0 ));
    DPRINTF("S26 PORT_STATUS_REGISTER1  :%x\n", athrs26_reg_read ( PORT_STATUS_REGISTER1 ));
    DPRINTF("S26 PORT_STATUS_REGISTER2  :%x\n", athrs26_reg_read ( PORT_STATUS_REGISTER2 ));
    DPRINTF("S26 PORT_STATUS_REGISTER3  :%x\n", athrs26_reg_read ( PORT_STATUS_REGISTER3 ));
    DPRINTF("S26 PORT_STATUS_REGISTER4  :%x\n", athrs26_reg_read ( PORT_STATUS_REGISTER4 ));

    DPRINTF("S26 PORT_CONTROL_REGISTER0 :%x\n", athrs26_reg_read ( PORT_CONTROL_REGISTER0 ));
    DPRINTF("S26 PORT_CONTROL_REGISTER1 :%x\n", athrs26_reg_read ( PORT_CONTROL_REGISTER1 ));
    DPRINTF("S26 PORT_CONTROL_REGISTER2 :%x\n", athrs26_reg_read ( PORT_CONTROL_REGISTER2 ));
    DPRINTF("S26 PORT_CONTROL_REGISTER3 :%x\n", athrs26_reg_read ( PORT_CONTROL_REGISTER3 ));
    DPRINTF("S26 PORT_CONTROL_REGISTER4 :%x\n", athrs26_reg_read ( PORT_CONTROL_REGISTER4 ));

#ifdef CONFIG_AG7240_QOS
    athrs26_reg_write(ATHR_QOS_PORT_1,(athrs26_reg_read(ATHR_QOS_PORT_1)|ATHR_ENABLE_TOS));
    athrs26_reg_write(ATHR_QOS_PORT_2,(athrs26_reg_read(ATHR_QOS_PORT_2)|ATHR_ENABLE_TOS));
    athrs26_reg_write(ATHR_QOS_PORT_3,(athrs26_reg_read(ATHR_QOS_PORT_3)|ATHR_ENABLE_TOS));
    athrs26_reg_write(ATHR_QOS_PORT_4,(athrs26_reg_read(ATHR_QOS_PORT_4)|ATHR_ENABLE_TOS));
#endif

    /* Disable WAN mac inside S26 */
    athrs26_reg_write(PORT_STATUS_REGISTER5,0x0);

#ifdef CONFIG_S26_SWITCH_ONLY_MODE
    /* Enable WAN mac inside S26 */
    if (is_ar7241())
       athrs26_reg_write(PORT_STATUS_REGISTER5,0x200);
#endif


#if defined(V54_BSP)
    DPRINTF("PORT_VLAN_REGISTER0=0x%x\n", athrs26_reg_read(PORT_VLAN_REGISTER0));
    DPRINTF("PORT_VLAN_REGISTER1=0x%x\n", athrs26_reg_read(PORT_VLAN_REGISTER1));
    DPRINTF("PORT_VLAN_REGISTER2=0x%x\n", athrs26_reg_read(PORT_VLAN_REGISTER2));
    DPRINTF("PORT_VLAN_REGISTER3=0x%x\n", athrs26_reg_read(PORT_VLAN_REGISTER3));
    DPRINTF("PORT_VLAN_REGISTER4=0x%x\n", athrs26_reg_read(PORT_VLAN_REGISTER4));

    if (vlan) {
        /* [26] force_port_vlan_en, [25:16] - port_vid_member [11:0] - port_vid */
        athrs26_reg_write(PORT_VLAN_REGISTER0, 0x001e0001);
        athrs26_reg_write(PORT_VLAN_REGISTER1, 0x00010001);
        athrs26_reg_write(PORT_VLAN_REGISTER2, 0x00010001);
        athrs26_reg_write(PORT_VLAN_REGISTER3, 0x00010001);
        athrs26_reg_write(PORT_VLAN_REGISTER4, 0x00010001);
    }

    DPRINTF(".PORT_VLAN_REGISTER0=0x%x\n", athrs26_reg_read(PORT_VLAN_REGISTER0));
    DPRINTF(".PORT_VLAN_REGISTER1=0x%x\n", athrs26_reg_read(PORT_VLAN_REGISTER1));
    DPRINTF(".PORT_VLAN_REGISTER2=0x%x\n", athrs26_reg_read(PORT_VLAN_REGISTER2));
    DPRINTF(".PORT_VLAN_REGISTER3=0x%x\n", athrs26_reg_read(PORT_VLAN_REGISTER3));
    DPRINTF(".PORT_VLAN_REGISTER4=0x%x\n", athrs26_reg_read(PORT_VLAN_REGISTER4));
#endif

    athr26_init_flag1 = 1;
}
static unsigned int phy_val_saved = 0;
/******************************************************************************
*
* athrs26_phy_off - power off the phy to change its speed
*
* Power off the phy
*/
void athrs26_phy_off(ag7240_mac_t *mac)
{
    struct net_device  *dev = mac->mac_dev;

    if (mac->mac_unit == ENET_UNIT_LAN)
        return;

    netif_carrier_off(dev);
    netif_stop_queue(dev);

    phy_val_saved = s26_rd_phy(ATHR_PHY4_ADDR,ATHR_PHY_CONTROL);
    s26_wr_phy( ATHR_PHY4_ADDR, ATHR_PHY_CONTROL, phy_val_saved | 0x800);
}

/******************************************************************************
*
* athrs26_phy_on - power on the phy after speed changed
*
* Power on the phy
*/
void athrs26_phy_on(ag7240_mac_t *mac)
{
    if ((mac->mac_unit == ENET_UNIT_LAN) || (phy_val_saved == 0))
        return;

    s26_wr_phy( ATHR_PHY4_ADDR, ATHR_PHY_CONTROL, phy_val_saved & 0xf7ff);

    mdelay(2000);
}

/******************************************************************************
*
* athrs26_mac_speed_set - set mac in s26 speed mode (actually RMII mode)
*
* Set mac speed mode
*/
void athrs26_mac_speed_set(ag7240_mac_t *mac, ag7240_phy_speed_t speed)
{
    if ((mac->mac_unit == ENET_UNIT_LAN))
        return;

    switch (speed) {
        case AG7240_PHY_SPEED_100TX:
            athrs26_reg_write (0x600, 0x7d);
            break;

        case AG7240_PHY_SPEED_10T:
            athrs26_reg_write (0x600, 0x7c);
            break;

        default:
            break;
    }
}

/******************************************************************************
*
* athrs26_phy_restart - restart auto-negotiation on the phy
*
* Re-start auto-negotiation on the phy
*/
void athrs26_phy_restart(int phyUnit)
{
    s26_wr_phy( phyUnit, ATHR_PHY_CONTROL, (s26_rd_phy(phyUnit, ATHR_PHY_CONTROL) | 0x200));
}

/******************************************************************************
*
* athrs26_phy_is_link_alive - test to see if the specified link is alive
*
* RETURNS:
*    TRUE  --> link is alive
*    FALSE --> link is down
*/
BOOL
athrs26_phy_is_link_alive(int phyUnit)
{
    uint16_t phyHwStatus;
    uint32_t phyBase;
    uint32_t phyAddr;

    phyBase = ATHR_PHYBASE(phyUnit);
    phyAddr = ATHR_PHYADDR(phyUnit);
    phyHwStatus = s26_rd_phy( phyAddr, ATHR_PHY_SPEC_STATUS);
    if (phyHwStatus & ATHR_STATUS_LINK_PASS)
        return TRUE;

    return FALSE;
}

/******************************************************************************
*
* athrs26_phy_setup - reset and setup the PHY associated with
* the specified MAC unit number.
*
* Resets the associated PHY port.
*
* RETURNS:
*    TRUE  --> associated PHY is alive
*    FALSE --> no LINKs on this ethernet unit
*/

BOOL
athrs26_phy_setup(int ethUnit)
{
    int       phyUnit;
    uint16_t  phyHwStatus;
    uint16_t  timeout;
    int       liveLinks = 0;
    uint32_t  phyBase = 0;
    BOOL      foundPhy = FALSE;
    uint32_t  phyAddr = 0;
    uint32_t  ar7240_revid;


    /* See if there's any configuration data for this enet */
    /* start auto negogiation on each phy */
    for (phyUnit=0; phyUnit < ATHR_PHY_MAX; phyUnit++) {

       foundPhy = TRUE;
       phyBase = ATHR_PHYBASE(phyUnit);
       phyAddr = ATHR_PHYADDR(phyUnit);
#ifdef CONFIG_S26_SWITCH_ONLY_MODE
        if (!is_ar7241()) {
           if (!ATHR_IS_ETHUNIT(phyUnit, ethUnit))
               continue;
        }
#else
        if (!ATHR_IS_ETHUNIT(phyUnit, ethUnit)) {
            continue;
        }
#endif
       s26_wr_phy(phyAddr, ATHR_AUTONEG_ADVERT,ATHR_ADVERTISE_ALL);
       DPRINTF("%s ATHR_AUTONEG_ADVERT %d :%x\n",__func__,phyAddr,
            s26_rd_phy(phyAddr,ATHR_AUTONEG_ADVERT));

#if !defined(V54_BSP)
       ar7240_revid = ar7240_reg_rd(AR7240_REV_ID) & AR7240_REV_ID_MASK;
#else
       ar7240_revid = ar7100_reg_rd(AR7100_REV_ID) & AR7100_REV_ID_MASK;
#endif
       if(ar7240_revid != AR7240_REV_1_0) {
           s26_wr_phy( phyAddr, ATHR_PHY_CONTROL,ATHR_CTRL_AUTONEGOTIATION_ENABLE
                      | ATHR_CTRL_SOFTWARE_RESET);
       }

       DPRINTF("%s ATHR_PHY_CONTROL %d :%x\n",__func__,phyAddr,
           s26_rd_phy(phyAddr,ATHR_PHY_CONTROL));
    }

    if (!foundPhy) {
        return FALSE; /* No PHY's configured for this ethUnit */
    }

    /*
     * After the phy is reset, it takes a little while before
     * it can respond properly.
     */
     if (ethUnit == ENET_UNIT_LAN)
        mdelay(1000);
     else
        mdelay(3000);

    /*
     * Wait up to 3 seconds for ALL associated PHYs to finish
     * autonegotiation.  The only way we get out of here sooner is
     * if ALL PHYs are connected AND finish autonegotiation.
     */
    for (phyUnit=0; (phyUnit < ATHR_PHY_MAX) /*&& (timeout > 0) */; phyUnit++) {
#ifdef CONFIG_S26_SWITCH_ONLY_MODE
        if (!is_ar7241()) {
           if (!ATHR_IS_ETHUNIT(phyUnit, ethUnit))
               continue;
        }
#else
        if (!ATHR_IS_ETHUNIT(phyUnit, ethUnit)) {
            continue;
        }
#endif

        timeout=20;
        for (;;) {
            phyHwStatus =  s26_rd_phy(phyAddr, ATHR_PHY_CONTROL);

            if (ATHR_RESET_DONE(phyHwStatus)) {
                DRV_PRINT(DRV_DEBUG_PHYSETUP,
                          ("Port %d, Neg Success\n", phyUnit));
                break;
            }
            if (timeout == 0) {
                DRV_PRINT(DRV_DEBUG_PHYSETUP,
                          ("Port %d, Negogiation timeout\n", phyUnit));
                break;
            }
            if (--timeout == 0) {
                DRV_PRINT(DRV_DEBUG_PHYSETUP,
                          ("Port %d, Negogiation timeout\n", phyUnit));
                break;
            }

            mdelay(150);
        }

        /* extend the cable length */
        s26_wr_phy(phyUnit, 29, 0x14);
        s26_wr_phy(phyUnit, 30, 0xf52);

        if (is_ar934x()) {
            /* s27 */
            /* Force Class A setting phys */
            s26_wr_phy(phyUnit, 29, 4);
            s26_wr_phy(phyUnit, 30, 0xebbb);
            s26_wr_phy(phyUnit, 29, 5);
            s26_wr_phy(phyUnit, 30, 0x2c47);
        }

#ifdef S26_VER_1_0
        //turn off power saving
       s26_wr_phy(phyUnit, 29, 41);
       s26_wr_phy(phyUnit, 30, 0);
       printk("def_ S26_VER_1_0\n");
#endif
    }

    /*
     * All PHYs have had adequate time to autonegotiate.
     * Now initialize software status.
     *
     * It's possible that some ports may take a bit longer
     * to autonegotiate; but we can't wait forever.  They'll
     * get noticed by mv_phyCheckStatusChange during regular
     * polling activities.
     */
    for (phyUnit=0; phyUnit < ATHR_PHY_MAX; phyUnit++) {
        if (!ATHR_IS_ETHUNIT(phyUnit, ethUnit)) {
            continue;
        }

        if (athrs26_phy_is_link_alive(phyUnit)) {
            liveLinks++;
            ATHR_IS_PHY_ALIVE(phyUnit) = TRUE;
        } else {
            ATHR_IS_PHY_ALIVE(phyUnit) = FALSE;
        }
        DRV_PRINT(DRV_DEBUG_PHYSETUP,
            ("eth%d: Phy Specific Status=%4.4x\n",
            ethUnit,
            s26_rd_phy(ATHR_PHYADDR(phyUnit),ATHR_PHY_SPEC_STATUS)));
    }

    return (liveLinks > 0);
}

/******************************************************************************
*
* athrs26_phy_is_fdx - Determines whether the phy ports associated with the
* specified device are FULL or HALF duplex.
*
* RETURNS:
*    1 --> FULL
*    0 --> HALF
*/
int
athrs26_phy_is_fdx(int ethUnit,int phyUnit)
{
    uint32_t  phyBase;
    uint32_t  phyAddr;
    uint16_t  phyHwStatus;
    int       ii = 200;

    if (athrs26_phy_is_link_alive(phyUnit)) {

         phyBase = ATHR_PHYBASE(phyUnit);
         phyAddr = ATHR_PHYADDR(phyUnit);

#if !defined(V54_BSP)
         do {
                phyHwStatus = s26_rd_phy(ATHR_PHYADDR(phyUnit),ATHR_PHY_SPEC_STATUS);
                mdelay(10);
          } while((!(phyHwStatus & ATHR_STATUS_RESOVLED)) && --ii);
#else
         do {
                phyHwStatus = s26_rd_phy(ATHR_PHYADDR(phyUnit),ATHR_PHY_SPEC_STATUS);
                if (phyHwStatus & ATHR_STATUS_RESOVLED) {
                    break;
                } else {
                    mdelay(5);
                }
         } while(--ii);
#endif

          if (phyHwStatus & ATHER_STATUS_FULL_DUPLEX)
                return TRUE;
    }

    return FALSE;
}

/******************************************************************************
*
* athrs26_phy_stab_wr - Function to Address 100Mbps stability issue, as
* suggested by EBU.
*
* RETURNS: none
*
*/
void athrs26_phy_stab_wr(int phy_id, BOOL phy_up, int phy_speed)
{
    if( phy_up && (phy_speed == AG7240_PHY_SPEED_100TX)) {
        s26_wr_phy(ATHR_PHYADDR(phy_id), ATHR_DEBUG_PORT_ADDRESS, 0x18);
        s26_wr_phy(ATHR_PHYADDR(phy_id), ATHR_DEBUG_PORT_DATA, 0xBA8);
    } else {
        s26_wr_phy(ATHR_PHYADDR(phy_id), ATHR_DEBUG_PORT_ADDRESS, 0x18);
        s26_wr_phy(ATHR_PHYADDR(phy_id), ATHR_DEBUG_PORT_DATA, 0x2EA);
    }
}

/******************************************************************************
*
* athrs26_phy_speed - Determines the speed of phy ports associated with the
* specified device.
*
* RETURNS:
*               AG7240_PHY_SPEED_10T, AG7240_PHY_SPEED_100TX;
*               AG7240_PHY_SPEED_1000T;
*/

int
athrs26_phy_speed(int ethUnit,int phyUnit)
{
    uint16_t  phyHwStatus;
    uint32_t  phyBase;
    uint32_t  phyAddr;
    int       ii = 200;


    if (athrs26_phy_is_link_alive(phyUnit)) {

        phyBase = ATHR_PHYBASE(phyUnit);
        phyAddr = ATHR_PHYADDR(phyUnit);
#if !defined(V54_BSP)
        do {
            phyHwStatus = s26_rd_phy(ATHR_PHYADDR(phyUnit),ATHR_PHY_SPEC_STATUS);
            mdelay(10);
        } while((!(phyHwStatus & ATHR_STATUS_RESOVLED)) && --ii);
#else
        do {
            phyHwStatus = s26_rd_phy(ATHR_PHYADDR(phyUnit),ATHR_PHY_SPEC_STATUS);
            if (phyHwStatus & ATHR_STATUS_RESOVLED) {
                break;
            } else {
                mdelay(5);
            }
        } while(--ii);
#endif

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
athrs26_phy_is_up(int ethUnit)
{
    int           phyUnit;
    uint16_t      phyHwStatus, phyHwControl;
    athrPhyInfo_t *lastStatus;
    int           linkCount   = 0;
    int           lostLinks   = 0;
    int           gainedLinks = 0;
    uint32_t      phyBase;
    uint32_t      phyAddr;

    for (phyUnit=0; phyUnit < ATHR_PHY_MAX; phyUnit++) {
#ifdef CONFIG_S26_SWITCH_ONLY_MODE
        if (!is_ar7241()) {
           if (!ATHR_IS_ETHUNIT(phyUnit, ethUnit))
               continue;
        }
#else
        if (!ATHR_IS_ETHUNIT(phyUnit, ethUnit)) {
            continue;
        }
#endif

        phyBase = ATHR_PHYBASE(phyUnit);
        phyAddr = ATHR_PHYADDR(phyUnit);

        lastStatus = &athrPhyInfo[phyUnit];

        if (lastStatus->isPhyAlive) { /* last known link status was ALIVE */

             phyHwStatus = s26_rd_phy(ATHR_PHYADDR(phyUnit),ATHR_PHY_SPEC_STATUS);

            /* See if we've lost link */
            if (phyHwStatus & ATHR_STATUS_LINK_PASS) { /* check realtime link */
#if !defined(V54_BSP)
                linkCount++;
#else
                linkCount |= (1 << phyUnit);
#endif
            } else {
                phyHwStatus = s26_rd_phy(ATHR_PHYADDR(phyUnit),ATHR_PHY_STATUS);
            /* If realtime failed check link in latch register before
	     * asserting link down.
             */
                if (phyHwStatus & ATHR_LATCH_LINK_PASS)
#if !defined(V54_BSP)
                    linkCount++;
#else
                    linkCount |= (1 << phyUnit);
#endif
			else
                    lostLinks++;
                DRV_PRINT(DRV_DEBUG_PHYCHANGE,("\nenet%d port%d down\n",
                                               ethUnit, phyUnit));
                lastStatus->isPhyAlive = FALSE;
            }
        } else { /* last known link status was DEAD */

            /* Check for reset complete */

                phyHwStatus = s26_rd_phy(ATHR_PHYADDR(phyUnit),ATHR_PHY_STATUS);

            if (!ATHR_RESET_DONE(phyHwStatus))
                continue;

                phyHwControl = s26_rd_phy(ATHR_PHYADDR(phyUnit),ATHR_PHY_CONTROL);

            /* Check for AutoNegotiation complete */

            if ((!(phyHwControl & ATHR_CTRL_AUTONEGOTIATION_ENABLE))
                 || ATHR_AUTONEG_DONE(phyHwStatus)) {
                    phyHwStatus = s26_rd_phy(ATHR_PHYADDR(phyUnit),ATHR_PHY_SPEC_STATUS);

                    if (phyHwStatus & ATHR_STATUS_LINK_PASS) {
                        gainedLinks++;
#if !defined(V54_BSP)
                        linkCount++;
#else
                        linkCount |= (1 << phyUnit);
#endif
                        DRV_PRINT(DRV_DEBUG_PHYCHANGE,("\nenet%d port%d up\n",
                                               ethUnit, phyUnit));
                        lastStatus->isPhyAlive = TRUE;
                   }
            }
        }
    }
    return (linkCount);

}

void athrs26_reg_dev(ag7240_mac_t **mac)
{
    ag7240_macs[0] = mac[0];
    ag7240_macs[0]->mac_speed = 0xff;
    ag7240_macs[1] = mac[1];
    ag7240_macs[1]->mac_speed = 0xff;
    return;

}

uint32_t
athrs26_reg_read(unsigned int s26_addr)
{
    unsigned int addr_temp;
    unsigned int s26_rd_csr_low, s26_rd_csr_high, s26_rd_csr;
    unsigned int data,unit = 0;
    unsigned int phy_address, reg_address;

    addr_temp = (s26_addr & 0xfffffffc) >>2;
    data = addr_temp >> 7;

    phy_address = 0x1f;
    reg_address = 0x10;

    if (is_ar7240()) {
       unit = 0;
    }
    else if(is_ar7241() || is_ar7242() || is_ar934x()) {
	unit = 1;
    }

    phy_reg_write(unit,phy_address, reg_address, data);

    phy_address = (0x17 & ((addr_temp >> 4) | 0x10));
    reg_address = ((addr_temp << 1) & 0x1e);
    s26_rd_csr_low = (uint32_t) phy_reg_read(unit,phy_address, reg_address);

    reg_address = reg_address | 0x1;
    s26_rd_csr_high = (uint32_t) phy_reg_read(unit,phy_address, reg_address);
    s26_rd_csr = (s26_rd_csr_high << 16) | s26_rd_csr_low ;

    return(s26_rd_csr);
}

void
athrs26_reg_write(unsigned int s26_addr, unsigned int s26_write_data)
{
    unsigned int addr_temp;
    unsigned int data;
    unsigned int phy_address, reg_address,unit = 0;


    addr_temp = (s26_addr &  0xfffffffc) >>2;
    data = addr_temp >> 7;

    phy_address = 0x1f;
    reg_address = 0x10;

    if (is_ar7240()) {
       unit = 0;
    }
    else if(is_ar7241() || is_ar7242() || is_ar934x()) {
	unit = 1;
    }

    phy_reg_write(unit,phy_address, reg_address, data);

    phy_address = (0x17 & ((addr_temp >> 4) | 0x10));

    reg_address = (((addr_temp << 1) & 0x1e) | 0x1);
    data = s26_write_data >> 16;
    phy_reg_write(unit,phy_address, reg_address, data);

    reg_address = ((addr_temp << 1) & 0x1e);
    data = s26_write_data  & 0xffff;
    phy_reg_write(unit,phy_address, reg_address, data);
}


unsigned int s26_rd_phy(unsigned int phy_addr, unsigned int reg_addr)
{

     unsigned int rddata;

     if(phy_addr >= ATHR_PHY_MAX) {
         DPRINTF("%s:Error invalid Phy Address:0x%x\n",__func__,phy_addr);
	 return -1 ;
     }

    // MDIO_CMD is set for read

    rddata = athrs26_reg_read(0x98);
    rddata = (rddata & 0x0) | (reg_addr<<16) | (phy_addr<<21) | (1<<27) | (1<<30) | (1<<31) ;
    athrs26_reg_write(0x98, rddata);

    rddata = athrs26_reg_read(0x98);
    rddata = rddata & (1<<31);

    // Check MDIO_BUSY status
    while(rddata){
        rddata = athrs26_reg_read(0x98);
        rddata = rddata & (1<<31);
    }

    // Read the data from phy

    rddata = athrs26_reg_read(0x98) & 0xffff;

    return(rddata);
}

void s26_wr_phy(unsigned int phy_addr, unsigned int reg_addr, unsigned int write_data)
{
    unsigned int rddata;

    if(phy_addr >= ATHR_PHY_MAX) {
        DPRINTF("%s:Error invalid Phy Address:0x%x\n",__func__,phy_addr);
        return ;
    }

    // MDIO_CMD is set for read

    rddata = athrs26_reg_read(0x98);
    rddata = (rddata & 0x0) | (write_data & 0xffff) | (reg_addr<<16) | (phy_addr<<21) | (0<<27) | (1<<30) | (1<<31) ;
    athrs26_reg_write(0x98, rddata);

    rddata = athrs26_reg_read(0x98);
    rddata = rddata & (1<<31);

       // Check MDIO_BUSY status
    while(rddata){
        rddata = athrs26_reg_read(0x98);
        rddata = rddata & (1<<31);
    }

}

// S27
void athrs27_reg_rmw(unsigned int s27_addr, unsigned int s27_write_data)
{
    int val = athrs26_reg_read(s27_addr);
    athrs26_reg_write(s27_addr,(val | s27_write_data));
}

int athrs26_ioctl(uint32_t *args, int cmd)
{
    struct eth_diag *etd =(struct eth_diag *) args;
#if defined(V54_BSP)
    int remap[5] = {3, 2, 1, 0, 4};
    if (is_ar934x()) {
        remap[0] = 4;
        remap[1] = 0;
    }
#else
    uint32_t ar7240_revid;
#endif

    if ( (cmd > SIOCDEVPRIVATE) && (cmd <= ATHR_FORCE_PHY) )
        printk("%s: cmd 0x%x\n", __func__, cmd);
    if(cmd  == ATHR_RD_PHY) {
        if(etd->ed_u.portnum != 0xf)
            etd->val = s26_rd_phy(remap[etd->ed_u.portnum],etd->phy_reg);
        else
            etd->val = athrs26_reg_read(etd->phy_reg);
    }
    else if(cmd  == ATHR_WR_PHY) {
        if(etd->ed_u.portnum != 0xf)
            s26_wr_phy(remap[etd->ed_u.portnum],etd->phy_reg,etd->val);
        else
            athrs26_reg_write(etd->phy_reg,etd->val);
    }
    else if(cmd == ATHR_FORCE_PHY) {
        if(etd->phy_reg < ATHR_PHY_MAX) {
            if(etd->val == 10) {
	           printk("Forcing 10Mbps %s on port:%d \n",
                         dup_str[etd->ed_u.duplex],(etd->phy_reg));
               athrs26_force_10M(remap[etd->phy_reg],etd->ed_u.duplex);
            }
            else if(etd->val == 100) {
	           printk("Forcing 100Mbps %s on port:%d \n",
                         dup_str[etd->ed_u.duplex],(etd->phy_reg));
               athrs26_force_100M(remap[etd->phy_reg],etd->ed_u.duplex);
            }
	        else if((etd->val == 0) || (etd->val == 1)) {
	           printk("Enabling Auto Neg on port:%d \n",(etd->phy_reg));

#if 0
#if !defined(V54_BSP)
               ar7240_revid = ar7240_reg_rd(AR7240_REV_ID) & AR7240_REV_ID_MASK;
#else
               ar7240_revid = ar7100_reg_rd(AR7100_REV_ID) & AR7100_REV_ID_MASK;
#endif

               if(ar7240_revid == AR7240_REV_1_0) {
                   s26_wr_phy(remap[etd->phy_reg],ATHR_DEBUG_PORT_ADDRESS,0x0);
                   s26_wr_phy(remap[etd->phy_reg],ATHR_DEBUG_PORT_DATA,0x2ee);
                   s26_wr_phy(remap[etd->phy_reg],ATHR_DEBUG_PORT_ADDRESS,0x3);
                   s26_wr_phy(remap[etd->phy_reg],ATHR_DEBUG_PORT_DATA,0x3a11);
               }
               s26_wr_phy(remap[etd->phy_reg],ATHR_PHY_CONTROL,0x9000);
#else
               athrs26_force_auto(remap[etd->phy_reg],etd->ed_u.duplex);
#endif
            }
            else
               return -EINVAL;
            if(ATHR_ETHUNIT(remap[etd->phy_reg]) == ENET_UNIT_WAN) {
#ifdef CONFIG_S26_SWITCH_ONLY_MODE
                if (is_ar7241())
                    ag7240_check_link(ag7240_macs[1],etd->phy_reg);
                else
#endif
#if !defined(V54_BSP)
                ag7240_check_link(ag7240_macs[0],etd->phy_reg);
#else
                ag7100_check_link(ag7240_macs[0],remap[etd->phy_reg]);
#endif
            }
            else {
#if !defined(V54_BSP)
                ag7240_check_link(ag7240_macs[1],etd->phy_reg);
#else
                ag7100_check_link(ag7240_macs[1],remap[etd->phy_reg]);
#endif
            }
        }
        else {
            return -EINVAL;
        }
    }
    else
        return -EINVAL;

    return 0;
}

int athrs26_mdc_check()
{
    int i;

    for (i=0; i<4000; i++) {
        if(athrs26_reg_read(0x10c) != 0x18007fff)
            return -1;
    }
    return 0;
}

void ar7240_s26_intr(void)
{
    int status = 0, intr_reg_val;
    uint32_t phyUnit = 0 ,phyBase = 0 ,phyAddr = 0;
    uint32_t phymask = 0x0;
    uint32_t linkDown = 0x0;

    athrs26_reg_write(S26_GLOBAL_INTR_MASK_REG,0x0);

    intr_reg_val = athrs26_reg_read(S26_GLOBAL_INTR_REG);

    /* clear global link interrupt */
    athrs26_reg_write(S26_GLOBAL_INTR_REG,intr_reg_val);

    if (intr_reg_val & PHY_LINK_CHANGE_REG) {

       for (phyUnit=0; phyUnit < ATHR_PHY_MAX; phyUnit++) {

           phyBase = ATHR_PHYBASE(phyUnit);
           phyAddr = ATHR_PHYADDR(phyUnit);
           status = s26_rd_phy(phyAddr,ATHR_PHY_INTR_STATUS);

           if(status & PHY_LINK_UP) {
               DPRINTF("LINK UP - Port %d:%x\n",phyAddr,status);
               phymask = (phymask | (1 << phyUnit));
           }
           if(status & PHY_LINK_DOWN) {
               DPRINTF("LINK DOWN - Port %d:%x\n",phyAddr,status);
               phymask = (phymask | (1 << phyUnit));
               linkDown = (linkDown | (1 << phyUnit));
           }
           if(status & PHY_LINK_DUPLEX_CHANGE) {
               DPRINTF("LINK DUPLEX CHANGE - Port %d:%x\n",phyAddr,status);
               phymask = (phymask | (1 << phyUnit));
           }
           if(status & PHY_LINK_SPEED_CHANGE) {
               DPRINTF("LINK SPEED CHANGE %d:%x\n",phyAddr,status);
               phymask = (phymask | (1 << phyUnit));
           }
       }
       for (phyUnit=0; phyUnit < ATHR_PHY_MAX; phyUnit++) {
           if ((phymask >> phyUnit) & 0x1) {

               phyBase = ATHR_PHYBASE(phyUnit);
               phyAddr = ATHR_PHYADDR(phyUnit);

               status = s26_rd_phy(phyAddr,ATHR_PHY_INTR_STATUS);

               if (!athrs26_phy_is_link_alive(phyUnit) && !((linkDown >> phyUnit) & 0x1))
                        continue;

               if(ATHR_ETHUNIT(phyUnit) == ENET_UNIT_WAN) {
#ifdef CONFIG_S26_SWITCH_ONLY_MODE
                    if (is_ar7241())
                        ag7240_check_link(ag7240_macs[1],phyUnit);
                    else
#endif
#if !defined(V54_BSP)
                    ag7240_check_link(ag7240_macs[0],phyUnit);
#else
                    ag7100_check_link(ag7240_macs[0],phyUnit);
#endif
               }
               else {
#if !defined(V54_BSP)
                    ag7240_check_link(ag7240_macs[1],phyUnit);
#else
                    ag7100_check_link(ag7240_macs[1],phyUnit);
#endif
               }
           }
       }
       athrs26_reg_write(S26_GLOBAL_INTR_MASK_REG,PHY_LINK_CHANGE_REG);
   }
   else  {
      DPRINTF("Spurious link interrupt:%s,status:%x\n",__func__,status);
      athrs26_reg_write(S26_GLOBAL_INTR_MASK_REG,PHY_LINK_CHANGE_REG);
   }
}

void ar7240_s26_enable_header()
{
        athrs26_reg_write(PORT_CONTROL_REGISTER0, 0x4804);
}
