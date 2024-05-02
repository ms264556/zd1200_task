#include <linux/stddef.h>
//#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <asm/byteorder.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/bitops.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <net/sch_generic.h>
#include <linux/workqueue.h>

#include "ag7100.h"
#include "ag7100_phy.h"
#include "ag7100_trc.h"

#ifdef V54_BSP
#define AG7100_GE0_MII_VAL      1
#define AG7100_GE0_RGMII_VAL    2
#define ETHNAMES 1

extern int athr_phy_ioctl(uint32_t *args, int cmd);
extern int bd_get_lan_macaddr(int, unsigned char*);
static ag7100_mac_t *ag7100_macs[AG7100_NMACS];
static int num_macs = 0;        // stores the number of MACS
extern unsigned char rks_is_gd8_board(void);
extern unsigned char rks_is_gd11_board(void);
extern unsigned char rks_is_gd17_board(void);
extern unsigned char rks_is_walle_board(void);
extern unsigned char rks_is_gd26_board(void);
extern unsigned char rks_is_spaniel_board(void);
extern unsigned char rks_is_gd36_board(void);
extern uint32_t ar7100_ahb_freq;
/* phy0: default to marvell 88e1116 */
int (*ag7100_phy_setup)(int) = mv_phySetup;
int (*ag7100_phy_is_up)(int) = mv_phyIsUp;
unsigned int (*ag7100_get_link_status)(int, int*, int*, ag7100_phy_speed_t*)
                                                = ag7100_get_link_status_mv;
/* phy1: default to null */
int (*ag7100_phy_setup_1)(int) = NULL;
int (*ag7100_phy_is_up_1)(int) = NULL;
unsigned int (*ag7100_get_link_status_1)(int, int*, int*, ag7100_phy_speed_t*)
                                                = NULL;
int mac0_dev_opened = 0;
extern unsigned char rks_is_gd26_ge(void);
int gd26_ge=0;
#else
#undef ETHNAMES
static ag7100_mac_t *ag7100_macs[2];
#endif

#if defined(CONFIG_AR7240)
#define AG7100_FORWARD_HANG_CHECK_TIMER_SECONDS       5
static struct timer_list       forward_hang_check_timer;
static struct timer_list       forward_hang_recover_timer;
#endif

static int tx_timeout_reset         = 0;
#if defined(CONFIG_VENETDEV)
#define VENETDEV_DEBUG      0
int dev_per_mac                     = 1;
int venetEnabled                    = 0;
static void ag7100_set_mac_from_link(ag7100_mac_t *mac, ag7100_phy_speed_t speed, int fdx);

static inline int venet_enable(ag7100_mac_t *mac)
{
    return (rks_is_gd11_board() && mac->mac_unit && venetEnabled) ||
            (rks_is_gd17_board() && !mac->mac_unit && venetEnabled) ||
            (rks_is_walle_board() && mac->mac_unit && venetEnabled);
}

struct vdev_sw_state *get_vdev_sw_state(struct net_device *dev)
{
    ag7100_mac_t *mac = *(ag7100_mac_t **)netdev_priv(dev);
    int i;

    if (0 == venet_enable(mac)) {
        printk(KERN_ALERT "[ALERT] This mac%u is not for the virtual ethernet feature [%s]!!!\n",
            mac->mac_unit, dev->name);
        return NULL;
    }

    for (i=0; i<dev_per_mac; i++) {
        if (&mac->vdev_state[i] == 0)
            break;
        if (mac->vdev_state[i].dev == 0)
            break;
        if ((u32)mac->vdev_state[i].dev == (u32)dev)
            return (&mac->vdev_state[i]);
    }

    return 0;
}
#endif
static void ag7100_hw_setup(ag7100_mac_t *mac);
static void ag7100_hw_stop(ag7100_mac_t *mac);
static void ag7100_oom_timer(unsigned long data);
int ag7100_check_link(ag7100_mac_t *mac, int phyUnit);
#ifdef DMA_DEBUG
static int  check_dma_enabled = 1;
static int  check_rx_fifo_enabled = 1;
static int  check_for_dma_hang(ag7100_mac_t *mac);
#endif
static int  ag7100_tx_alloc(ag7100_mac_t *mac);
static int  ag7100_rx_alloc(ag7100_mac_t *mac);
static void ag7100_rx_free(ag7100_mac_t *mac);
static void ag7100_tx_free(ag7100_mac_t *mac);
static int  ag7100_ring_alloc(ag7100_ring_t *r, int count);
static int  ag7100_rx_replenish(ag7100_mac_t *mac);
static int  ag7100_tx_reap(ag7100_mac_t *mac);
#ifdef V54_BSP
#ifdef RKS_SYSTEM_HOOKS
#define skb_rx netif_rx
#else
#define skb_rx netif_receive_skb
#endif
int ag7100_num_tx_unreap = 0; // track number of outstanding unreaped TX
#define MAX_TX_UNREAP   20
int ag7100_max_tx_unreap = MAX_TX_UNREAP;
#endif
static void ag7100_ring_release(ag7100_mac_t *mac, ag7100_ring_t  *r);
static void ag7100_ring_free(ag7100_ring_t *r);
static void ag7100_tx_timeout_task(struct work_struct *work);
#ifndef V54_BSP
static void ag7100_get_default_macaddr(ag7100_mac_t *mac, u8 *mac_addr);
#endif
static int  ag7100_poll(struct napi_struct *napi, int budget);
static void ag7100_buffer_free(struct sk_buff *skb);
#if defined(CONFIG_AR9100) || defined(CONFIG_AG7100_DMA_HANG_FIX)
void ag7100_dma_reset(ag7100_mac_t *mac);
static void mac_fast_reset(ag7100_mac_t *mac,ag7100_desc_t *ds);
#endif
static int  ag7100_recv_packets(struct net_device *dev, ag7100_mac_t *mac,
                                int max_work, int *work_done);
static irqreturn_t ag7100_intr(int cpl, void *dev_id);
static struct sk_buff * ag7100_buffer_alloc(void);
//
// forward declarations
static struct net_device_stats *ag7100_get_stats(struct net_device *dev);
#if V54_BSP
#if ETHNAMES
#define MAX_ETH_DEV         5
char ethname_array[MAX_ETH_DEV][16] = {{0}};
int default_ethname_remap[] = {0, 1, 2, 3, 4};
int gd11_ethname_remap[] = {2, 1, 0, 0, 0};
int gd11_ethname_remap_1[] = {0, 1, 2, 0, 0};
int walle_ethname_remap[] = {4, 3, 2, 1, 0};
int gd36_ethname_remap[] = {1, 0, 0, 0, 0};
int *ethname_remap = &default_ethname_remap[0];
#endif
#endif


char *mii_str[2][4] = {
    {"GMii", "Mii", "RGMii", "RMii"},
    {"RGMii", "RMii"}
};
char *spd_str[] = {"10Mbps", "100Mbps", "1000Mbps"};
char *dup_str[] = {"half duplex", "full duplex"};

typedef struct {
    int up;
    ag7100_phy_speed_t speed;
    int fdx;
} port_state_t;

port_state_t current_conn[] = {
    {0, AG7100_PHY_SPEED_10T, 0},
    {0, AG7100_PHY_SPEED_10T, 0},
    {0, AG7100_PHY_SPEED_10T, 0},
    {0, AG7100_PHY_SPEED_10T, 0},
    {0, AG7100_PHY_SPEED_10T, 0}
};

extern void athrs26_reg_write(uint32_t reg_addr, uint32_t reg_val);

#if V54_BSP
#include "proc_eth.c"
#endif

#define MODULE_NAME "AG7100"

/* if 0 compute in init */
int tx_len_per_ds = 0;
int phy_in_reset = 0;
#if defined(CONFIG_AR9100) && defined(CONFIG_AG7100_GE1_RMII)
void  ag7100_tx_flush(ag7100_mac_t *mac);
void howl_10baset_war(ag7100_mac_t *mac);
#endif
module_param(tx_len_per_ds, int, 0);
MODULE_PARM_DESC(tx_len_per_ds, "Size of DMA chunk");

/* if 0 compute in init */
int tx_max_desc_per_ds_pkt=0;

/* if 0 compute in init */
#if defined(CONFIG_AR9100)
int fifo_3 = 0x780008;
#else
int fifo_3 = 0;
#endif
module_param(fifo_3, int, 0);
MODULE_PARM_DESC(fifo_3, "fifo cfg 3 settings");

int mii0_if = AG7100_MII0_INTERFACE;
module_param(mii0_if, int, 0);
MODULE_PARM_DESC(mii0_if, "mii0 connect");

int mii1_if = AG7100_MII1_INTERFACE;
module_param(mii1_if, int, 0);
MODULE_PARM_DESC(mii1_if, "mii1 connect");
#if defined(CONFIG_AR7240)
int xmii_val = 0x1c000000;
#endif
#ifndef CONFIG_AR9100
int gige_pll = 0x0110000;
#else
#define SW_PLL 0x1f000000ul
int gige_pll = 0x1a000000;
#endif
module_param(gige_pll, int, 0);
MODULE_PARM_DESC(gige_pll, "Pll for (R)GMII if");

int fifo_5 = 0x3ffff;
module_param(fifo_5, int, 0);
MODULE_PARM_DESC(fifo_5, "fifo cfg 5 settings");

#if HANDLE_FLIP_PHY
// code to handle cross-connecting mac and phy
int flip_phy = 0 ;      // only valid values are 0 and 1
module_param(flip_phy, int, 0);
MODULE_PARM_DESC(flip_phy, "Cross-connect MAC and PHY");
#endif

#define addr_to_words(addr, w1, w2)  {                                 \
    w1 = (addr[0] << 24) | (addr[1] << 16) | (addr[2] << 8) | addr[3]; \
    w2 = (addr[4] << 24) | (addr[5] << 16) | 0;                        \
}

/*
 * Defines specific to this implemention
 */

#define CONFIG_AG7100_LEN_PER_TX_DS     1536
#ifndef CONFIG_AG7100_LEN_PER_TX_DS
#error Please run menuconfig and define CONFIG_AG7100_LEN_PER_TX_DS
#endif

#define CONFIG_AG7100_NUMBER_TX_PKTS    100
#ifndef CONFIG_AG7100_NUMBER_TX_PKTS
#error Please run menuconfig and define CONFIG_AG7100_NUMBER_TX_PKTS
#endif

#define CONFIG_AG7100_NUMBER_RX_PKTS    252
#ifndef CONFIG_AG7100_NUMBER_RX_PKTS
#error Please run menuconfig and define CONFIG_AG7100_NUMBER_RX_PKTS
#endif
#define AG7100_TX_FIFO_LEN          2048
#define AG7100_TX_MIN_DS_LEN        128
#define AG7100_TX_MAX_DS_LEN        AG7100_TX_FIFO_LEN

#define AG7100_TX_MTU_LEN           1536

#ifdef CONFIG_AR9100
#define AG7100_MIN_MTU              68
#define AG7100_MAX_MTU              (AG7100_TX_MTU_LEN - 18)
#endif

#define AG7100_TX_DESC_CNT           CONFIG_AG7100_NUMBER_TX_PKTS*tx_max_desc_per_ds_pkt
#define AG7100_TX_REAP_THRESH        AG7100_TX_DESC_CNT/2
#define AG7100_TX_QSTART_THRESH      4*tx_max_desc_per_ds_pkt

#define AG7100_RX_DESC_CNT           CONFIG_AG7100_NUMBER_RX_PKTS

#define AG7100_NAPI_WEIGHT           64
#define AG7100_PHY_POLL_SECONDS      2
int dma_flag = 0;
static inline int ag7100_tx_reap_thresh(ag7100_mac_t *mac)
{
    ag7100_ring_t *r = &mac->mac_txring;
#if defined(CONFIG_AR9100) && defined(CONFIG_AG7100_GE1_RMII)
    if(mac->speed_10t)
        return (ag7100_ndesc_unused(mac, r) < 2);
    else
#endif
    return (ag7100_ndesc_unused(mac, r) < AG7100_TX_REAP_THRESH);
}

static inline int ag7100_tx_ring_full(ag7100_mac_t *mac)
{
    ag7100_ring_t *r = &mac->mac_txring;

    ag7100_trc_new(ag7100_ndesc_unused(mac, r),"tx ring full");
    return (ag7100_ndesc_unused(mac, r) < tx_max_desc_per_ds_pkt + 2);
}

#if ETHNAMES
char *ethnames = NULL;
module_param(ethnames, charp, 0);
MODULE_PARM_DESC(ethnames, "Name list for Eth devices");
#endif

#if defined(CONFIG_MV_PHY)
static void
ag7100_mii_setup(ag7100_mac_t *mac, uint32_t mii_ctrl, uint32_t mii_ctrl_val)
{
#if defined(CONFIG_AR7240)
    uint32_t mgmt_cfg_val;
#endif

    ag7100_reg_wr(mac, AG7100_MAC_CFG1, (AG7100_MAC_CFG1_RX_EN |
                  AG7100_MAC_CFG1_TX_EN));
    ag7100_reg_rmw_set(mac, AG7100_MAC_CFG2, (AG7100_MAC_CFG2_PAD_CRC_EN |
                       AG7100_MAC_CFG2_LEN_CHECK));

    ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_0, 0x1f00);

#if !defined(CONFIG_AR7240)
    ar7100_reg_wr(mii_ctrl, mii_ctrl_val);
    ag7100_reg_wr(mac, AG7100_MAC_MII_MGMT_CFG, AG7100_MGMT_CFG_CLK_DIV_20);

    ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_1, 0xfff0000);
    ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_2, 0xAAA0555);
    ag7100_reg_rmw_set(mac, AG7100_MAC_FIFO_CFG_4, 0xffff);
    ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_5, 0x7ffef);
#else
    if ((ar7240_reg_rd(AR7100_REV_ID) & AR7100_REV_ID_MASK) == AR7240_REV_1_2) {
        mgmt_cfg_val = 0x2;
        if (mac->mac_unit == 0) {
            ag7100_reg_wr(mac, AG7100_MAC_MII_MGMT_CFG, mgmt_cfg_val | (1 << 31));
            ag7100_reg_wr(mac, AG7100_MAC_MII_MGMT_CFG, mgmt_cfg_val);
        }
    } else if (mac->mac_unit == 0 && is_ar7242()) {
        ag7100_reg_rmw_set(mac, AG7100_MAC_CFG1,(AG7100_MAC_CFG1_RX_FCTL | AG7100_MAC_CFG1_TX_FCTL));

        mgmt_cfg_val = 0xe;
        ar7100_reg_rmw_set(AG7240_ETH_CFG, AG7240_ETH_CFG_RGMII_GE0);
        ag7100_reg_wr(mac, AG7100_MAC_MII_MGMT_CFG, mgmt_cfg_val | (1 << 31));
        ag7100_reg_wr(mac, AG7100_MAC_MII_MGMT_CFG, mgmt_cfg_val);
    } else {
        mgmt_cfg_val = 0x4;
        ag7100_reg_wr(mac, AG7100_MAC_MII_MGMT_CFG, mgmt_cfg_val | (1 << 31));
        ag7100_reg_wr(mac, AG7100_MAC_MII_MGMT_CFG, mgmt_cfg_val);
    }

    ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_1, 0x10ffff);
    ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_2, 0x015500aa);
    /*
    * Weed out junk frames (CRC errored, short collision'ed frames etc.)
    */
    ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_4, 0x3ffff);

    /*
     * Drop CRC Errors, Pause Frames ,Length Error frames, Truncated Frames
     * dribble nibble and rxdv error frames.
     */
    ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_5, 0x66be2);
#endif
}
#endif

