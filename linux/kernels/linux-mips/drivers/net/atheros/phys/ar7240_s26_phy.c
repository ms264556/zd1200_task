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
#include "athrs_ioctl.h"
#include "athrs_phy.h"
#include "athrs_mac.h"
#include "ar7240_s26_phy.h"

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

#ifdef CONFIG_ATHR_VLAN_IGMP
#define PYTHON_OK   0
#define PYTHON_ERR  1
#define PYTHON_FULL 2
#endif

#define DRV_LOG(DBG_SW, X0, X1, X2, X3, X4, X5, X6)
#define DRV_MSG(x,a,b,c,d,e,f)
#define DRV_PRINT(DBG_SW,X)

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

#ifdef S26_PHY_DEBUG
#define DPRINTF(_fmt,...) do {         \
                printk(MODULE_NAME":"_fmt, __VA_ARGS__);      \
} while (0)
#else
#define DPRINTF(_fmt,...)
#endif

extern int phy_in_reset;
extern int athr_gmac_check_link(athr_gmac_t *mac,int phyUnit);
extern int athr_gmac_get_link_st(athr_gmac_t *mac, int *link, int *fdx,
		athr_phy_speed_t *speed,int phyUnit);
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

static athr_gmac_t *athr_macs[2];
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
    s26_wr_phy(0,phy_addr,ATHR_DEBUG_PORT_ADDRESS,0x29);
    s26_wr_phy(0,phy_addr,ATHR_DEBUG_PORT_DATA,0x36c0);

}

void athrs26_sleep_off(int phy_addr)
{
    s26_wr_phy(0,phy_addr,ATHR_DEBUG_PORT_ADDRESS,0xb);
    s26_wr_phy(0,phy_addr,ATHR_DEBUG_PORT_DATA,0x3c00);
}

void athrs26_enable_link_intrs(void *arg)
{
     athr_gmac_t *mac = (athr_gmac_t *)arg;
     int phyUnit = 0, phyAddr = 0;

    /* Enable global PHY link status interrupt */
    athrs26_reg_write(S26_GLOBAL_INTR_MASK_REG,PHY_LINK_CHANGE_REG);

    if (mac_has_flag(mac,ETH_SWONLY_MODE)) {
        if (mac->mac_unit == 1) {
            for (phyUnit = 0; phyUnit < ATHR_PHY_MAX; phyUnit++) {
                phyAddr = ATHR_PHYADDR(phyUnit);
                s26_wr_phy(0,phyAddr,ATHR_PHY_INTR_ENABLE,PHY_LINK_INTRS);
            }
        }
        return ;
    }

    if (mac->mac_unit == ENET_UNIT_WAN) {
        s26_wr_phy(0,ATHR_PHY4_ADDR,ATHR_PHY_INTR_ENABLE,PHY_LINK_INTRS);
    }
    else {
        for (phyUnit = 0; phyUnit < ATHR_PHY_MAX - 1; phyUnit++) {
            phyAddr = ATHR_PHYADDR(phyUnit);
            s26_wr_phy(0,phyAddr,ATHR_PHY_INTR_ENABLE,PHY_LINK_INTRS);
        }
    }
}

