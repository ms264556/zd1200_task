#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>

MODULE_DESCRIPTION("--- Hello Ruckus SDK ---");
MODULE_AUTHOR("Ruckus");
MODULE_LICENSE("GPL");

static int __init hello_init(void)
{
    pr_info("Hello, Ruckus SDK v1.0\n");
    return 0;
}

static void __exit hello_exit(void)
{
    printk(KERN_INFO "See you\n");
}

module_init(hello_init);
module_exit(hello_exit);