static int
ag7100_open(struct net_device *dev)
{
#ifndef V54_BSP
    unsigned int w1 = 0, w2 = 0;
#endif
    ag7100_mac_t *mac = *(ag7100_mac_t **)netdev_priv(dev);
    int st;
#if defined(CONFIG_AR9100) && defined(SWITCH_AHB_FREQ)
    u32 tmp_pll, pll;
#endif
#if defined(CONFIG_VENETDEV)
    struct vdev_sw_state    *vdev_state = NULL;
    if (venet_enable(mac))
        vdev_state = get_vdev_sw_state(dev);
#endif

#ifndef V54_BSP
    assert(mac);
#else
    if (!mac)
    {
        printk("ag7100 open: dev %x !mac\n", (unsigned int)dev);
        return 1;
    }
#endif

#if !defined(CONFIG_AR7240)
    printk("%s: mac=%p dev=%p mac_unit=%d mac0_dev_opened=%d\n",
        __func__, mac, dev, mac->mac_unit, mac0_dev_opened);
    // start timer and interrupt if called by kernel
    if ((ag7100_macs[0] == mac) && (mac->mac_dev == dev) && (mac0_dev_opened == 1)) {
        if (rks_is_walle_board() || rks_is_gd26_board() || rks_is_spaniel_board() || rks_is_gd36_board()) {
            if (!gd26_ge) {
                athrs26_enable_linkIntrs(mac->mac_unit);
                athrs26_phy_restart(4); // phy addr 4
           } else {
                init_timer(&mac->mac_phy_timer);
                mac->mac_phy_timer.data     = (unsigned long)mac;
                mac->mac_phy_timer.function = (void *)ag7100_check_link;
                ag7100_check_link(mac, 0);
            }
        } else {
            /*
             * phy link mgmt
             */
            init_timer(&mac->mac_phy_timer);
            mac->mac_phy_timer.data     = (unsigned long)mac;
            mac->mac_phy_timer.function = (void *)ag7100_check_link;
            ag7100_check_link(mac, 0);
        }
        dev->trans_start = jiffies;
        ag7100_int_enable(mac);
        ag7100_rx_start(mac);
#if !defined(CONFIG_AR7240)
        ag7100_start_rx_count(mac);
#endif
        mac0_dev_opened = 2;    // done with special init sequence
#if defined(MAC_HW_UNSTOP)
        mac->mac_init = 1;      // mac0
#endif
        return 0;
    }
#else
    printk("%s: mac=%p dev=%p mac_unit=%d\n",
        __func__, mac, dev, mac->mac_unit);
#endif

#if defined(CONFIG_VENETDEV)
  if (venet_enable(mac)) {
    vdev_state->up = 1;
    mac->mac_open++;
    if (mac->mac_open > 1) {
        //printk("%s: unit_on_MAC %d\n", __func__, vdev_state->unit_on_MAC);
        netif_carrier_off(dev);
        netif_stop_queue(dev);
        if (rks_is_walle_board()) {
            athrs26_phy_restart(vdev_state->unit_on_MAC);
        }
        dev->trans_start = jiffies;
        return 0;
    }
    mac->mac_dev = dev;
  }
#endif

#if defined(MAC_HW_UNSTOP)
    if (mac->mac_init) {
        napi_enable(&mac->napi);
        if (!rks_is_walle_board()) {
            init_timer(&mac->mac_phy_timer);
            mac->mac_phy_timer.data     = (unsigned long)mac;
            mac->mac_phy_timer.function = (void *)ag7100_check_link;
            ag7100_check_link(mac, 0);
        } else {
            athrs26_enable_linkIntrs(mac->mac_unit);
            if (!mac->mac_unit) {
                athrs26_phy_restart(4); // phy addr 4
            } else {
                int i;
                for (i=0; i<4; i++)
                    athrs26_phy_restart(i);
            }
        }
        dev->trans_start = jiffies;
        ag7100_int_enable(mac);
        ag7100_rx_start(mac);
#if !defined(CONFIG_AR7240)
        ag7100_start_rx_count(mac);
#endif
        return 0;
    }
#endif

    st = request_irq(mac->mac_irq, ag7100_intr, IRQF_DISABLED, dev->name, dev);
    if (st < 0)
    {
        printk(MODULE_NAME ": request irq %d failed %d\n", mac->mac_irq, st);
        return 1;
    }
    if (ag7100_tx_alloc(mac)) goto tx_failed;
    if (ag7100_rx_alloc(mac)) goto rx_failed;

#if defined(CONFIG_MV_PHY)
    if (rks_is_gd8_board()) {
        mii0_if = AG7100_GE0_MII_VAL;
    } else if (rks_is_walle_board()) {
        if (!mac->mac_unit) {
            /* mii */
            mii0_if = 1;
        } else {
            /* gmii */
            mii1_if = 1;
        }
     } else if (rks_is_gd26_board() || rks_is_spaniel_board() || rks_is_gd36_board()) {
        if (!mac->mac_unit) {
            if (!gd26_ge)
                /* mii */
                mii0_if = 1;
            else
                mii0_if = 2;
        } else {
            /* gmii */
            mii1_if = 0;
        }
     } else {
        if (!mac->mac_unit) {
            /* rgmii 0 */
            mii0_if = 2;
        } else {
            /* rgmii 1 */
            mii1_if = 0;
        }
    }
#endif

#if defined(CONFIG_AR7240)
    if (rks_is_walle_board()){
    uint32_t mask;
    unsigned long flags;

    udelay(20);
    mask = ag7100_reset_mask(mac->mac_unit);

    /*
    * put into reset, hold, pull out.
    */
    spin_lock_irqsave(&mac->mac_lock, flags);
    ar7100_reg_rmw_set(AR7100_RESET, mask);
    udelay(10);
    ar7100_reg_rmw_clear(AR7100_RESET, mask);
    udelay(10);
    spin_unlock_irqrestore(&mac->mac_lock, flags);
    }
#endif

    ag7100_hw_setup(mac);
#if defined(CONFIG_AR9100) && defined(SWITCH_AHB_FREQ)
    /*
    * Reduce the AHB frequency to 100MHz while setting up the
    * S26 phy.
    */
    pll= ar7100_reg_rd(AR7100_PLL_CONFIG);
    tmp_pll = pll& ~((PLL_DIV_MASK << PLL_DIV_SHIFT) | (PLL_REF_DIV_MASK << PLL_REF_DIV_SHIFT));
    tmp_pll = tmp_pll | (0x64 << PLL_DIV_SHIFT) |
        (0x5 << PLL_REF_DIV_SHIFT) | (1 << AHB_DIV_SHIFT);

    ar7100_reg_wr_nf(AR7100_PLL_CONFIG, tmp_pll);
    udelay(100*1000);
#endif

#if defined(CONFIG_ATHRS26_PHY)
    /* if using header for register configuration, we have to     */
    /* configure s26 register after frame transmission is enabled */
    if (mac->mac_unit == 1) /* wan phy */ {
        athrs26_reg_init();
        mac->mac_speed = -1;/*set initial state*/
    }
#endif
    if (rks_is_gd17_board() && (!mac->mac_unit)) {
        // AR8228
        athrs16_reg_init();
    }
    if (rks_is_walle_board() || rks_is_gd26_board() || rks_is_gd36_board()) {
        if (!mac->mac_unit) {
            if (!gd26_ge) {
                athrs26_reg_init();
            }
        } else {
#if defined(CONFIG_VENETDEV)
            athrs26_reg_init_lan(venetEnabled);
#else
            athrs26_reg_init_lan(0);
#endif
        }
    }

#if defined(V54_BSP) && defined(CONFIG_MV_PHY)
    if (rks_is_gd11_board() && (mac->mac_unit)) {
        ag7100_phy_setup_1(mac->mac_unit);
    } else if ((rks_is_gd26_board() || rks_is_gd36_board()) && (mac->mac_unit) && gd26_ge) {
        phy_in_reset = 1;
        ag7100_phy_setup_1(mac->mac_unit);
        phy_in_reset = 0;
    } else
#endif
    {
        phy_in_reset = 1;
        ag7100_phy_setup(mac->mac_unit);
        phy_in_reset = 0;
    }

#if defined(CONFIG_AR9100) && defined(SWITCH_AHB_FREQ)
    ar7100_reg_wr_nf(AR7100_PLL_CONFIG, pll);
    udelay(100*1000);
#endif
#if defined(CONFIG_VENETDEV)
    if (venet_enable(mac)) {
        int fdx, phy_up, rc;
        ag7100_phy_speed_t  speed;
        mac->devIrq = dev;
        if (rks_is_gd11_board()) {
            rc = ag7100_get_link_status_1(mac->mac_unit, &phy_up, &fdx, &speed);
            if (rc >= 0) {
                printk("ag7100 open: phy_up(%d) fdx(%d) speed(%d)\n", phy_up, fdx, speed);
                /* set mac1 to 100Mb full */
                ag7100_set_mac_from_link(mac, AG7100_PHY_SPEED_100TX, 1);
            }
        } else {
            /* gd17 or walle */
            if (rks_is_walle_board()) {
                rc = phy_up = fdx = speed = 0;
            } else {
                rc = ag7100_get_link_status(mac->mac_unit, &phy_up, &fdx, &speed);
            }

            if (rc >= 0) {
                printk("ag7100 open: phy_up(%d) fdx(%d) speed(%d)\n", phy_up, fdx, speed);
                /* set mac1 to 1000Mb full */
                ag7100_set_mac_from_link(mac, AG7100_PHY_SPEED_1000T, 1);
            }
        }
    }
#endif
#ifdef V54_BSP
    {
        u8 *macAddr = dev->dev_addr;

        ag7100_reg_wr(mac, AG7100_GE_MAC_ADDR1,
            (unsigned int)((macAddr[0]<<24) | (macAddr[1]<<16) | (macAddr[2]<<8) | macAddr[3]));
        ag7100_reg_wr(mac, AG7100_GE_MAC_ADDR2,
            (unsigned int)((macAddr[4]<<24) | (macAddr[5]<<16)));
    }
#else
    /*
    * set the mac addr
    */
    addr_to_words(dev->dev_addr, w1, w2);
    ag7100_reg_wr(mac, AG7100_GE_MAC_ADDR1, w1);
    ag7100_reg_wr(mac, AG7100_GE_MAC_ADDR2, w2);
#endif
    dev->trans_start = jiffies;

    /*
     * Keep carrier off while initialization and switch it once the link is up.
     */
    netif_carrier_off(dev);
    netif_stop_queue(dev);

    napi_enable(&mac->napi);

    mac->dma_check = 0;
#if !defined(CONFIG_AR7240)
    // do not start timer and interrupt if called by ag7100_init
    if ((ag7100_macs[0] == mac) && (mac->mac_dev == dev) && !mac0_dev_opened)
        return 0;
#endif

    if ((!rks_is_walle_board() && !rks_is_gd26_board() && !rks_is_spaniel_board() && !rks_is_gd36_board()) ||
        ((rks_is_gd26_board() || rks_is_spaniel_board() || rks_is_gd36_board()) && !mac->mac_unit && gd26_ge)) {
        /*
         * phy link mgmt
         */
        init_timer(&mac->mac_phy_timer);
        mac->mac_phy_timer.data     = (unsigned long)mac;
#ifdef V54_BSP
        mac->mac_phy_timer.function = (void *)ag7100_check_link;
#else
        mac->mac_phy_timer.function = ag7100_check_link;
#endif
        ag7100_check_link(mac, 0);
    }


    ag7100_int_enable(mac);

    if (rks_is_walle_board()) {
        athrs26_enable_linkIntrs(mac->mac_unit);
        if (!mac->mac_unit) {
            athrs26_phy_restart(4); // phy addr 4
        } else {
            int i;
            for (i=0; i<4; i++)
                athrs26_phy_restart(i);
        }
    }
    if (rks_is_gd26_board() || rks_is_gd36_board()) {
        if (!mac->mac_unit && !gd26_ge) {
            athrs26_enable_linkIntrs(mac->mac_unit);
            athrs26_phy_restart(4); // phy addr 4
        } else {
            printk("%s: %d\n", __func__, __LINE__);
            athrs26_enable_linkIntrs(mac->mac_unit);
            athrs26_phy_restart(0);
        }
    }

    ag7100_rx_start(mac);

#if !defined(CONFIG_AR7240)
    ag7100_start_rx_count(mac);
#endif

/* enable MAC RX/TX at last */
#if defined(CONFIG_AR7240)
    if (rks_is_walle_board())
        ag7100_reg_rmw_set(mac, AG7100_MAC_CFG1,    (AG7100_MAC_CFG1_RX_EN | AG7100_MAC_CFG1_TX_EN));
#endif

#if defined(MAC_HW_UNSTOP)
    mac->mac_init = 1;  // mac1
#endif

#if defined(CONFIG_VENETDEV)
    if (rks_is_walle_board() && venet_enable(mac) && tx_timeout_reset) {
            int i;
            for (i=0; i<dev_per_mac; i++) {
                vdev_state = get_vdev_sw_state(mac->vdev_state[i].dev);
                if (vdev_state->dev != dev) {
                    vdev_state->up = 1;
                    mac->mac_open++;
                    netif_carrier_off(vdev_state->dev);
                    netif_stop_queue(vdev_state->dev);
                    vdev_state->dev->trans_start = jiffies;
                }
            }
        tx_timeout_reset = 0;
    }
#endif
#ifdef DMA_DEBUG
    if (rks_is_walle_board()) {
        init_timer(&mac->mac_dbg_timer);
        mac->mac_dbg_timer.data     = (unsigned long)mac;
        mac->mac_dbg_timer.function = (void *)check_for_dma_hang;
        mod_timer(&mac->mac_dbg_timer, jiffies + (1*HZ/2));
    }
#endif

    return 0;

rx_failed:
    ag7100_tx_free(mac);
tx_failed:
    free_irq(mac->mac_irq, dev);
    return 1;
}

static int
ag7100_stop(struct net_device *dev)
{
    ag7100_mac_t *mac = *(ag7100_mac_t **)netdev_priv(dev);
    unsigned long flags;
#if !defined(MAC_HW_UNSTOP)
    int tmpdata;
#endif

#if defined(CONFIG_VENETDEV)
    if (venet_enable(mac)) {
        if (!tx_timeout_reset) {
            struct vdev_sw_state    *vdev_state;
            vdev_state = get_vdev_sw_state(dev);
            vdev_state->up = 0;
            vdev_state->carrier = 0;
            mac->mac_open--;
            if (mac->mac_open) {
                int i;
                netif_stop_queue(dev);
                netif_carrier_off(dev);
                for (i=0; i<dev_per_mac; i++) {
                    if (mac->vdev_state[i].up) {
                        mac->mac_dev = mac->vdev_state[i].dev;
                        break;
                    }
                }
                return 0;
            }
        }
    }
#endif
    if (rks_is_walle_board() || rks_is_gd26_board() || rks_is_gd36_board())
        athrs26_disable_linkIntrs(mac->mac_unit);

    napi_disable(&mac->napi);

#ifdef DMA_DEBUG
    if (rks_is_walle_board())
        del_timer(&mac->mac_dbg_timer);
#endif

    spin_lock_irqsave(&mac->mac_lock, flags);

#if !defined(MAC_HW_UNSTOP)
   //athrs26_phy_off(mac);

    tmpdata = ag7100_reg_rd(mac, AG7100_MAC_CFG1);
    ag7100_reg_wr(mac, AG7100_MAC_CFG1, tmpdata & (~(
				(AG7100_MAC_CFG1_TX_EN|AG7100_MAC_CFG1_RX_EN))));
#endif
    netif_stop_queue(dev);
    netif_carrier_off(dev);

    ag7100_hw_stop(mac);
#if !defined(MAC_HW_UNSTOP)
#if defined(CONFIG_VENETDEV)
    /* need correct dev address to free irq */
    if (venet_enable(mac))
        free_irq(mac->mac_irq, mac->devIrq);
    else
        free_irq(mac->mac_irq, dev);
#else
    free_irq(mac->mac_irq, dev);
#endif
#endif

   /*
    *  WAR for bug:32681 reduces the no of TX buffers to five from the
    *  actual number  of allocated buffers. Revert the value before freeing
    *  them to avoid memory leak
    */
#if defined(CONFIG_AR9100) && defined(CONFIG_AG7100_GE1_RMII)
    mac->mac_txring.ring_nelem = AG7100_TX_DESC_CNT;
    mac->speed_10t = 0;
#endif

#if !defined(MAC_HW_UNSTOP)
    ag7100_tx_free(mac);
    ag7100_rx_free(mac);
#endif

    if ((!rks_is_walle_board() && !rks_is_gd26_board() && !rks_is_spaniel_board() && !rks_is_gd36_board()) ||
        ((rks_is_gd26_board() || rks_is_spaniel_board() || rks_is_gd36_board()) && !mac->mac_unit && gd26_ge))
        del_timer(&mac->mac_phy_timer);

    spin_unlock_irqrestore(&mac->mac_lock, flags);
    /*ag7100_trc_dump();*/
    return 0;
}

static void
ag7100_hw_setup(ag7100_mac_t *mac)
{
    ag7100_ring_t *tx = &mac->mac_txring, *rx = &mac->mac_rxring;
    ag7100_desc_t *r0, *t0;
#if defined(CONFIG_AR7240)
    uint32_t mgmt_cfg_val;
    u32 check_cnt;
#endif

#ifdef CONFIG_AR9100
    /*
     *  When the interface goes up and down while traffic is present the MAC is
     *  not getting reset properly by enabling AG7100_MAC_CFG1_SOFT_RST bit.
     *  Hence doing a complete MAC reset. Fix for Bug:
     */
    int mask = 0;

    if(mac->mac_unit)
        mask = AR7100_RESET_GE1_MAC;
    else
        mask = AR7100_RESET_GE0_MAC;

    ar7100_reg_rmw_set(AR7100_RESET, mask);
    mdelay(100);
    ar7100_reg_rmw_clear(AR7100_RESET, mask);
    mdelay(100);
#endif

#ifdef CONFIG_AR9100
#ifndef CONFIG_PORT0_AS_SWITCH
    if(mac->mac_unit) {
    ag7100_reg_wr(mac, AG7100_MAC_CFG1, (AG7100_MAC_CFG1_RX_EN |
        AG7100_MAC_CFG1_TX_EN|AG7100_MAC_CFG1_RX_FCTL|AG7100_MAC_CFG1_TX_FCTL));
    } else {
	 ag7100_reg_wr(mac, AG7100_MAC_CFG1, (AG7100_MAC_CFG1_RX_EN |
        AG7100_MAC_CFG1_TX_EN|AG7100_MAC_CFG1_RX_FCTL));
    }
#else /* if CONFIG_PORT0_AS_SWITCH is enabled */
#if 1 /* Enable RX Flow control on both LAN and WAN */
    ag7100_reg_wr(mac, AG7100_MAC_CFG1, (AG7100_MAC_CFG1_RX_EN |
            AG7100_MAC_CFG1_TX_EN|AG7100_MAC_CFG1_RX_FCTL));
#else
    if(mac->mac_unit) {  /* Wan port */
    ag7100_reg_wr(mac, AG7100_MAC_CFG1, (AG7100_MAC_CFG1_RX_EN |
        AG7100_MAC_CFG1_TX_EN|AG7100_MAC_CFG1_RX_FCTL));
    } else { /* Lan port */
         ag7100_reg_wr(mac, AG7100_MAC_CFG1, (AG7100_MAC_CFG1_RX_EN |
        AG7100_MAC_CFG1_TX_EN |AG7100_MAC_CFG1_RX_FCTL|AG7100_MAC_CFG1_TX_FCTL));
    }
#endif
#endif
#else
#if !defined(CONFIG_AR7240)
    /* clear the rx fifo state if any */
    ag7100_reg_wr(mac, AG7100_DMA_RX_STATUS, ag7100_reg_rd(mac, AG7100_DMA_RX_STATUS));
#endif
#if defined(CONFIG_AR7240)
    if (rks_is_walle_board()) {
        ag7100_reg_wr(mac, AG7100_MAC_CFG1, 0); //remove RESET
    } else {
        ag7100_reg_wr(mac, AG7100_MAC_CFG1, (AG7100_MAC_CFG1_RX_EN |
                  AG7100_MAC_CFG1_TX_EN));
    }
#else
    ag7100_reg_wr(mac, AG7100_MAC_CFG1, (AG7100_MAC_CFG1_RX_EN |
                  AG7100_MAC_CFG1_TX_EN));
#endif
#endif
#if defined(CONFIG_VENETDEV)
    if (venet_enable(mac)) {
        ag7100_reg_rmw_set(mac, AG7100_MAC_CFG2, (AG7100_MAC_CFG2_CRC_EN));
        ag7100_reg_rmw_clear(mac, AG7100_MAC_CFG2, (AG7100_MAC_CFG2_PAD_CRC_EN |
                           AG7100_MAC_CFG2_LEN_CHECK));
    } else {
        ag7100_reg_rmw_set(mac, AG7100_MAC_CFG2, (AG7100_MAC_CFG2_PAD_CRC_EN |
                           AG7100_MAC_CFG2_LEN_CHECK));
    }
#else
    ag7100_reg_rmw_set(mac, AG7100_MAC_CFG2, (AG7100_MAC_CFG2_PAD_CRC_EN |
                       AG7100_MAC_CFG2_LEN_CHECK));
#endif

    ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_0, 0x1f00);

#if defined(CONFIG_AR7240)
    if (mac->mac_unit) {
        ag7100_reg_rmw_set(mac, AG7100_MAC_CFG1,(AG7100_MAC_CFG1_RX_FCTL | AG7100_MAC_CFG1_TX_FCTL));
        ag7100_reg_rmw_set(mac, AG7100_MAC_CFG2, AG7100_MAC_CFG2_IF_1000);
    } else {
        if (!gd26_ge) {
            /* walle and corsica wan port: 10/100 */
            ag7100_reg_rmw_set(mac, AG7100_MAC_CFG1,(AG7100_MAC_CFG1_RX_FCTL | AG7100_MAC_CFG1_TX_FCTL));
            ag7100_reg_rmw_set(mac, AG7100_MAC_CFG2, AG7100_MAC_CFG2_IF_10_100);
        } else {
            /* ar8035, lantiq */
            ag7100_reg_rmw_set(mac, AG7100_MAC_CFG1,(AG7100_MAC_CFG1_RX_FCTL | AG7100_MAC_CFG1_TX_FCTL));
            ag7100_reg_rmw_set(mac, AG7100_MAC_CFG2, AG7100_MAC_CFG2_IF_1000);
        }
        if (is_ar7242()) {
            ag7100_reg_rmw_set(mac, AG7100_MAC_CFG1,(AG7100_MAC_CFG1_RX_FCTL | AG7100_MAC_CFG1_TX_FCTL));
        }
    }
    /* Disable AG7100_MAC_CFG2_LEN_CHECK to fix the bug that
     * the mac address is mistaken as length when enabling Atheros header
     */
    if (rks_is_walle_board() && mac->mac_unit)
       ag7100_reg_rmw_clear(mac, AG7100_MAC_CFG2, AG7100_MAC_CFG2_LEN_CHECK)

    if (is_ar934x()) {
        mgmt_cfg_val = 0x9; /* 2: internal switch, 9: internal switch or latiq, ar8035 */
        ag7100_reg_wr(mac, AG7100_MAC_MII_MGMT_CFG, mgmt_cfg_val | (1 << 31));
        ag7100_reg_wr(mac, AG7100_MAC_MII_MGMT_CFG, mgmt_cfg_val);
        if (gd26_ge) {
            ar7100_reg_rmw_set(AG7240_ETH_CFG, AG7240_ETH_CFG_RGMII_GE0);
        }
    } else if ((ar7100_reg_rd(AR7100_REV_ID) & AR7100_REV_ID_MASK) == AR7240_REV_1_2) {
        mgmt_cfg_val = 0x2;
        if (mac->mac_unit == 0) {
            ag7100_reg_wr(mac, AG7100_MAC_MII_MGMT_CFG, mgmt_cfg_val | (1 << 31));
            ag7100_reg_wr(mac, AG7100_MAC_MII_MGMT_CFG, mgmt_cfg_val);
        }
    }
    else {
        switch (ar7100_ahb_freq/1000000) {
            case 150:
                     mgmt_cfg_val = 0x7;
                     break;
            case 175:
                     mgmt_cfg_val = 0x5;
                     break;
            case 200:
                     mgmt_cfg_val = 0x4;
                     break;
            case 210:
                      mgmt_cfg_val = 0x9;
                      break;
            case 220:
                      mgmt_cfg_val = 0x9;
                      break;
            default:
                     mgmt_cfg_val = 0x7;
        }
        if ((is_ar7241() || is_ar7242())) {

            /* External MII mode */
            if (mac->mac_unit == 0 && is_ar7242()) {
#if defined(V54_BSP)
                 mgmt_cfg_val = 0xe;
#else
                 mgmt_cfg_val = 0x6;
#endif
                 ar7100_reg_rmw_set(AG7240_ETH_CFG, AG7240_ETH_CFG_RGMII_GE0);
                 ag7100_reg_wr(mac, AG7100_MAC_MII_MGMT_CFG, mgmt_cfg_val | (1 << 31));
                 ag7100_reg_wr(mac, AG7100_MAC_MII_MGMT_CFG, mgmt_cfg_val);
            }
#if !defined(V54_BSP)
            /* gd17 uses one mac only */
	        /* Virian */
            mgmt_cfg_val = 0x4;

#ifdef CONFIG_S26_SWITCH_ONLY_MODE
            if (is_ar7241()) {
                ar7100_reg_rmw_set(AG7240_ETH_CFG, AG7240_ETH_CFG_SW_ONLY_MODE);
                ag7100_reg_rmw_set(ag7100_macs[0], AG7100_MAC_CFG1, AG7100_MAC_CFG1_SOFT_RST);;
            }
#endif
            ag7100_reg_wr(ag7100_macs[1], AG7100_MAC_MII_MGMT_CFG, mgmt_cfg_val | (1 << 31));
            ag7100_reg_wr(ag7100_macs[1], AG7100_MAC_MII_MGMT_CFG, mgmt_cfg_val);
	        printk("Virian MDC CFG Value ==> %x\n",mgmt_cfg_val);
#endif
        }
	    else { /* Python 1.0 & 1.1 */
		if (mac->mac_unit == 0) {
			check_cnt = 0;
			while (check_cnt++ < 10) {
				ag7100_reg_wr(mac, AG7100_MAC_MII_MGMT_CFG, mgmt_cfg_val | (1 << 31));
				ag7100_reg_wr(mac, AG7100_MAC_MII_MGMT_CFG, mgmt_cfg_val);
				if(athrs26_mdc_check() == 0)
					break;
			}
			if(check_cnt == 11)
				printk("%s: MDC check failed\n", __func__);
		}
	}
    }

#else
    /*
     * set the mii if type - NB reg not in the gigE space
     */
    ar7100_reg_wr(mii_reg(mac), mii_if(mac));
    ag7100_reg_wr(mac, AG7100_MAC_MII_MGMT_CFG, AG7100_MGMT_CFG_CLK_DIV_20);
#endif

#ifdef CONFIG_AR7100_EMULATION
    ag7100_reg_rmw_set(mac, AG7100_MAC_FIFO_CFG_4, 0x3ffff);
    ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_1, 0xfff0000);
 /*   ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_2, 0x1fff); */
    ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_2, 0xAAA0555);
