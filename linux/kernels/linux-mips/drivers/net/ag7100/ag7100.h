#ifndef _AG7100_H
#define _AG7100_H

#include <linux/types.h>
#include <linux/spinlock_types.h>
#include <linux/workqueue.h>
#include <asm/system.h>
#include <linux/netdevice.h>
#include "ar7100.h"
#include "ag7100_trc.h"

#ifdef V54_BSP
#define BCM_PHY_ID1             0x0143
#define MV_PHY_ID1              0x0141
#define MV_PHY_MODEL(x)         ((x & 0x03f0) >> 4)
#define MV_PHY_MODEL_1121R      0x0B
#define RTL_PHY_ID1             0x1C
#define BCM_PHY0_ADDR           0
#define MV_PHY0_ADDR            0x1e
#define MV_PHY1_ADDR            0x1f
#define RTL_PHY0_ADDR           0
#define RTL_PHY1_ADDR           1
#define RTL_PHY2_ADDR           2
#define AR8228_PHY0_ADDR        0
#define AR8228_PHY_ID0          0x4D
#define AR8228_PHY_ID1          0x41
#define AR8021_PHY_ADDR         6
#define LANTIQ_PHY0_ADDR        6

#if defined(CONFIG_AR934x)
#define ATHR_SWITCH_CLK_SPARE		     0x18050024
#endif

#if !defined(CONFIG_AR9100) && !defined(CONFIG_AR7240)
#define MAC_HW_UNSTOP           1
#endif

#if defined(CONFIG_MV_PHY) && !defined(CONFIG_AR9100)
#define AG7100_NMACS            2
#else
#define AG7100_NMACS            1
#endif

#define DMA_DEBUG   1
#define HANDLE_FLIP_PHY 1               /* cross-connecting mac and phy in revA*/

#if HANDLE_FLIP_PHY
// only support 2 MACs
extern int flip_phy ;
#define MAC_TO_PHY(n)   ((flip_phy == n) ? 0 : 1)
#else
#define MAC_TO_PHY(n)   (n)
#endif

#endif

/*
 * h/w descriptor
 */
typedef struct {
    uint32_t    pkt_start_addr;

    uint32_t    is_empty       :  1;
    uint32_t    res1           :  6;
    uint32_t    more           :  1;
    uint32_t    res2           :  3;
    uint32_t    ftpp_override  :  5;
    uint32_t    res3           :  4;
    uint32_t    pkt_size       : 12;

    uint32_t    next_desc      ;
}ag7100_desc_t;

/*
 * s/w descriptor
 */
typedef struct {
    struct sk_buff *buf_pkt;        /*ptr to skb*/
    int             buf_nds;        /*no. of desc for this skb*/
    ag7100_desc_t  *buf_lastds;     /*the last desc. (for convenience)*/
#ifdef DMA_DEBUG
    unsigned long   trans_start;    /*  descriptor time stamp */
#endif
}ag7100_buffer_t;

/*
 * Tx and Rx descriptor rings;
 */
typedef struct {
    ag7100_desc_t     *ring_desc;           /* hardware descriptors */
    dma_addr_t         ring_desc_dma;       /* dma addresses of desc*/
    ag7100_buffer_t   *ring_buffer;         /* OS buffer info       */
    int                ring_head;           /* producer index       */
    int                ring_tail;           /* consumer index       */
    int                ring_nelem;          /* nelements            */
}ag7100_ring_t;

typedef struct {
    int stats;
}ag7100_stats_t;

/*
 * 0, 1, 2: based on hardware values for mii ctrl bits [5,4]
 */
typedef enum {
    AG7100_PHY_SPEED_10T,
    AG7100_PHY_SPEED_100TX,
    AG7100_PHY_SPEED_1000T,
}ag7100_phy_speed_t;

/*
 * Represents an ethernet MAC. Contains ethernet devices (LAN and WAN)
 */
#define AG7100_NVDEVS   2

