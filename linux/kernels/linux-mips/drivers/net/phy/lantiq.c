/*
 *	Driver for lantiq PHYs
 *
 */
#include <linux/module.h>
#include <linux/ethtool.h>
#include <linux/phy.h>
#include "linux/types.h"
#include "linux/delay.h"
#include "../ag7100/eth_diag.h"
#include "ar531x_bsp.h"


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
#define MII_1000BT_STATUS_REG   0x0A
#define MII_MMDCTRL_REG         0x0D
#define MII_MMDDATA_REG         0x0E
#define MII_XSTAT_REG           0x0F
#define MII_PHYPERF_REG         0x10
#define MII_IMASK_REG           0x19
#define MII_ISTAT_REG           0x1A


#define MII_PHY_RESET           0x8000
#define MII_RESTART_AN          0x0200
#define MII_LINK_STATUS_MASK    0x04
#define MII_1000BT_MASK         0x0300

#define MII_INTF_STATUS_REG     0x18
#define MII_SPEED_MASK          0x03
#define MII_DUPLEX_MASK         0x08


MODULE_DESCRIPTION("Lantiq PHY driver");
MODULE_AUTHOR("Ruckus");
MODULE_LICENSE("GPL");

char *dup_str[] = {"half duplex", "full duplex"};

void lantiqPhy_force_100M(struct phy_device *phydev,int duplex)
{
     phy_write(phydev, MII_CTRL_REG, (0x2000|(duplex << 8)));
}

void lantiqPhy_force_10M(struct phy_device *phydev,int duplex)
{
     phy_write(phydev, MII_CTRL_REG, (0x0000|(duplex << 8)));
}

void lantiqPhy_force_auto(struct phy_device *phydev)
{
     phy_write(phydev, MII_CTRL_REG, 0x9000);
}

int lantiqPhy_ioctl(struct phy_device *phydev, uint32_t *args, int cmd)
{
    struct eth_diag *etd =(struct eth_diag *) args;
    unsigned short val;
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
            val = phy_read(phydev, (phy_reg & 0xffff));
            if (val >= 0) {
                etd->val = val;
            }
        }
    } else if(cmd  == ATHR_WR_PHY) {
        if(etd->ed_u.portnum != 0xf) {
            phy_write(phydev, (phy_reg & 0xffff), etd->val);
        }
    }
    else if(cmd == ATHR_FORCE_PHY) {
        struct ethtool_cmd eth_cmd;

        phy_ethtool_gset(phydev, &eth_cmd);

        /*
        printk("supported=0x%08x advertising=0x%08x\n"
            "speed=%d duplex=%d port=%d phy_address=%d transceiver=%d\n"
            "autoneg=%d\n",
            eth_cmd.supported,
            eth_cmd.advertising,
            eth_cmd.speed,
            eth_cmd.duplex,
            eth_cmd.port,
            eth_cmd.phy_address,
            eth_cmd.transceiver,
            eth_cmd.autoneg);
        */

        if(etd->val == 10) {
            printk("Forcing 10Mbps %s on port:%d \n",
                dup_str[etd->ed_u.duplex], (etd->phy_reg));
            //lantiqPhy_force_10M(phydev, etd->ed_u.duplex);
            eth_cmd.speed   = 10;
            eth_cmd.duplex  = etd->ed_u.duplex;
            eth_cmd.autoneg = 0;
            phy_ethtool_sset(phydev, &eth_cmd);
        } else if(etd->val == 100) {
            printk("Forcing 100Mbps %s on port:%d \n",
                dup_str[etd->ed_u.duplex],(etd->phy_reg));
            //lantiqPhy_force_100M(phydev, etd->ed_u.duplex);
            eth_cmd.speed   = 100;
            eth_cmd.duplex  = etd->ed_u.duplex;
            eth_cmd.autoneg = 0;
            phy_ethtool_sset(phydev, &eth_cmd);
        } else if(etd->val == 0) {
            printk("Enabling Auto Neg on port:%d \n",(etd->phy_reg));
            //lantiqPhy_force_auto(phydev);
            eth_cmd.speed   = 1000;
            eth_cmd.duplex  = 1;
            eth_cmd.autoneg = 1;
            phy_ethtool_sset(phydev, &eth_cmd);
        } else
            return -EINVAL;
    }
    else
        return -EINVAL;

    return 0;
}