#else
#if !defined(CONFIG_AR7240)
    ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_1, 0xfff0000);
    /* ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_2, 0x1fff); */
    ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_2, 0xAAA0555);
    /*
    * Weed out junk frames (CRC errored, short collision'ed frames etc.)
    */
    ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_4, 0xffff);
#ifdef CONFIG_AR9100
    /* Drop CRC Errors and Pause Frames */
    ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_5, 0x7ffef); /* Forward pause frames */
#else
    ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_5, 0x7ffef);
#endif
#else // CONFIG_AR7240
    ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_1, 0x10ffff);
    ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_2, 0x015500aa);
    /*
    * Weed out junk frames (CRC errored, short collision'ed frames etc.)
    */
    ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_4, 0x3ffff);

    /*
     * Drop CRC Errors, Pause Frames ,Length Error frames, Truncated Frames
     * dribble nibble and rxdv error frames.
     */
    //printk("Setting Drop CRC Errors, Pause Frames and Length Error frames \n");
    if (rks_is_walle_board()) {
        if (mac->mac_unit) {
            ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_5, 0xe6bc0);
        } else {
            ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_5, 0x66bc2);
        }
    } else {
        ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_5, 0x66bc2);
    }

    if (mac->mac_unit == 0 && is_ar7242()){
       ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_5, 0xe6be2);
    }
#endif // !defined(CONFIG_AR7240)
#endif // CONFIG_AR7100_EMULATION

    t0  =  &tx->ring_desc[0];
    r0  =  &rx->ring_desc[0];

    ag7100_reg_wr(mac, AG7100_DMA_TX_DESC, ag7100_desc_dma_addr(tx, t0));
    ag7100_reg_wr(mac, AG7100_DMA_RX_DESC, ag7100_desc_dma_addr(rx, r0));

    printk(MODULE_NAME ": cfg1 %#x cfg2 %#x\n", ag7100_reg_rd(mac, AG7100_MAC_CFG1),
            ag7100_reg_rd(mac, AG7100_MAC_CFG2));
}

static void
ag7100_hw_stop(ag7100_mac_t *mac)
{
    ag7100_rx_stop(mac);
    ag7100_tx_stop(mac);
    ag7100_int_disable(mac);

	/*
	 *  Not required for HOWL as we are doing fulchip reset while `ifconfig up`.
	 */
#if !defined(MAC_HW_UNSTOP)
#if !defined(CONFIG_AR9100) && !defined(CONFIG_AR7240)
    /*
     * put everything into reset.
     * Dont Reset WAN MAC as we are using eth0 MDIO to access S26 Registers.
     * if(mac->mac_unit == 1 || is_ar7241() || is_ar7242())
     */
    ag7100_reg_rmw_set(mac, AG7100_MAC_CFG1, AG7100_MAC_CFG1_SOFT_RST);
#endif
#endif
}

/*
 * program the usb pll (misnomer) to genrate appropriate clock
 * Write 2 into control field
 * Write pll value
 * Write 3 into control field
 * Write 0 into control field
 */
#ifdef CONFIG_AR9100
#define ag7100_pll_shift(_mac)      (((_mac)->mac_unit) ? 22: 20)
#define ag7100_pll_offset(_mac)     \
    (((_mac)->mac_unit) ? AR9100_ETH_INT1_CLK : \
                          AR9100_ETH_INT0_CLK)
#else
#define ag7100_pll_shift(_mac)      (((_mac)->mac_unit) ? 19: 17)
#define ag7100_pll_offset(_mac)     \
    (((_mac)->mac_unit) ? AR7100_USB_PLL_GE1_OFFSET : \
                          AR7100_USB_PLL_GE0_OFFSET)
#endif

#if !defined(CONFIG_AR7240)
static void
ag7100_set_pll(ag7100_mac_t *mac, unsigned int pll)
{
#ifdef CONFIG_AR9100
#define ETH_PLL_CONFIG AR9100_ETH_PLL_CONFIG
#else
#define ETH_PLL_CONFIG AR7100_USB_PLL_CONFIG
#endif
  uint32_t shift, reg, val;

  shift = ag7100_pll_shift(mac);
  reg   = ag7100_pll_offset(mac);

    val  = ar7100_reg_rd(ETH_PLL_CONFIG);
  val &= ~(3 << shift);
  val |=  (2 << shift);
    ar7100_reg_wr(ETH_PLL_CONFIG, val);
  udelay(100);

  ar7100_reg_wr(reg, pll);

  val |=  (3 << shift);
    ar7100_reg_wr(ETH_PLL_CONFIG, val);
  udelay(100);

  val &= ~(3 << shift);
    ar7100_reg_wr(ETH_PLL_CONFIG, val);
  udelay(100);

  printk(MODULE_NAME ": pll reg %#x: %#x\n", reg, ar7100_reg_rd(reg));
}
#endif

#if defined(CONFIG_AR9100) && defined(CONFIG_AG7100_GE1_RMII)


/*
 * Flush from tail till the head and free all the socket buffers even if owned by DMA
 * before we change the size of the ring buffer to avoid memory leaks and reset the ring buffer.
 *
 * WAR for Bug: 32681
 */

void
ag7100_tx_flush(ag7100_mac_t *mac)
{
    ag7100_ring_t   *r     = &mac->mac_txring;
    int              head  = r->ring_nelem , tail = 0, flushed = 0, i;
    ag7100_desc_t   *ds;
    ag7100_buffer_t *bf;
    uint32_t    flags;


    ar7100_flush_ge(mac->mac_unit);

    while(flushed != head)
    {
        ds   = &r->ring_desc[tail];

        bf      = &r->ring_buffer[tail];
        if(bf->buf_pkt) {
            for(i = 0; i < bf->buf_nds; i++)
            {
                ag7100_intr_ack_tx(mac);
                ag7100_ring_incr(tail);
            }

            ag7100_buffer_free(bf->buf_pkt);
            bf->buf_pkt = NULL;
        }
        else
            ag7100_ring_incr(tail);

        ag7100_tx_own(ds);
        flushed ++;
    }
    r->ring_head = r->ring_tail = 0;

    return;
}

/*
 * Work around to recover from Tx failure when connected to 10BASET.
 * Bug: 32681.
 *
 * After AutoNeg to 10Mbps Half Duplex, under some un-identified circumstances
 * during the init sequence, the MAC is in some illegal state
 * that stops the TX and hence no TXCTL to the PHY.
 * On Tx Timeout from the software, the reset sequence is done again which recovers the
 * MAC and Tx goes through without any problem.
 * Instead of waiting for the application to transmit and recover, we transmit
 * 40 dummy Tx pkts on negogiating as 10BASET.
 * Reduce the number of TX buffers from 40 to 5 so that in case of TX failures we do
 * a immediate reset and retrasmit again till we successfully transmit all of them.
 */

void
howl_10baset_war(ag7100_mac_t *mac)
{

    struct sk_buff *dummy_pkt;
    struct net_device *dev = mac->mac_dev;
    ag7100_desc_t *ds;
    ag7100_ring_t *r;
    int i=6;
    printk("Calling howl_10T war \n");
    /*
     * Create dummy packet
     */
    dummy_pkt = dev_alloc_skb(64);
    skb_put(dummy_pkt, 60);
    atomic_dec(&dummy_pkt->users);
    while(--i >= 0) {
        dummy_pkt->data[i] = 0xff;
    }
#ifdef V54_BSP
    if (!bd_get_lan_macaddr(mac->mac_unit, (dummy_pkt->data + 6))) {
        ag7100_get_macaddr(mac->mac_unit, (dummy_pkt->data + 6));
    }
#else
    ag7100_get_default_macaddr(mac,(dummy_pkt->data + 6));
#endif
    dummy_pkt->dev = dev;
    i = 40;

   /*
    *  Reduce the no of TX buffers to five from the actual number
    *  of allocated buffers and link the fifth descriptor to first.
    *  WAR for Bug:32681 to cause early Tx Timeout in 10BASET.
    */
    ag7100_tx_flush(mac);
    ds = mac->mac_txring.ring_desc;
    r = &mac->mac_txring;
    r->ring_nelem = 5;
    ds[r->ring_nelem - 1].next_desc = ag7100_desc_dma_addr(r, &ds[0]);
    ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_3, 0x300020);

    mac->speed_10t = 1;
    while(i-- && mac->speed_10t) {
        netif_carrier_on(dev);
        netif_start_queue(dev);

        mdelay(100);
        ag7100_hard_start(dummy_pkt,dev);

        netif_carrier_off(dev);
        netif_stop_queue(dev);
    }
    return ;
}
#endif

/*
 * Several fields need to be programmed based on what the PHY negotiated
 * Ideally we should quiesce everything before touching the pll, but:
 * 1. If its a linkup/linkdown, we dont care about quiescing the traffic.
 * 2. If its a single gigE PHY, this can only happen on lup/ldown.
 * 3. If its a 100Mpbs switch, the link will always remain at 100 (or nothing)
 * 4. If its a gigE switch then the speed should always be set at 1000Mpbs,
 *    and the switch should provide buffering for slower devices.
 *
 * XXX Only gigE PLL can be changed as a parameter for now. 100/10 is hardcoded.
 * XXX Need defines for them -
 * XXX FIFO settings based on the mode
 */
static void
ag7100_set_mac_from_link(ag7100_mac_t *mac, ag7100_phy_speed_t speed, int fdx)
{
#ifdef CONFIG_ATHRS26_PHY
    int change_flag = 0;

    if(mac->mac_speed !=  speed)
        change_flag = 1;

    if(change_flag)
    {
        athrs26_phy_off(mac);
        athrs26_mac_speed_set(mac, speed);
        athrs26_phy_on(mac);
    }
#endif
   /*
    *  Flush TX descriptors , reset the MAC and relink all descriptors.
    *  WAR for Bug:32681
    */

#if defined(CONFIG_AR9100) && defined(CONFIG_AG7100_GE1_RMII)
    if(mac->speed_10t && (speed != AG7100_PHY_SPEED_10T)) {
        mac->speed_10t = 0;
        ag7100_tx_flush(mac);
        mdelay(500);
	ag7100_dma_reset(mac);
    }
#endif

    mac->mac_speed =  speed;
    mac->mac_fdx   =  fdx;

#if !defined(CONFIG_AR7240)
    ag7100_set_mii_ctrl_speed(mac, speed);
#endif
    ag7100_set_mac_duplex(mac, fdx);
    ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_3, fifo_3);
#if !defined(CONFIG_AR9100) && !defined(CONFIG_AR7240)
    ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_5, fifo_5);
#endif

    switch (speed)
    {
        case AG7100_PHY_SPEED_1000T:
#ifdef CONFIG_AR9100
            ag7100_reg_wr(mac, AG7100_MAC_FIFO_CFG_3, 0x780fff);
#endif
            ag7100_set_mac_if(mac, 1);
#ifdef CONFIG_AR9100
		if (mac->mac_unit == 0)
		{ /* eth0 */
            ag7100_set_pll(mac, gige_pll);
		}
		else
		{
		ag7100_set_pll(mac, SW_PLL);
		}
#else
#if !defined(CONFIG_AR7240)
            ag7100_set_pll(mac, gige_pll);
#else
            if (is_ar7242() &&( mac->mac_unit == 0))
                ar7100_reg_wr(AR7242_ETH_XMII_CONFIG,xmii_val);
            if (is_ar934x() && (mac->mac_unit == 0) && gd26_ge) {
                ar7100_reg_wr(AR7242_ETH_XMII_CONFIG,0x0e000000);
                if (rks_is_gd26_board()) {
                    ar7100_reg_rmw_set(AG7240_ETH_CFG, (2 << AG7240_ETH_CFG_RXD_DELAY_SHIFT));
                    ar7100_reg_rmw_set(AG7240_ETH_CFG, (2 << AG7240_ETH_CFG_RDV_DELAY_SHIFT));
                    ar7100_reg_rmw_set(AG7240_ETH_CFG, (2 << AG7240_ETH_CFG_TXD_DELAY_SHIFT));
                    ar7100_reg_rmw_set(AG7240_ETH_CFG, (2 << AG7240_ETH_CFG_TEN_DELAY_SHIFT));
                } else if (rks_is_spaniel_board()) {
                    ar7100_reg_rmw_set(AG7240_ETH_CFG, (1 << AG7240_ETH_CFG_RXD_DELAY_SHIFT));
                    ar7100_reg_rmw_set(AG7240_ETH_CFG, (1 << AG7240_ETH_CFG_RDV_DELAY_SHIFT));
                    ar7100_reg_rmw_set(AG7240_ETH_CFG, (2 << AG7240_ETH_CFG_TXD_DELAY_SHIFT));
                    ar7100_reg_rmw_set(AG7240_ETH_CFG, (2 << AG7240_ETH_CFG_TEN_DELAY_SHIFT));
                } else {
                    /* gd36 */
                    ar7100_reg_rmw_clear(AG7240_ETH_CFG, (0xff << AG7240_ETH_CFG_RXD_DELAY_SHIFT));
                    ar7100_reg_rmw_set(AG7240_ETH_CFG, (1 << AG7240_ETH_CFG_RXD_DELAY_SHIFT));
                    ar7100_reg_rmw_set(AG7240_ETH_CFG, (1 << AG7240_ETH_CFG_RDV_DELAY_SHIFT));
                }
            }
#endif
#endif
            ag7100_reg_rmw_set(mac, AG7100_MAC_FIFO_CFG_5, (1 << 19));
            break;

        case AG7100_PHY_SPEED_100TX:
            ag7100_set_mac_if(mac, 0);
            ag7100_set_mac_speed(mac, 1);
#ifndef CONFIG_AR7100_EMULATION
#ifdef CONFIG_AR9100
            if (mac->mac_unit == 0)
            { /* eth0 */
                ag7100_set_pll(mac, 0x13000a44);
            }
            else
            {
                ag7100_set_pll(mac, SW_PLL);
            }
#else
#if !defined(CONFIG_AR7240)
            ag7100_set_pll(mac, 0x0001099);
#else
            if (is_ar7242() &&( mac->mac_unit == 0))
                ar7100_reg_wr(AR7242_ETH_XMII_CONFIG,0x0101);
            if (is_ar934x() && (mac->mac_unit == 0) && gd26_ge) {
                ar7100_reg_wr(AR7242_ETH_XMII_CONFIG,0x0101);
                if (rks_is_gd26_board()) {
                    ar7100_reg_rmw_clear(AG7240_ETH_CFG, (3 << AG7240_ETH_CFG_RXD_DELAY_SHIFT));
                    ar7100_reg_rmw_clear(AG7240_ETH_CFG, (3 << AG7240_ETH_CFG_RDV_DELAY_SHIFT));
                    ar7100_reg_rmw_clear(AG7240_ETH_CFG, (3 << AG7240_ETH_CFG_TXD_DELAY_SHIFT));
                    ar7100_reg_rmw_clear(AG7240_ETH_CFG, (3 << AG7240_ETH_CFG_TEN_DELAY_SHIFT));
                } else if (rks_is_spaniel_board()) {
                    ar7100_reg_rmw_set(AG7240_ETH_CFG, AG7240_ETH_CFG_RXD_DELAY);
                    ar7100_reg_rmw_set(AG7240_ETH_CFG, AG7240_ETH_CFG_RDV_DELAY);
                    ar7100_reg_rmw_clear(AG7240_ETH_CFG, (3 << AG7240_ETH_CFG_TXD_DELAY_SHIFT));
                    ar7100_reg_rmw_clear(AG7240_ETH_CFG, (3 << AG7240_ETH_CFG_TEN_DELAY_SHIFT));
                } else {
                    /* gd36 */
                    ar7100_reg_rmw_clear(AG7240_ETH_CFG, (0xff << AG7240_ETH_CFG_RXD_DELAY_SHIFT));
                }
            }
#endif
#endif
#endif
            ag7100_reg_rmw_clear(mac, AG7100_MAC_FIFO_CFG_5, (1 << 19));
            break;

        case AG7100_PHY_SPEED_10T:
#ifdef V54_BSP
        default:
#endif
            ag7100_set_mac_if(mac, 0);
            ag7100_set_mac_speed(mac, 0);
#ifdef CONFIG_AR9100
            if (mac->mac_unit == 0)
            { /* eth0 */
                ag7100_set_pll(mac, 0x00441099);
            }
            else
            {
                ag7100_set_pll(mac, SW_PLL);
            }
#else
#if !defined(CONFIG_AR7240)
            ag7100_set_pll(mac, 0x00991099);
#else
            if (is_ar7242() &&( mac->mac_unit == 0))
                ar7100_reg_wr(AR7242_ETH_XMII_CONFIG,0x1616);
            if (is_ar934x() && (mac->mac_unit == 0) && gd26_ge) {
                ar7100_reg_wr(AR7242_ETH_XMII_CONFIG,0x1313);
                if (rks_is_gd26_board()) {
                    ar7100_reg_rmw_clear(AG7240_ETH_CFG, (3 << AG7240_ETH_CFG_RXD_DELAY_SHIFT));
                    ar7100_reg_rmw_clear(AG7240_ETH_CFG, (3 << AG7240_ETH_CFG_RDV_DELAY_SHIFT));
                    ar7100_reg_rmw_clear(AG7240_ETH_CFG, (3 << AG7240_ETH_CFG_TXD_DELAY_SHIFT));
                    ar7100_reg_rmw_clear(AG7240_ETH_CFG, (3 << AG7240_ETH_CFG_TEN_DELAY_SHIFT));
                } else if (rks_is_spaniel_board()) {
                    ar7100_reg_rmw_set(AG7240_ETH_CFG, AG7240_ETH_CFG_RXD_DELAY);
                    ar7100_reg_rmw_set(AG7240_ETH_CFG, AG7240_ETH_CFG_RDV_DELAY);
                    ar7100_reg_rmw_clear(AG7240_ETH_CFG, (3 << AG7240_ETH_CFG_TXD_DELAY_SHIFT));
                    ar7100_reg_rmw_clear(AG7240_ETH_CFG, (3 << AG7240_ETH_CFG_TEN_DELAY_SHIFT));
                } else {
                    /* gd36 */
                    ar7100_reg_rmw_clear(AG7240_ETH_CFG, (0xff << AG7240_ETH_CFG_RXD_DELAY_SHIFT));
                }
            }
#endif
#endif
#if defined(CONFIG_AR9100) && defined(CONFIG_AG7100_GE1_RMII)
        if((speed == AG7100_PHY_SPEED_10T) && !mac->speed_10t) {
           howl_10baset_war(mac);
        }
#endif
            ag7100_reg_rmw_clear(mac, AG7100_MAC_FIFO_CFG_5, (1 << 19));
            break;

#ifndef V54_BSP
        default:
            assert(0);
#endif
    }
#if 0
#ifdef CONFIG_ATHRS26_PHY
    if(change_flag)
        athrs26_phy_on(mac);
#endif
#endif
#if !defined(V54_BSP)
    printk(MODULE_NAME ": cfg_1: %#x\n", ag7100_reg_rd(mac, AG7100_MAC_FIFO_CFG_1));
    printk(MODULE_NAME ": cfg_2: %#x\n", ag7100_reg_rd(mac, AG7100_MAC_FIFO_CFG_2));
    printk(MODULE_NAME ": cfg_3: %#x\n", ag7100_reg_rd(mac, AG7100_MAC_FIFO_CFG_3));
    printk(MODULE_NAME ": cfg_4: %#x\n", ag7100_reg_rd(mac, AG7100_MAC_FIFO_CFG_4));
    printk(MODULE_NAME ": cfg_5: %#x\n", ag7100_reg_rd(mac, AG7100_MAC_FIFO_CFG_5));
#else
    printk(MODULE_NAME ": cfg_1:%#x cfg_2:%#x cfg_3:%#x cfg_4:%#x cfg_5:%#x\n",
        ag7100_reg_rd(mac, AG7100_MAC_FIFO_CFG_1),
        ag7100_reg_rd(mac, AG7100_MAC_FIFO_CFG_2),
        ag7100_reg_rd(mac, AG7100_MAC_FIFO_CFG_3),
        ag7100_reg_rd(mac, AG7100_MAC_FIFO_CFG_4),
        ag7100_reg_rd(mac, AG7100_MAC_FIFO_CFG_5));
#endif
}