#ifdef V54_BSP
typedef struct sw_stats {
    uint32_t stat_tx;       /* reaped packets */
    uint32_t stat_tx_int;   /* isr AG7100_INTR_TX*/
    uint32_t stat_tx_be;    /* isr AG7100_INTR_TX_BUS_ERROR */
    uint32_t stat_tx_full;  /* same as tx_fifo_errors*/
    uint32_t stat_rx;       /* isr AG7100_INTR_RX | AG7100_INTR_RX_OVF */
    uint32_t stat_rx_be;    /* isr AG7100_INTR_RX_BUS_ERROR */
    uint32_t stat_rx_full;  /* poll AG7100_RX_STATUS_OOM */
    uint32_t stat_tx_timeout;
#if defined(CONFIG_AR7240)
    uint32_t stat_fwd_hang;
#endif
    uint32_t stat_dma_reset;
    uint32_t stat_rx_ff_err;
} sw_stats_t;
#endif

typedef struct {
    struct net_device      *mac_dev;
    uint32_t                mac_unit;
    uint32_t                mac_base;
    int                     mac_irq;
    ag7100_ring_t           mac_txring;
    ag7100_ring_t           mac_rxring;
    ag7100_stats_t          mac_stats;
    spinlock_t              mac_lock;
    struct timer_list       mac_oom_timer;
    struct work_struct      mac_tx_timeout;
    struct net_device_stats mac_net_stats;
    ag7100_phy_speed_t      mac_speed;
    int                     mac_fdx;
    struct timer_list       mac_phy_timer;
#ifdef DMA_DEBUG
    struct timer_list       mac_dbg_timer;
#endif
#if defined(CONFIG_AR9100) && defined(CONFIG_AG7100_GE1_RMII)
    int                     speed_10t;
#endif
    ag7100_trc_t            tb;
#ifdef V54_BSP
    struct sw_stats         sw_stats;
    int                     mac_init;
    int                     dma_check;
#endif
#ifdef CONFIG_VENETDEV
    struct vdev_sw_state   *vdev_state;
    int                     mac_open;
    struct net_device       *devIrq;
#endif
    struct napi_struct      napi;
}ag7100_mac_t;

#ifdef CONFIG_VENETDEV
typedef struct vdev_sw_state {
    int                     enetUnit;        /* system unit number "eth%d" */
    int                     unit_on_MAC;     /* MAC-relative unit number */
    int                     up;
    int                     carrier;
    struct net_device_stats net_stats;
    struct net_device       *dev;
    ag7100_mac_t            *mac;
} vdev_sw_state_t;
#endif

#define net_rx_packets      mac_net_stats.rx_packets
#define net_rx_fifo_errors  mac_net_stats.rx_fifo_errors
#define net_tx_packets      mac_net_stats.tx_packets
#define net_rx_bytes        mac_net_stats.rx_bytes
#define net_tx_bytes        mac_net_stats.tx_bytes
#define net_rx_over_errors  mac_net_stats.rx_over_errors
#define net_tx_dropped      mac_net_stats.tx_dropped;

#define ag7100_dev_up(_dev)                                     \
    (((_dev)->flags & (IFF_RUNNING|IFF_UP)) != (IFF_RUNNING|IFF_UP))

typedef enum {
    AG7100_RX_STATUS_DONE,
    AG7100_RX_STATUS_NOT_DONE,
    AG7100_RX_STATUS_OOM,
#if defined(CONFIG_AR9100) || defined(CONFIG_AG7100_DMA_HANG_FIX)
    AG7100_RX_DMA_HANG
#endif
}ag7100_rx_status_t;

/*
 * This defines the interconnects between MAC and PHY at compile time
 * There are several constraints - the Kconfig largely takes care of them
 * at compile time.
 */
#if defined (CONFIG_AG7100_GE0_GMII)
    #define     AG7100_MII0_INTERFACE   0
#elif defined (CONFIG_AG7100_GE0_MII)
    #define     AG7100_MII0_INTERFACE   1
#elif defined (CONFIG_AG7100_GE0_RGMII)
    #define     AG7100_MII0_INTERFACE   2
