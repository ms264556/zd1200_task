/*
 * EHCI HCD (Host Controller Driver) for USB.
 *
 * Bus Glue for Atheros USB controller
 *
 */

#include <linux/platform_device.h>
#include <asm/mach-ar7100/ar7100.h>
#include <linux/delay.h>
#define SA_INTERRUPT IRQF_DISABLED

extern int usb_disabled(void);

static int ath_usb_ehci_init(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	int ret;

	printk("%s\n", __FUNCTION__);

	/* EHCI Register offset 0x100 - Info from ChipIdea */
	ehci->caps = hcd->regs + 0x100;	/* Device/Host Capa Reg */
	ehci->regs = hcd->regs + 0x100 +	/* Device/Host Oper Reg */
			HC_LENGTH(readl(&ehci->caps->hc_capbase));

	/*Reading HC Structural Parameters */
	ehci->hcs_params = readl(&ehci->caps->hcs_params);
#if 0
	printk("HCS Params %x\n", ehci->hcs_params);
	printk("HCC Params %x\n", readl(&ehci->caps->hcc_params));
#endif

	ret = ehci_init(hcd);
	if (ret) {
		return ret;
	}
	/*
	 * Informs USB Subsystem abt embedded TT.
	 * Sets to host mode
	 */
	hcd->has_tt = 1;
	ehci->sbrn = 0x20;
	ehci_reset(ehci);

	/*
	 * Set Bits 0 and 1 in USB Mode register to set the controller to HOST mode during
	 * intialization.
	 */
	return ret;
}

static void
ar7100_start_ehc(struct device *dev)
{
#if defined(CONFIG_AR934x)
    /* USB Soft Reset Sequence - Power Down the USB PHY PLL */
	ar7100_reg_rmw_set(ATH_USB_RESET,
			   ATH_RESET_USBSUS_OVRIDE | ATH_RESET_USB_PHY_ANALOG |
               ATH_RESET_USB_PHY_PLL_PWD_EXT);
	mdelay(10);

    /* USB Soft Reset Sequence - Power Up the USB PHY PLL */
    ar7100_reg_wr(ATH_USB_RESET,(ar7100_reg_rd(ATH_USB_RESET) & ~(ATH_RESET_USB_PHY_PLL_PWD_EXT)));
	mdelay(10);

	ar7100_reg_wr(ATH_USB_RESET,
		      ((ar7100_reg_rd(ATH_USB_RESET) & ~(ATH_RESET_USB_HOST))
		       | ATH_RESET_USBSUS_OVRIDE));
	mdelay(10);

	ar7100_reg_wr(ATH_USB_RESET,
		      ((ar7100_reg_rd(ATH_USB_RESET) &
			~(ATH_RESET_USB_PHY | ATH_RESET_USB_PHY_ANALOG)) |
		       ATH_RESET_USBSUS_OVRIDE));
	mdelay(10);
#else
    int mask = AR7100_RESET_USB_HOST|AR7100_RESET_USB_PHY;

	printk(KERN_DEBUG __FILE__
		": starting AR7100 EHCI USB Controller...");

    ar7100_reg_rmw_set(AR7100_RESET, mask);
    mdelay(1000);
    ar7100_reg_rmw_clear(AR7100_RESET, mask);

    //ar7100_reg_wr(AR7100_USB_CONFIG, 0x20);
    //ar7100_reg_rmw_clear(AR7100_USB_CONFIG, 0x4);

    /*Turning on the Buff and Desc swap bits */
    ar7100_reg_wr(AR7100_USB_CONFIG, 0x30000);

    /* WAR for HW bug. Here it adjusts the duration between two SOFS */
    /* Was: ar7100_reg_wr(AR7100_USB_FLADJ_VAL,0x20400); */
    ar7100_reg_wr(AR7100_USB_FLADJ_VAL,0x20c00);

    mdelay(900);
    printk("done. reset %#x usb config %#x\n", ar7100_reg_rd(AR7100_RESET),
            ar7100_reg_rd(AR7100_USB_CONFIG));
#endif
}