#ifdef ETH_SOFT_LED
static int
led_control_func(ATH_LED_CONTROL *pledctrl)
{
    uint32_t i=0,cnt,reg_addr;
    const LED_BLINK_RATES  *bRateTab;
    static uint32_t pkt_count;
    ag7240_mac_t *mac;

    if (is_ar7240()) {
        mac = ag7240_macs[1];
        atomic_inc(&Ledstatus);

        /*
         *  MDIO access will fail While PHY is in RESET phase.
         */
        if(phy_in_reset)
              goto done;

        if (mac->mac_ifup) {
            for (i = 0 ; i < 4 ; i ++) {
                bRateTab = BlinkRateTable_100M;
                if (pledctrl->ledlink[i]) {
                    reg_addr = 0x2003c + ((i + 1) << 8);
                    /* Rx good byte count */
                    cnt = athrs26_reg_read(reg_addr);

                    reg_addr = 0x20084 + ((i + 1) << 8);
                    /* Tx good byte count */
                    cnt = cnt + athrs26_reg_read(reg_addr);

                    if (cnt == 0) {
                        s26_wr_phy(i,0x19,(s26_rd_phy(i,0x19) | (0x3c0)));
                        continue;
                    }
                    if (pledctrl->speed[i] == AG7240_PHY_SPEED_10T) {
                        bRateTab = BlinkRateTable_10M;
                    }
                    while (bRateTab++) {
                        if (cnt <= bRateTab->rate) {
                            s26_wr_phy(i,0x18,((bRateTab->timeOn << 12)|
                                (bRateTab->timeOff << 8)));
                            s26_wr_phy(i,0x19,(s26_rd_phy(i,0x19) & ~(0x280)));
                            break;
                        }
                    }
                } else {
                    s26_wr_phy(i,0x19,0x0);
                }
            }
            /* Flush all LAN MIB counters */
            athrs26_reg_write(0x80,((1 << 17) | (1 << 24)));
            while ((athrs26_reg_read(0x80) & 0x20000) == 0x20000);
            athrs26_reg_write(0x80,0);
        }

        mac = ag7240_macs[0];
        bRateTab = BlinkRateTable_100M;
        cnt = 0;
        if (mac->mac_ifup) {
            if (pledctrl->ledlink[4]) {
                cnt += ag7240_reg_rd(mac,AG7240_RX_BYTES_CNTR) +
                               ag7240_reg_rd(mac,AG7240_TX_BYTES_CNTR);

                if (ag7240_get_diff(pkt_count,cnt) == 0) {
                    s26_wr_phy(4,0x19,(s26_rd_phy(4,0x19) | (0x3c0)));
                    goto done;
                }
                if (pledctrl->speed[4] == AG7240_PHY_SPEED_10T) {
                    bRateTab = BlinkRateTable_10M;
                }
                while (bRateTab++) {
                    if (ag7240_get_diff(pkt_count,cnt) <= bRateTab->rate) {
                        s26_wr_phy(4,0x18,((bRateTab->timeOn << 12)|
                            (bRateTab->timeOff << 8)));
                        s26_wr_phy(4,0x19,(s26_rd_phy(4,0x19) & ~(0x280)));
                        break;
                    }
                }
                pkt_count = cnt;
            } else {
                s26_wr_phy(4,0x19,0x0);
            }
        }
    }
done:
#ifdef CHECK_DMA_STATUS
    if(ag7240_get_diff(prev_ts,jiffies) >= (1*HZ / 2)) {
        if (ag7240_macs[0]->mac_ifup) check_for_dma_status(ag7240_macs[0]);
        if (ag7240_macs[1]->mac_ifup) check_for_dma_status(ag7240_macs[1]);
        prev_ts = jiffies;
    }
#endif
    mod_timer(&PLedCtrl.led_timer,(jiffies + AG7240_LED_POLL_SECONDS));
    atomic_dec(&Ledstatus);
    return 0;
}
#endif

#ifdef DMA_DEBUG
static int check_dma_status_pause(ag7240_mac_t *mac) {

    int RxFsm,TxFsm,RxFD,RxCtrl,TxCtrl;

    RxFsm = ag7100_reg_rd(mac,AG7240_DMA_RXFSM);
    TxFsm = ag7100_reg_rd(mac,AG7240_DMA_TXFSM);
    RxFD  = ag7100_reg_rd(mac,AG7240_DMA_XFIFO_DEPTH);
    RxCtrl = ag7100_reg_rd(mac,AG7100_DMA_RX_CTRL);
    TxCtrl = ag7100_reg_rd(mac,AG7100_DMA_TX_CTRL);


    if ((RxFsm & AG7240_DMA_DMA_STATE) == 0x3
        && ((RxFsm >> 4) & AG7240_DMA_AHB_STATE) == 0x6) {
        return 0;
    }
    else if ((((TxFsm >> 4) & AG7240_DMA_AHB_STATE) <= 0x0) &&
             ((RxFsm & AG7240_DMA_DMA_STATE) == 0x0) &&
             (((RxFsm >> 4) & AG7240_DMA_AHB_STATE) == 0x0) &&
             (RxFD  == 0x0) && (RxCtrl == 1) && (TxCtrl == 1)) {
        return 0;
    }
    else if (((((TxFsm >> 4) & AG7240_DMA_AHB_STATE) <= 0x4) &&
            ((RxFsm & AG7240_DMA_DMA_STATE) == 0x0) &&
            (((RxFsm >> 4) & AG7240_DMA_AHB_STATE) == 0x0)) ||
            (((RxFD >> 16) <= 0x20) && (RxCtrl == 1)) ) {
        return 1;
    }
    else {
        printk(" FIFO DEPTH = %x",RxFD);
        printk(" RX DMA CTRL = %x",RxCtrl);
        printk("mac:%d RxFsm:%x TxFsm:%x\n",mac->mac_unit,RxFsm,TxFsm);
        return 2;
    }
}

static inline uint32_t
ag7100_get_diff(uint32_t t1,uint32_t t2)
{
    return (t1 > t2 ? (0xffffffff - (t1 - t2)) : t2 - t1);
}

int check_for_dma_hang(ag7100_mac_t *mac) {

    ag7100_ring_t   *r     = &mac->mac_txring;
    int             head  = r->ring_head, tail = r->ring_tail;
    ag7100_desc_t   *ds;
    ag7100_buffer_t *bp;

    if (check_dma_enabled) {
    ar7100_flush_ge(mac->mac_unit);

    while(tail != head)
    {
        ds   = &r->ring_desc[tail];
        bp   =  &r->ring_buffer[tail];

        if(ag7100_tx_owned_by_dma(ds)) {
                if ((ag7100_get_diff(bp->trans_start,jiffies)) > (1 * HZ/10)) {

                /*
                 * If the DMA is in pause state reset kernel watchdog timer
                 */

                if(check_dma_status_pause(mac)) {
                   mac->mac_dev->trans_start = jiffies;
                   return 0;
                }
                printk(MODULE_NAME ": Tx Dma status mac%d: %s\n", mac->mac_unit,
                    ag7100_tx_stopped(mac) ? "inactive" : "active");

                    // walle: can't reset mac1, ports are virtualized
                    if (ag7100_macs[0] == mac) {
                        mac->dma_check = 1;
                ag7100_dma_reset(mac);
                return 1;
           }
        }
            }
        ag7100_ring_incr(tail);
        }
    }
    if (check_rx_fifo_enabled) {
        if (ag7100_macs[0] == mac) {
            if (ag7100_reg_rd(mac, AG7100_MAC_FIFO_CFG_5) & 0x200000) {
                mac->sw_stats.stat_rx_ff_err++;
                mac->dma_check = 1;
                ag7100_dma_reset(mac);
                return 1;
            }
        }
    }
    mod_timer(&mac->mac_dbg_timer, jiffies + (1*HZ/2));
    return 0;
}
#endif

/*
 * phy link state management
 */
int ag7100_check_link(ag7100_mac_t *mac, int phyUnit)
{
    struct net_device  *dev     = mac->mac_dev;
    int                 carrier = netif_carrier_ok(dev), fdx, phy_up;
    ag7100_phy_speed_t  speed;
    int                 rc = -1;

#if defined(V54_BSP) && defined(CONFIG_MV_PHY)
    phy_up = fdx = speed = 0;
#endif

    /* The vitesse switch uses an indirect method to communicate phy status
    * so it is best to limit the number of calls to what is necessary.
    * However a single call returns all three pieces of status information.
    *
    * This is a trivial change to the other PHYs ergo this change.
    *
    */
#if defined(CONFIG_VENETDEV)
    if (venet_enable(mac)) {
        int                 phy_up, fdx, i, rc=-1;
        ag7100_phy_speed_t  speed;

        if (rks_is_gd11_board()) {
            rc = ag7100_get_link_status_1(mac->mac_unit, &phy_up, &fdx, &speed);
        } else  if (rks_is_gd17_board()) {
            /* gd17 */
            rc = ag7100_get_link_status(mac->mac_unit, &phy_up, &fdx, &speed);
        } else  if (rks_is_walle_board()) {
            /* walle */
            rc = ag7100_get_link_status_s26(mac->mac_unit, &phy_up, &fdx, &speed, phyUnit);
            //printk("%s: rc(%d) mac_unit(%d) phy_up(0x%x) fdx(%d) speed(%d) phyUint(%d)\n",
            //    __func__, rc, mac->mac_unit, phy_up, fdx, speed, phyUnit);
        }
        if (rc < 0)
            goto done;

        if (rks_is_gd11_board()) {
            for (i=0; i<dev_per_mac; i++) {
                if (mac->vdev_state[i].up) {
                    current_conn[i].up = (phy_up & (LINK_MASK << i)) ? 1 : 0;
                    current_conn[i].speed = (speed >> (SPEED_SHIFT * i)) & SPEED_MASK;
                    current_conn[i].fdx = (fdx >> i) & DUPLEX_MASK;
                    if (!(phy_up & (LINK_MASK << i))) {
                        if (mac->vdev_state[i].carrier) {
                            netif_carrier_off(mac->vdev_state[i].dev);
                            netif_stop_queue(mac->vdev_state[i].dev);
                            mac->vdev_state[i].carrier = 0;
                            printk("ag7100 link: vdev %x down\n",
                                (unsigned int)mac->vdev_state[i].dev);
                        }
                    } else if (mac->vdev_state[i].carrier == 0) {
                        netif_carrier_on(mac->vdev_state[i].dev);
                        netif_start_queue(mac->vdev_state[i].dev);
                        mac->vdev_state[i].carrier = 1;
                        printk("ag7100 link: vdev %x up, speed(%d) fdx(%d)\n",
                            (unsigned int)mac->vdev_state[i].dev,
                            ((speed >> (SPEED_SHIFT * i)) & SPEED_MASK),
                            ((fdx >> i) & DUPLEX_MASK));
                    }
                }
            }
        } else if (rks_is_gd17_board()) {
            for (i=0; i<dev_per_mac; i++) {
                int linkMask;
                if (i)
                    linkMask = 0x10;
                else
                    linkMask = 0x0f;

                if (mac->vdev_state[i].up) {
                    if (!(phy_up & linkMask)) {
                        if (mac->vdev_state[i].carrier) {
                            netif_carrier_off(mac->vdev_state[i].dev);
                            netif_stop_queue(mac->vdev_state[i].dev);
                            mac->vdev_state[i].carrier = 0;
                            printk("ag7100 link: vdev %x down\n",
                                (unsigned int)mac->vdev_state[i].dev);
                        }
                    } else if (mac->vdev_state[i].carrier == 0) {
                        netif_carrier_on(mac->vdev_state[i].dev);
                        netif_start_queue(mac->vdev_state[i].dev);
                        mac->vdev_state[i].carrier = 1;
                        printk("ag7100 link: vdev %x up, speed(0x%x) fdx(0x%x)\n",
                            (unsigned int)mac->vdev_state[i].dev,
                            (speed & (i?0xff:0x300)), (fdx & linkMask));
                    }
                }
            }
            speed = AG7100_PHY_SPEED_1000T;
            fdx = 1;
        } else if (rks_is_walle_board()) {
            current_conn[phyUnit].up = (phy_up & (1 << phyUnit)) ? 1 : 0;
            current_conn[phyUnit].speed = speed;
            current_conn[phyUnit].fdx = fdx;
            if (mac->vdev_state[phyUnit].up) {
                if (!(phy_up & (1 << phyUnit))) {
                    if (mac->vdev_state[phyUnit].carrier) {
                        netif_carrier_off(mac->vdev_state[phyUnit].dev);
                        netif_stop_queue(mac->vdev_state[phyUnit].dev);
                        mac->vdev_state[phyUnit].carrier = 0;
                        printk("ag7100 link: vdev %x down\n",
                            (unsigned int)mac->vdev_state[phyUnit].dev);
                    }
                } else if (mac->vdev_state[phyUnit].carrier == 0) {
                    netif_carrier_on(mac->vdev_state[phyUnit].dev);
                    netif_start_queue(mac->vdev_state[phyUnit].dev);
                    mac->vdev_state[phyUnit].carrier = 1;
                    printk("ag7100 link: vdev %x up, speed(%d) fdx(%d)\n",
                        (unsigned int)mac->vdev_state[phyUnit].dev,
                        (speed & SPEED_MASK), (fdx & DUPLEX_MASK));
                }
                speed = AG7100_PHY_SPEED_1000T;
                fdx = 1;
            }
        }

        goto done;
    }
#endif
#if defined(V54_BSP) && defined(CONFIG_MV_PHY)
    if (rks_is_gd11_board() && (mac->mac_unit)) {
        rc = ag7100_get_link_status_1(mac->mac_unit, &phy_up, &fdx, &speed);
        /* gd11: set to 100Mb full if not venet_enable */
        fdx = 1;
        speed = AG7100_PHY_SPEED_100TX;
    } else
#endif
    if (rks_is_walle_board()) {
        rc = ag7100_get_link_status_s26(mac->mac_unit, &phy_up, &fdx, &speed, phyUnit);
    } else if (rks_is_gd26_board() || rks_is_spaniel_board() || rks_is_gd36_board()) {
        if (!mac->mac_unit && gd26_ge) {
            rc = ag7100_get_link_status(mac->mac_unit, &phy_up, &fdx, &speed);
            /* put ge infromation in current_conn[4] */
            phyUnit = 4;
            phy_up <<= 4;
        } else {
            if (!rks_is_spaniel_board())
                rc = ag7100_get_link_status_s26(mac->mac_unit, &phy_up, &fdx, &speed, phyUnit);
        }
    } else {
        rc = ag7100_get_link_status(mac->mac_unit, &phy_up, &fdx, &speed);
    }
    if (rc < 0)
        goto done;

    if (rks_is_walle_board() || rks_is_gd26_board() || rks_is_spaniel_board() || rks_is_gd36_board()) {
        //printk("unit(%d) up(0x%x) fdx(0x%x) speed(0x%x) phyUnit(%d)\n", mac->mac_unit, phy_up, fdx, speed, phyUnit);
        current_conn[phyUnit].up = (phy_up & (1 << phyUnit)) ? 1 : 0;
        current_conn[phyUnit].speed = speed;
        current_conn[phyUnit].fdx = fdx;
        if (!rks_is_spaniel_board()) {
            if (mac->mac_unit) {
                phy_up &= 0xf;
                if (phy_up) {
                    /* mac1: phy 0-3 */
                    fdx = 1;
                    speed = AG7100_PHY_SPEED_1000T;
                }
            } else {
                if (!gd26_ge)
                    phy_up &= 0x10;
            }
        }
    } else if (rks_is_gd11_board()) {
        if (mac->mac_unit) {
            current_conn[0].up = (phy_up & LINK_MASK) ? 1 : 0;
            current_conn[0].speed = (speed >> SPEED_SHIFT) & SPEED_MASK;
            current_conn[0].fdx = fdx & DUPLEX_MASK;
        } else {
            current_conn[2].up = phy_up ? 1 : 0;
            current_conn[2].speed = speed;
            current_conn[2].fdx = fdx;
        }
    } else {
        current_conn[mac->mac_unit].up = phy_up ? 1 : 0;
        current_conn[mac->mac_unit].speed = speed;
        current_conn[mac->mac_unit].fdx = fdx;
    }

    if (!phy_up)
    {
        if (carrier)
        {
            printk(MODULE_NAME ": unit %d: phy not up carrier %d\n", mac->mac_unit, carrier);
            netif_carrier_off(dev);
            netif_stop_queue(dev);
        }
        goto done;
    }

    /*
     * phy is up. Either nothing changed or phy setttings changed while we
     * were sleeping.
     */

    if ((fdx < 0) || (speed < 0))
    {
        printk(MODULE_NAME ": phy not connected?\n");
#ifdef V54_BSP
        goto done;
#else
        return 0;
#endif
    }

    if (carrier && (speed == mac->mac_speed) && (fdx == mac->mac_fdx))
        goto done;

    printk(MODULE_NAME ": unit %d phy is up...", mac->mac_unit);
    printk("%s %s %s\n", mii_str[mac->mac_unit][mii_if(mac)],
                         spd_str[speed], dup_str[fdx]);

    ag7100_set_mac_from_link(mac, speed, fdx);

#if 0 && defined(CONFIG_AR7240)
    if (is_ar7240())
        mac_fast_reset(mac, NULL);
#endif

#if !defined(CONFIG_AR7240)
    printk(MODULE_NAME ": done cfg2 %#x ifctl %#x miictrl %#x \n",
            ag7100_reg_rd(mac, AG7100_MAC_CFG2),
            ag7100_reg_rd(mac, AG7100_MAC_IFCTL),
            ar7100_reg_rd(mii_reg(mac)));
#else
    printk(MODULE_NAME ": done cfg2 %#x ifctl %#x miictrl  \n",
           ag7100_reg_rd(mac, AG7100_MAC_CFG2),
           ag7100_reg_rd(mac, AG7100_MAC_IFCTL));
#endif

    /*
     * in business
     */
    netif_carrier_on(dev);
    netif_start_queue(dev);

done:
#ifdef V54_BSP
    if ( ag7100_num_tx_unreap > 0 ) {
        // check to see if we need to reap.
        mac->sw_stats.stat_tx += ag7100_tx_reap(mac);
    }
#endif

    if ((!rks_is_walle_board() && !rks_is_gd26_board() && !rks_is_spaniel_board() && !rks_is_gd36_board()) ||
        ((rks_is_gd26_board() || rks_is_spaniel_board() || rks_is_gd36_board()) && !mac->mac_unit && gd26_ge))
        mod_timer(&mac->mac_phy_timer, jiffies + AG7100_PHY_POLL_SECONDS*HZ);


    return 0;
}

static void
ag7100_choose_phy(uint32_t phy_addr)
{
#ifdef CONFIG_AR7100_EMULATION
    if (phy_addr == 0x10)
    {
        ar7100_reg_rmw_set(AR7100_MII0_CTRL, (1 << 6));
    }
    else
    {
        ar7100_reg_rmw_clear(AR7100_MII0_CTRL, (1 << 6));
    }
#endif
}

uint16_t
ag7100_mii_read(int unit, uint32_t phy_addr, uint8_t reg)
{
    ag7100_mac_t *mac   = ag7100_unit2mac(0);
    uint16_t      addr  = (phy_addr << AG7100_ADDR_SHIFT) | reg, val;
    volatile int           rddata;
    uint16_t      ii = 0x1000;

    if (is_ar934x())
        mac = ag7100_unit2mac(unit);

    ag7100_choose_phy(phy_addr);

    ag7100_reg_wr(mac, AG7100_MII_MGMT_CMD, 0x0);
    ag7100_reg_wr(mac, AG7100_MII_MGMT_ADDRESS, addr);
    ag7100_reg_wr(mac, AG7100_MII_MGMT_CMD, AG7100_MGMT_CMD_READ);

    do
    {
        udelay(5);
        rddata = ag7100_reg_rd(mac, AG7100_MII_MGMT_IND) & 0x1;
    }while(rddata && --ii);

    val = ag7100_reg_rd(mac, AG7100_MII_MGMT_STATUS);
    ag7100_reg_wr(mac, AG7100_MII_MGMT_CMD, 0x0);

    return val;
}

void
ag7100_mii_write(int unit, uint32_t phy_addr, uint8_t reg, uint16_t data)
{
    ag7100_mac_t *mac   = ag7100_unit2mac(0);
    uint16_t      addr  = (phy_addr << AG7100_ADDR_SHIFT) | reg;
    volatile int rddata;
    uint16_t      ii = 0x1000;

    if (is_ar934x())
        mac = ag7100_unit2mac(unit);

    ag7100_choose_phy(phy_addr);

    ag7100_reg_wr(mac, AG7100_MII_MGMT_ADDRESS, addr);
    ag7100_reg_wr(mac, AG7100_MII_MGMT_CTRL, data);

    do
    {
        rddata = ag7100_reg_rd(mac, AG7100_MII_MGMT_IND) & 0x1;
    }while(rddata && --ii);
}

/*
 * Tx operation:
 * We do lazy reaping - only when the ring is "thresh" full. If the ring is
 * full and the hardware is not even done with the first pkt we q'd, we turn
 * on the tx interrupt, stop all q's and wait for h/w to
 * tell us when its done with a "few" pkts, and then turn the Qs on again.
 *
 * Locking:
 * The interrupt only touches the ring when Q's stopped  => Tx is lockless,
 * except when handling ring full.
 *
 * Desc Flushing: Flushing needs to be handled at various levels, broadly:
 * - The DDr FIFOs for desc reads.
 * - WB's for desc writes.
 */
static void
ag7100_handle_tx_full(ag7100_mac_t *mac)
{
    unsigned long         flags;
#ifdef V54_BSP
    mac->sw_stats.stat_tx_full++;
#endif
#if defined(CONFIG_AR9100) && defined(CONFIG_AG7100_GE1_RMII)
    if(!mac->speed_10t)
#endif
#ifndef V54_BSP
    /* check this at ag7100_hard_start */
    assert(!netif_queue_stopped(mac->mac_dev));
#endif

    mac->mac_net_stats.tx_fifo_errors ++;

#if defined(CONFIG_VENETDEV)
    if (venet_enable(mac)) {
        int i;
        for(i=0; i<dev_per_mac; i++)
            netif_stop_queue(mac->vdev_state[i].dev);
    } else {
        netif_stop_queue(mac->mac_dev);
    }
#else
    netif_stop_queue(mac->mac_dev);
#endif

    spin_lock_irqsave(&mac->mac_lock, flags);
    ag7100_intr_enable_tx(mac);
    spin_unlock_irqrestore(&mac->mac_lock, flags);
}

/* ******************************
 *
 * Code under test - do not use
 *
 * ******************************
 */