#elif defined (CONFIG_AG7100_GE0_RMII)
    #define     AG7100_MII0_INTERFACE   3
#endif /*defined (AG7100_GE0_GMII)*/

/*
 * Port 1 may or may not be connected
 */
#if defined(CONFIG_AG7100_GE1_IS_CONNECTED)

#ifndef CONFIG_MV_PHY
    #define AG7100_NMACS            2
#endif

    #if defined (CONFIG_AG7100_GE1_RGMII)
        #define AG7100_MII1_INTERFACE   0
    #elif defined (CONFIG_AG7100_GE1_RMII)
        #define AG7100_MII1_INTERFACE   1
    #endif /*AG7100_GE1_RGMII*/
#else
#ifndef CONFIG_MV_PHY
    #define AG7100_NMACS            1
#endif
    #define AG7100_MII1_INTERFACE   0xff        /*not connected*/
#endif  /*AG7100_GE1_IS_CONNECTED*/

#define mii_reg(_mac)   (AR7100_MII0_CTRL + ((_mac)->mac_unit * 4))
#define mii_if(_mac)    (((_mac)->mac_unit == 0) ? mii0_if : mii1_if)

#define ag7100_set_mii_ctrl_speed(_mac, _spd)   do {                        \
    ar7100_reg_rmw_clear(mii_reg(_mac), (3 << 4));                          \
    ar7100_reg_rmw_set(mii_reg(_mac), ((_spd) << 4));                       \
}while(0);

//------------------------------------------------------------------------------
#ifdef V54_BSP
#ifdef CONFIG_VLAN_8021Q
#define VLAN_TPID                   0x8100
#define VLAN_TAG_LEN                4
#define ATH_HEADER_LEN              4
#define _ETH_MAX_FRAME_LEN      ( ETH_FRAME_LEN + VLAN_TAG_LEN + ATH_HEADER_LEN )
#else
#define _ETH_MAX_FRAME_LEN      ( ETH_FRAME_LEN + ATH_HEADER_LEN )
#endif
#endif

#define LINK_SHIFT                  1
#define LINK_MASK                   1
#define DUPLEX_SHIFT                1
#define DUPLEX_MASK                 1
#define SPEED_SHIFT                 2
#define SPEED_MASK                  3

#ifdef CONFIG_VENETDEV
#define RTL_TYPE                    0x8899
#define RTL_PROTOCOL                0xb
#define RTL_PROTOCOL_SHIFT          12
#define RTL_TYPE_LEN                2
#define RTL_TAG_LEN                 2
#define RTL_MGNT_TAG_LEN            (RTL_TYPE_LEN+RTL_TAG_LEN)
#define RTL_SRC_PORT_ID_MASK        0x07

#define ATHR_VER_PRI_TYPE_CPU       0x8080
#define ATHR_PORT_6                 6
#define ATHR_MGNT_TAG_LEN           2
#define ATHR_SRC_PORT_ID_MASK       0x7f

#define AR7240_VER_PRI_TYPE         0x40
#define AR7240_CPU_VER              0x70
#endif
//------------------------------------------------------------------------------

/*
 * IP needs 16 bit alignment. But RX DMA needs 4 bit alignment. We sacrifice IP
 * Plus Reserve extra head room for wmac
 */
#define ETHERNET_FCS_SIZE            4
#define AG7100_RX_RESERVE           ((LWAPP_TUNNEL_HEADER_ROOM) - (NET_SKB_PAD))
#ifdef V54_BSP
#define AG7100_RX_BUF_SIZE      \
    (AG7100_RX_RESERVE + ETH_HLEN + _ETH_MAX_FRAME_LEN + ETHERNET_FCS_SIZE)
#else
#define AG7100_RX_BUF_SIZE      \
    (AG7100_RX_RESERVE + ETH_HLEN + ETH_FRAME_LEN + ETHERNET_FCS_SIZE)
#endif


