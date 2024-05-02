#include <linux/config.h>
#include <linux/init.h>
#include <linux/pci.h>
#include "../ar531x/ar531xlnx.h"

/*
 * PCI IRQ map
 */
int __init pcibios_map_irq(struct pci_dev *dev, uint8_t slot, uint8_t pin)
{
    printk("%s: dev %p\n", __FUNCTION__, dev);
    return (AR531X_IRQ_LCBUS_PCI);
}

int
pcibios_plat_dev_init(struct pci_dev *dev)
{
    return 0;
}
