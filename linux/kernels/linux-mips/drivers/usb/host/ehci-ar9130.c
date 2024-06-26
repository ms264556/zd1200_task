/*
 * EHCI Host Controller Driver
 *
 * Atheros USB Controller
 *
 */

#include "../gadget/ar9130_defs.h"

#ifdef CONFIG_USB_AR9130_OTG_MODULE
#warning "OTG enabled in host"
#define CONFIG_USB_AR9130_OTG
#endif

#ifdef	CONFIG_USB_AR9130_OTG
static int ar9130_start_hc(struct ehci_hcd *ehci, struct device *dev)
{
printk ("ar9130_start_hc %p, %p\n", ehci_to_hcd(ehci), &ehci_to_hcd(ehci)->self);
	if (ehci->transceiver) {
		int status = otg_set_host(ehci->transceiver,
					&ehci_to_hcd(ehci)->self);
		dev_info(dev, "init %s transceiver, status %d\n",
				ehci->transceiver->label, status);
		if (status) {
			if (ehci->transceiver)
				put_device(ehci->transceiver->dev);
		}
		return status;
	} else {
		dev_err(dev, "can't find transceiver\n");
		return -ENODEV;
	}
}
#endif

#ifdef	CONFIG_USB_OTG
void start_hnp(struct ehci_hcd *ehci)
{
	unsigned long	flags;
	otg_start_hnp(ehci->transceiver);

	local_irq_save(flags);
	ehci->transceiver->state = OTG_STATE_A_SUSPEND;
	local_irq_restore(flags);
}
#endif

static int ar9130_ehci_init(struct usb_hcd *hcd)
{
    struct ehci_hcd *ehci = hcd_to_ehci(hcd);
    int ret;

    ar9130_debug_fn("__enter %s\n",__FUNCTION__);

    /* EHCI Register offset 0x100 - Info from ChipIdea */
    ehci->caps = hcd->regs + 0x100;     /* Device/Host Capa Reg*/
    ehci->regs = hcd->regs + 0x100 +    /* Device/Host Oper Reg*/
        HC_LENGTH(readl(&ehci->caps->hc_capbase));

    /*Reading HC Structural Parameters */
    ehci->hcs_params = readl(&ehci->caps->hcs_params);
    ar9130_debug_dev("HCS Params %x \n\n",ehci->hcs_params);
    ar9130_debug_dev("HCC Params %x \n",readl(&ehci->caps->hcc_params));

#if 0
    ret = ehci_halt(ehci);
    if(ret){
        return ret;
    }
#endif

    ret = ehci_init(hcd);
    if(ret){
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
#if 0
    ar9130_reg_wr(AR9130_USB_MODE,(ar9130_reg_rd(AR9130_USB_MODE) | 3));

    printk(" %s Intialize USB CONTROLLER in host mode: %x\n",__func__,ar9130_reg_rd(AR9130_USB_MODE));

    writel((readl(&ehci->regs->port_status[0]) | (1 << 28) ), &ehci->regs->port_status[0]);
    printk(" %s Port Status %x \n",__func__,readl(&ehci->regs->port_status[0])); // 26thFeb
#endif
    return ret;
}

static struct hc_driver ehci_hc_ar9130_driver = {
    .description        =   hcd_name,
    .product_desc       =   "ATH EHCI",
    .hcd_priv_size      =   sizeof(struct ehci_hcd),

    /*
     * generic hardware linkage
     */
#ifndef CONFIG_USB_AR9130_OTG
    .irq                =   ehci_irq,
#endif
    .flags              =   HCD_MEMORY | HCD_USB2,
    /*
     * basic lifecycle operations
     */
    .reset              =   ar9130_ehci_init,
    .start              =   ehci_run,
#ifdef CONFIG_PM
    .suspend            =   ehci_bus_suspend,
    .resume             =   ehci_bus_resume,
#endif
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
    .bus_suspend        =   ehci_bus_suspend,
    .bus_resume         =   ehci_bus_resume,
};

#ifndef CONFIG_USB_AR9130_OTG
int usb_ehci_ar9130_probe(struct hc_driver *driver,
        struct usb_hcd **hcd_out,
        struct platform_device *pdev)
{
    struct usb_hcd *hcd;
    int ret;
    struct ehci_hcd *ehci;

    ar9130_debug_dev("No of Resources %d \n",pdev->num_resources);

    /* Verify the Host Mode of the Driver */
    if (pdev->resource[1].flags != IORESOURCE_IRQ) {
        printk ("resource[1] is not IORESOURCE_IRQ");
        ret = -ENOMEM;
    }

    hcd = usb_create_hcd(driver,&pdev->dev,pdev->dev.bus->name);
    if(!hcd){
        ret = -ENOMEM;
        goto err;
    }
    hcd->rsrc_start = pdev->resource[0].start;
    hcd->rsrc_len   = pdev->resource[0].end - pdev->resource[0].start + 1;

    if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len,
                driver->description)) {
        dev_dbg(&pdev->dev, "controller already in use\n");
        ret = -EBUSY;
        goto err1;
    }

    hcd->regs = ioremap(hcd->rsrc_start,hcd->rsrc_len);
    if(hcd->regs == NULL){
        dev_dbg(&pdev->dev,"error mapping memory \n");
        ret = -EFAULT;
        goto err2;
    }

    /* EHCI Register offset 0x100 - Info from ChipIdea */
    ehci =hcd_to_ehci(hcd);
    ehci->caps = hcd->regs + 0x100;     /* Device/Host Capa Reg*/
    ehci->regs = hcd->regs + 0x140;     /* Device/Host Oper Reg*/

    ar9130_debug_dev("hcd->regs %p \n",hcd->regs);
    ar9130_debug_dev("Host Capability Reg %p \n",ehci->caps);
    ar9130_debug_dev("Host Operational Reg %p \n",ehci->regs);

    /* Added 5_29_07 */
