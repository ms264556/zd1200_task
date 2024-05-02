//#include <linux/config.h>
#include <linux/init.h>
#include <linux/pci.h>
#include "ar7100.h"
extern int ar7240_cpu(void);

/*
 * PCI IRQ map
 */
int __init pcibios_map_irq(const struct pci_dev *dev, uint8_t slot, uint8_t pin)
{
    pr_debug("fixing irq for slot %d pin %d\n", slot, pin);

#ifdef CONFIG_AR7100_EMULATION
    printk("Returning IRQ %d\n", AR7100_PCI_IRQ_DEV0);
    return AR7100_PCI_IRQ_DEV0;
#else
#if defined(CONFIG_QCA955x)
extern struct pci_ops ar7100_pci_ops;
#define ATH_PCI_CMD_INIT	(PCI_COMMAND_MEMORY |		\
				 PCI_COMMAND_MASTER |		\
				 PCI_COMMAND_INVALIDATE |	\
				 PCI_COMMAND_PARITY |		\
				 PCI_COMMAND_SERR |		\
				 PCI_COMMAND_FAST_BACK)

		/*
		 * clear any lingering errors and register core error IRQ
		 */
		ar7100_pci_ops.write(dev->bus, 0,
				PCI_COMMAND, 4, ATH_PCI_CMD_INIT);
#endif
    if (!ar7240_cpu() && !is_ar934x()) {
        switch(slot)
        {
            case 0:
                return AR7100_PCI_IRQ_DEV0;
            case 1:
                return AR7100_PCI_IRQ_DEV1;
            case 2:
                return AR7100_PCI_IRQ_DEV2;
            default:
                printk("unknown slot!\n");
                return -1;
        }
    } else {
	return AR7100_PCI_IRQ_DEV0;
    }
#endif
}

int
pcibios_plat_dev_init(struct pci_dev *dev)
{
        return 0;
}