static int ehci_ar7100_init(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	int ret;

	ehci->caps = hcd->regs + 0x100;	/* Device/Host Capa Reg */
	ehci->regs = hcd->regs + 0x100 +	/* Device/Host Oper Reg */
			HC_LENGTH(ehci_readl(ehci, &ehci->caps->hc_capbase));
	/*Reading HC Structural Parameters */
	ehci->hcs_params = ehci_readl(ehci, &ehci->caps->hcs_params);



	ret = ehci_init(hcd);
	if (ret)
		return ret;


	return 0;
}

static void ar7100_stop_ehc(struct platform_device *dev)
{
	printk(KERN_DEBUG __FILE__
	       ": stopping AR7100 EHCI USB Controller\n");
    /*
     * XXX put in release code here
     */

}

int
usb_ehci_ar7100_probe (const struct hc_driver       *driver,
                       struct       usb_hcd         **hcd_out,
                       struct       platform_device *pdev)
{
	int retval;
	struct usb_hcd *hcd;
    struct ehci_hcd *ehci;

    printk("\nprobing ehci...\n");
	if (pdev->resource[1].flags != IORESOURCE_IRQ) {
		printk ("resource[1] is not IORESOURCE_IRQ");
		retval = -ENOMEM;
	}

	hcd = usb_create_hcd(driver, &pdev->dev, "AR7100_usb");
	if (!hcd)
		return -ENOMEM;

	hcd->rsrc_start = pdev->resource[0].start;
	hcd->rsrc_len   = pdev->resource[0].end - pdev->resource[0].start + 1;

	if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len, driver->description)) {
		printk("request_mem_region failed");
		retval = -EBUSY;
		goto err1;
	}

	hcd->regs = ioremap(hcd->rsrc_start, hcd->rsrc_len);
	if (!hcd->regs) {
		printk("ioremap failed");
		retval = -ENOMEM;
		goto err2;
	}

#if defined(CONFIG_AR934x)
	ehci              =   hcd_to_ehci(hcd);
	ehci->caps        =   hcd->regs + 0x100;
	ehci->regs        =   hcd->regs + 0x140;
	printk("hcd->regs is %p\n", hcd->regs);
	printk("ehci->caps is %p\n", ehci->caps);
	printk("ehci->regs is %p\n", ehci->regs);
	ar7100_start_ehc(pdev);
	retval = usb_add_hcd(hcd, pdev->resource[1].start, SA_INTERRUPT);
#else
    printk("hcd->regs is %p\n", hcd->regs);
	ar7100_start_ehc(pdev);

    ehci              =   hcd_to_ehci(hcd);
    ehci->caps        =   hcd->regs;
    printk("ehci->caps is %p\n", ehci->caps);
    printk("ehci->caps->hc_base is %#x\n", ehci->caps->hc_capbase);
    ehci->regs        =   hcd->regs + HC_LENGTH(readl(&ehci->caps->hc_capbase));
    ehci->hcs_params  =   readl(&ehci->caps->hcs_params);


	retval = usb_add_hcd(hcd, pdev->resource[1].start, IRQF_DISABLED | IRQF_SHARED);
#endif
	if (retval == 0) {
        printk("...probing done\n");
		return retval;
    }

	ar7100_stop_ehc(pdev);
	iounmap(hcd->regs);
 err2:
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
 err1:
	usb_put_hcd(hcd);
	return retval;
}


/* may be called without controller electrically present */
/* may be called with controller, bus, and devices active */

/**
 * usb_hcd_ar7100_remove - shutdown processing for AR7100-based HCDs
 * @dev: USB Host Controller being removed
 * Context: !in_interrupt()
 *
 * Reverses the effect of usb_hcd_ar7100_probe(), first invoking
 * the HCD's stop() method.  It is always called from a thread
 * context, normally "rmmod", "apmd", or something similar.
 *
 */
void
usb_ehci_ar7100_remove(struct platform_device *pdev)
{
 struct usb_hcd *hcd = platform_get_drvdata(pdev);
	usb_remove_hcd(hcd);
	ar7100_stop_ehc(pdev);
	iounmap(hcd->regs);
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd(hcd);
}

