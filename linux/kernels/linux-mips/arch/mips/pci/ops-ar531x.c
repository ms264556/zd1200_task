#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <asm/delay.h>

#include "../ar531x/ar531xlnx.h"
#define IDSEL_SHIFT 17
#define BIT(_x) (1 << (_x))
extern void *cfg_space;

/*
 * PCI cfg an I/O routines are done by programming a
 * command/byte enable register, and then read/writing
 * the data from a data regsiter. We need to ensure
 * these transactions are atomic or we will end up
 * with corrupt data on the bus or in a driver.
 */
static DEFINE_SPINLOCK(ar531x_pci_lock);

/*
 * Check for PCI errors (aborts, parity etc.), for configuration cycles
 * PCI error reg: 1:0
 * AHB error reg: 0
 * Both write-back-1 to clear.
 */
int
ar531x_check_error(void)
{
#if 0   /* TODO: check ar531x regs support */
    uint32_t error = 0, trouble = 0, status, val;
    int pending;

    error = ar531x_reg_rd(ar531x_PCI_ERROR) & 3;

    if (error) {
        /*printk("PCI error %d at PCI addr 0x%x\n",
                error, ar531x_reg_rd(ar531x_PCI_ERROR_ADDRESS));*/
        ar531x_reg_wr(ar531x_PCI_ERROR, error);
        ar531x_local_read_config(PCI_STATUS, 2, &status);
        status |= (PCI_STATUS_REC_MASTER_ABORT);
        ar531x_local_write_config(PCI_STATUS, 2, &status);
        ar531x_local_read_config(PCI_STATUS, 2, &status);
        trouble = 1;
    }

    error = 0;
    error = ar531x_reg_rd(ar531x_PCI_AHB_ERROR) & 1;

    if (error) {
        /*printk("AHB error at AHB address 0x%x\n",
                  ar531x_reg_rd(ar531x_PCI_AHB_ERROR_ADDRESS));*/
        ar531x_reg_wr(ar531x_PCI_AHB_ERROR, error);
        ar531x_local_read_config(PCI_COMMAND, 4, &status);
        trouble = 1;
    }

    return trouble;
#else
    return 0;
#endif
}

int
ar531x_pci_read(uint32_t addr, uint32_t* data)
{
	unsigned long flags;
	int retval = 0;
    uint32_t val;

    /* 2 devices */
    if ((addr > (1 << (IDSEL_SHIFT+1))) || (addr < (1 << IDSEL_SHIFT)))
        return 1;

	spin_lock_irqsave(&ar531x_pci_lock, flags);

    /* configuration space */
    val = sysRegRead(AR5315_PCI_MISC_CONFIG);
    val |= AR5315_PCIMISC_CFG_SEL;
    sysRegWrite(AR5315_PCI_MISC_CONFIG, val);

    *data = *(uint32_t *)(cfg_space + (addr & ~3));

    /* memory space */
    val = sysRegRead(AR5315_PCI_MISC_CONFIG);
    val &= ~(AR5315_PCIMISC_CFG_SEL);
    sysRegWrite(AR5315_PCI_MISC_CONFIG, val);

	spin_unlock_irqrestore(&ar531x_pci_lock, flags);

    printk("%s: addr 0x%x data 0x%x\n", __FUNCTION__, addr, *data);

	return retval;
}

int
ar531x_pci_write(uint32_t addr, uint32_t data)
{
	unsigned long flags;
	int retval = 0;
    uint32_t val;

    /* 2 devices */
    if ((addr > (1 << (IDSEL_SHIFT+1))) || (addr < (1 << IDSEL_SHIFT)))
        return 1;

    printk("%s: addr 0x%x data 0x%x\n", __FUNCTION__, addr, data);

	spin_lock_irqsave(&ar531x_pci_lock, flags);

    /* configuration space */
    val = sysRegRead(AR5315_PCI_MISC_CONFIG);
    val |= AR5315_PCIMISC_CFG_SEL;
    sysRegWrite(AR5315_PCI_MISC_CONFIG, val);

    *(uint32_t *)(cfg_space + (addr & ~3)) = data;

    /* memory space */
    val = sysRegRead(AR5315_PCI_MISC_CONFIG);
    val &= ~(AR5315_PCIMISC_CFG_SEL);
    sysRegWrite(AR5315_PCI_MISC_CONFIG, val);

	spin_unlock_irqrestore(&ar531x_pci_lock, flags);

	return retval;
}

/*
 * This is assuming idsel of device 0 is connected to Address line 17
 * Address for type 0 config is as follows:
 * AD:
 *  1:0 00 indicates type zero transaction
 *  7:2    indicates the target config dword
 *  10:8   indicates the target function within the physical device
 *  31:11  are reserved (and most probably used to connect idsels)
 */
uint32_t
ar531x_config_addr(uint8_t bus_num, uint16_t devfn, int where)
{
	uint32_t addr;

	if (!bus_num) {
		/* type 0 */
		addr = (1 << (IDSEL_SHIFT + PCI_SLOT(devfn))) |
               ((PCI_FUNC(devfn)) << 8)               |
		       (where & ~3);
	} else {
		/* type 1 */
		addr = (bus_num << 16) | ((PCI_SLOT(devfn)) << 11) |
			((PCI_FUNC(devfn)) << 8) | (where & ~3) | 1;
	}

	return addr;
}

