#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/cpumask.h>
#include <linux/delay.h>

#include <asm/delay.h>
#include "../ar531x/ar531xlnx.h"

#define PCI_BRIDGE_DEV      0
#define PCI_MINI_PCI_DEV    1
void *cfg_space=NULL;
extern int ar531x_local_read_config(int where, int size, uint32_t *value);

/*
 * init the pci controller
 */

static struct resource ar531x_io_resource = {
	"PCI IO space",
	0x0000,
	0,
	IORESOURCE_IO
};

static struct resource ar531x_mem_resource = {
	"PCI memory space",
	HOST_PCI_MBAR0,
	HOST_PCI_MBAR0+0x20000-1,
	IORESOURCE_MEM
};

extern struct pci_ops ar531x_pci_ops;

static struct pci_controller ar531x_pci_controller = {
	.pci_ops	    = &ar531x_pci_ops,
	.mem_resource	= &ar531x_mem_resource,
	.io_resource	= &ar531x_io_resource,
};

static int __init ar531x_pcibios_init(void)
{
    uint32_t val, base;
    int i;
    void *membar0=NULL;

    printk("%s: *********************** \n", __FUNCTION__);

    val = sysRegRead(AR5315_ENDIAN_CTL);
    printk("AR5315_ENDIAN_CTL(0x%x)\n", val);
    val |= (CONFIG_PCIAHB | CONFIG_PCIAHB_BRIDGE);
    sysRegWrite(AR5315_ENDIAN_CTL, val);
    printk("AR5315_ENDIAN_CTL(0x%x)\n", val);

    val = sysRegRead(AR5315_PCICLK);
    printk("AR5315_PCICLK(0x%x)\n", val);
    val = PCICLK_IN_FREQ_DIV_6 << 2;
    sysRegWrite(AR5315_PCICLK, val);
    printk("AR5315_PCICLK(0x%x)\n", val);

    val = sysRegRead(AR5315_AHB_ARB_CTL);
    printk("AR5315_AHB_ARB_CTL(0x%x)\n", val);
    val |= ARB_PCI;
    sysRegWrite(AR5315_AHB_ARB_CTL, val);
    printk("AR5315_AHB_ARB_CTL(0x%x)\n", val);

    val = sysRegRead(AR5315_IF_CTL);
    printk("AR5315_IF_CTL(0x%x)\n", val);
    val &= ~(IF_PCI_CLK_MASK | IF_MASK);
    val |= (IF_PCI | IF_PCI_HOST | IF_PCI_INTR | (IF_PCI_CLK_OUTPUT_CLK << IF_PCI_CLK_SHIFT));
    sysRegWrite(AR5315_IF_CTL, val);
    printk("AR5315_IF_CTL(0x%x)\n", val);

    /* reset pci */
    val = sysRegRead(AR5315_PCI_MISC_CONFIG);
    printk("AR5315_PCI_MISC_CONFIG(0x%x)\n", val);
    val &= ~(AR5315_PCIMISC_RST_MODE);
    val |= AR5315_PCIRST_LOW;
    sysRegWrite(AR5315_PCI_MISC_CONFIG, val);
    printk("AR5315_PCI_MISC_CONFIG(0x%x)\n", val);

    mdelay(100);

    /* clear reset and disable external access cache */
    val = sysRegRead(AR5315_PCI_MISC_CONFIG);
    printk("AR5315_PCI_MISC_CONFIG(0x%x)\n", val);
    val &= ~(AR5315_PCIMISC_RST_MODE);
    val |= (AR5315_PCIRST_HIGH | AR5315_PCICACHE_DIS);
    sysRegWrite(AR5315_PCI_MISC_CONFIG, val);
    printk("AR5315_PCI_MISC_CONFIG(0x%x)\n", val);

    mdelay(500);

    /* Map 32MB @ 0x80000000 */
    cfg_space = ioremap(AR5315_PCIEXT, 0x2000000);
    printk("cfg_space = 0x%08x\n", (uint32_t)cfg_space);

#if 1
    base = 0x10000;
    for (i=0; i<64; i+=4) {
        ar531x_local_read_config(i, 4, &val);
        printk("0x%08x = 0x%08x\n", (base+i), val);
    }
    ar531x_local_write_config(base+0x10, 4, 0xffffffff);
    ar531x_local_read_config(base+0x10, 4, &val);
    printk("0x%08x wr 0xffffffff => 0x%08x\n", (base+0x10), val);
    ar531x_local_write_config(base+0x14, 4, 0xffffffff);
    ar531x_local_read_config(base+0x14, 4, &val);
    printk("0x%08x wr 0xffffffff => 0x%08x\n", (base+0x14), val);
    ar531x_local_write_config(base+0x18, 4, 0xffffffff);
    ar531x_local_read_config(base+0x18, 4, &val);
    printk("0x%08x wr 0xffffffff => 0x%08x\n", (base+0x18), val);
#endif

    ar531x_local_write_config(0x10, 4, HOST_PCI_MBAR0);
    ar531x_local_write_config(0x14, 4, HOST_PCI_MBAR1);
    ar531x_local_write_config(0x18, 4, HOST_PCI_MBAR2);
    /*
        PCI_CMD_MEM_ENABLE | PCI_CMD_MASTER_ENABLE | PCI_CMD_MON_ENABLE |
        PCI_CMD_WI_ENABLE | PCI_CMD_PERR_ENABLE | PCI_CMD_SERR_ENABLE |
        PCI_CMD_FBTB_ENABLE
    */
    ar531x_local_write_config(0x04, 4, 0x0000035e);

#if 0
    ar531x_local_read_config(0x10, 4, &val);
    printk("0x%08x => 0x%08x\n", (base+0x10), val);
    ar531x_local_read_config(0x14, 4, &val);
    printk("0x%08x => 0x%08x\n", (base+0x14), val);
    ar531x_local_read_config(0x18, 4, &val);
    printk("0x%08x => 0x%08x\n", (base+0x18), val);
    ar531x_local_read_config(0x04, 4, &val);
    printk("0x%08x => 0x%08x\n", (base+0x04), val);

    /* configuration space */
    val = sysRegRead(AR5315_PCI_MISC_CONFIG);
    printk("AR5315_PCI_MISC_CONFIG(0x%x)\n", val);
    val |= AR5315_PCIMISC_CFG_SEL;
    sysRegWrite(AR5315_PCI_MISC_CONFIG, val);
    printk("AR5315_PCI_MISC_CONFIG(0x%x)\n", val);

    /* mPCI */
    base = 0x20000;
    for (i=0; i<64; i+=4) {
        val = *(uint32_t *)(cfg_space + ((base+i)&~3));
        printk("0x%08x = 0x%08x\n", (base+i), val);
    }

    /* memory space */
    val = sysRegRead(AR5315_PCI_MISC_CONFIG);
    printk("AR5315_PCI_MISC_CONFIG(0x%x)\n", val);
    val &= ~(AR5315_PCIMISC_CFG_SEL);
    sysRegWrite(AR5315_PCI_MISC_CONFIG, val);
    printk("AR5315_PCI_MISC_CONFIG(0x%x)\n", val);
#endif

    val = sysRegRead(AR5315_PCI_INT_STATUS);
    printk("AR5315_PCI_INT_STATUS(0x%x)\n", val);
    val |= (AR5315_PCI_ABORT_INT | AR5315_PCI_EXT_INT);
    sysRegWrite(AR5315_PCI_INT_STATUS, val);
    printk("AR5315_PCI_INT_STATUS(0x%x)\n", val);

    val = sysRegRead(AR5315_PCI_INT_MASK);
    printk("AR5315_PCI_INT_MASK(0x%x)\n", val);
    val |= (AR5315_PCI_ABORT_INT | AR5315_PCI_EXT_INT);
    sysRegWrite(AR5315_PCI_INT_MASK, val);
    printk("AR5315_PCI_INT_MASK(0x%x)\n", val);

    val = sysRegRead(AR5315_PCI_INTEN_REG);
    printk("AR5315_PCI_INTEN_REG(0x%x)\n", val);
    val |= AR5315_PCI_INT_ENABLE;
    sysRegWrite(AR5315_PCI_INTEN_REG, val);
    printk("AR5315_PCI_INTEN_REG(0x%x)\n", val);

	register_pci_controller(&ar531x_pci_controller);

    return 0;
}

arch_initcall(ar531x_pcibios_init);