/*-------------------------------------------------------------------------*/

static const struct hc_driver ehci_ar7100_hc_driver = {
	.description        =   hcd_name,
	.product_desc       =   "AR7100 EHCI",
	.hcd_priv_size      =   sizeof(struct ehci_hcd),
	/*
	 * generic hardware linkage
	 */
	.irq                =   ehci_irq,
	.flags              =   HCD_MEMORY | HCD_USB2,
	/*
	 * basic lifecycle operations
	 */
#if defined(CONFIG_AR934x)
	.reset              =   ath_usb_ehci_init,
#else
    .reset              =   ehci_init,
#endif
	.start              =	ehci_run,
	.stop               =   ehci_stop,
	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue        =   ehci_urb_enqueue,
	.urb_dequeue        =   ehci_urb_dequeue,
	.endpoint_disable   =   ehci_endpoint_disable,
	/*
	 * scheduling support
	 */
	.get_frame_number   =   ehci_get_frame,
	/*
	 * root hub support
	 */
	.hub_status_data    =   ehci_hub_status_data,
	.hub_control        =   ehci_hub_control,
};

/*-------------------------------------------------------------------------*/

#if defined(CONFIG_USB_EHCI_ATH_REGISTERS)
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#define USB_REG_ARRSIZE(arr) (sizeof(arr) / sizeof(arr[0]))

static struct proc_dir_entry *regs_proc_ent = NULL;
static DEFINE_SPINLOCK(ehci_ath_debug_lock);

static int ehci_ath_debug_proc_show(struct seq_file *s, void *unused)
{
	unsigned int id_regs[] = {
		AR7100_USB_EHCI_BASE + 0x000,
		AR7100_USB_EHCI_BASE + 0x004,
		AR7100_USB_EHCI_BASE + 0x008,
		AR7100_USB_EHCI_BASE + 0x00C,
		AR7100_USB_EHCI_BASE + 0x010,
		AR7100_USB_EHCI_BASE + 0x014,
	};
	unsigned int device_host_timer_regs[] = {
		AR7100_USB_EHCI_BASE + 0x080,
		AR7100_USB_EHCI_BASE + 0x084,
		AR7100_USB_EHCI_BASE + 0x088,
		AR7100_USB_EHCI_BASE + 0x08C,
	};
	unsigned int device_host_cap_regs[] = {
		AR7100_USB_EHCI_BASE + 0x100,
		AR7100_USB_EHCI_BASE + 0x102,
		AR7100_USB_EHCI_BASE + 0x104,
		AR7100_USB_EHCI_BASE + 0x108,
		AR7100_USB_EHCI_BASE + 0x120,
		AR7100_USB_EHCI_BASE + 0x122,
	};
	unsigned int device_host_op_regs[] = {
		AR7100_USB_EHCI_BASE + 0x140,
		AR7100_USB_EHCI_BASE + 0x144,
		AR7100_USB_EHCI_BASE + 0x148,
		AR7100_USB_EHCI_BASE + 0x14C,
		AR7100_USB_EHCI_BASE + 0x154,
		AR7100_USB_EHCI_BASE + 0x158,
		AR7100_USB_EHCI_BASE + 0x15C,
		AR7100_USB_EHCI_BASE + 0x160,
		AR7100_USB_EHCI_BASE + 0x164,
		AR7100_USB_EHCI_BASE + 0x178,
		AR7100_USB_EHCI_BASE + 0x17C,
		AR7100_USB_EHCI_BASE + 0x184,
		AR7100_USB_EHCI_BASE + 0x1A8,
		AR7100_USB_EHCI_BASE + 0x1AC,
		AR7100_USB_EHCI_BASE + 0x1B0,
		AR7100_USB_EHCI_BASE + 0x1B4,
		AR7100_USB_EHCI_BASE + 0x1B8,
		AR7100_USB_EHCI_BASE + 0x1BC,
		AR7100_USB_EHCI_BASE + 0x1C0,
		AR7100_USB_EHCI_BASE + 0x1C4,
		AR7100_USB_EHCI_BASE + 0x1C8,
		AR7100_USB_EHCI_BASE + 0x1CC,
		AR7100_USB_EHCI_BASE + 0x1D0,
		AR7100_USB_EHCI_BASE + 0x1D4,
	};
	int i;

	spin_lock_irq(&ehci_ath_debug_lock);

	seq_printf(s, "AR7100/934x USB Controller Registers\n");

	seq_printf(s, "Identification Registers\n");
	seq_printf(s, "---------------------------------\n");
	for (i = 0; i < USB_REG_ARRSIZE(id_regs); i++) {
		seq_printf(s, "%p : 0x%08x\n",
		           (void *)id_regs[i], ar7100_reg_rd(id_regs[i]));
	}

	seq_printf(s, "Device/Host Timer Registers\n");
	seq_printf(s, "---------------------------------\n");
	for (i = 0; i < USB_REG_ARRSIZE(device_host_timer_regs); i++) {
		seq_printf(s, "%p : 0x%08x\n",
		           (void *)device_host_timer_regs[i], ar7100_reg_rd(device_host_timer_regs[i]));
	}

	seq_printf(s, "Device/Host Capability Registers\n");
	seq_printf(s, "---------------------------------\n");
	for (i = 0; i < USB_REG_ARRSIZE(device_host_cap_regs); i++) {
		seq_printf(s, "%p : 0x%08x\n",
		           (void *)device_host_cap_regs[i], ar7100_reg_rd(device_host_cap_regs[i]));
	}

	seq_printf(s, "Device/Host Operational Registers\n");
	seq_printf(s, "---------------------------------\n");
	for (i = 0; i < USB_REG_ARRSIZE(device_host_op_regs); i++) {
		seq_printf(s, "%p : 0x%08x\n",
		           (void *)device_host_op_regs[i], ar7100_reg_rd(device_host_op_regs[i]));
	}

	spin_unlock_irq(&ehci_ath_debug_lock);
	seq_printf(s, "\n");

	return 0;
}