int
ar531x_local_read_config(int where, int size, uint32_t *value)
{

	uint32_t addr, data, val;
	unsigned long flags;

	if ((size == 2) && (where & 1))
		return PCIBIOS_BAD_REGISTER_NUMBER;
	else if ((size == 4) && (where & 3))
		return PCIBIOS_BAD_REGISTER_NUMBER;

    /* host config space @ 0x10000 */
    addr = (1 << 16) | where;

	spin_lock_irqsave(&ar531x_pci_lock, flags);

    /* configuration space */
    val = sysRegRead(AR5315_PCI_MISC_CONFIG);
    val |= AR5315_PCIMISC_CFG_SEL;
    sysRegWrite(AR5315_PCI_MISC_CONFIG, val);

    data = *(uint32_t *)(cfg_space + (addr & ~3));

    /* memory space */
    val = sysRegRead(AR5315_PCI_MISC_CONFIG);
    val &= ~(AR5315_PCIMISC_CFG_SEL);
    sysRegWrite(AR5315_PCI_MISC_CONFIG, val);

	spin_unlock_irqrestore(&ar531x_pci_lock, flags);

	if (size == 1)
		*value = (data >> ((where & 3) << 3)) & 0xff;
	else if (size == 2)
		*value = (data >> ((where & 3) << 3)) & 0xffff;
	else
		*value = data;

	return PCIBIOS_SUCCESSFUL;
}

int
ar531x_local_write_config(int where, int size, uint32_t value)
{
	uint32_t addr, data, val;
	unsigned long flags;

	if ((size == 2) && (where & 1))
		return PCIBIOS_BAD_REGISTER_NUMBER;
	else if ((size == 4) && (where & 3))
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (ar531x_local_read_config(where, size, &data))
		return PCIBIOS_DEVICE_NOT_FOUND;

	if (size == 1)
		data = (data & ~(0xff << ((where & 3) << 3))) |
		    (value << ((where & 3) << 3));
	else if (size == 2)
		data = (data & ~(0xffff << ((where & 3) << 3))) |
		    (value << ((where & 3) << 3));
	else
		data = value;

    /* host config space @ 0x10000 */
    addr = (1 << 16) | where;

	spin_lock_irqsave(&ar531x_pci_lock, flags);

    /* configuration space */
    val = sysRegRead(AR5315_PCI_MISC_CONFIG);
    val |= AR5315_PCIMISC_CFG_SEL;
    sysRegWrite(AR5315_PCI_MISC_CONFIG, val);

    *(uint32_t *)(cfg_space + (addr & ~3)) = data;

    /* memory space */
    val = sysRegRead(AR5315_PCI_MISC_CONFIG);
    val &= ~(AR5315_PCIMISC_CFG_SEL);
    sysRegWrite(AR5315_PCI_MISC_CONFIG, val);

	spin_unlock_irqrestore(&ar531x_pci_lock, flags);

	return PCIBIOS_SUCCESSFUL;
}

static int
ar531x_pci_read_config(struct pci_bus *bus, unsigned int devfn, int where,
                       int size, uint32_t *value)
{
	uint32_t addr, data;
	uint8_t bus_num = bus->number;

	if ((size == 2) && (where & 1))
		return PCIBIOS_BAD_REGISTER_NUMBER;
	else if ((size == 4) && (where & 3))
		return PCIBIOS_BAD_REGISTER_NUMBER;

	addr = ar531x_config_addr(bus_num, devfn, where);

#if 0
    printk("%s: bus_num 0x%x devfn 0x%x where 0x%x size 0x%x addr 0x%x\n",
        __FUNCTION__, bus_num, devfn, where, size, addr);
#endif

	if (ar531x_pci_read(addr, &data))
		return PCIBIOS_DEVICE_NOT_FOUND;

	if (size == 1)
		*value = (data >> ((where & 3) << 3)) & 0xff;
	else if (size == 2)
		*value = (data >> ((where & 3) << 3)) & 0xffff;
	else
		*value = data;

	return PCIBIOS_SUCCESSFUL;
}

static int
ar531x_pci_write_config(struct pci_bus *bus,  unsigned int devfn, int where,
                        int size, uint32_t value)
{
	uint32_t addr, data;
	uint8_t bus_num = bus->number;

	if ((size == 2) && (where & 1))
		return PCIBIOS_BAD_REGISTER_NUMBER;
	else if ((size == 4) && (where & 3))
		return PCIBIOS_BAD_REGISTER_NUMBER;

	addr = ar531x_config_addr(bus_num, devfn, where);
	if (ar531x_pci_read(addr, &data))
		return PCIBIOS_DEVICE_NOT_FOUND;

	if (size == 1)
		data = (data & ~(0xff << ((where & 3) << 3))) |
		    (value << ((where & 3) << 3));
	else if (size == 2)
		data = (data & ~(0xffff << ((where & 3) << 3))) |
		    (value << ((where & 3) << 3));
	else
		data = value;

	if (ar531x_pci_write(addr, data))
		return PCIBIOS_DEVICE_NOT_FOUND;

	return PCIBIOS_SUCCESSFUL;
}

struct pci_ops ar531x_pci_ops = {
	.read =  ar531x_pci_read_config,
	.write = ar531x_pci_write_config,
};