void athrs26_disable_link_intrs(void *arg)
{
     athr_gmac_t *mac = (athr_gmac_t *)arg;
     int phyUnit = 0, phyAddr = 0;

     if (mac_has_flag(mac,ETH_SWONLY_MODE)) {
         if (mac->mac_unit == 1) {
             for (phyUnit = 0; phyUnit < ATHR_PHY_MAX; phyUnit++) {
                 phyAddr = ATHR_PHYADDR(phyUnit);
                 s26_wr_phy(0,phyAddr,ATHR_PHY_INTR_ENABLE,0x0);
             }
         }
         return ;
     }

     if (mac->mac_unit == ENET_UNIT_WAN) {
         s26_wr_phy(0,ATHR_PHY4_ADDR,ATHR_PHY_INTR_ENABLE,0x0);
     }
     else {
         for (phyUnit = 0; phyUnit < ATHR_PHY_MAX - 1; phyUnit++) {
             phyAddr = ATHR_PHYADDR(phyUnit);
             s26_wr_phy(0,phyAddr,ATHR_PHY_INTR_ENABLE,0x0);
         }
     }
}
#if 0
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
   /*
    *  Force MDI and MDX to alternate ports
    *  Phy 0,2 and 4 -- MDI
    *  Phy 1 and 3 -- MDX
    */
    if(phyAddr%2) {
        s26_wr_phy(0,phyAddr,ATHR_PHY_FUNC_CONTROL,0x820);
    }
    else {
        s26_wr_phy(0,phyAddr,ATHR_PHY_FUNC_CONTROL,0x800);
    }

    ar7240_revid = ar7240_reg_rd(AR7240_REV_ID) & AR7240_REV_ID_MASK;

    if(ar7240_revid == AR7240_REV_1_0) {
        s26_wr_phy(0,phyAddr,ATHR_DEBUG_PORT_ADDRESS,0x0);
        s26_wr_phy(0,phyAddr,ATHR_DEBUG_PORT_DATA,0x2ee);
        s26_wr_phy(0,phyAddr,ATHR_DEBUG_PORT_ADDRESS,0x3);
        s26_wr_phy(0,phyAddr,ATHR_DEBUG_PORT_DATA,0x3a11);
    }
    s26_wr_phy(0,phyAddr,ATHR_PHY_CONTROL,(0xa000|(duplex << 8)));
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

    ar7240_revid = ar7240_reg_rd(AR7240_REV_ID) & AR7240_REV_ID_MASK;

    if(ar7240_revid == AR7240_REV_1_0) {
        s26_wr_phy(0,phyAddr,ATHR_PHY_FUNC_CONTROL,0x800);
        s26_wr_phy(0,phyAddr,ATHR_PHY_CONTROL,0x8100);
        s26_wr_phy(0,phyAddr,ATHR_DEBUG_PORT_ADDRESS,0x0);
        s26_wr_phy(0,phyAddr,ATHR_DEBUG_PORT_DATA,0x12ee);
        s26_wr_phy(0,phyAddr,ATHR_DEBUG_PORT_ADDRESS,0x3);
        s26_wr_phy(0,phyAddr,ATHR_DEBUG_PORT_DATA,0x3af0);
    }
    s26_wr_phy(0,phyAddr,ATHR_PHY_CONTROL,(0x8000 |(duplex << 8)));
    phy_in_reset = 0;
}
#endif
int athrs26_reg_init(void *arg)
{
    uint32_t ar7240_revid;
    uint32_t rd_data;
    athr_gmac_t *mac = (athr_gmac_t *)arg;

    /* if using header for register configuration, we have to     */
    /* configure s26 register after frame transmission is enabled */
    if (athr26_init_flag)
        return 0;

    ar7240_revid = ar7240_reg_rd(AR7240_REV_ID) & AR7240_REV_ID_MASK;

    if(ar7240_revid == AR7240_REV_1_0) {
#ifdef S26_FORCE_100M
        athrs26_force_100M(ATHR_PHY4_ADDR,1);
#endif
#ifdef S26_FORCE_10M
        athrs26_force_10M(ATHR_PHY4_ADDR,1);
#endif
    } else {
        s26_wr_phy(0,ATHR_PHY4_ADDR,ATHR_PHY_CONTROL,0x9000);

        /* Class A setting for 100M */
        s26_wr_phy(0,ATHR_PHY4_ADDR, 29, 5);
        s26_wr_phy(0,ATHR_PHY4_ADDR, 30, (s26_rd_phy(0,ATHR_PHY4_ADDR, 30)&((~2)&0xffff)));
    }

    /* Enable flow control on CPU port */
    athrs26_reg_write(PORT_STATUS_REGISTER0,
                      (athrs26_reg_read(PORT_STATUS_REGISTER0) | 0x30));

    /* Disable WAN mac inside S26 */
    athrs26_reg_write(PORT_STATUS_REGISTER5,0x0);

    /* Enable WAN mac inside S26 */
    if (mac_has_flag(mac,ETH_SWONLY_MODE) || is_ar933x() )
        athrs26_reg_write(PORT_STATUS_REGISTER5,0x200);

    /* Enable MDIO Access if PHY is Powered-down */
    s26_wr_phy(0,ATHR_PHY4_ADDR,ATHR_DEBUG_PORT_ADDRESS,0x3);
    rd_data = s26_rd_phy(0,ATHR_PHY4_ADDR,ATHR_DEBUG_PORT_DATA);
    s26_wr_phy(0,ATHR_PHY4_ADDR,ATHR_DEBUG_PORT_ADDRESS,0x3);
    s26_wr_phy(0,ATHR_PHY4_ADDR,ATHR_DEBUG_PORT_DATA,(rd_data & 0xfffffeff) );

    if (mac_has_flag(mac,ATHR_S26_HEADER))
        athrs26_reg_write(PORT_CONTROL_REGISTER0, 0x4804);
    else
        athrs26_reg_write(PORT_CONTROL_REGISTER0, 0x4004);

    athr26_init_flag = 1;

    return 0;
}