static ag7100_desc_t *
ag7100_get_tx_ds(ag7100_mac_t *mac, int *len, unsigned char **start)
{
    ag7100_desc_t      *ds;
    int                len_this_ds;
    ag7100_ring_t      *r   = &mac->mac_txring;
#ifdef DMA_DEBUG
    ag7100_buffer_t    *bp;
#endif

    /* force extra pkt if remainder less than 8 bytes */
    if (*len > tx_len_per_ds)
        if (*len < (tx_len_per_ds + 8))
            len_this_ds = tx_len_per_ds - 8;
        else
            len_this_ds = tx_len_per_ds;
    else
        len_this_ds    = *len;

    ds = &r->ring_desc[r->ring_head];

    ag7100_trc_new(ds,"ds addr");
    ag7100_trc_new(ds,"ds len");
#if defined(CONFIG_AR9100) || defined(CONFIG_AG7100_DMA_HANG_FIX)
    if(ag7100_tx_owned_by_dma(ds))
        ag7100_dma_reset(mac);
#else
    assert(!ag7100_tx_owned_by_dma(ds));
#endif

    ds->pkt_size       = len_this_ds;
    ds->pkt_start_addr = virt_to_phys(*start);
    ds->more           = 1;

    *len   -= len_this_ds;
    *start += len_this_ds;
#ifdef DMA_DEBUG
    if (rks_is_walle_board()) {
        bp = &r->ring_buffer[r->ring_head];
        bp->trans_start = jiffies; /*Time stamp each packet */
    }
#endif

    ag7100_ring_incr(r->ring_head);

    return ds;
}
struct pause_fr {
	struct ethhdr eth_hdr;
	uint16_t opcode;
	uint16_t control;
	char *data;
};
void pkt_dump(char *data,int pkt_size) {


    int i;
    printk("**********Packet dump******* %p size:%d\n", data, pkt_size);
    for(i=0;i<=pkt_size;i++) {
       printk("%x:", (unsigned)(data[i]&0xff));
       if(!(i%20))
          printk("\n");
    }
    printk("\n");
    printk("***********end***************\n");
}

int pause_frame(char *data)
{
	struct pause_fr *p_fr;
	p_fr = (struct pause_fr *)data;
	if(p_fr->eth_hdr.h_proto == 0x8808) {
		printk("%s opcode:%x control:%x\n",__func__,p_fr->opcode,p_fr->control);
		return 1;
       }
	return 0;
}
int
ag7100_hard_start(struct sk_buff *skb, struct net_device *dev)
{
    ag7100_mac_t       *mac     = *(ag7100_mac_t **)netdev_priv(dev);
    ag7100_ring_t      *r       = &mac->mac_txring;
    ag7100_buffer_t    *bp;
#ifdef V54_BSP
    ag7100_desc_t      *ds, *fds = NULL;
#else
    ag7100_desc_t      *ds, *fds;
#endif
    unsigned char      *start;
    int                len;
    int                nds_this_pkt;
#if defined(CONFIG_VENETDEV)
    struct vdev_sw_state    *vdev_state = NULL;
    if (venet_enable(mac))
        vdev_state = get_vdev_sw_state(dev);
#endif

#ifdef V54_BSP
    if (netif_queue_stopped(dev)) {
        static int qsCnt;
        printk("dev %x q stopped %d ", (unsigned int)dev, ++qsCnt);
        goto dropit;
    }
#endif

#ifdef VSC73XX_DEBUG
    {
        static int vsc73xx_dbg;
        if (vsc73xx_dbg == 0) {
            vsc73xx_get_link_status_dbg();
            vsc73xx_dbg = 1;
        }
        vsc73xx_dbg = (vsc73xx_dbg + 1) % 10;
    }
#endif

#if defined(CONFIG_ATHRS26_PHY) && defined(HEADER_EN)
    /* add header to normal frames */
    /* check if normal frames */
    if ((mac->mac_unit == 0) && (!((skb->cb[0] == 0x7f) && (skb->cb[1] == 0x5d))))
    {
        skb_push(skb, HEADER_LEN);
        skb->data[0] = 0x10; /* broadcast = 0; from_cpu = 0; reserved = 1; port_num = 0 */
        skb->data[1] = 0x80; /* reserved = 0b10; priority = 0; type = 0 (normal) */
    }

#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
    if (unlikely((skb->len <= 0)
        || (skb->len > (dev->mtu + ETH_HLEN + HEADER_LEN + 4))))
    { /* vlan tag length = 4 */
        printk(MODULE_NAME ": [%d] bad skb, dev->mtu=%d,ETH_HLEN=%d len %d\n", mac->mac_unit, dev->mtu, ETH_HLEN,  skb->len);
        goto dropit;
    }
#else
    if (unlikely((skb->len <= 0)
        || (skb->len > (dev->mtu + ETH_HLEN + HEADER_LEN))))
    {
        printk(MODULE_NAME ": [%d] bad skb, dev->mtu=%d,ETH_HLEN=%d len %d\n", mac->mac_unit, dev->mtu, ETH_HLEN,  skb->len);
        goto dropit;
    }
#endif

#else
#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
    if (unlikely((skb->len <= 0) || (skb->len > (dev->mtu + ETH_HLEN + 4))))
    { /* vlan tag length = 4 */
        printk(MODULE_NAME ": bad skb, len %d\n", skb->len);
        goto dropit;
    }
#else
    if (unlikely((skb->len <= 0) || (skb->len > (dev->mtu + ETH_HLEN))))
    {
        printk(MODULE_NAME ": bad skb, len %d\n", skb->len);
        goto dropit;
    }
#endif
#endif

#if defined(CONFIG_VENETDEV)
    if (venet_enable(mac)) {
        int pad         = 0;
        int alloc_skb   = 0;
        int header_len  = 0;
        int headroom, tailroom;
        u16 tag;
        struct sk_buff *new_skb;

        headroom = skb_headroom(skb);
        tailroom = skb_tailroom(skb);

#if VENETDEV_DEBUG
        printk("tx skb dev=0x%x dev=0x%x\n", (u32)skb->dev, (u32)dev);
        printk("tx skb len=%d headroom=%d tailroom=%d data=0x%x\n",
            skb->len, headroom, tailroom, (u32)skb->data);
        if (0) {
            int cnt;
            for (cnt = 0; cnt < skb->len; cnt++) {
                if (cnt && !(cnt % 16))
                    printk("\n");
                printk("%02x ", *(skb->data + cnt));
            }
            printk("\n");
        }
#endif
        if (rks_is_gd11_board())
            header_len = RTL_MGNT_TAG_LEN;
        else
            header_len = ATHR_MGNT_TAG_LEN*2;

        if (skb_cloned(skb))
            alloc_skb++;

        if (skb->len < 60)
            pad = 60 - skb->len;

        if (headroom < header_len) {
            headroom += header_len;
            alloc_skb++;
        }

        if (tailroom < pad) {
            tailroom += pad;
            alloc_skb++;
        }

        if (alloc_skb) {
            new_skb = skb_copy_expand(skb, headroom, tailroom, GFP_ATOMIC);

            if (new_skb) {
                dev_kfree_skb(skb);
                skb = new_skb;
            } else {
                printk("Cannot alloc skb\n");
                goto dropit;
            }
#if VENETDEV_DEBUG
            printk("tx new skb len=%d headroom=%d tailroom=%d data=0x%x\n",
                   skb->len, skb_headroom(skb), skb_tailroom(skb), (u32)skb->data);
#endif
        }
        if (pad) {
            skb_put(skb, pad);
            memset((skb->data + skb->len - pad), 0, pad);
        }

#if VENETDEV_DEBUG
        printk("tx padded&fcs skb len=%d headroom=%d tailroom=%d data=0x%x\n",
            skb->len, skb_headroom(skb), skb_tailroom(skb), (u32)skb->data);
        if (0) {
            int cnt;
            for (cnt = 0; cnt < skb->len; cnt++) {
                if (cnt && !(cnt % 16))
                    printk("\n");
                printk("%02x ", *(skb->data + cnt));
            }
            printk("\n");
        }
#endif

        if (rks_is_gd11_board()) {
            skb_push(skb, RTL_MGNT_TAG_LEN);
            memcpy(skb->data, (skb->data + RTL_MGNT_TAG_LEN), (ETH_ALEN * 2));
            *(u16 *)(skb->data + (ETH_ALEN * 2)) = RTL_TYPE;
            tag = (RTL_PROTOCOL << RTL_PROTOCOL_SHIFT) | (vdev_state->unit_on_MAC + 1);
            *(u16 *)(skb->data + (ETH_ALEN * 2) + RTL_TYPE_LEN) = tag;
        } else if (rks_is_gd17_board()) {
            /* gd17 */
            skb_push(skb, ATHR_MGNT_TAG_LEN);
            memcpy(skb->data, (skb->data + ATHR_MGNT_TAG_LEN), (ETH_ALEN * 2));
            *(u16 *)(skb->data + (ETH_ALEN * 2)) = ATHR_VER_PRI_TYPE_CPU |
                    (vdev_state->unit_on_MAC ? 0x40 : 0x1e);
        } else if (rks_is_walle_board()) {
            skb_push(skb, ATHR_MGNT_TAG_LEN);
            *(skb->data) = AR7240_CPU_VER | (vdev_state->unit_on_MAC + 1);
            *(skb->data + 1) = AR7240_VER_PRI_TYPE;
        }

#if VENETDEV_DEBUG
        printk("tx unit_on_MAC=%d\n", vdev_state->unit_on_MAC);
        printk("tx tagged skb len=%d headroom=%d tailroom=%d data=0x%x\n",
            skb->len, skb_headroom(skb), skb_tailroom(skb), (u32)skb->data);
        if (1) {
            int cnt;
            for (cnt = 0; cnt < skb->len; cnt++) {
                if (cnt && !(cnt % 16))
                    printk("\n");
                printk("%02x ", *(skb->data + cnt));
            }
            printk("\n");
        }
#endif
    }
#endif

    if (ag7100_tx_reap_thresh(mac))
#ifdef V54_BSP
        mac->sw_stats.stat_tx +=
#endif
        ag7100_tx_reap(mac);

    ag7100_trc_new(r->ring_head, "hard-stop hd");
    ag7100_trc_new(r->ring_tail, "hard-stop tl");

    ag7100_trc_new(skb->len,    "len this pkt");
    ag7100_trc_new(skb->data,   "ptr 2 pkt");

    dma_cache_wback((unsigned long)skb->data, skb->len);

    bp                  = &r->ring_buffer[r->ring_head];
    bp->buf_pkt         = skb;
    len                 = skb->len;
    start               = skb->data;

#ifdef V54_BSP
    if (len <=4)
    {
        printk("ag7100 tx len <= 4\n");
        goto dropit;
    }
#else
    assert(len>4);
#endif

    nds_this_pkt = 1;
    fds = ds = ag7100_get_tx_ds(mac, &len, &start);

    while (len>0)
    {
        ds = ag7100_get_tx_ds(mac, &len, &start);
        nds_this_pkt++;
        ag7100_tx_give_to_dma(ds);
    }

    ds->more        = 0;
    ag7100_tx_give_to_dma(fds);

    bp->buf_lastds  = ds;
    bp->buf_nds     = nds_this_pkt;

    ag7100_trc_new(ds,"last ds");
    ag7100_trc_new(nds_this_pkt,"nmbr ds for this pkt");

    wmb();

#ifdef V54_BSP
    ag7100_num_tx_unreap++;
#endif
    mac->net_tx_packets ++;
    mac->net_tx_bytes += skb->len;
    TX_BROADCAST_STATS((&mac->mac_net_stats), skb, 1);

#if defined(CONFIG_VENETDEV)
    if (venet_enable(mac)) {
        if (vdev_state) {
            vdev_state->net_stats.tx_packets++;
            vdev_state->net_stats.tx_bytes += skb->len;
            TX_BROADCAST_STATS((&vdev_state->net_stats), skb, 1);
        }
    }
#endif
    ag7100_trc(ag7100_reg_rd(mac, AG7100_DMA_TX_CTRL),"dma idle");

    ag7100_tx_start(mac);
#if 0
    if(mac->mac_unit && (mac->net_tx_packets > 150)) {
	   printk("************STOPPING DMA*************\n");
       ag7100_tx_stop(mac);
       return NETDEV_TX_OK;
    }
    else
        ag7100_tx_start(mac);
#endif


    if (unlikely(ag7100_tx_ring_full(mac))){
#if 0
	if(mac->mac_unit)
		printk(MODULE_NAME" TX ring FULL %s\n",__func__);
#endif
#if defined(CONFIG_VENETDEV)
        if (venet_enable(mac)) {
            if (vdev_state) {
                vdev_state->net_stats.tx_fifo_errors++;
            }
        }
#endif
        ag7100_handle_tx_full(mac);
    }
    dev->trans_start = jiffies;

#ifdef V54_BSP
    if ( ag7100_num_tx_unreap > ag7100_max_tx_unreap ) {
        mac->sw_stats.stat_tx += ag7100_tx_reap(mac);
    }
#endif
    return NETDEV_TX_OK;

dropit:
#ifdef V54_BSP
    printk("dropping skb %#x\n", (u32)skb);
#else
    printk(MODULE_NAME ": dropping skb %08x\n", skb);
#endif
    kfree_skb(skb);
    return NETDEV_TX_OK;
}

/*
 * Interrupt handling:
 * - Recv NAPI style (refer to Documentation/networking/NAPI)
 *
 *   2 Rx interrupts: RX and Overflow (OVF).
 *   - If we get RX and/or OVF, schedule a poll. Turn off _both_ interurpts.
 *
 *   - When our poll's called, we
 *     a) Have one or more packets to process and replenish
 *     b) The hardware may have stopped because of an OVF.
 *
 *   - We process and replenish as much as we can. For every rcvd pkt
 *     indicated up the stack, the head moves. For every such slot that we
 *     replenish with an skb, the tail moves. If head catches up with the tail
 *     we're OOM. When all's done, we consider where we're at:
 *
 *      if no OOM:
 *      - if we're out of quota, let the ints be disabled and poll scheduled.
 *      - If we've processed everything, enable ints and cancel poll.
 *
 *      If OOM:
 *      - Start a timer. Cancel poll. Ints still disabled.
 *        If the hardware's stopped, no point in restarting yet.
 *
 *      Note that in general, whether we're OOM or not, we still try to
 *      indicate everything recvd, up.
 *
 * Locking:
 * The interrupt doesnt touch the ring => Rx is lockless
 *
 */