#ifdef CONFIG_MACH_HORNET
    ar9130_reg_rmw_set(AR9130_RESET,AR9130_RESET_USBSUS_OVRIDE);
    mdelay(10);

    ar9130_reg_wr(AR9130_RESET,((ar9130_reg_rd(AR9130_RESET) & ~(AR9130_RESET_USB_HOST)) |
                AR9130_RESET_USBSUS_OVRIDE));
    mdelay(10);

    ar9130_reg_wr(AR9130_RESET,((ar9130_reg_rd(AR9130_RESET) & ~(AR9130_RESET_USB_PHY)) |
                AR9130_RESET_USBSUS_OVRIDE));
    mdelay(10);
#else
    ar9130_reg_rmw_set(AR9130_RESET,AR9130_RESET_USBSUS_OVRIDE |AR7240_RESET_USB_PHY_ANALOG);
    mdelay(10);

    ar9130_reg_wr(AR9130_RESET,((ar9130_reg_rd(AR9130_RESET) & ~(AR9130_RESET_USB_HOST)) |
							   AR9130_RESET_USBSUS_OVRIDE));
    mdelay(10);

    ar9130_reg_wr(AR9130_RESET,((ar9130_reg_rd(AR9130_RESET) & ~(AR9130_RESET_USB_PHY | AR7240_RESET_USB_PHY_ANALOG)) |
							   AR9130_RESET_USBSUS_OVRIDE));
    mdelay(10);
#endif
    /*
     * Reset host controller
     */

#if 0
    /*
     * Set Bits 0 and 1 in USB Mode register to set the controller to HOST mode during
     * intialization.
     */

    ar9130_reg_wr(AR9130_USB_MODE,(ar9130_reg_rd(AR9130_USB_MODE) | 3));

    printk("Intialize USB CONTROLLER in host mode: %x\n",ar9130_reg_rd(AR9130_USB_MODE));
#endif

    printk("Port Status %x \n",readl(&ehci->regs->port_status[0]));
#if 0
	writel((readl(&ehci->regs->port_status[0]) | (1 << 28) ), &ehci->regs->port_status[0]);
    printk("Port Status %x \n",readl(&ehci->regs->port_status[0])); // 26thFeb
#endif
    ret = usb_add_hcd(hcd,pdev->resource[1].start,IRQF_DISABLED);


    if(ret != 0){
        goto err3;
    }
    return ret;

err3:
    iounmap(hcd->regs);
err2:
    release_mem_region(hcd->rsrc_start,hcd->rsrc_len);
err1:
    usb_put_hcd(hcd);
err:
    dev_err(&pdev->dev,"init %s fail, %d \n",pdev->dev.bus->name,ret);
    return ret;
}