int athrs26_reg_init_lan(void *arg)
{
    int i = 60;
    int       phyUnit;
    uint32_t  phyBase = 0;
    BOOL      foundPhy = FALSE;
    uint32_t  phyAddr = 0;
    uint32_t  queue_ctrl_reg = 0;
    uint32_t  ar7240_revid;
    uint32_t rd_data;
    athr_gmac_t *mac = (athr_gmac_t *)arg;

    /* if using header for register configuration, we have to     */
    /* configure s26 register after frame transmission is enabled */
    if (athr26_init_flag1)
        return 0;

    /* reset switch */
    printk(MODULE_NAME ": resetting s26\n");
    athrs26_reg_write(0x0, athrs26_reg_read(0x0)|0x80000000);

    while(i--) {
        mdelay(100);
        if(!(athrs26_reg_read(0x0)&0x80000000))
            break;
    }
    printk(MODULE_NAME ": s26 reset done\n");

    /* 100M Half Duplex Throughput setting */
    athrs26_reg_write(0xe4,0x079c1040);

    /* Change HOL settings
     * 25: PORT_QUEUE_CTRL_ENABLE.
     * 24: PRI_QUEUE_CTRL_EN.
     */
    athrs26_reg_write(0x118,0x0032b5555);

    for (phyUnit=0; phyUnit < ATHR_PHY_MAX; phyUnit++) {

        if ((ATHR_ETHUNIT(phyUnit) == ENET_UNIT_WAN) &&
            !mac_has_flag(mac,ETH_SWONLY_MODE))
            continue;

        foundPhy = TRUE;
        phyBase = ATHR_PHYBASE(phyUnit);
        phyAddr = ATHR_PHYADDR(phyUnit);

        ar7240_revid = ar7240_reg_rd(AR7240_REV_ID) & AR7240_REV_ID_MASK;

        if(ar7240_revid == AR7240_REV_1_0) {
#ifdef S26_FORCE_100M
            athrs26_force_100M(phyAddr,1);
#endif
#ifdef S26_FORCE_10M
            athrs26_force_10M(phyAddr,1);
#endif
        }
        else {
            s26_wr_phy(0,phyAddr,ATHR_PHY_CONTROL,0x9000);

            /* Class A setting for 100M */
            s26_wr_phy(0,phyAddr, 29, 5);
            s26_wr_phy(0,phyAddr, 30, (s26_rd_phy(0,phyAddr, 30)&((~2)&0xffff)));
        }

        /* Enable MDIO Access if PHY is Powered-down */
        s26_wr_phy(0,phyAddr,ATHR_DEBUG_PORT_ADDRESS,0x3);
        rd_data = s26_rd_phy(0,phyAddr,ATHR_DEBUG_PORT_DATA);
        s26_wr_phy(0,phyAddr,ATHR_DEBUG_PORT_ADDRESS,0x3);
        s26_wr_phy(0,phyAddr,ATHR_DEBUG_PORT_DATA,(rd_data & 0xfffffeff) );

        /* Change HOL settings
         * 25: PORT_QUEUE_CTRL_ENABLE.
         * 24: PRI_QUEUE_CTRL_EN.
         */
        queue_ctrl_reg = (0x100 * phyUnit) + 0x218 ;
        athrs26_reg_write(queue_ctrl_reg,0x032b5555);

        DPRINTF("S26 ATHR_PHY_FUNC_CONTROL (%d):%x\n",phyAddr,
            s26_rd_phy(0,phyAddr,ATHR_PHY_FUNC_CONTROL));
        DPRINTF("S26 PHY ID  (%d) :%x\n",phyAddr,
            s26_rd_phy(0,phyAddr,ATHR_PHY_ID1));
        DPRINTF("S26 PHY CTRL  (%d) :%x\n",phyAddr,
            s26_rd_phy(0,phyAddr,ATHR_PHY_CONTROL));
        DPRINTF("S26 ATHR PHY STATUS  (%d) :%x\n",phyAddr,
            s26_rd_phy(0,phyAddr,ATHR_PHY_STATUS));
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
     * status[9]=1'h1;     - Link En - enable mac auto-neg with phy
     * status[12]=1'h1;    - Phy Link En - enable flow control config auto-neg with phy
     */
    athrs26_reg_write(PORT_STATUS_REGISTER1, 0x1200);  /* LAN - 1 */
    athrs26_reg_write(PORT_STATUS_REGISTER2, 0x1200);  /* LAN - 2 */
    athrs26_reg_write(PORT_STATUS_REGISTER3, 0x1200);  /* LAN - 3 */
    athrs26_reg_write(PORT_STATUS_REGISTER4, 0x1200);  /* LAN - 4 */

    /* QM Control */
    athrs26_reg_write(0x38, 0xc000050e);

    /*
     * status[11]=1'h0;    - CPU Disable
     * status[7] = 1'b1;   - Learn One Lock
     * status[14] = 1'b0;  - Learn Enable
     */
    if (mac_has_flag(mac,ATHR_S26_HEADER))
        athrs26_reg_write(PORT_CONTROL_REGISTER0, 0x4804);
    else
        athrs26_reg_write(PORT_CONTROL_REGISTER0, 0x4004);

    /* Tag Priority Mapping */
    athrs26_reg_write(0x70, 0xfa50);

    /* Enable ARP packets to CPU port */
    athrs26_reg_write(ARL_TBL_CTRL_REG,(athrs26_reg_read(ARL_TBL_CTRL_REG) | 0x100000));

    /* Enable Broadcast packets to CPU port */
    athrs26_reg_write(FLD_MASK_REG,(athrs26_reg_read(FLD_MASK_REG) | S26_ENABLE_CPU_BROADCAST ));

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

    /* Disable WAN mac inside S26 */
    athrs26_reg_write(PORT_STATUS_REGISTER5,0x0);
#ifdef CONFIG_ATHR_VLAN_IGMP
    /* Set Max MTU to 1518+6 for vlan and header space. */
    athrs26_reg_write(0x30,(athrs26_reg_read(0x30)&0xfffff800)|0x5f4);
#endif
    if(mac_has_flag(mac,ATHR_S26_HEADER))
        /* Set CPU port0 to Atheros Header Enable. */
        athrs26_reg_write(0x104,athrs26_reg_read(0x104)|(0x1<<11));
    /* Enable WAN mac inside S26 */
    if (mac_has_flag(mac,ETH_SWONLY_MODE) || is_ar933x())
        athrs26_reg_write(PORT_STATUS_REGISTER5,0x200);

    athr26_init_flag1 = 1;

    return 0;
}
//static unsigned int phy_val_saved = 0;
/******************************************************************************
*
* athrs26_phy_off - power off the phy to change its speed
*
* Power off the phy
*/
#if 0
void athrs26_phy_off(athr_mac_t *mac)
{
    struct net_device  *dev = mac->mac_dev;

    if (mac->mac_unit == ENET_UNIT_LAN)
        return;

    netif_carrier_off(dev);
    netif_stop_queue(dev);

    phy_val_saved = s26_rd_phy(0,ATHR_PHY4_ADDR,ATHR_PHY_CONTROL);
    s26_wr_phy(0, ATHR_PHY4_ADDR, ATHR_PHY_CONTROL, phy_val_saved | 0x800);
}

/******************************************************************************
*
* athrs26_phy_on - power on the phy after speed changed
*
* Power on the phy
*/
void athrs26_phy_on(athr_mac_t *mac)
{
    if ((mac->mac_unit == ENET_UNIT_LAN) || (phy_val_saved == 0))
        return;

    s26_wr_phy(0, ATHR_PHY4_ADDR, ATHR_PHY_CONTROL, phy_val_saved & 0xf7ff);

    mdelay(2000);
}

/******************************************************************************
*
* athrs26_mac_speed_set - set mac in s26 speed mode (actually RMII mode)
*
* Set mac speed mode
*/
void athrs26_mac_speed_set(athr_mac_t *mac, athr_phy_speed_t speed)
{
    if ((mac->mac_unit == ENET_UNIT_LAN))
        return;

    switch (speed) {
        case ATHR_PHY_SPEED_100T:
            athrs26_reg_write (0x600, 0x7d);
            break;

        case ATHR_PHY_SPEED_10T:
            athrs26_reg_write (0x600, 0x7c);
            break;

        default:
            break;
    }
}
#endif
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
    phyHwStatus = s26_rd_phy(0, phyAddr, ATHR_PHY_SPEC_STATUS);
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

int
athrs26_phy_setup(void *arg)
{
    int       phyUnit;
    uint16_t  phyHwStatus;
    uint16_t  timeout;
    int       liveLinks = 0;
    uint32_t  phyBase = 0;
    BOOL      foundPhy = FALSE;
    uint32_t  phyAddr = 0;
    uint32_t  ar7240_revid;
    athr_gmac_t *mac = (athr_gmac_t *)arg;
    int ethUnit = mac->mac_unit;


    /* See if there's any configuration data for this enet */
    /* start auto negogiation on each phy */
    for (phyUnit=0; phyUnit < ATHR_PHY_MAX; phyUnit++) {

        foundPhy = TRUE;
        phyBase = ATHR_PHYBASE(phyUnit);
        phyAddr = ATHR_PHYADDR(phyUnit);

        if (!ATHR_IS_ETHUNIT(phyUnit, ethUnit)) {
            continue;
        }

        s26_wr_phy(0,phyAddr, ATHR_AUTONEG_ADVERT,ATHR_ADVERTISE_ALL);
        DPRINTF("%s ATHR_AUTONEG_ADVERT %d :%x\n",__func__,phyAddr,
            s26_rd_phy(0,phyAddr,ATHR_AUTONEG_ADVERT));

        ar7240_revid = ar7240_reg_rd(AR7240_REV_ID) & AR7240_REV_ID_MASK;
        if(ar7240_revid != AR7240_REV_1_0) {
            s26_wr_phy(0, phyAddr, ATHR_PHY_CONTROL,ATHR_CTRL_AUTONEGOTIATION_ENABLE
                      | ATHR_CTRL_SOFTWARE_RESET);
        }

        DPRINTF("%s ATHR_PHY_CONTROL %d :%x\n",__func__,phyAddr,
           s26_rd_phy(0,phyAddr,ATHR_PHY_CONTROL));
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

        if ((ATHR_ETHUNIT(phyUnit) == ENET_UNIT_WAN) &&
                !mac_has_flag(mac,ETH_SWONLY_MODE))
            continue;

        timeout=20;
        for (;;) {
            phyHwStatus =  s26_rd_phy(0,phyAddr, ATHR_PHY_CONTROL);

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
        s26_wr_phy(0,phyUnit, 29, 0x14);
        s26_wr_phy(0,phyUnit, 30, 0xf52);

#ifdef S26_VER_1_0
        /* turn off power saving */
        s26_wr_phy(0,phyUnit, 29, 41);
        s26_wr_phy(0,phyUnit, 30, 0);
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
            s26_rd_phy(0,ATHR_PHYADDR(phyUnit),ATHR_PHY_SPEC_STATUS)));
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

        do {
            phyHwStatus = s26_rd_phy(0,ATHR_PHYADDR(phyUnit),ATHR_PHY_SPEC_STATUS);
            mdelay(10);
        } while((!(phyHwStatus & ATHR_STATUS_RESOVLED)) && --ii);

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
    if( phy_up && (phy_speed == ATHR_PHY_SPEED_100T)) {
        s26_wr_phy(0,ATHR_PHYADDR(phy_id), ATHR_DEBUG_PORT_ADDRESS, 0x18);
        s26_wr_phy(0,ATHR_PHYADDR(phy_id), ATHR_DEBUG_PORT_DATA, 0xBA8);
#ifdef CONFIG_MACH_HORNET
        /* For IEEE 100M voltage test */
        s26_wr_phy(0,ATHR_PHYADDR(phy_id), ATHR_DEBUG_PORT_ADDRESS, 0x4);
        s26_wr_phy(0,ATHR_PHYADDR(phy_id), ATHR_DEBUG_PORT_DATA, 0xebbb);
        s26_wr_phy(0,ATHR_PHYADDR(phy_id), ATHR_DEBUG_PORT_ADDRESS, 0x5);
        s26_wr_phy(0,ATHR_PHYADDR(phy_id), ATHR_DEBUG_PORT_DATA, 0x2c47);
#endif /* CONFIG_MACH_HORNET */
    } else {
        s26_wr_phy(0,ATHR_PHYADDR(phy_id), ATHR_DEBUG_PORT_ADDRESS, 0x18);
        s26_wr_phy(0,ATHR_PHYADDR(phy_id), ATHR_DEBUG_PORT_DATA, 0x2EA);
    }
}

/******************************************************************************
*
* athrs26_phy_speed - Determines the speed of phy ports associated with the
* specified device.
*
* RETURNS:
*               ATHR_PHY_SPEED_10T, AG7240_PHY_SPEED_100T;
*               ATHR_PHY_SPEED_1000T;
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
        do {
            phyHwStatus = s26_rd_phy(0,ATHR_PHYADDR(phyUnit),ATHR_PHY_SPEC_STATUS);
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

        if(mac_has_flag(athr_macs[ENET_UNIT_LAN],ETH_SWONLY_MODE))
            ethUnit = ENET_UNIT_LAN;

        if (!ATHR_IS_ETHUNIT(phyUnit,ethUnit))
            continue;

        phyBase = ATHR_PHYBASE(phyUnit);
        phyAddr = ATHR_PHYADDR(phyUnit);

        lastStatus = &athrPhyInfo[phyUnit];

        if (lastStatus->isPhyAlive) { /* last known link status was ALIVE */

            phyHwStatus = s26_rd_phy(0,ATHR_PHYADDR(phyUnit),ATHR_PHY_SPEC_STATUS);

            /* See if we've lost link */
            if (phyHwStatus & ATHR_STATUS_LINK_PASS) { /* check realtime link */
                linkCount++;
            } else {
                phyHwStatus = s26_rd_phy(0,ATHR_PHYADDR(phyUnit),ATHR_PHY_STATUS);
            /* If realtime failed check link in latch register before
             * asserting link down.
             */
                if (phyHwStatus & ATHR_LATCH_LINK_PASS)
                    linkCount++;
                else
                    lostLinks++;
                DRV_PRINT(DRV_DEBUG_PHYCHANGE,("\nenet%d port%d down\n",
                                               ethUnit, phyUnit));
                lastStatus->isPhyAlive = FALSE;
            }
        } else { /* last known link status was DEAD */

            /* Check for reset complete */

            phyHwStatus = s26_rd_phy(0,ATHR_PHYADDR(phyUnit),ATHR_PHY_STATUS);

            if (!ATHR_RESET_DONE(phyHwStatus))
                continue;

                phyHwControl = s26_rd_phy(0,ATHR_PHYADDR(phyUnit),ATHR_PHY_CONTROL);

            /* Check for AutoNegotiation complete */

            if ((!(phyHwControl & ATHR_CTRL_AUTONEGOTIATION_ENABLE))
                 || ATHR_AUTONEG_DONE(phyHwStatus)) {
                phyHwStatus = s26_rd_phy(0,ATHR_PHYADDR(phyUnit),ATHR_PHY_SPEC_STATUS);

                if (phyHwStatus & ATHR_STATUS_LINK_PASS) {
                    gainedLinks++;
                    linkCount++;
                    DRV_PRINT(DRV_DEBUG_PHYCHANGE,("\nenet%d port%d up\n",
                                               ethUnit, phyUnit));
                    lastStatus->isPhyAlive = TRUE;
                }
            }
        }
    }
    return (linkCount);

}