static irqreturn_t
ag7100_intr(int cpl, void *dev_id)
{
    struct net_device *dev  = (struct net_device *)dev_id;
    ag7100_mac_t      *mac  = *(ag7100_mac_t **)netdev_priv(dev);
    int   isr, imr, status, handled = 0;
#if defined(CONFIG_VENETDEV)
    if (venet_enable(mac))
        dev  = mac->mac_dev;
#endif

    isr   = ag7100_get_isr(mac);
    imr   = ag7100_reg_rd(mac, AG7100_DMA_INTR_MASK);

    ag7100_trc(isr,"isr");
    ag7100_trc(imr,"imr");

#ifdef V54_BSP
    if (isr != (isr & imr))
        printk("ag7100 non-dma interrupt\n");
#else
    assert(isr == (isr & imr));
#endif

    if (likely(isr & (AG7100_INTR_RX | AG7100_INTR_RX_OVF)))
    {
        handled = 1;
        if (likely(netif_running(dev))) {
            if(likely(napi_schedule_prep(&mac->napi)))
            {
                ag7100_intr_disable_recv(mac);
#if !defined(CONFIG_AR9100)
                status = ag7100_reg_rd(mac, AG7100_DMA_RX_STATUS);
                if (!rks_is_walle_board() && (mac->dma_check != 1)) {
                    assert((status & AG7100_RX_STATUS_PKT_RCVD));
                    assert((status >> 16));
                }
#endif
                __napi_schedule(&mac->napi);
            }
            else
            {
                printk(MODULE_NAME ": driver bug! interrupt while in poll\n");
#ifndef V54_BSP
                assert(0);
#endif
                ag7100_intr_disable_recv(mac);
            }
        } else {
           ag7100_intr_disable_recv(mac);
            printk(MODULE_NAME": device isn't in running state!\n");
        }
        /*ag7100_recv_packets(dev, mac, 200, &budget);*/
#ifdef V54_BSP
        mac->sw_stats.stat_rx++;
#endif
    }
    if (likely(isr & AG7100_INTR_TX))
    {
        handled = 1;
#if 0
	    if(mac->mac_unit)
		    printk(MODULE_NAME" TX recover %s\n",__func__);
#endif
        ag7100_intr_ack_tx(mac);
#ifdef V54_BSP
        mac->sw_stats.stat_tx_int++;
#endif
        ag7100_tx_reap(mac);
    }
    if (unlikely(isr & AG7100_INTR_RX_BUS_ERROR))
    {
#ifndef V54_BSP
        assert(0);
#else
        printk("ag7100 intr isr %#x (AG7100_INTR_RX_BUS_ERROR)\n", isr);
        mac->sw_stats.stat_rx_be++;
#endif
        handled = 1;
        ag7100_intr_ack_rxbe(mac);
    }
    if (unlikely(isr & AG7100_INTR_TX_BUS_ERROR))
    {
#ifndef V54_BSP
        assert(0);
#else
        printk("ag7100 intr isr %#x (AG7100_INTR_TX_BUS_ERROR)\n", isr);
        mac->sw_stats.stat_tx_be++;
#endif
        handled = 1;
        ag7100_intr_ack_txbe(mac);
    }

    if (!handled)
    {
#ifndef V54_BSP
        assert(0);
#endif
        printk(MODULE_NAME ": unhandled intr isr %#x\n", isr);
    }

    return IRQ_HANDLED;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void ag7100_poll_controller(struct net_device *dev)
{
	unsigned long flags;
        ag7100_mac_t *mac = *(ag7100_mac_t **)netdev_priv(dev);
	local_irq_save(flags);
	ag7100_intr(mac->mac_irq, dev);
	local_irq_restore(flags);
}
#endif

 /*
  * Rx and Tx DMA hangs and goes to an invalid state in HOWL boards
  * when the link partner is forced to 10/100 Mode.By resetting the MAC
  * we are able to recover from this state.This is a software  WAR and
  * will be removed once we have a hardware fix.
  */

#if defined(CONFIG_AR9100) || defined(CONFIG_AG7100_DMA_HANG_FIX)
void ag7100_dma_reset(ag7100_mac_t *mac)
{
    uint32_t mask;

    ag7100_tx_stop(mac);
    ag7100_rx_stop(mac);
    ag7100_intr_disable_recv(mac);
    mac->sw_stats.stat_dma_reset++;
    if(mac->mac_unit)
        mask = AR7100_RESET_GE1_MAC;
    else
        mask = AR7100_RESET_GE0_MAC;

    ar7100_reg_rmw_set(AR7100_RESET, mask);
    mdelay(100);
    ar7100_reg_rmw_clear(AR7100_RESET, mask);
    mdelay(100);

#if defined(CONFIG_AR9100) && defined(CONFIG_AG7100_GE1_RMII)
    mac->speed_10t = 0;
#endif
    schedule_work(&mac->mac_tx_timeout);
}

static void mac_fast_reset(ag7100_mac_t *mac,ag7100_desc_t *ds)
{
    ag7100_ring_t   *r     = &mac->mac_txring;
    uint32_t rx_ds,tx_ds;
    int flags,mask,int_mask;
    unsigned int w1 = 0, w2 = 0;

    printk("%s: \n", __func__);
    spin_lock_irqsave(&mac->mac_lock, flags);

    int_mask = ag7100_reg_rd(mac,AG7100_DMA_INTR_MASK);

    ag7100_tx_stop(mac);
    ag7100_rx_stop(mac);

    rx_ds = ag7100_reg_rd(mac,AG7100_DMA_RX_DESC);
    tx_ds = ag7100_reg_rd(mac,AG7100_DMA_TX_DESC);
    mask = ag7100_reset_mask(mac->mac_unit);

    /*
    * put into reset, hold, pull out.
    */
    ar7100_reg_rmw_set(AR7100_RESET, mask);
    udelay(10);
    ar7100_reg_rmw_clear(AR7100_RESET, mask);
    udelay(10);

    ag7100_hw_setup(mac);

    if (ds) {
        ag7100_reg_wr(mac,AG7100_DMA_TX_DESC,
            ag7100_desc_dma_addr(r,ds));
    }
    else {
        ag7100_reg_wr(mac,AG7100_DMA_TX_DESC,tx_ds);
    }
    ag7100_reg_wr(mac,AG7100_DMA_RX_DESC,rx_ds);

    /*
     * set the mac addr
     */

    addr_to_words(mac->mac_dev->dev_addr, w1, w2);
    ag7100_reg_wr(mac, AG7100_GE_MAC_ADDR1, w1);
    ag7100_reg_wr(mac, AG7100_GE_MAC_ADDR2, w2);

    ag7100_set_mac_from_link(mac, mac->mac_speed, mac->mac_fdx);

    mac->dma_check = 1;
    ag7100_tx_start(mac);
    ag7100_rx_start(mac);

    /*
     * Restore interrupts
     */
     ag7100_reg_wr(mac,AG7100_DMA_INTR_MASK,int_mask);

     spin_unlock_irqrestore(&mac->mac_lock,flags);

}

#endif

#if defined(CONFIG_AR7240)
static irqreturn_t
ag7240_link_intr(int cpl, void *dev_id) {

	ar7240_s26_intr();
	return IRQ_HANDLED;
}
#endif

static int
ag7100_poll(struct napi_struct *napi, int budget)
{
    ag7100_mac_t       *mac       = container_of(napi, ag7100_mac_t, napi);
    struct net_device  *dev       = mac->mac_dev;
    int                work_done  = 0;
    ag7100_rx_status_t  ret;
    unsigned long       flags;

    ret = ag7100_recv_packets(dev, mac, budget, &work_done);

#if defined(CONFIG_AR9100) || defined(CONFIG_AG7100_DMA_HANG_FIX)
    if(ret == AG7100_RX_DMA_HANG)
    {
        napi_complete(napi);
#ifdef V54_BSP
        ag7100_trc_dump();
        printk("ag7100 DMA reset rx ...\n");
        spin_lock_irqsave(&mac->mac_lock, flags);
        ag7100_dma_reset(mac);
        spin_unlock_irqrestore(&mac->mac_lock, flags);
#else
        ag7100_dma_reset(mac);
#endif
        return work_done;
    }
#endif
    if (likely(ret == AG7100_RX_STATUS_DONE))
    {
        napi_complete(napi);
        spin_lock_irqsave(&mac->mac_lock, flags);
        ag7100_intr_enable_recv(mac);
        spin_unlock_irqrestore(&mac->mac_lock, flags);
        /* if work_done equal to budget, net_rx_action will think that rx is not completed and move napi.poll_list to the polling list tail*/
        /* but  napi_complete has cleaned up napi.poll_list, so the result would be crashed*/
        if (unlikely(work_done == budget))
            work_done--;
    }
    else if (likely(ret == AG7100_RX_STATUS_NOT_DONE))
    {
        /*
         * We have work left
         */
        napi_complete(napi);
        napi_reschedule(napi);
        /* if work_done equal to budget, net_rx_action will think that rx is not completed and move napi.poll_list to the polling list tail*/
        /* but  napi_complete has cleaned up napi.poll_list, so the result would be crashed*/
        if (unlikely(work_done == budget))
            work_done--;
    }
    else if (ret == AG7100_RX_STATUS_OOM)
    {
        printk(MODULE_NAME ": oom..?\n");
#ifdef V54_BSP
        mac->sw_stats.stat_rx_full++;
#endif
        /*
         * Start timer, stop polling, but do not enable rx interrupts.
         */
        mod_timer(&mac->mac_oom_timer, jiffies+1);
        napi_complete(napi);
        /* if work_done equal to budget, net_rx_action will think that rx is not completed and move napi.poll_list to the polling list tail*/
        /* but  napi_complete has cleaned up napi.poll_list, so the result would be crashed*/
        if (unlikely(work_done == budget))
            work_done--;
    }

    return work_done;
}

static inline void
_SKB_INPUT_SET_DEV(struct sk_buff *skb, struct net_device *dev)
{
    /*
     * eth_type_trans() also pulls the ether header
     * and set      skb->mac.raw        = skb->data;
     */
    // skb->mac.raw needs to be set for RX_BROADCAST_STATS()
    skb->protocol       = eth_type_trans(skb, dev);
    skb->dev            = dev;
    skb->input_dev      = NULL; /* this would be set later in netif_rx... */
    return;
}

static int
ag7100_recv_packets(struct net_device *dev, ag7100_mac_t *mac,
                    int quota, int *work_done)
{
    ag7100_ring_t       *r     = &mac->mac_rxring;
    ag7100_desc_t       *ds;
    ag7100_buffer_t     *bp;
    struct sk_buff      *skb;
    ag7100_rx_status_t   ret   = AG7100_RX_STATUS_DONE;
    int head = r->ring_head, len, status, iquota = quota, more_pkts, rep;
#ifdef V54_BSP
    int tail = r->ring_tail;
#endif
#if defined(CONFIG_VENETDEV)
    struct vdev_sw_state    *vdev_state=NULL;
    int srcPort=0;
    int retcode=0;
#endif

    ag7100_trc(iquota,"iquota");
#if !defined(CONFIG_AR9100)
    status = ag7100_reg_rd(mac, AG7100_DMA_RX_STATUS);
#endif

process_pkts:
    ag7100_trc(status,"status");
#if !defined(CONFIG_AR9100)
    /*
    * Under stress, the following assertion fails.
    *
    * On investigation, the following `appears' to happen.
    *   - pkts received
    *   - rx intr
    *   - poll invoked
    *   - process received pkts
    *   - replenish buffers
    *   - pkts received
    *
    *   - NO RX INTR & STATUS REG NOT UPDATED <---
    *
    *   - s/w doesn't process pkts since no intr
    *   - eventually, no more buffers for h/w to put
    *     future rx pkts
    *   - RX overflow intr
    *   - poll invoked
    *   - since status reg is not getting updated
    *     following assertion fails..
    *
    * Ignore the status register.  Regardless of this
    * being a rx or rx overflow, we have packets to process.
    * So, we go ahead and receive the packets..
    */
    if (!rks_is_walle_board() && (mac->dma_check != 1)) {
        /*  SCG-12761: Under heavy traffic and low memory condition,
               *  Rx overflow will be triggered constantly.
               *  Somehow ring buffer cannot be replenished due to no system memory
               *  Thus, OOM timer will be triggered to get memory for ring buffer
               *  But driver did not call ag7100_rx_start to enable DMA Rx function (Rx overflow check will do this)
               *  After OOM timer reschedules poll function, status register will show no pkt recv due to DMA Rx is disabled
               *  It will cause unexpected behavior.
               *  The solution is to skip this Rx and go to enable DMA Rx function again for recovery.
               */
        if ((status & AG7100_RX_STATUS_PKT_RCVD) == 0) {
            printk(MODULE_NAME "[%s]AG7100_RX_STATUS_PKT_RCVD is not set!!, bypass SW receive procedure to recover Rx function\n", __func__);
            goto done;
        }
    }
#endif
    /*
     * Flush the DDR FIFOs for our gmac
     */
    ar7100_flush_ge(mac->mac_unit);

#ifndef V54_BSP
    assert(quota > 0); /* WCL */
#endif

    while(quota)
    {
        ds    = &r->ring_desc[head];

        ag7100_trc(head,"hd");
        ag7100_trc(ds,  "ds");

        if (ag7100_rx_owned_by_dma(ds))
        {
#if defined(CONFIG_AR9100) || defined(CONFIG_AG7100_DMA_HANG_FIX)
            if(quota == iquota)
            {
                *work_done = quota = 0;
                return AG7100_RX_DMA_HANG;
            }
#else
            assert(quota != iquota); /* WCL */
#endif
            break;
		}
        ag7100_intr_ack_rx(mac);

        bp                  = &r->ring_buffer[head];
        len                 = ds->pkt_size;
        skb                 = bp->buf_pkt;
#ifndef V54_BSP
        assert(skb);
#else
        if ( !skb ) {
            // check for head catching up with tail and OOM.
            if ( head == tail ) {
#if 1   //DEBUG
                printk("*** %s: head overruns tail ***\n", __FUNCTION__);
#endif
                break;
            }
            assert_rx(skb, mac);
        }
#endif
#if defined(CONFIG_VENETDEV)
        if (venet_enable(mac)) {
#if VENETDEV_DEBUG
            printk("rx dev 0x%x\n", (u32)dev);
            printk("rx ds pkt size=%d skb headroom=%d tailroom=%d\n", len, skb_headroom(skb), skb_tailroom(skb));
            if (1) {
                int cnt;
                for (cnt=0; cnt<len; cnt++) {
                    if (cnt && !(cnt%16))
                        printk("\n");
                    printk("%02x ", *(skb->data + cnt));
                }
                printk("\n");
            }
#endif
            if (rks_is_gd11_board()) {
                if (*(u16 *)(skb->data+(ETH_ALEN*2)) == RTL_TYPE) {
                    char dst_src_addr[(ETH_ALEN*2)];
                    srcPort = (*(u16 *)(skb->data+ETH_HLEN) & RTL_SRC_PORT_ID_MASK) - 1;
                    memcpy(dst_src_addr, skb->data, (ETH_ALEN*2));
                    memcpy((skb->data+RTL_MGNT_TAG_LEN), dst_src_addr, (ETH_ALEN*2));
                    skb->data   += RTL_MGNT_TAG_LEN;
                    skb->tail   += RTL_MGNT_TAG_LEN;
                    len         -= RTL_MGNT_TAG_LEN;
                }
            } else if (rks_is_gd17_board()) {
                /* gd17 */
                char dst_src_addr[(ETH_ALEN*2)];
                srcPort =
                    ((*(u16 *)(skb->data+(ETH_ALEN*2)) & ATHR_SRC_PORT_ID_MASK) <
                        ATHR_PORT_6) ? 0 : 1;
                memcpy(dst_src_addr, skb->data, (ETH_ALEN*2));
                memcpy((skb->data+ATHR_MGNT_TAG_LEN), dst_src_addr, (ETH_ALEN*2));
                skb->data   += ATHR_MGNT_TAG_LEN;
                skb->tail   += ATHR_MGNT_TAG_LEN;
                len         -= ATHR_MGNT_TAG_LEN;
            } else if (rks_is_walle_board()) {
                srcPort = (*(skb->data) & 0x0f) - 1;
                skb->data   += ATHR_MGNT_TAG_LEN;
                skb->tail   += ATHR_MGNT_TAG_LEN;
                len         -= ATHR_MGNT_TAG_LEN;
            }

#if VENETDEV_DEBUG
            printk("rx from srcPort=%d\n", srcPort);
            printk("rx tag removed len=%d skb headroom=%d tailroom=%d\n", len, skb_headroom(skb), skb_tailroom(skb));
            if (0){
                int cnt;
                for (cnt=0; cnt<len; cnt++) {
                    if (cnt && !(cnt%16))
                        printk("\n");
                    printk("%02x ", *(skb->data + cnt));
                }
                printk("\n");
            }
#endif
        }

        /* Wall-E access ports use VLAN ID 4091-4094; Drop all other tagged packets. */
        if (rks_is_walle_board() && ((srcPort > 3) || (srcPort < 0))) {
            bp->buf_pkt         = NULL;
            quota--;
            kfree_skb(skb);
            ag7100_ring_incr(head);
            continue;
        }

#endif
        skb_put(skb, len - ETHERNET_FCS_SIZE);

#if defined(CONFIG_ATHRS26_PHY) && defined(HEADER_EN)
        uint8_t type;
        uint16_t def_vid;

        if (mac->mac_unit == 0)
        {
            type = (skb->data[1]) & 0xf;

            if (type == NORMAL_PACKET)
            {
#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
                /* cpu egress tagged */
                if (is_cpu_egress_tagged())
                {
                    if ((skb->data[12 + HEADER_LEN] != 0x81) || (skb->data[13 + HEADER_LEN] != 0x00))
                    {
                        def_vid = athrs26_defvid_get(skb->data[0] & 0xf);
                        skb_push(skb, 2); /* vid lenghth - header length */
                        memmove(&skb->data[0], &skb->data[4], 12); /* remove header and add vlan tag */

                        skb->data[12] = 0x81;
                        skb->data[13] = 0x00;
                        skb->data[14] = (def_vid >> 8) & 0xf;
                        skb->data[15] = def_vid & 0xff;
                    }
                }
                else
#endif
                    skb_pull(skb, 2); /* remove attansic header */

                mac->net_rx_packets ++;
                mac->net_rx_bytes += skb->len;

                /*
                 * also pulls the ether header
                 */
                _SKB_INPUT_SET_DEV(skb, dev);
                /* need to set skb->dev for use by RX_BROADCAST_STATS() */
                RX_BROADCAST_STATS((&mac->mac_net_stats), skb);

                bp->buf_pkt         = NULL;
                dev->last_rx        = jiffies;
                quota--;

                netif_receive_skb(skb);
            }
            else
            {
                mac->net_rx_packets ++;
                mac->net_rx_bytes += skb->len;

                _SKB_INPUT_SET_DEV(skb, dev);
                /* need to set skb->dev for use by RX_BROADCAST_STATS() */
                RX_BROADCAST_STATS((&mac->mac_net_stats), skb);

                bp->buf_pkt         = NULL;
                dev->last_rx        = jiffies;
                quota--;

                if (type == READ_WRITE_REG_ACK)
                {
                    header_receive_skb(skb);
                }
                else
                {
                    kfree_skb(skb);
                }
            }
        }
        else
        {
            mac->net_rx_packets ++;
            mac->net_rx_bytes += skb->len;

            pause_frame(skb->data);
            /*
             * also pulls the ether header
             */
            _SKB_INPUT_SET_DEV(skb, dev);
            /* need to set skb->dev for use by RX_BROADCAST_STATS() */
            RX_BROADCAST_STATS((&mac->mac_net_stats), skb);

            bp->buf_pkt         = NULL;
            dev->last_rx        = jiffies;
            quota--;

            netif_receive_skb(skb);
        }
#else
        mac->net_rx_packets ++;
        mac->net_rx_bytes += skb->len;

        /*
         * also pulls the ether header
         */
#if 0 && defined(V54_BSP)
        // skb->mac.raw set in eth_type_trans()
        skb->mac.raw        = skb->data;
#endif
        skb->protocol       = eth_type_trans(skb, dev);
        skb->dev            = dev;
        skb->input_dev      = NULL; /* this would be set later in netif_rx... */
        // need to set skb->dev for use by RX_BROADCAST_STATS()
        // skb->mac.raw needs to be set for RX_BROADCAST_STATS()
        RX_BROADCAST_STATS((&mac->mac_net_stats), skb);

#if defined(CONFIG_VENETDEV)
        if (venet_enable(mac)) {
            vdev_state = &mac->vdev_state[srcPort];

            skb->dev = vdev_state->dev;

            vdev_state->net_stats.rx_packets++;
            vdev_state->net_stats.rx_bytes += skb->len;
            RX_BROADCAST_STATS((&vdev_state->net_stats), skb);

            if ((u32)dev != (u32)skb->dev) {
                if (!compare_ether_addr(skb_mac_header(skb), skb->dev->dev_addr))
                    skb->pkt_type = 0;
            }
#if VENETDEV_DEBUG
            printk("rx to skb dev=0x%x\n", (u32)skb->dev);
#endif
        } else {
            skb->dev            = dev;
        }
#else
#if 0
        // 0/08/14
        // we don't use CONFIG_VENETDEV and mac->vdev_state
        // move this to earlier when the mac stats are handled
        skb->dev            = dev;
#endif
#endif
        bp->buf_pkt         = NULL;
#if defined(CONFIG_VENETDEV)
        if (venet_enable(mac))
            skb->dev->last_rx   = jiffies;
        else
            dev->last_rx        = jiffies;
#else
        dev->last_rx        = jiffies;
#endif

        quota--;

#if defined(MAC_HW_UNSTOP)
        if (netif_queue_stopped(skb->dev)) {
            kfree_skb(skb);
            ag7100_ring_incr(head);
            continue;
        }
#endif

#if defined(CONFIG_VENETDEV)
        if (venet_enable(mac)) {
            retcode = skb_rx(skb);
#if VENETDEV_DEBUG
        if (retcode)
            printk("rx skb_rx retcode=0x%x dev=0x%x\n", retcode,(u32)skb->dev);
#endif
        } else {
            skb_rx(skb);
        }
#else
#if defined(V54_BSP) && defined(RKS_SYSTEM_HOOKS)
        skb_rx(skb);
#else
        netif_receive_skb(skb);
#endif
#endif
#endif
        ag7100_ring_incr(head);
    }

#ifndef V54_BSP
    assert(iquota != quota);
#endif
    r->ring_head   =  head;

    rep = ag7100_rx_replenish(mac);
#ifdef CONFIG_AR9100
    if(rep < 0)
    {
        *work_done =0 ;
        return AG7100_RX_DMA_HANG;
    }
#endif
    /*
     * let's see what changed while we were slogging.
     * ack Rx in the loop above is no flush version. It will get flushed now.
     */
    status       =  ag7100_reg_rd(mac, AG7100_DMA_RX_STATUS);
    more_pkts    =  (status & AG7100_RX_STATUS_PKT_RCVD);

    ag7100_trc(more_pkts,"more_pkts");

    if (!more_pkts) goto done;
    /*
     * more pkts arrived; if we have quota left, get rolling again
     */
    if (quota)      goto process_pkts;
    /*
     * out of quota
     */
    ret = AG7100_RX_STATUS_NOT_DONE;

done:
    *work_done   = (iquota - quota);

#ifdef V54_BSP
    if (unlikely(ag7100_rx_ring_empty(mac)))
#else
    if (unlikely(ag7100_rx_ring_full(mac)))
#endif
        return AG7100_RX_STATUS_OOM;
    /*
     * !oom; if stopped, restart h/w
     */

    if (unlikely(status & AG7100_RX_STATUS_OVF))
    {
        mac->net_rx_over_errors ++;
#if defined(CONFIG_VENETDEV)
    if (venet_enable(mac) && NULL != vdev_state)
        vdev_state->net_stats.rx_over_errors++;
#endif
        ag7100_intr_ack_rxovf(mac);
        ag7100_rx_start(mac);
    }

    return ret;
}

static struct sk_buff *
    ag7100_buffer_alloc(void)
{
    struct sk_buff *skb;

    skb = dev_alloc_skb(AG7100_RX_BUF_SIZE);
    if (unlikely(!skb))
        return NULL;
    skb_reserve(skb, AG7100_RX_RESERVE);

    return skb;
}

static void
ag7100_buffer_free(struct sk_buff *skb)
{
    if (in_irq())
        dev_kfree_skb_irq(skb);
    else
        dev_kfree_skb(skb);
}

/*
 * Head is the first slot with a valid buffer. Tail is the last slot
 * replenished. Tries to refill buffers from tail to head.
 */
static int
ag7100_rx_replenish(ag7100_mac_t *mac)
{
    ag7100_ring_t   *r     = &mac->mac_rxring;
    int              head  = r->ring_head, tail = r->ring_tail, refilled = 0;
    ag7100_desc_t   *ds;
    ag7100_buffer_t *bf;

    ag7100_trc(head,"hd");
    ag7100_trc(tail,"tl");

#ifdef V54_BSP
    if (unlikely(ag7100_rx_ring_full(mac))) {
        // already full
        return 0;
    }
#endif
    do
    {
        bf                  = &r->ring_buffer[tail];
        ds                  = &r->ring_desc[tail];

        ag7100_trc(ds,"ds");
#if defined(CONFIG_AR9100) || defined(CONFIG_AG7100_DMA_HANG_FIX)
        if(ag7100_rx_owned_by_dma(ds))
        {
            return -1;
        }
#else
        assert(!ag7100_rx_owned_by_dma(ds));
#endif
        assert(!bf->buf_pkt);

        bf->buf_pkt         = ag7100_buffer_alloc();
        if (!bf->buf_pkt)
        {
            printk(MODULE_NAME ": outta skbs!\n");
            break;
        }
        /* !caution: if we reserve space in ag7100_buffer_alloc, we need to subtract it here.*/
        dma_cache_inv((unsigned long)bf->buf_pkt->data, AG7100_RX_BUF_SIZE - AG7100_RX_RESERVE);
        ds->pkt_start_addr  = virt_to_phys(bf->buf_pkt->data);

        ag7100_rx_give_to_dma(ds);
        refilled ++;

        ag7100_ring_incr(tail);

    } while(tail != head);
    /*
     * Flush descriptors
     */
    wmb();

    r->ring_tail = tail;
    ag7100_trc(refilled,"refilled");

    return refilled;
}

#ifdef V54_BSP
#include <asm/atomic.h>
static atomic_t _tx_reap_ok = ATOMIC_INIT(1);
#define _ENTER_TX_REAP_FAILED() ( atomic_sub_if_positive(1, &_tx_reap_ok) < 0 )
#define _LEAVE_TX_REAP()    atomic_inc(&_tx_reap_ok)
#endif
/*
 * Reap from tail till the head or whenever we encounter an unxmited packet.
 */
static int
ag7100_tx_reap(ag7100_mac_t *mac)
{
#ifdef V54_BSP
    ag7100_ring_t   *r;
    int              head, tail, reaped, i;
#else
    ag7100_ring_t   *r     = &mac->mac_txring;
    int              head  = r->ring_head, tail = r->ring_tail, reaped = 0, i;
#endif
    ag7100_desc_t   *ds;
    ag7100_buffer_t *bf;
    unsigned long    flags;

#ifdef V54_BSP
    if ( _ENTER_TX_REAP_FAILED() ) {
        printk("---> %s in progress !!!\n", __FUNCTION__);
        return 0;
    }
    r       = &mac->mac_txring;
    head    = r->ring_head;
    tail    = r->ring_tail;
    reaped  = 0;
#endif
    ag7100_trc_new(head,"hd");
    ag7100_trc_new(tail,"tl");

    ar7100_flush_ge(mac->mac_unit);

    while(tail != head)
    {
        ds   = &r->ring_desc[tail];

        ag7100_trc_new(ds,"ds");

        if(ag7100_tx_owned_by_dma(ds))
            break;

        bf      = &r->ring_buffer[tail];
        assert(bf->buf_pkt);

        ag7100_trc_new(bf->buf_lastds,"lastds");

        if(ag7100_tx_owned_by_dma(bf->buf_lastds))
            break;

        for(i = 0; i < bf->buf_nds; i++)
        {
            ag7100_intr_ack_tx(mac);
            ag7100_ring_incr(tail);
        }

#ifdef V54_BSP
        if (bf->buf_pkt)
#endif
        ag7100_buffer_free(bf->buf_pkt);
        bf->buf_pkt = NULL;

        reaped ++;
    }

    r->ring_tail = tail;

#if defined(CONFIG_VENETDEV)
    if (venet_enable(mac)) {
        for (i=0; i<dev_per_mac; i++) {
            if (netif_queue_stopped(mac->vdev_state[i].dev) &&
                (ag7100_ndesc_unused(mac, r) >= AG7100_TX_QSTART_THRESH) &&
                netif_carrier_ok(mac->vdev_state[i].dev)) {
                if (ag7100_reg_rd(mac, AG7100_DMA_INTR_MASK) & AG7100_INTR_TX) {
                    spin_lock_irqsave(&mac->mac_lock, flags);
                    ag7100_intr_disable_tx(mac);
                    spin_unlock_irqrestore(&mac->mac_lock, flags);
                }
                netif_wake_queue(mac->vdev_state[i].dev);
            }
        }
    } else {
    if (netif_queue_stopped(mac->mac_dev) &&
        (ag7100_ndesc_unused(mac, r) >= AG7100_TX_QSTART_THRESH) &&
        netif_carrier_ok(mac->mac_dev)) {
        if (ag7100_reg_rd(mac, AG7100_DMA_INTR_MASK) & AG7100_INTR_TX) {
            spin_lock_irqsave(&mac->mac_lock, flags);
            ag7100_intr_disable_tx(mac);
            spin_unlock_irqrestore(&mac->mac_lock, flags);
        }
        netif_wake_queue(mac->mac_dev);
    }
    }
#else
    if (netif_queue_stopped(mac->mac_dev) &&
        (ag7100_ndesc_unused(mac, r) >= AG7100_TX_QSTART_THRESH) &&
        netif_carrier_ok(mac->mac_dev))
    {
        if (ag7100_reg_rd(mac, AG7100_DMA_INTR_MASK) & AG7100_INTR_TX)
        {
            spin_lock_irqsave(&mac->mac_lock, flags);
            ag7100_intr_disable_tx(mac);
            spin_unlock_irqrestore(&mac->mac_lock, flags);
        }
        netif_wake_queue(mac->mac_dev);
    }
#endif
#ifdef V54_BSP
    ag7100_num_tx_unreap -= reaped;
    if (ag7100_num_tx_unreap < 0 ) ag7100_num_tx_unreap = 0;
    _LEAVE_TX_REAP();
#endif
    return reaped;
}

/*
 * allocate and init rings, descriptors etc.
 */
static int
ag7100_tx_alloc(ag7100_mac_t *mac)
{
    ag7100_ring_t *r = &mac->mac_txring;
    ag7100_desc_t *ds;
    int i, next;

    if (ag7100_ring_alloc(r, AG7100_TX_DESC_CNT))
        return 1;

    ag7100_trc(r->ring_desc,"ring_desc");

    ds = r->ring_desc;
    for(i = 0; i < r->ring_nelem; i++ )
    {
        ag7100_trc_new(ds,"tx alloc ds");
        next                =   (i == (r->ring_nelem - 1)) ? 0 : (i + 1);
        ds[i].next_desc     =   ag7100_desc_dma_addr(r, &ds[next]);
        ag7100_tx_own(&ds[i]);
    }

    return 0;
}

static int
ag7100_rx_alloc(ag7100_mac_t *mac)
{
    ag7100_ring_t *r  = &mac->mac_rxring;
    ag7100_desc_t *ds;
#ifndef V54_BSP
    int i, next, tail = r->ring_tail;
#else
    int i, next, tail = 0;
#endif
    ag7100_buffer_t *bf;

    if (ag7100_ring_alloc(r, AG7100_RX_DESC_CNT))
        return 1;

    ag7100_trc(r->ring_desc,"ring_desc");

    ds = r->ring_desc;
    for(i = 0; i < r->ring_nelem; i++ )
    {
        next                =   (i == (r->ring_nelem - 1)) ? 0 : (i + 1);
        ds[i].next_desc     =   ag7100_desc_dma_addr(r, &ds[next]);
    }

    for (i = 0; i < AG7100_RX_DESC_CNT; i++)
    {
        bf                  = &r->ring_buffer[tail];
        ds                  = &r->ring_desc[tail];

        bf->buf_pkt         = ag7100_buffer_alloc();
        if (!bf->buf_pkt)
            goto error;

        /* !caution: if we reserve space in ag7100_buffer_alloc, we need to subtract it here.*/
        dma_cache_inv((unsigned long)bf->buf_pkt->data, AG7100_RX_BUF_SIZE - AG7100_RX_RESERVE);
        ds->pkt_start_addr  = virt_to_phys(bf->buf_pkt->data);

        ag7100_rx_give_to_dma(ds);
        ag7100_ring_incr(tail);
    }

    return 0;
error:
    printk(MODULE_NAME ": unable to allocate rx\n");
    ag7100_rx_free(mac);
    return 1;
}

static void
ag7100_tx_free(ag7100_mac_t *mac)
{
    ag7100_ring_release(mac, &mac->mac_txring);
    ag7100_ring_free(&mac->mac_txring);
}

static void
ag7100_rx_free(ag7100_mac_t *mac)
{
    ag7100_ring_release(mac, &mac->mac_rxring);
    ag7100_ring_free(&mac->mac_rxring);
}

static int
ag7100_ring_alloc(ag7100_ring_t *r, int count)
{
    int desc_alloc_size, buf_alloc_size;

    desc_alloc_size = sizeof(ag7100_desc_t)   * count;
    buf_alloc_size  = sizeof(ag7100_buffer_t) * count;

    memset(r, 0, sizeof(ag7100_ring_t));

    r->ring_buffer = (ag7100_buffer_t *)kmalloc(buf_alloc_size, GFP_KERNEL);
    printk("%s Allocated %d at 0x%lx\n",__func__,buf_alloc_size,(unsigned long) r->ring_buffer);
    if (!r->ring_buffer)
    {
        printk(MODULE_NAME ": unable to allocate buffers\n");
        return 1;
    }

    r->ring_desc  =  (ag7100_desc_t *)dma_alloc_coherent(NULL,
                                                          desc_alloc_size,
                                                          &r->ring_desc_dma,
                                                          GFP_DMA);
    if (! r->ring_desc)
    {
        printk(MODULE_NAME ": unable to allocate coherent descs\n");
        kfree(r->ring_buffer);
        printk("%s Freeing at 0x%lx\n",__func__,(unsigned long) r->ring_buffer);
        return 1;
    }

    memset(r->ring_buffer, 0, buf_alloc_size);
    memset(r->ring_desc,   0, desc_alloc_size);
    r->ring_nelem   = count;

#ifdef V54_BSP
    r->ring_head = r->ring_tail = 0;
    r->ring_buffer[0].buf_pkt = NULL;
#endif

    return 0;
}

static void
ag7100_ring_release(ag7100_mac_t *mac, ag7100_ring_t  *r)
{
    int i;

    if (!r->ring_buffer) {
        printk(KERN_ERR "%s free r->ring_buffer twice!\n",__func__);
        return;
    }
    for(i = 0; i < r->ring_nelem; i++)
        if (r->ring_buffer[i].buf_pkt) {
            ag7100_buffer_free(r->ring_buffer[i].buf_pkt);
            r->ring_buffer[i].buf_pkt = NULL;
        }
}

static void
ag7100_ring_free(ag7100_ring_t *r)
{
    if (r->ring_desc) {
        dma_free_coherent(NULL, sizeof(ag7100_desc_t)*r->ring_nelem, r->ring_desc,
                          r->ring_desc_dma);
        r->ring_desc = NULL;
    }
    if (r->ring_buffer) {
        kfree(r->ring_buffer);
        printk("%s Freeing at 0x%lx\n",__func__,(unsigned long) r->ring_buffer);
        r->ring_buffer = NULL;
    }
}

/*
 * Error timers
 */
static void
ag7100_oom_timer(unsigned long data)
{
    ag7100_mac_t *mac = (ag7100_mac_t *)data;
    int val;

    printk(MODULE_NAME "[%s] oom timer is triggered\n", __func__);
    ag7100_trc(data,"data");
    ag7100_rx_replenish(mac);
#ifdef V54_BSP
    if (ag7100_rx_ring_empty(mac))
#else
    if (ag7100_rx_ring_full(mac))
#endif
    {
        val = mod_timer(&mac->mac_oom_timer, jiffies+1);
        assert(!val);
    }
    else
        napi_schedule(&mac->napi);
}

static void
ag7100_tx_timeout(struct net_device *dev)
{
    ag7100_mac_t *mac = *(ag7100_mac_t **)netdev_priv(dev);
    ag7100_trc(dev,"dev");
    printk("%s: dev(%p)\n", __func__, dev);
    mac->sw_stats.stat_tx_timeout++;

    if (rks_is_walle_board()) {
        ag7100_int_disable(mac);
        athrs26_disable_linkIntrs(mac->mac_unit);
        ag7100_rx_stop(mac);
        ag7100_tx_stop(mac);

        tx_timeout_reset = 1;

#if defined(CONFIG_VENETDEV)
        if (venet_enable(mac)) {
            int i;
            struct vdev_sw_state    *vdev_state;
            ag7100_rx_stop(mac);
            for (i=0; i<dev_per_mac; i++) {
                vdev_state = get_vdev_sw_state(mac->vdev_state[i].dev);
                vdev_state->up = 0;
                vdev_state->carrier = 0;
                netif_stop_queue(mac->vdev_state[i].dev);
                netif_carrier_off(mac->vdev_state[i].dev);
            }
            mac->mac_open = 0;
        } else
#endif
        {
            netif_stop_queue(dev);
            netif_carrier_off(dev);
        }
    }
#if defined(CONFIG_VENETDEV)
	else if (rks_is_gd11_board() && venet_enable(mac)) {
			// TODO: for zf4336. should not only for gd11...
            struct vdev_sw_state    *vdev_state;
			int i;
            for (i=0; i<dev_per_mac; i++) {
	            vdev_state = get_vdev_sw_state(mac->vdev_state[i].dev);
	            netif_stop_queue(vdev_state->dev);
	            netif_carrier_off(vdev_state->dev);
	            vdev_state->up = 1;
	            vdev_state->carrier = 0;
				vdev_state->dev->trans_start = jiffies;
            }
			return;
		}
#endif

    /*
     * Do the reset outside of interrupt context
     */
    schedule_work(&mac->mac_tx_timeout);
}

/*
*  1) set port0 as loopback
*  2) delay 100ms
*  3) cancel port0's loopback to return normal
*
*/
static void ag7100_forward_hang_recover_action(void)
{
    athrs26_reg_write(PORT_CONTROL_REGISTER0,
                          athrs26_reg_read(PORT_CONTROL_REGISTER0) | 0x1000);
    mdelay(100);
    athrs26_reg_write(PORT_CONTROL_REGISTER0,
                          athrs26_reg_read(PORT_CONTROL_REGISTER0) & (~0x1000));
}

static void
ag7100_tx_timeout_task(struct work_struct *work)
{
	ag7100_mac_t *mac = container_of(work, ag7100_mac_t, mac_tx_timeout);
    ag7100_trc(mac,"mac");
    ag7100_stop(mac->mac_dev);
    ag7100_open(mac->mac_dev);

    if (mac == ag7100_macs[1])
        ag7100_forward_hang_recover_action();
}

#ifdef V54_BSP
static void
ag7100_get_macaddr(int unit, u8 *mac_addr)
{
    static u8  fixed_addr[6] = {0x00, 0x0d, 0x0b, 0x13, 0x6b, 0x16};
    memcpy(mac_addr, fixed_addr, sizeof(fixed_addr));
    mac_addr[5] = unit;
}
#else
static void
ag7100_get_default_macaddr(ag7100_mac_t *mac, u8 *mac_addr)
{
    /* Use MAC address stored in Flash */

    u8 *eep_mac_addr = (mac->mac_unit) ? AR7100_EEPROM_GE1_MAC_ADDR:
        AR7100_EEPROM_GE0_MAC_ADDR;

    printk(MODULE_NAME "CHH: Mac address for unit %d\n",mac->mac_unit);
    printk(MODULE_NAME "CHH: %02x:%02x:%02x:%02x:%02x:%02x \n",
        eep_mac_addr[0],eep_mac_addr[1],eep_mac_addr[2],
        eep_mac_addr[3],eep_mac_addr[4],eep_mac_addr[5]);

    /*
    ** Check for a valid manufacturer prefix.  If not, then use the defaults
    */

    if(eep_mac_addr[0] == 0x00 &&
       eep_mac_addr[1] == 0x03 &&
       eep_mac_addr[2] == 0x7f)
    {
        memcpy(mac_addr, eep_mac_addr, 6);
    }
    else
    {
        /* Use Default address at top of range */
        mac_addr[0] = 0x00;
        mac_addr[1] = 0x03;
        mac_addr[2] = 0x7F;
        mac_addr[3] = 0xFF;
        mac_addr[4] = 0xFF;
        mac_addr[5] = 0xFF - mac->mac_unit;
    }
}
#endif

static int
ag7100_do_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
#if !defined(V54_BSP)
#ifndef CONFIG_ATHRS26_PHY
    printk(MODULE_NAME ": unsupported ioctl\n");
    return -EOPNOTSUPP;
#else
    return athr_ioctl(ifr->ifr_data, cmd);
#endif
#else
    ag7100_mac_t *mac = *(ag7100_mac_t **)netdev_priv(dev);

    //printk("%s: %x\n", __func__, cmd);
    if (rks_is_walle_board()) {
        return athrs26_ioctl((uint32_t *)ifr, cmd);
    } else if (rks_is_gd26_board()) {
        if (!mac->mac_unit && gd26_ge)
            return lantiqPhy_ioctl((uint32_t *)ifr, cmd);
        else
            return athrs26_ioctl((uint32_t *)ifr, cmd);
    } else if (rks_is_gd36_board()) {
        if (!mac->mac_unit && gd26_ge)
            return athr_phy_ioctl((uint32_t *)ifr, cmd);
        else
            return athrs26_ioctl((uint32_t *)ifr, cmd);
    } else if (rks_is_spaniel_board()) {
            return athr_phy_ioctl((uint32_t *)ifr, cmd);
    } else if (rks_is_gd11_board() && (mac == ag7100_macs[1])) {
        return rtl8363_ioctl((uint32_t *)ifr, cmd);
    } else {
        /* retriever, gd9, gd10, gd11, gd12, bermuda, gd22 */
        return mvPhy_ioctl((uint32_t *)ifr, cmd);
    }
#endif
    return -EOPNOTSUPP;
}