#define ag7100_mac_base(_no)    (_no) ? AR7100_GE1_BASE    : AR7100_GE0_BASE
#define ag7100_mac_irq(_no)     (_no) ? AR7100_CPU_IRQ_GE1 : AR7100_CPU_IRQ_GE0

#if defined(CONFIG_AR7240)
#define ag7100_reset_mask(_no) (_no) ? (AR7100_RESET_GE1_MAC)  \
                                     : (AR7100_RESET_GE0_MAC)
#else
#define ag7100_reset_mask(_no) (_no) ? (AR7100_RESET_GE1_MAC |  \
                                        AR7100_RESET_GE1_PHY)   \
                                     : (AR7100_RESET_GE0_MAC |  \
                                        AR7100_RESET_GE0_PHY)
#endif

#define ag7100_unit2mac(_unit)     ag7100_macs[(_unit)]

#define assert(_cond)   do           {                          \
    if(!(_cond))       {                                        \
        ag7100_trc_dump();                                      \
        printk("%s:%d: assertion failed\n", __func__, __LINE__); \
        BUG();                                                   \
    }                                                           \
}while(0);

#ifdef V54_BSP
#define assert_rx(_cond, mac)   do           {                             \
    if(!(_cond))       {                                                   \
        ag7100_ring_t   *r     = &mac->mac_rxring;                         \
        printk("assert %s %d\n*** head=%d tail=%d pkt=%p\n", __FILE__, __LINE__, \
                r->ring_head, r->ring_tail, r->ring_buffer[r->ring_tail].buf_pkt); \
        panic("*** ag7100 RX ***");                                        \
    }                                                                      \
}while(0);
#endif


/*
 * Config/Mac Register definitions
 */
#define AG7100_MAC_CFG1             0x00
#define AG7100_MAC_CFG2             0x04
#define AG7100_MAC_IFCTL            0x38

/*
 * fifo control registers
 */
#define AG7100_MAC_FIFO_CFG_0      0x48
#define AG7100_MAC_FIFO_CFG_1      0x4c
#define AG7100_MAC_FIFO_CFG_2      0x50
#define AG7100_MAC_FIFO_CFG_3      0x54
#define AG7100_MAC_FIFO_CFG_4      0x58

#define AG7100_MAC_FIFO_CFG_5      0x5c
#define AG7100_BYTE_PER_CLK_EN     (1 << 19)

#define AG7100_MAC_FIFO_RAM_0      0x60
#define AG7100_MAC_FIFO_RAM_1      0x64
#define AG7100_MAC_FIFO_RAM_2      0x68
#define AG7100_MAC_FIFO_RAM_3      0x6c
#define AG7100_MAC_FIFO_RAM_4      0x70
#define AG7100_MAC_FIFO_RAM_5      0x74
#define AG7100_MAC_FIFO_RAM_6      0x78
#define AG7100_MAC_FIFO_RAM_7      0x7c

/*
 * fields
 */
#define AG7100_MAC_CFG1_SOFT_RST       (1 << 31)
#define AG7100_MAC_CFG1_RX_RST         (1 << 19)
#define AG7100_MAC_CFG1_TX_RST         (1 << 18)
#define AG7100_MAC_CFG1_LOOPBACK       (1 << 8)
#define AG7100_MAC_CFG1_RX_EN          (1 << 2)
#define AG7100_MAC_CFG1_TX_EN          (1 << 0)
#define AG7100_MAC_CFG1_RX_FCTL        (1 << 5)
#define AG7100_MAC_CFG1_TX_FCTL        (1 << 4)


#define AG7100_MAC_CFG2_FDX            (1 << 0)
#define AG7100_MAC_CFG2_CRC_EN         (1 << 1)
#define AG7100_MAC_CFG2_PAD_CRC_EN     (1 << 2)
#define AG7100_MAC_CFG2_LEN_CHECK      (1 << 4)
#define AG7100_MAC_CFG2_HUGE_FRAME_EN  (1 << 5)
#define AG7100_MAC_CFG2_IF_1000        (1 << 9)
#define AG7100_MAC_CFG2_IF_10_100      (1 << 8)

