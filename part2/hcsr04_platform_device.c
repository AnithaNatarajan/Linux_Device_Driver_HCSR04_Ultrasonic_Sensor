/*
 * Platform device program to bind to the platform driver.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include "hcsr04_platform_device.h"

static void hcsr_platform_device_release (struct device *dev)
{
   printk("Plaform device: Released..");
}


static struct hcsr_platform_device hcsr_dev1 = {
		.name	= "HCSRdevice1",
		.dev_no 	= 20,
		.plf_dev = {
			.name	= "hcsr_plf_dev_1",
			.id	= -1,
			.dev = {
			     .release = hcsr_platform_device_release,
			 }
		}
};

static struct hcsr_platform_device hcsr_dev2 = {
		.name	= "HCSRdevice2",
		.dev_no 	= 55,
		.plf_dev = {
			.name	= "hcsr_plf_dev_2",
			.id	= -1,
			.dev = {
			     .release = hcsr_platform_device_release,
			 }

		}
};

/**
 * Register the device during module initialization
 */

static int p_device_init(void)
{
	int ret = 0;

	/* Register the device */
	platform_device_register(&hcsr_dev1.plf_dev);
	printk(KERN_ALERT "Platform device: registered HCSRdevice1.. \n");

	platform_device_register(&hcsr_dev2.plf_dev);
	printk(KERN_ALERT "Platform device: registered HCSRdevice2.. \n");

	return ret;
}

static void p_device_exit(void)
{
    	platform_device_unregister(&hcsr_dev1.plf_dev);
	platform_device_unregister(&hcsr_dev2.plf_dev);

	printk(KERN_ALERT "Exiting...\n");
}

module_init(p_device_init);
module_exit(p_device_exit);
MODULE_LICENSE("GPL");