static struct net_device_stats
    *ag7100_get_stats(struct net_device *dev)
{
    ag7100_mac_t *mac = *(ag7100_mac_t **)netdev_priv(dev);
    struct Qdisc *sch;
    int i;

    sch = rcu_dereference(dev->qdisc);
    mac->mac_net_stats.tx_dropped = sch->qstats.drops;

#if !defined(CONFIG_AR7240)
    i = ag7100_get_rx_count(mac) - mac->net_rx_packets;
    if (i<0)
#endif
        i=0;

    mac->mac_net_stats.rx_missed_errors = i;

#if defined(CONFIG_VENETDEV)
    if (venet_enable(mac)) {
        struct vdev_sw_state    *vdev_state;
        struct net_device_stats *net_stats;

        vdev_state = get_vdev_sw_state(dev);
        if (vdev_state)
            net_stats = &vdev_state->net_stats;
        else {
            printk("ag7100 cannot find vdev stats\n");
            net_stats = &mac->mac_net_stats;
        }

        return (net_stats);
    } else {
        return (&mac->mac_net_stats);
    }
#else
    return (&mac->mac_net_stats);
#endif
}

static void
ag7100_vet_tx_len_per_pkt(unsigned int *len)
{
    unsigned int l;

    /* make it into words */
    l = *len & ~3;

    /*
    * Not too small
    */
    if (l < AG7100_TX_MIN_DS_LEN)
    {
        l = AG7100_TX_MIN_DS_LEN;
    }
    else
    {
        /* Avoid len where we know we will deadlock, that
        * is the range between fif_len/2 and the MTU size
        */
        if (l > AG7100_TX_FIFO_LEN/2)
        {
            if (l < AG7100_TX_MTU_LEN)
            {
                l = AG7100_TX_MTU_LEN;
            }
            else if (l > AG7100_TX_MAX_DS_LEN)
            {
                l = AG7100_TX_MAX_DS_LEN;
            }
        }
    }
        *len = l;
}

#if defined(V54_BSP)
void probe_ethphy(ag7100_mac_t *mac, unsigned short *phy0Id,
                                    unsigned short *phy1Id)
{
    // all 11n boards have marvell 88e1116 as phy0 except gd8 uses broadcom

    // phy0
    if (rks_is_gd8_board()) {
        // broadcom ac201
        ag7100_mii_setup(mac, AR7100_MII0_CTRL, AG7100_GE0_MII_VAL);
        if ((*phy0Id=ag7100_mii_read(0, BCM_PHY0_ADDR, 2)) == BCM_PHY_ID1) {
            printk("phy0Id[0x%x]\n", *phy0Id);
            ag7100_phy_setup        = bcm_phySetup;
            ag7100_phy_is_up        = bcm_ethIsLinkUp;
            ag7100_get_link_status  = ag7100_get_link_status_brcm;
        }
    } else if (rks_is_gd17_board()){
        // AR8228
        ag7100_mii_setup(mac, AR7100_MII0_CTRL, AG7100_GE0_RGMII_VAL);
        *phy0Id=ag7100_mii_read(0, AR8228_PHY0_ADDR, 2);
        printk("phy0Id[0x%x] -\n", *phy0Id);
        ag7100_phy_setup        = athrs16_phy_setup;
        ag7100_phy_is_up        = athrs16_phy_is_up;
        ag7100_get_link_status  = ag7100_get_link_status_s16;
    } else if (rks_is_walle_board()){
        ag7100_phy_setup        = athrs26_phy_setup;
        ag7100_phy_is_up        = athrs26_phy_is_up;
        ag7100_get_link_status  = NULL;
    } else if (rks_is_gd26_board() || rks_is_spaniel_board() || rks_is_gd36_board()) {
        if (rks_is_spaniel_board() || rks_is_gd36_board())
            gd26_ge = 1;
        else
            gd26_ge = rks_is_gd26_ge();
        printk(" : gd26_ge=%d\n", gd26_ge);
        if (gd26_ge) {
            if (!rks_is_spaniel_board() && !rks_is_gd36_board()) {
                /* mac0: lantiq */
                ag7100_phy_setup        = lantiq_phySetup;
                ag7100_phy_is_up        = lantiq_phyIsUp;
                ag7100_get_link_status  = ag7100_get_link_status_lantiq;
            } else {
                /* athrf1 */
                ag7100_phy_setup        = athr_phy_setup;
                ag7100_phy_is_up        = athr_phy_is_up;
                ag7100_get_link_status  = ag7100_get_link_status_athrf1;
            }
        } else {
            /* mac0: internal p4 */
            ag7100_phy_setup        = athrs26_phy_setup;
            ag7100_phy_is_up        = athrs26_phy_is_up;
            ag7100_get_link_status  = NULL;
        }
    } else {
        // marvell 88e1116
        ag7100_mii_setup(mac, AR7100_MII0_CTRL, AG7100_GE0_RGMII_VAL);
        if ((*phy0Id=ag7100_mii_read(0, MV_PHY0_ADDR, 2)) == MV_PHY_ID1) {
            unsigned short phyId2;
            phyId2 = ag7100_mii_read(0, MV_PHY0_ADDR, 3);
            phyId2 = MV_PHY_MODEL(phyId2);
            printk("phy0Id[0x%x 0x%x]\n", *phy0Id, phyId2);
            if (phyId2 == MV_PHY_MODEL_1121R) {
                flip_phy = 1;   // cross-connect mac-phy
            }
        } else {
            printk("phy0Id[0x%x] -\n", *phy0Id);
        }
    }

    // phy1
    if (rks_is_gd11_board()) {
        if ((*phy1Id = ag7100_mii_read(1, RTL_PHY0_ADDR, 2)) == RTL_PHY_ID1) {
            ag7100_phy_setup_1          = rtl8363_phySetup;
            ag7100_phy_is_up_1          = rtl8363_phyIsUp;
            ag7100_get_link_status_1    = ag7100_get_link_status_rtl;
        }
    }
    else {
        if (rks_is_gd17_board()) {
            *phy1Id = ag7100_mii_read(1, AR8021_PHY_ADDR, 2);
        } else if (rks_is_walle_board()) {
        } else if (rks_is_gd26_board() || rks_is_gd36_board()) {
            if (gd26_ge) {
                /* mac1: internal p0 */
                ag7100_phy_setup_1          = athrs26_phy_setup;
                ag7100_phy_is_up_1          = athrs26_phy_is_up;
                ag7100_get_link_status_1    = NULL;
            }
        } else if (rks_is_spaniel_board()) {
            /* do nothing */
        } else {
            // can't read 2nd marvell 88e1116 phy id
            *phy1Id = ag7100_mii_read(1, MV_PHY1_ADDR, 2);
        }
    }
    printk("phy1Id[0x%x]\n", *phy1Id);
}
#endif

#ifdef CONFIG_AR9100
static int ag7100_change_mtu(struct net_device *dev, int new_mtu)
{
    if (new_mtu < AG7100_MIN_MTU || new_mtu > AG7100_MAX_MTU)
        return -EINVAL;

    dev->mtu = new_mtu;

    return 0;
}
#endif

#if defined(CONFIG_AR7240)
extern int eth_dbg;