static int ehci_drv_ar9130_probe(struct platform_device *pdev)
{
    struct usb_hcd *hcd =  platform_get_drvdata(pdev);

    if(usb_disabled()){
        ar9130_error("USB_DISABLED \n");
        return -ENODEV;
    }
    return usb_ehci_ar9130_probe(&ehci_hc_ar9130_driver,&hcd,pdev);
}

static int ehci_drv_ar9130_remove(struct platform_device *pdev)
{
    struct usb_hcd *hcd = platform_get_drvdata(pdev);

    usb_remove_hcd(hcd);
    iounmap(hcd->regs);
    release_mem_region(hcd->rsrc_start,hcd->rsrc_len);
    usb_put_hcd(hcd);
    return 0;
}

static struct platform_driver ehci_hcd_ar9130_driver ={
    .probe  = ehci_drv_ar9130_probe,
    .remove = ehci_drv_ar9130_remove,
    .driver = {
#ifdef CONFIG_MACH_AR7240
        .name   = "ar7240-ehci",
#else
        .name   = "ar7100-ehci",
#endif
    },
};

#else

int usb_otg_ar9130_probe(struct hc_driver *driver)
{
    struct ar9130_otg *ar9130_otg;
    struct usb_hcd *hcd;
    struct ehci_hcd *ehci;
    struct device *dev;
    int ret;

    printk(" %s \n",__func__);
    ar9130_otg = ar9130_get_otg();
    if (ar9130_otg == NULL) {
        return -EINVAL;
    }
    dev = ar9130_otg->dev;

    hcd = usb_create_hcd(driver, dev, dev->bus->name);
    if(!hcd){
        ret = -ENOMEM;
        goto err;
    }
    hcd->rsrc_start = 0;
    hcd->rsrc_len   = 0;

    hcd->regs = ar9130_otg->reg_base;
    if(hcd->regs == NULL){
        dev_dbg(dev,"error mapping memory \n");
        ret = -EFAULT;
        goto err1;
    }

    /* EHCI Register offset 0x100 - Info from ChipIdea */
    ehci = hcd_to_ehci(hcd);
    ehci->caps = hcd->regs + 0x100;     /* Device/Host Capa Reg*/
    ehci->regs = hcd->regs + 0x140;     /* Device/Host Oper Reg*/

    ar9130_otg->ehci = ehci; /* Temp To Test HNP */

    printk("hcd->regs %p, %p \n", hcd, hcd->regs);
    printk("Host Capability Reg %p \n",ehci->caps);
    printk("Host Operational Reg %p \n",ehci->regs);

    ehci->transceiver = &ar9130_otg->otg;

    printk ("usb_add_hcd\n");
    ret = usb_add_hcd(hcd, 0, 0);
    if(ret != 0){
        goto err1;
    }
    dev_set_drvdata(dev, hcd);

    ehci_hc_ar9130_driver.irq = ehci_irq;
    ret = ar9130_start_hc(ehci, dev);
    if (ret != 0) {
        goto err1;
    }

    return ret;

err1:
    usb_put_hcd(hcd);
err:
    dev_err(dev,"init %s fail, %d \n", dev->bus_id, ret);
    return ret;
}
#endif
#if 0
static int __devinit ehci_hcd_ar9130_init (void)
{
	printk("######## %s ###########\n",__func__);
#ifdef CONFIG_USB_AR9130_OTG
    return usb_otg_ar9130_probe(&ehci_hc_ar9130_driver);
#else
    return driver_register(&ehci_hcd_ar9130_driver);
#endif
}

static void __devexit ehci_hcd_ar9130_cleanup(void)
{
#ifdef CONFIG_USB_AR9130_OTG
    struct ar9130_otg *ar9130_otg = ar9130_get_otg();
    printk("Host Reg otg \n");
    if (ar9130_otg) {
        struct usb_hcd *hcd = dev_get_drvdata(ar9130_otg->dev);
        struct ehci_hcd *ehci = hcd_to_ehci(hcd);
        usb_remove_hcd(hcd);
        otg_set_host(ehci->transceiver, &hcd->self);
        dev_set_drvdata(ar9130_otg->dev, NULL);
        usb_put_hcd(hcd);
    }
#else
    driver_unregister(&ehci_hcd_ar9130_driver);
#endif
}

device_initcall(ehci_hcd_ar9130_init);
module_exit(ehci_hcd_ar9130_cleanup);
#endif