#define AG7100_MAC_IFCTL_SPEED         (1 << 16)

/*
 * DMA (tx/rx) register defines
 */
#define AG7100_DMA_TX_CTRL              0x180
#define AG7100_DMA_TX_DESC              0x184
#define AG7100_DMA_TX_STATUS            0x188
#define AG7100_DMA_RX_CTRL              0x18c
#define AG7100_DMA_RX_DESC              0x190
#define AG7100_DMA_RX_STATUS            0x194
#define AG7100_DMA_INTR_MASK            0x198
#define AG7100_DMA_INTR                 0x19c

#define AG7240_DMA_RXFSM                0x1b0
#define AG7240_DMA_TXFSM                0x1b4
#define AG7240_DMA_XFIFO_DEPTH          0x1a8

/*
 * DMA status mask.
 */

#define AG7240_DMA_DMA_STATE           0x3
#define AG7240_DMA_AHB_STATE           0x7

/*
 * tx/rx ctrl and status bits
 */
#define AG7100_TXE                      (1 << 0)
#define AG7100_TX_STATUS_PKTCNT_SHIFT   16
#define AG7100_TX_STATUS_PKT_SENT       0x1
#define AG7100_TX_STATUS_URN            0x2
#define AG7100_TX_STATUS_BUS_ERROR      0x8

#define AG7100_RXE                      (1 << 0)

#define AG7100_RX_STATUS_PKTCNT_MASK    0xff0000
#define AG7100_RX_STATUS_PKT_RCVD       (1 << 0)
#define AG7100_RX_STATUS_OVF            (1 << 2)
#define AG7100_RX_STATUS_BUS_ERROR      (1 << 3)

/*
 * Int and int mask
 */
#define AG7100_INTR_TX                  (1 << 0)
#define AG7100_INTR_TX_URN              (1 << 1)
#define AG7100_INTR_TX_BUS_ERROR        (1 << 3)
#define AG7100_INTR_RX                  (1 << 4)
#define AG7100_INTR_RX_OVF              (1 << 6)
#define AG7100_INTR_RX_BUS_ERROR        (1 << 7)

/*
 * MII registers
 */
#define AG7100_MAC_MII_MGMT_CFG         0x20
#ifdef V54_BSP
#define AG7100_MGMT_CFG_CLK_DIV_20      0x07
#else
#define AG7100_MGMT_CFG_CLK_DIV_20      0x06
#endif
#define AG7100_MII_MGMT_CMD             0x24
#define AG7100_MGMT_CMD_READ            0x1
#define AG7100_MII_MGMT_ADDRESS         0x28
#define AG7100_ADDR_SHIFT               8
#define AG7100_MII_MGMT_CTRL            0x2c
#define AG7100_MII_MGMT_STATUS          0x30
#define AG7100_MII_MGMT_IND             0x34
#define AG7100_MGMT_IND_BUSY            (1 << 0)
#define AG7100_MGMT_IND_INVALID         (1 << 2)
#define AG7100_GE_MAC_ADDR1             0x40
#define AG7100_GE_MAC_ADDR2             0x44
#define AG7100_MII0_CONTROL             0x18070000

#if defined(CONFIG_AR7240)
/*
 * MIB Registers
 */
#define AG7240_RX_PKT_CNTR		0xa0
#define AG7240_TX_PKT_CNTR		0xe4
#define AG7240_RX_BYTES_CNTR		0x9c
#define AG7240_TX_BYTES_CNTR		0xe0
#define AG7240_RX_LEN_ERR_CNTR		0xc0
#define AG7240_RX_OVL_ERR_CNTR		0xd0
#define AG7240_RX_CRC_ERR_CNTR		0xa4
#define AG7240_RX_FRM_ERR_CNTR		0xbc
#define AG7240_RX_CODE_ERR_CNTR		0xc4
#define AG7240_RX_CRS_ERR_CNTR		0xc8
#define AG7240_RX_DROP_CNTR		0xdc
#define AG7240_TX_DROP_CNTR		0x114
#define AG7240_RX_MULT_CNTR		0xa8
#define AG7240_TX_MULT_CNTR		0xe8
#define AG7240_TOTAL_COL_CNTR		0x10c
#define AG7240_TX_CRC_ERR_CNTR		0x11c