static int lantiq_config_init(struct phy_device *phydev)
{
        int reg, err;

        reg = phy_read(phydev, MII_CTRL_REG);
        if (reg < 0)
            return reg;

        reg |= MII_PHY_RESET;
        err = phy_write(phydev, MII_CTRL_REG, reg);
        if (err < 0)
            return err;
        mdelay(500);

        if (rks_is_gd34_board() && ((phydev->phy_id & 0xffff0000) == 0xd5650000)) {
            if ((ar531x_boardData->major != 65535) && (ar531x_boardData->major >= 1536)) {
            /*
                 * SC8800-S
             * Cyprus + Lantiq : irq 1-3 are shared by phy0-phy2 and wifi0-wifi2
             * clear any pending interrupts after reset - design uses poll
             * rather than interrupts.
             */
            reg = phy_read(phydev, MII_ISTAT_REG);
            // printk("phyaddr(%x) istat(%x)\n", phydev->addr, reg);
            // phydev->addr: 1: eth0, 3: eth1, 2: eth2
            if (phydev->addr == 1) {
                // clear eth2(phydev->addr=2) pending interrupts early
                reg = mdiobus_read(phydev->bus, 2, MII_ISTAT_REG);
                // printk("phyaddr(2) istat(%x)\n", reg);
            }
            if (phydev->addr != 2) {
                /* lantiq rgmii port */
                    phy_write(phydev, 0x17, 0xcd00);

                    /* drive strength change */
                    phy_write(phydev, 0x0d, 0x001f);
                    phy_write(phydev, 0x0e, 0x0ea9);
                    phy_write(phydev, 0x0d, 0x401f);
                    phy_write(phydev, 0x0e, 0x000d);
                    /* enlarge the Ethernet signal amplitude */
                    phy_write(phydev, 0x13, 0x0701);

                    reg = phy_read(phydev, MII_CTRL_REG);
                    reg |= MII_RESTART_AN;
                    phy_write(phydev, MII_CTRL_REG, reg);
                    printk(": phy addr %d miictrl(0x%x)\n", phydev->addr, phy_read(phydev, 0x17));
                }
            } else {
                /*
                 * SC8800-S-AC: O2
                 * Cyprus + Lantiq : irq 1-3 are shared by phy0-phy2 and wifi0-wifi2
                 * clear any pending interrupts after reset - design uses poll
                 * rather than interrupts.
                 */
                reg = phy_read(phydev, MII_ISTAT_REG);
                // printk("phyaddr(%x) istat(%x)\n", phydev->addr, reg);
                // phydev->addr: 1: eth0, 3: eth1, 2: eth2
                if (phydev->addr == 1) {
                    // clear eth2(phydev->addr=2) pending interrupts early
                    reg = mdiobus_read(phydev->bus, 2, MII_ISTAT_REG);
                    // printk("phyaddr(2) istat(%x)\n", reg);
                }
                if (phydev->addr != 2) {
                    /* lantiq rgmii port */
                phy_write(phydev, 0x17, 0xbe00);
                reg = phy_read(phydev, MII_CTRL_REG);
                reg |= MII_RESTART_AN;
                phy_write(phydev, MII_CTRL_REG, reg);
                printk(": phy addr %d miictrl(0x%x)\n", phydev->addr, phy_read(phydev, 0x17));
                }
            }
        }
        return 0;
}


static struct phy_driver lantiq_driver = {
	.phy_id		= 0xd565a400,
	.phy_id_mask	= 0xffffff0,
	.name		= "XWAY PHY11G PHY",
	.features	= (PHY_GBIT_FEATURES),
//	.flags		= PHY_HAS_INTERRUPT,
	.config_init	= lantiq_config_init,
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.driver		= { .owner = THIS_MODULE },
};

static struct phy_driver athrf1_driver = {
	.phy_id		= 0x04dd071,
	.phy_id_mask	= 0xffffff0,
	.name		= "AR8035 PHY",
	.features	= (PHY_GBIT_FEATURES),
//	.flags		= PHY_HAS_INTERRUPT,
	.config_init	= lantiq_config_init,
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.driver		= { .owner = THIS_MODULE },
};

static int __init lantiq_phy_init(void)
{
	int ret;

	ret = phy_driver_register(&athrf1_driver);
	if (ret)
		return ret;
	ret = phy_driver_register(&lantiq_driver);
	return ret;
}

static void __exit lantiq_phy_exit(void)
{
	phy_driver_unregister(&athrf1_driver);
	phy_driver_unregister(&lantiq_driver);
}

module_init(lantiq_phy_init);
module_exit(lantiq_phy_exit);