static int ehci_ath_debug_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ehci_ath_debug_proc_show, PDE(inode)->data);
}

static struct file_operations proc_ops = {
	.open		= ehci_ath_debug_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const char regs_filename[] = "usb_registers";

static void ehci_ath_debug_init(void)
{
	regs_proc_ent = create_proc_entry(regs_filename, 0644, NULL);
	if (!regs_proc_ent)
		return;
	printk(KERN_CRIT "/proc/%s created ...\n", regs_filename);

	regs_proc_ent->proc_fops = &proc_ops;
}

static void ehci_ath_debug_fini(void)
{
	if (regs_proc_ent)
		remove_proc_entry(regs_filename, NULL);
	regs_proc_ent = NULL;
}
#endif

static int
ehci_hcd_ar7100_drv_probe(struct platform_device *pdev)
{
    struct usb_hcd          *hcd    = platform_get_drvdata(pdev);

	printk ("\nIn ar7100_ehci_drv_probe");

	if (usb_disabled()) {
        printk("usb disabled...?\n");
		return -ENODEV;
    }

#if defined(CONFIG_USB_EHCI_ATH_REGISTERS)
	ehci_ath_debug_init();
#endif

	return(usb_ehci_ar7100_probe(&ehci_ar7100_hc_driver, &hcd, pdev));
}


static int
ehci_hcd_ar7100_drv_remove(struct platform_device *pdev)
{
#if defined(CONFIG_USB_EHCI_ATH_REGISTERS)
	ehci_ath_debug_fini();
#endif

	usb_ehci_ar7100_remove(pdev);
	return 0;
}

static struct  platform_driver ehci_hcd_ar7100_driver = {
	.probe		=   ehci_hcd_ar7100_drv_probe,
	.remove		=   ehci_hcd_ar7100_drv_remove,
	.driver = {
        .name   = "ar7100-ehci",
    },
};