/*
 * Ethernet config registers
 */
#define AG7240_ETH_CFG                  0x18070000
#define AG7240_ETH_CFG_RGMII_GE0        (1<<0)
#define AG7240_ETH_CFG_MII_GE0          (1<<1)
#define AG7240_ETH_CFG_GMII_GE0         (1<<2)
#define AG7240_ETH_CFG_MII_GE0_MASTER   (1<<3)
#define AG7240_ETH_CFG_MII_GE0_SLAVE    (1<<4)
#define AG7240_ETH_CFG_GE0_ERR_EN       (1<<5)
#define AG7240_ETH_CFG_SW_ONLY_MODE     (1<<6)
#define AG7240_ETH_CFG_SW_PHY_SWAP      (1<<7)
#define AG7240_ETH_CFG_SW_PHY_ADDR_SWAP (1<<8)
#define AG7240_ETH_CFG_RXD_DELAY        (1 << 14)
#define AG7240_ETH_CFG_RDV_DELAY        (1 << 16)
#define AG7240_ETH_CFG_TXD_DELAY        (1 << 18)
#define AG7240_ETH_CFG_TEN_DELAY        (1 << 20)
#define AG7240_ETH_CFG_RXD_DELAY_SHIFT  14
#define AG7240_ETH_CFG_RDV_DELAY_SHIFT  16
#define AG7240_ETH_CFG_TXD_DELAY_SHIFT  18
#define AG7240_ETH_CFG_TEN_DELAY_SHIFT  20
#endif


/*
 * Everything but TX
 */
#define AG7100_INTR_MASK    (AG7100_INTR_RX | AG7100_INTR_RX_OVF |  \
                             AG7100_INTR_RX_BUS_ERROR |             \
                             AG7100_INTR_TX_BUS_ERROR              \
                             /*| AG7100_INTR_TX_URN | AG7100_INTR_TX*/)

#define ag7100_reg_rd(_mac, _reg)                                       \
            (ar7100_reg_rd((_mac)->mac_base + (_reg)))

#define ag7100_reg_wr(_mac, _reg, _val)                                 \
                ar7100_reg_wr((_mac)->mac_base + (_reg), (_val));

/*
 * The no flush version
 */
#define ag7100_reg_wr_nf(_mac, _reg, _val)                             \
                ar7100_reg_wr_nf((_mac)->mac_base + (_reg), (_val));

#define ag7100_reg_rmw_set(_mac, _reg, _mask)                           \
                 ar7100_reg_rmw_set((_mac)->mac_base + (_reg), (_mask));

#define ag7100_reg_rmw_clear(_mac, _reg, _mask)                          \
                 ar7100_reg_rmw_clear((_mac)->mac_base + (_reg), (_mask));


#define ag7100_desc_dma_addr(_r, _ds)                                   \
    (u32)((ag7100_desc_t *)(_r)->ring_desc_dma + ((_ds) - ((_r)->ring_desc)))


/*
 * tx/rx stop start
 */
#define ag7100_tx_stopped(_mac)                                         \
    (!(ag7100_reg_rd((_mac), AG7100_DMA_TX_CTRL) & AG7100_TXE))

#define ag7100_rx_start(_mac)                                           \
    ag7100_reg_wr((_mac), AG7100_DMA_RX_CTRL, AG7100_RXE)

#define ag7100_rx_stop(_mac)                                            \
    ag7100_reg_wr((_mac), AG7100_DMA_RX_CTRL, 0)

#define ag7100_tx_start(_mac)                                           \
    ag7100_reg_wr((_mac), AG7100_DMA_TX_CTRL, AG7100_TXE)

#define ag7100_tx_stop(_mac)                                            \
    ag7100_reg_wr((_mac), AG7100_DMA_TX_CTRL, 0)