unsigned int athrs26_reg_read(unsigned int s26_addr)
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
    else if(is_ar7241() || is_ar7242() || is_ar933x()) {
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

void athrs26_reg_write(unsigned int s26_addr, unsigned int s26_write_data)
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
    else if(is_ar7241() || is_ar7242() || is_ar933x()) {
        unit = 1;
    }

    if(is_ar933x())
    {
        //The writing sequence need special care for register 0x40,0x50,0x98, because the busy bit design
        //0x98: L->H (write low address first, then high address), 0x40: H->L, 0x50: H->L
        //Other registers: any sequence is working.
        if(s26_addr!=0x98)
        {
            phy_reg_write(unit, phy_address, reg_address, data);

            phy_address = 0x17 & ((addr_temp >> 4) | 0x10);
            reg_address = ((addr_temp << 1) & 0x1e) | 0x1;
            data =  s26_write_data >> 16;
            phy_reg_write(unit, phy_address, reg_address, data);

            reg_address = reg_address & 0x1e;
            data = s26_write_data  & 0xffff;
            phy_reg_write(unit, phy_address, reg_address, data);
        }
        else
        {
            phy_reg_write(unit, phy_address, reg_address, data);

            phy_address = (0x17 & ((addr_temp >> 4) | 0x10));
            reg_address = ((addr_temp << 1) & 0x1e);

            data = s26_write_data  & 0xffff;
            phy_reg_write(unit, phy_address, reg_address, data);

            reg_address = (((addr_temp << 1) & 0x1e) | 0x1);
            data = s26_write_data >> 16;
            phy_reg_write(unit, phy_address, reg_address, data);

        }
    }
    else
    {
        phy_reg_write(unit,phy_address, reg_address, data);

        phy_address = (0x17 & ((addr_temp >> 4) | 0x10));

        reg_address = (((addr_temp << 1) & 0x1e) | 0x1);
        data = s26_write_data >> 16;
        phy_reg_write(unit,phy_address, reg_address, data);

        reg_address = ((addr_temp << 1) & 0x1e);
        data = s26_write_data  & 0xffff;
        phy_reg_write(unit,phy_address, reg_address, data);
    }
}