static void ag7100_forward_hang_check_timer(unsigned long data)
{
    if (likely(eth_dbg)) {
        /* if tx byte of port0 becomes 0 and one of LAN ports is filtered, enter recover action */
        if ((!athrs26_reg_read(0x20084)) &&
            (athrs26_reg_read(0x20150) ||
            athrs26_reg_read(0x20250) ||
            athrs26_reg_read(0x20350) ||
            athrs26_reg_read(0x20450))) {
            ag7100_macs[1]->sw_stats.stat_fwd_hang++;

            /* we had better not call mdelay() in hard/soft interrupt context, so trigger another timer here */
            athrs26_reg_write(PORT_CONTROL_REGISTER0,
                athrs26_reg_read(PORT_CONTROL_REGISTER0) | 0x1000);
            forward_hang_recover_timer.expires = jiffies + HZ/10;
            add_timer(&forward_hang_recover_timer);
        }
    }

    if(eth_dbg)
        mod_timer(&forward_hang_check_timer, jiffies + eth_dbg * HZ);
    else
        mod_timer(&forward_hang_check_timer,
            jiffies + AG7100_FORWARD_HANG_CHECK_TIMER_SECONDS * HZ);
}

static void ag7100_forward_hang_recover_timer(unsigned long data)
{
    athrs26_reg_write(PORT_CONTROL_REGISTER0,
                          athrs26_reg_read(PORT_CONTROL_REGISTER0) & (~0x1000));
}
#endif

static const struct net_device_ops ag7100_netdev_ops = {
    .ndo_open           = ag7100_open,
    .ndo_stop           = ag7100_stop,
    .ndo_start_xmit     = ag7100_hard_start,
    //	.ndo_set_multicast_list = ag7100_set_multicast_list,
    .ndo_do_ioctl       = ag7100_do_ioctl,
    .ndo_tx_timeout     = ag7100_tx_timeout,
#ifdef CONFIG_AR9100
    .ndo_change_mtu     = ag7100_change_mtu,
#else
    .ndo_change_mtu     = eth_change_mtu,
#endif
    .ndo_set_mac_address= eth_mac_addr,
    .ndo_validate_addr  = eth_validate_addr,
    .ndo_get_stats      = ag7100_get_stats,
#ifdef CONFIG_NET_POLL_CONTROLLER
    .ndo_poll_controller= ag7100_poll_controller,
#endif
};

/*
 * All allocations (except irq and rings).
 */
static int __init
ag7100_init(void)
{
    int i;
    struct net_device *dev;
    ag7100_mac_t      *mac=NULL;
    ag7100_mac_t      **pmac;
    uint32_t mask;
#if defined(CONFIG_MV_PHY)
    unsigned short phy0Id, phy1Id;
#endif
#if ETHNAMES
    int ethnameIdx=0;
#endif
#if defined(CONFIG_VENETDEV)
    int             totalDev=1; // gd11 totalDev will be 1 or 3
    int             nextDev=0;
#endif
#if HANDLE_FLIP_PHY
    if ( flip_phy != 1 || AG7100_NMACS != 2 )  flip_phy = 0 ; //  only support 0 or 1
#endif
#if ETHNAMES
    for ( i=0; i< MAX_ETH_DEV; i++ ) {
        sprintf(ethname_array[i], "eth%d", i);
    }
    if ( ethnames != NULL ) {
        char *ptr, *ptr1;
        char buffer[32];
        strcpy( buffer, ethnames );
        ptr = buffer;
        for(i=0; i < MAX_ETH_DEV; i++ ) {
            ptr1 = strpbrk( ptr, "@" );
            if( ptr1 != NULL ) {                /* Have we hit the last one? */
                    *ptr1 = '\0';               /* make name into string     */
                    ptr1++;                     /* bump over EOS             */
            }
            strcpy( ethname_array[i], ptr );    /* copy name to array   */
            if (ptr1 == NULL)
                break;
#if defined(CONFIG_VENETDEV)
            totalDev++;
#endif
            ptr = ptr1;
        }
    }
    if (rks_is_gd11_board()) {
        ethname_remap = &gd11_ethname_remap_1[0];
#if defined(CONFIG_VENETDEV)
        if (totalDev > 1)
            ethname_remap = &gd11_ethname_remap[0];
#endif
    } else if (rks_is_walle_board()) {
        ethname_remap = &walle_ethname_remap[0];
    } else if (rks_is_gd36_board()) {
        ethname_remap = &gd36_ethname_remap[0];
    }
#endif

    /*
    * tx_len_per_ds is the number of bytes per data transfer in word increments.
    *
    * If the value is 0 than we default the value to a known good value based
    * on benchmarks. Otherwise we use the value specified - within some
    * cosntraints of course.
    *
    * Tested working values are 256, 512, 768, 1024 & 1536.
    *
    * A value of 256 worked best in all benchmarks. That is the default.
    *
    */

    /* Tested 256, 512, 768, 1024, 1536 OK, 1152 and 1280 failed*/
    if (0 == tx_len_per_ds) {
        tx_len_per_ds = CONFIG_AG7100_LEN_PER_TX_DS;
    } else {
        switch (tx_len_per_ds) {
            case 256:
            case 512:
            case 768:
            case 1024:
            case 1536:
                break;
            default:
                tx_len_per_ds = CONFIG_AG7100_LEN_PER_TX_DS;
                break;
        }
    }

    ag7100_vet_tx_len_per_pkt( &tx_len_per_ds);

#if defined(V54_BSP)
    printk(MODULE_NAME ": Length per segment %d\n", tx_len_per_ds);
#endif

    /*
    * Compute the number of descriptors for an MTU
    */
#if !defined(CONFIG_AR9100) && !defined(CONFIG_AR7240)
    tx_max_desc_per_ds_pkt = AG7100_TX_MAX_DS_LEN / tx_len_per_ds;
    if (AG7100_TX_MAX_DS_LEN % tx_len_per_ds) tx_max_desc_per_ds_pkt++;
#else
    tx_max_desc_per_ds_pkt =1;
#endif

#if defined(V54_BSP)
    printk(MODULE_NAME ": Max segments per packet %d\n", tx_max_desc_per_ds_pkt);
    printk(MODULE_NAME ": Max tx descriptor count    %d\n", AG7100_TX_DESC_CNT);
    printk(MODULE_NAME ": Max rx descriptor count    %d\n", AG7100_RX_DESC_CNT);
#endif

    /*
    * Let hydra know how much to put into the fifo in words (for tx)
    */
    if (0 == fifo_3) {
        if (rks_is_walle_board() || rks_is_gd26_board() || rks_is_spaniel_board() || rks_is_gd36_board())
            fifo_3 = 0x1f00140;
        else
            fifo_3 = 0x000001ff | ((AG7100_TX_FIFO_LEN-tx_len_per_ds)/4)<<16;
    }

#if !defined(V54_BSP)
    printk(MODULE_NAME ": fifo cfg 3 %08x\n", fifo_3);
#endif

    /*
    ** Do the rest of the initializations
    */

#if defined(CONFIG_VENETDEV)
    if (rks_is_gd11_board() && (totalDev > 2)) {
        venetEnabled = 1;
        dev_per_mac = totalDev - 1;
    } else if (rks_is_gd17_board() && (totalDev > 1)) {
        // gd17(vf7835): ar8228+ar8021
        venetEnabled = 1;
        dev_per_mac = totalDev;
    } else if (rks_is_walle_board() && (totalDev > 2)) {
        venetEnabled = 1;
        dev_per_mac = totalDev - 1;
    }
    printk("%s: venetEnabled=%d dev_per_mac=%d\n", __func__, venetEnabled, dev_per_mac);
    for(i = 0; i < ((totalDev<AG7100_NMACS)?totalDev:AG7100_NMACS); i++)
    {
        if (rks_is_gd11_board() && i && !venetEnabled)
            break;
#else
    for(i = 0; i < AG7100_NMACS; i++)
    {
#endif
#if defined(CONFIG_AR7240)
        if (rks_is_gd17_board() && i)
            break;
#endif
        if (rks_is_spaniel_board() & i)
            break;

        mac = kmalloc(sizeof(ag7100_mac_t), GFP_KERNEL);
        if (!mac)
        {
            printk(MODULE_NAME ": unable to allocate mac\n");
            return 1;
        }
        memset(mac, 0, sizeof(ag7100_mac_t));
        printk("mac%d=%p\n", i, mac);

        mac->mac_unit               =  i;
        mac->mac_base               =  ag7100_mac_base(i);
        mac->mac_irq                =  ag7100_mac_irq(i);
        ag7100_macs[i]              =  mac;
        spin_lock_init(&mac->mac_lock);
        mac->mac_fdx                =  -1;
        mac->mac_speed              =  -1;
        /*
         * out of memory timer
         */
        init_timer(&mac->mac_oom_timer);
        mac->mac_oom_timer.data     = (unsigned long)mac;
        mac->mac_oom_timer.function = ag7100_oom_timer;
        /*
         * watchdog task
         */
#ifdef V54_BSP
        INIT_WORK(&mac->mac_tx_timeout, (void *)ag7100_tx_timeout_task);
#else
        INIT_WORK(&mac->mac_tx_timeout, ag7100_tx_timeout_task);
#endif

#if ETHNAMES
        printk("%s\n", ethname_array[ethname_remap[ethnameIdx]]);
        dev = alloc_netdev(sizeof(void *), ethname_array[ethname_remap[ethnameIdx++]], ether_setup);
#else
        dev = alloc_etherdev(sizeof(void *));
#endif
        if (!dev)
        {
            kfree(mac);
            printk("%s Freeing at 0x%lx\n",__func__,(unsigned long) mac);
            printk(MODULE_NAME ": unable to allocate etherdev\n");
            return 1;
        }

        mac->mac_dev         =  dev;
        dev->priv_flags      |= IFF_ETHER_DEVICE;
        dev->netdev_ops      = &ag7100_netdev_ops;
//        dev->weight          =  AG7100_NAPI_WEIGHT;
        pmac                 = netdev_priv(dev);
        *pmac                =  mac;
        netif_napi_add(dev, &mac->napi, ag7100_poll, AG7100_NAPI_WEIGHT);


#ifdef V54_BSP
        if (!bd_get_lan_macaddr(mac->mac_unit, dev->dev_addr)) {
            ag7100_get_macaddr(mac->mac_unit, dev->dev_addr);
        }
        {
            int addr;
            printk("dev%d=0x%x macaddr=", mac->mac_unit, (u32)dev);
            for (addr=0; addr<ETH_ALEN; addr++) {
                printk("%02x ", dev->dev_addr[addr]);
            }
            printk("\n");
        }
#else
        ag7100_get_default_macaddr(mac, dev->dev_addr);
#endif

#if defined(CONFIG_MV_PHY)
        if (!mac->mac_unit) {
            phy0Id = phy1Id = 0;
            probe_ethphy(mac, &phy0Id, &phy1Id);
        }
#endif
#if defined(CONFIG_VENETDEV)
        if (rks_is_gd11_board() && !mac->mac_unit) {
            if (phy1Id != RTL_PHY_ID1) {
                venetEnabled = 0;
                printk("venetEnabled=0\n");
            } else {
                nextDev = 1;
            }
        } else if (rks_is_gd17_board() && !mac->mac_unit) {
            if (phy0Id != AR8228_PHY_ID0) {
                venetEnabled = 0;
                printk("venetEnabled=0\n");
            }
        } else if (rks_is_walle_board() && !mac->mac_unit) {
            nextDev = 1;
        }

        if (venet_enable(mac)) {
            dev->hard_header_len += 10;

            mac->vdev_state =
                kmalloc((sizeof(vdev_sw_state_t)*dev_per_mac), GFP_KERNEL);

            if (!mac->vdev_state) {
                printk("ag7100 cannot allocate vdev_state\n");
                goto failed;
            } else {
                memset(mac->vdev_state, 0, (sizeof(vdev_sw_state_t)*dev_per_mac));
                mac->vdev_state[0].enetUnit     = nextDev;
                mac->vdev_state[0].unit_on_MAC  = 0;
                mac->vdev_state[0].up           = 0;
                mac->vdev_state[0].carrier      = 0;
                mac->vdev_state[0].dev          = dev;
                mac->vdev_state[0].mac          = mac;
                nextDev++;
#if VENETDEV_DEBUG
                printk("dev=0x%x vdev 0 unit_on_MAC(%d)\n", (unsigned int)dev, mac->vdev_state[0].unit_on_MAC);
#endif
            }
        }
#endif
        if (register_netdev(dev))
        {
            printk(MODULE_NAME ": register netdev failed\n");
            goto failed;
        }

#if defined(CONFIG_VENETDEV)
        if (venet_enable(mac)) {
            struct net_device *vdev;
            int unitOnMac;

            for (unitOnMac=1; unitOnMac < dev_per_mac; unitOnMac++) {
#if ETHNAMES
                printk("%s\n", ethname_array[ethname_remap[ethnameIdx]]);
                vdev = alloc_netdev(sizeof(void *), ethname_array[ethname_remap[ethnameIdx++]], ether_setup);
#else
                vdev = alloc_etherdev(sizeof(void *));
#endif
                vdev->priv_flags      |= IFF_ETHER_DEVICE;
                vdev->netdev_ops      = &ag7100_netdev_ops;
                pmac                  = netdev_priv(vdev);
                *pmac                 =  mac;

#if 0 /* to do.by gun.maybe we need to judge in  ag7100_do_ioctl*/
                if (rks_is_walle_board() || rks_is_gd11_board())
                    vdev->do_ioctl    =  ag7100_do_ioctl;
#endif
                if (!bd_get_lan_macaddr(nextDev, vdev->dev_addr)) {
                    ag7100_get_macaddr(nextDev, vdev->dev_addr);
                } else {
                    int addr;
                    printk("dev%d=0x%x macaddr=", nextDev, (u32)vdev);
                    for (addr=0; addr<ETH_ALEN; addr++) {
                        printk("%02x ", vdev->dev_addr[addr]);
                    }
                    printk("\n");
                }

                vdev->hard_header_len += 10;

                mac->vdev_state[unitOnMac].enetUnit     = nextDev;
                mac->vdev_state[unitOnMac].unit_on_MAC  = unitOnMac;
                mac->vdev_state[unitOnMac].up           = 0;
                mac->vdev_state[unitOnMac].carrier      = 0;
                mac->vdev_state[unitOnMac].dev          = vdev;
                mac->vdev_state[unitOnMac].mac          = mac;
                nextDev++;
#if VENETDEV_DEBUG
                printk("vdev=0x%x vdev unit %d unit_on_MAC(%d)\n", (unsigned int)vdev, unitOnMac, unitOnMac);
#endif

                if (register_netdev(vdev)) {
                    printk("ag7100 register vdev failed\n");
                    goto failed;
                }
            }
        }
#endif
#if 0 && defined(CONFIG_AR7240)
	netif_carrier_off(dev);
#endif
#ifdef CONFIG_AR9100
        ag7100_reg_rmw_set(mac, AG7100_MAC_CFG1, AG7100_MAC_CFG1_SOFT_RST
				| AG7100_MAC_CFG1_RX_RST | AG7100_MAC_CFG1_TX_RST);
#else
#if !defined(CONFIG_AR7240)
        ag7100_reg_rmw_set(mac, AG7100_MAC_CFG1, AG7100_MAC_CFG1_SOFT_RST);
#else
        ag7100_reg_rmw_set(mac, AG7100_MAC_CFG1, AG7100_MAC_CFG1_SOFT_RST
				| AG7100_MAC_CFG1_RX_RST | AG7100_MAC_CFG1_TX_RST);
#endif
#endif
        udelay(20);
        mask = ag7100_reset_mask(mac->mac_unit);
#if defined(CONFIG_AR7240)
        mask |= AR7240_RESET_ETH_SWITCH;
#endif

#if 0 && defined(CONFIG_AR934x) /* 9.2.0.128 */
        /* trigger ag7240_link_intr when request_irq is called? */
        if (is_ar7241() || is_ar7242() ||  is_ar934x())
             mask |=  ATH_RESET_GE0_MDIO | ATH_RESET_GE1_MDIO;
#endif

        /*
         * put into reset, hold, pull out.
         */
        ar7100_reg_rmw_set(AR7100_RESET, mask);
        mdelay(100);
        ar7100_reg_rmw_clear(AR7100_RESET, mask);
        mdelay(100);
    }
#ifdef V54_BSP
    num_macs = i;       // num_macs == AG7100_NMACS
    mac_create_proc_entries();
#endif

#if defined(CONFIG_AR7240)
    if (rks_is_walle_board() || rks_is_gd26_board() || rks_is_gd36_board()) {
        int st;
        /*
         * Enable link interrupts for PHYs
         */
        dev = ag7100_macs[0]->mac_dev;  /* 9.2.0.128: dev = mac->mac_dev; */
        st = request_irq(AR7240_MISC_IRQ_ENET_LINK, ag7240_link_intr, 0, dev->name, dev);

        if (st < 0)
        {
            printk(MODULE_NAME ": request irq %d failed %d\n", AR7240_MISC_IRQ_ENET_LINK, st);
            goto failed;
        }
        athrs26_reg_dev(ag7100_macs);

        init_timer(&forward_hang_check_timer);
        forward_hang_check_timer.function = ag7100_forward_hang_check_timer;
        mod_timer(&forward_hang_check_timer, jiffies + AG7100_FORWARD_HANG_CHECK_TIMER_SECONDS * HZ);

        init_timer(&forward_hang_recover_timer);
        forward_hang_recover_timer.function = ag7100_forward_hang_recover_timer;
    }
#endif

    ag7100_trc_init();

#if 0 //#ifdef CONFIG_AR9100
#define AP83_BOARD_NUM_ADDR ((char *)0xbf7f1244)

	board_version = (AP83_BOARD_NUM_ADDR[0] - '0') +
			((AP83_BOARD_NUM_ADDR[1] - '0') * 10);
#endif

#if defined(CONFIG_ATHRS26_PHY)
    athrs26_reg_dev(ag7100_macs);
#endif
#if 0 && defined(CONFIG_AR7240)
#ifdef ETH_SOFT_LED
    init_timer(&PLedCtrl.led_timer);
    PLedCtrl.led_timer.data     = (unsigned long)(&PLedCtrl);
    PLedCtrl.led_timer.function = (void *)led_control_func;
    mod_timer(&PLedCtrl.led_timer,(jiffies + AG7240_LED_POLL_SECONDS));
#endif
#endif

#if !defined(CONFIG_AR7240)
    if (ag7100_open(ag7100_macs[0]->mac_dev) == 0)
        mac0_dev_opened = 1;
#endif

    return 0;

failed:
    for(i = 0; i < AG7100_NMACS; i++)
    {
        if (!ag7100_macs[i])
            continue;
        if (ag7100_macs[i]->mac_dev)
            free_netdev(ag7100_macs[i]->mac_dev);
#if defined(CONFIG_VENETDEV)
        if (venet_enable(ag7100_macs[i])) {
            int unitOnMac;
            for (unitOnMac=1; unitOnMac<dev_per_mac; unitOnMac++) {
                if (ag7100_macs[i]->vdev_state[unitOnMac].dev)
                    free_netdev(ag7100_macs[i]->vdev_state[unitOnMac].dev);
            }
            if (ag7100_macs[i]->vdev_state)
                kfree(ag7100_macs[i]->vdev_state);
        }
#endif
        kfree(ag7100_macs[i]);
        printk("%s Freeing at 0x%lx\n",__func__,(unsigned long) ag7100_macs[i]);
    }
    return 1;
}

static void __exit
ag7100_cleanup(void)
{
    int i;

#if defined(CONFIG_AR7240)
    if (rks_is_walle_board())
        del_timer(&forward_hang_check_timer);
    if (rks_is_walle_board() || rks_is_gd26_board() || rks_is_gd36_board())
        free_irq(AR7240_MISC_IRQ_ENET_LINK, ag7100_macs[0]->mac_dev);
#endif

    for(i = 0; i < AG7100_NMACS; i++)
    {
#ifdef V54_BSP
    mac_cleanup_proc_entries();
#endif
#if defined(CONFIG_VENETDEV)
        if (venet_enable(ag7100_macs[i])) {
            int unitOnMac;
            for (unitOnMac=0; unitOnMac<dev_per_mac; unitOnMac++) {
                if (ag7100_macs[i]->vdev_state[unitOnMac].dev) {
                    if (ag7100_macs[i]->devIrq == ag7100_macs[i]->vdev_state[unitOnMac].dev)
                        continue;
#if VENETDEV_DEBUG
                    printk("cleanup: free dev=0x%x\n", (u32)ag7100_macs[i]->vdev_state[unitOnMac].dev);
#endif
                    unregister_netdev(ag7100_macs[i]->vdev_state[unitOnMac].dev);
                    free_netdev(ag7100_macs[i]->vdev_state[unitOnMac].dev);
                }
            }
#if VENETDEV_DEBUG
            printk("cleanup: free devIrq=0x%x\n", (u32)ag7100_macs[i]->devIrq);
#endif
            if (ag7100_macs[i]->devIrq) {
                unregister_netdev(ag7100_macs[i]->devIrq);
                free_netdev(ag7100_macs[i]->devIrq);
            }
            if (ag7100_macs[i]->vdev_state)
                kfree(ag7100_macs[i]->vdev_state);
        } else {
            unregister_netdev(ag7100_macs[i]->mac_dev);
            free_netdev(ag7100_macs[i]->mac_dev);
        }
#else
        unregister_netdev(ag7100_macs[i]->mac_dev);
        free_netdev(ag7100_macs[i]->mac_dev);
#endif
        kfree(ag7100_macs[i]);
        printk("%s Freeing at 0x%lx\n",__func__,(unsigned long) ag7100_macs[i]);
    }
    printk(MODULE_NAME ": cleanup done\n");
}

module_init(ag7100_init);
module_exit(ag7100_cleanup);