static inline int
ag7100_ndesc_unused(ag7100_mac_t *mac, ag7100_ring_t *ring)
{
    int head = ring->ring_head, tail = ring->ring_tail;

    return ((tail > head ? 0 : ring->ring_nelem) + tail - head);
}
static inline int ag7100_rx_ring_full(ag7100_mac_t *mac)
{
    ag7100_ring_t *r    = &mac->mac_rxring;
    int            tail = r->ring_tail;

    return ((r->ring_head == tail) && r->ring_buffer[tail].buf_pkt);
}

#ifdef V54_BSP
static inline int ag7100_rx_ring_empty(ag7100_mac_t *mac)
{
    ag7100_ring_t *r    = &mac->mac_rxring;
    int            tail = r->ring_tail;

    return ((r->ring_head == tail) && !r->ring_buffer[tail].buf_pkt);
}
#endif

#define ag7100_ring_incr(_idx)                                             \
        if(unlikely(++(_idx) == r->ring_nelem)) (_idx) = 0;

/*
 * ownership of descriptors between DMA and cpu
 */
#define ag7100_rx_owned_by_dma(_ds)     ((_ds)->is_empty == 1)
#define ag7100_rx_give_to_dma(_ds)      ((_ds)->is_empty = 1)
#define ag7100_tx_owned_by_dma(_ds)     ((_ds)->is_empty == 0)
#define ag7100_tx_give_to_dma(_ds)      ((_ds)->is_empty = 0)
#define ag7100_tx_own(_ds)              ((_ds)->is_empty = 1)

/*
 * Interrupts
 * ----------
 */
#define ag7100_get_isr(_mac) ag7100_reg_rd((_mac), AG7100_DMA_INTR);
#define ag7100_int_enable(_mac)                                         \
    ag7100_reg_wr(_mac, AG7100_DMA_INTR_MASK, AG7100_INTR_MASK)

#define ag7100_int_disable(_mac)                                        \
    ag7100_reg_wr(_mac, AG7100_DMA_INTR_MASK, 0)
/*
 * ACKS:
 * - We just write our bit - its write 1 to clear.
 * - These are not rmw's so we dont need locking around these.
 * - Txurn and rxovf are not fastpath and need consistency, so we use the flush
 *   version of reg write.
 * - ack_rx is done every packet, and is largely only for statistical purposes;
 *   so we use the no flush version and later cause an explicite flush.
 */
#define ag7100_intr_ack_txurn(_mac)                                           \
    ag7100_reg_wr((_mac), AG7100_DMA_TX_STATUS, AG7100_TX_STATUS_URN);
#define ag7100_intr_ack_rx(_mac)                                              \
    ag7100_reg_wr_nf((_mac), AG7100_DMA_RX_STATUS, AG7100_RX_STATUS_PKT_RCVD);
#define ag7100_intr_ack_rxovf(_mac)                                           \
    ag7100_reg_wr((_mac), AG7100_DMA_RX_STATUS, AG7100_RX_STATUS_OVF);
/*
 * Not used currently
 */
#define ag7100_intr_ack_tx(_mac)                                              \
    ag7100_reg_wr((_mac), AG7100_DMA_TX_STATUS, AG7100_TX_STATUS_PKT_SENT);
#define ag7100_intr_ack_txbe(_mac)                                            \
    ag7100_reg_wr((_mac), AG7100_DMA_TX_STATUS, AG7100_TX_STATUS_BUS_ERROR);
#define ag7100_intr_ack_rxbe(_mac)                                            \
    ag7100_reg_wr((_mac), AG7100_DMA_RX_STATUS, AG7100_RX_STATUS_BUS_ERROR);

/*
 * Enable Disable. These are Read-Modify-Writes. Sometimes called from ISR
 * sometimes not. So the caller explicitely handles locking.
 */
#define ag7100_intr_disable_txurn(_mac)                                     \
    ag7100_reg_rmw_clear((_mac), AG7100_DMA_INTR_MASK, AG7100_INTR_TX_URN);