void athrs26_reg_rmw(unsigned int s26_addr, unsigned int s26_write_data)
{
    int val = athrs26_reg_read(s26_addr);
    athrs26_reg_write(s26_addr,(val | s26_write_data));
}

unsigned int s26_rd_phy(int ethUnit,unsigned int phy_addr, unsigned int reg_addr)
{

     unsigned int rddata;

     if(phy_addr >= ATHR_PHY_MAX) {
         DPRINTF("%s:Error invalid Phy Address:0x%x\n",__func__,phy_addr);
         return -1 ;
     }

    /* MDIO_CMD is set for read */

    rddata = athrs26_reg_read(0x98);
    rddata = (rddata & 0x0) | (reg_addr<<16)
              | (phy_addr<<21) | (1<<27)
              | (1<<30) | (1<<31) ;

    athrs26_reg_write(0x98, rddata);

    rddata = athrs26_reg_read(0x98);
    rddata = rddata & (1<<31);

    /* Check MDIO_BUSY status */
    while(rddata){
        rddata = athrs26_reg_read(0x98);
        rddata = rddata & (1<<31);
    }

    /* Read the data from phy */

    rddata = athrs26_reg_read(0x98) & 0xffff;

    return(rddata);
}

void s26_wr_phy(int ethUnit,unsigned int phy_addr, unsigned int reg_addr, unsigned int write_data)
{
    unsigned int rddata;

    if(phy_addr >= ATHR_PHY_MAX) {
        DPRINTF("%s:Error invalid Phy Address:0x%x\n",__func__,phy_addr);
        return ;
    }

    /* MDIO_CMD is set for read */

    rddata = athrs26_reg_read(0x98);
    rddata = (rddata & 0x0) | (write_data & 0xffff)
               | (reg_addr<<16) | (phy_addr<<21)
               | (0<<27) | (1<<30) | (1<<31) ;

    athrs26_reg_write(0x98, rddata);

    rddata = athrs26_reg_read(0x98);
    rddata = rddata & (1<<31);

    /* Check MDIO_BUSY status */
    while(rddata){
        rddata = athrs26_reg_read(0x98);
        rddata = rddata & (1<<31);
    }

}
#if 0
int athrs26_ioctl(struct net_device *dev,void *args, int cmd)
{
    struct ifreq * ifr = (struct ifreq *) args;
    struct eth_cfg_params *ethcfg;
    uint32_t ar7240_revid;
    athr_gmac_t *mac0 = athr_macs[0];
    athr_gmac_t *mac1 = athr_macs[1];

#ifdef CONFIG_ATHR_VLAN_IGMP
    struct arl_struct * arl = (struct arl_struct *) (&ifr->ifr_ifru.ifru_mtu);
    unsigned int vlan_value = ifr->ifr_ifru.ifru_ivalue;
    unsigned short vlan_id = vlan_value >> 16;
    unsigned short mode = vlan_value >> 16;
    unsigned short vlan_port = vlan_value & 0x1f;
    unsigned int flag = 0;
    uint32_t ret = 0;
#else
    unsigned int vlan_value = ifr->ifr_ifru.ifru_ivalue;
    unsigned short vlan_port = vlan_value & 0x1f;
    unsigned int flag = 0;
#endif

    ethcfg = (struct eth_cfg_params *)ifr->ifr_data;

    switch(cmd){
#ifdef CONFIG_ATHR_VLAN_IGMP
        case IOCTL_PACKET_FLAG:
            printk("athr_gmac::IOCTL_PACKET_FLAG %d \n",vlan_value);
            set_packet_inspection_flag(vlan_value);
            break;

        case IOCTL_VLAN_ADDPORTS:
            if(vlan_id>4095) return -EINVAL;
            printk("athr_gmac::IOCTL_ADD_PORT vid = %d ports=%x.\n",vlan_id,vlan_port);
            ret = athr_ioctl_vlan_addports(vlan_id,vlan_port);
            break;

        case IOCTL_VLAN_DELPORTS:
            if(vlan_id>4095) return -EINVAL;
            printk("athr_gmac::IOCTL_DEL_PORT vid = %d ports=%x.\n",vlan_id,vlan_port);
            ret = athr_ioctl_vlan_delports(vlan_id,vlan_port);
            break;

        case IOCTL_VLAN_SETTAGMODE:
            printk("athr_gmac::IOCTL_VLAN_SETTAGMODE mode=%d portno=%d .\n",mode,vlan_port);
            ret = athr_port_egvlanmode_set(vlan_port,mode);
            break;

        case IOCTL_VLAN_SETDEFAULTID:
            if(vlan_id>4095) return -EINVAL;
            printk("athr_gmac::IOCTL_VLAN_SETDEFAULTID vid = %d portno=%d.\n",vlan_id,vlan_port);
            ret = athr_port_default_vid_set(vlan_port,vlan_id);
            break;

        case IOCTL_IGMP_ON_OFF:
        {
            int tmp = 0;
            tmp = vlan_value & (0x1 << 7);
            vlan_port &= ~(0x1 << 7);
            if(vlan_port>4) return -EINVAL;
            if(tmp != 0){
                printk("athr_gmac::Enable IGMP snooping in port no %x.\n",vlan_port);
                ret= athr_port_igmps_status_set(vlan_port,1);
            }else{
                printk("athr_gmac::Disable IGMP snooping in port no %x.\n",vlan_port);
                ret= athr_port_igmps_status_set(vlan_port,0);
            }
        }
            break;

        case IOCTL_LINK_GETSTAT:
            if(vlan_port>4){/* if port=WAN */
                int fdx, phy_up;
                athr_phy_speed_t  speed;
                athr_gmac_get_link_st(mac0, &phy_up, &fdx, &speed, 4);
                ifr->ifr_ifru.ifru_ivalue = (speed<<16|fdx<<8|phy_up);
                printk("athr_gmac::IOCTL_LINK_GETSTAT portno WAN is %x.\n",ifr->ifr_ifru.ifru_ivalue);
            }else if(vlan_port > 0){
                flag = athrs26_phy_is_link_alive(vlan_port-1);
                ifr->ifr_ifru.ifru_ivalue = flag;
                printk("athr_gmac::IOCTL_LINK_GETSTAT portno %d is %s.\n",vlan_port,flag?"up":"down");
            }else{
                ifr->ifr_ifru.ifru_ivalue = 1;
            }
            /* PHY 0-4 <---> port 1-5 in user space. */
            break;

        case IOCTL_VLAN_ENABLE:
            athr_ioctl_enable_vlan();
            printk("athr_gmac::IOCTL_VLAN_ENABLE.\n");
            break;

        case IOCTL_VLAN_DISABLE:
            athr_ioctl_disable_vlan();
            printk("athr_gmac::IOCTL_VLAN_DISABLE.\n");
            break;

        case IOCTL_ARL_ADD:
            ret = athr_fdb_add(arl->mac_addr,arl->port_map,arl->sa_drop);
            printk("athr_gmac::IOCTL_ARL_ADD,mac:[%x.%x.%x.%x.%x.%x] port[%x] drop %d\n",
                arl->mac_addr.uc[0],arl->mac_addr.uc[1],arl->mac_addr.uc[2],arl->mac_addr.uc[3],
                arl->mac_addr.uc[4],arl->mac_addr.uc[5],arl->port_map,arl->sa_drop);
            break;

        case IOCTL_ARL_DEL:
            ret = athr_fdb_del(arl->mac_addr);
            printk("athr_gmac::IOCTL_ARL_DEL mac:[%x.%x.%x.%x.%x.%x].\n",arl->mac_addr.uc[0],arl->mac_addr.uc[1],
            arl->mac_addr.uc[2],arl->mac_addr.uc[3],arl->mac_addr.uc[4],arl->mac_addr.uc[5]);
            break;
        case IOCTL_MCAST_CLR:
            /* 0: switch off the unkown multicast packets over vlan. 1: allow the unknown multicaset packets over vlans. */
            if(!vlan_value)
                athr_clear_multi();
            else
                athr_set_multi();
                printk("athr_gmac::IOCTL_MCAST_CLR --- %s.\n", vlan_value?"enable Multicast":"disable Multicast");
            break;
#else
        case IOCTL_LINK_GETSTAT:
            if(vlan_port>4){/* if port=WAN */
                int fdx, phy_up;
                athr_phy_speed_t  speed;
                athr_gmac_get_link_st(mac0, &phy_up, &fdx, &speed, 4);
                ifr->ifr_ifru.ifru_ivalue = (speed<<16|fdx<<8|phy_up);
                printk("athr_gmac::IOCTL_LINK_GETSTAT portno WAN is %x.\n",ifr->ifr_ifru.ifru_ivalue);
            }else if(vlan_port > 0){
                flag = athrs26_phy_is_link_alive(vlan_port-1);
                ifr->ifr_ifru.ifru_ivalue = flag;
                printk("athr_gmac::IOCTL_LINK_GETSTAT portno %d is %s.\n",vlan_port,flag?"up":"down");
            }else{
                ifr->ifr_ifru.ifru_ivalue = 1;
            }
            /* PHY 0-4 <---> port 1-5 in user space. */
            break;


#endif
        case RD_PHY:
            if(ethcfg->portnum != 0xf)
                ethcfg->val = s26_rd_phy(0,ethcfg->portnum,ethcfg->phy_reg);
            else
                ethcfg->val = athrs26_reg_read(ethcfg->phy_reg);
            break;
        case WR_PHY:
            if(ethcfg->portnum != 0xf)
                s26_wr_phy(0,ethcfg->portnum,ethcfg->phy_reg,ethcfg->val);
            else
                athrs26_reg_write(ethcfg->phy_reg,ethcfg->val);
            break;
        case FORCE_PHY:
            printk("Duplex %d\n",ethcfg->duplex);
            if(ethcfg->phy_reg < ATHR_PHY_MAX) {
                if(ethcfg->val == 10) {
                    printk("Forcing 10Mbps %s on port:%d \n",
                         dup_str[ethcfg->duplex],(ethcfg->phy_reg));
                    //athrs26_force_10M(ethcfg->phy_reg,ethcfg->duplex);
                }else if(ethcfg->val == 100) {
                    printk("Forcing 100Mbps %s on port:%d \n",
                         dup_str[ethcfg->duplex],(ethcfg->phy_reg));
                    //athrs26_force_100M(ethcfg->phy_reg,ethcfg->duplex);
                }else if(ethcfg->val == 0) {
                    printk("Enabling Auto Neg on port:%d \n",(ethcfg->phy_reg));
                    ar7240_revid = ar7240_reg_rd(AR7240_REV_ID) & AR7240_REV_ID_MASK;

                    if(ar7240_revid == AR7240_REV_1_0) {
                        s26_wr_phy(0,ethcfg->phy_reg,ATHR_DEBUG_PORT_ADDRESS,0x0);
                        s26_wr_phy(0,ethcfg->phy_reg,ATHR_DEBUG_PORT_DATA,0x2ee);
                        s26_wr_phy(0,ethcfg->phy_reg,ATHR_DEBUG_PORT_ADDRESS,0x3);
                        s26_wr_phy(0,ethcfg->phy_reg,ATHR_DEBUG_PORT_DATA,0x3a11);
                    }
                    s26_wr_phy(0,ethcfg->phy_reg,ATHR_PHY_CONTROL,0x9000);
               }else
                   return -EINVAL;

               if(ATHR_ETHUNIT(ethcfg->phy_reg) == ENET_UNIT_WAN) {
                   if(mac_has_flag(athr_macs[ENET_UNIT_LAN],ETH_SWONLY_MODE))
                       mac1->ops->check_link(mac1,ethcfg->phy_reg);
                   else
                       mac0->ops->check_link(mac0,ethcfg->phy_reg);
               }else{
                   mac1->ops->check_link(mac0,ethcfg->phy_reg);
               }
            break;
            }
        default:
            return -EINVAL;
        }
    return 0;
}
#endif