#define ag7100_intr_enable_txurn(_mac)                                      \
    ag7100_reg_rmw_set((_mac), AG7100_DMA_INTR_MASK, AG7100_INTR_TX_URN);

#define ag7100_intr_enable_tx(_mac)                                      \
    ag7100_reg_rmw_set((_mac), AG7100_DMA_INTR_MASK, AG7100_INTR_TX);

#define ag7100_intr_disable_tx(_mac)                                     \
    ag7100_reg_rmw_clear((_mac), AG7100_DMA_INTR_MASK, AG7100_INTR_TX);

#define ag7100_intr_disable_recv(_mac)                                      \
    ag7100_reg_rmw_clear(mac, AG7100_DMA_INTR_MASK,                         \
                        (AG7100_INTR_RX | AG7100_INTR_RX_OVF));

#define ag7100_intr_enable_recv(_mac)                                      \
    ag7100_reg_rmw_set(mac, AG7100_DMA_INTR_MASK,                          \
                        (AG7100_INTR_RX | AG7100_INTR_RX_OVF));

#if !defined(CONFIG_AR7240)
static inline void ag7100_start_rx_count(ag7100_mac_t *mac)
{
    if (mac->mac_unit == 0) {
        printk("Writing %d\n", PERF_CTL_GE0_PKT_CNT);
        ar7100_perf0_ctl(PERF_CTL_GE0_PKT_CNT);
    }
    else {
        printk("Writing %d\n", PERF_CTL_GE1_PKT_CNT);
        ar7100_perf1_ctl(PERF_CTL_GE1_PKT_CNT);
    }
}

static inline uint32_t ag7100_get_rx_count(ag7100_mac_t *mac)
{
    if (mac->mac_unit == 0) {
        return (ar7100_reg_rd(AR7100_PERF0_COUNTER));
    }
    else {
        return (ar7100_reg_rd(AR7100_PERF1_COUNTER));
    }
}
#endif
/*
 * link settings
 */
static inline void ag7100_set_mac_duplex(ag7100_mac_t *mac, int fdx)
{
    if (fdx) {
        ag7100_reg_rmw_set(mac, AG7100_MAC_CFG2, AG7100_MAC_CFG2_FDX);
    }
    else {
        ag7100_reg_rmw_clear(mac, AG7100_MAC_CFG2, AG7100_MAC_CFG2_FDX);
    }
}

static inline void ag7100_set_mac_if(ag7100_mac_t *mac, int is_1000)
{
    ag7100_reg_rmw_clear(mac, AG7100_MAC_CFG2, (AG7100_MAC_CFG2_IF_1000|
                         AG7100_MAC_CFG2_IF_10_100));
    if (is_1000) {
        ag7100_reg_rmw_set(mac, AG7100_MAC_CFG2, AG7100_MAC_CFG2_IF_1000);
        ag7100_reg_rmw_set(mac, AG7100_MAC_FIFO_CFG_5, AG7100_BYTE_PER_CLK_EN);
    }
    else {
        ag7100_reg_rmw_set(mac, AG7100_MAC_CFG2, AG7100_MAC_CFG2_IF_10_100);
        ag7100_reg_rmw_clear(mac,AG7100_MAC_FIFO_CFG_5, AG7100_BYTE_PER_CLK_EN);
    }
}

static inline void ag7100_set_mac_speed(ag7100_mac_t *mac, int is100)
{
    if (is100) {
        ag7100_reg_rmw_set(mac, AG7100_MAC_IFCTL, AG7100_MAC_IFCTL_SPEED);
    }
    else {
        ag7100_reg_rmw_clear(mac, AG7100_MAC_IFCTL, AG7100_MAC_IFCTL_SPEED);
    }
}

uint16_t ag7100_mii_read(int unit, uint32_t phy_addr, uint8_t reg);
void ag7100_mii_write(int unit, uint32_t phy_addr, uint8_t reg, uint16_t data);

#endif