int athrs26_mdc_check()
{
    int i;

    for (i=0; i<4000; i++) {
        if(athrs26_reg_read(0x10c) != 0x18007fff)
            return -1;
    }
    return 0;
}


void ar7240_s26_intr(void *arg)
{
    int status = 0, intr_reg_val;
    uint32_t phyUnit = 0 ,phyBase = 0 ,phyAddr = 0;
    uint32_t phymask = 0x0;
    uint32_t linkDown = 0x0;
    athr_gmac_t *mac0 = athr_macs[0];
    athr_gmac_t *mac1 = athr_macs[1];


    athrs26_reg_write(S26_GLOBAL_INTR_MASK_REG,0x0);

    intr_reg_val = athrs26_reg_read(S26_GLOBAL_INTR_REG);


    /* clear global link interrupt */
    athrs26_reg_write(S26_GLOBAL_INTR_REG,intr_reg_val);

    if (intr_reg_val & PHY_LINK_CHANGE_REG) {

       for (phyUnit=0; phyUnit < ATHR_PHY_MAX; phyUnit++) {

           phyBase = ATHR_PHYBASE(phyUnit);
           phyAddr = ATHR_PHYADDR(phyUnit);
           status = s26_rd_phy(0,phyAddr,ATHR_PHY_INTR_STATUS);

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

               status = s26_rd_phy(0,phyAddr,ATHR_PHY_INTR_STATUS);

               if (!athrs26_phy_is_link_alive(phyUnit) && !((linkDown >> phyUnit) & 0x1))
                        continue;
               if(ATHR_ETHUNIT(phyUnit) == ENET_UNIT_WAN) {
                    if (mac_has_flag(mac1,ETH_SWONLY_MODE))
                        mac1->ops->check_link(mac1,phyUnit);
                    else
                        mac0->ops->check_link(mac0,phyUnit);
               }
               else {
                    mac1->ops->check_link(mac1,phyUnit);
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

int athrs26_register_ops(void *arg)
{
  athr_gmac_t *mac   = (athr_gmac_t *) arg;
  athr_phy_ops_t *ops = mac->phy;

  if(!ops)
     ops = kmalloc(sizeof(athr_phy_ops_t), GFP_KERNEL);

  memset(ops,0,sizeof(athr_phy_ops_t));

  ops->mac            =  mac;
  ops->is_up          =  athrs26_phy_is_up;
  ops->is_alive       =  athrs26_phy_is_link_alive;
  ops->speed          =  athrs26_phy_speed;
  ops->is_fdx         =  athrs26_phy_is_fdx;
  ops->ioctl          =  NULL;
  ops->setup          =  athrs26_phy_setup;
  ops->stab_wr        =  athrs26_phy_stab_wr;
  ops->link_isr       =  ar7240_s26_intr;
  ops->en_link_intrs  =  athrs26_enable_link_intrs;
  ops->dis_link_intrs =  athrs26_disable_link_intrs;
  ops->read_phy_reg   =  s26_rd_phy;
  ops->write_phy_reg  =  s26_wr_phy;
  ops->read_mac_reg   =  athrs26_reg_read;
  ops->write_mac_reg  =  athrs26_reg_write;

  if (mac->mac_unit == 0) {
     ops->init       =  athrs26_reg_init;
     athr_macs[0]    = mac;
     ops->arg_ad[0]  = (void *) mac;
     ops->port_map = 0x70;   /* set GE0 and  port - 5 */

     if(athr_macs[1])
     {
        ops->arg_ad[1]  = athr_macs[1];
        athr_macs[1]->phy->arg_ad[0] = mac;
     }
  }
  else {
     ops->init       =  athrs26_reg_init_lan;
     athr_macs[1]    = mac;
     ops->arg_ad[1]  = (void *) mac;

     if(athr_macs[0])
     {
        ops->arg_ad[0]  = athr_macs[0];
        athr_macs[0]->phy->arg_ad[1] = mac;
     }

     if (mac_has_flag(mac,ETH_SWONLY_MODE))
        ops->port_map  = 0xbf; /* set GE1 port 0-3 */
     else
        ops->port_map  = 0xaf; /* set GE1 port 0-3 */
  }

  mac->phy = ops;
  return 0;
}
