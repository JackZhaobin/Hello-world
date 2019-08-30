/* Xilinx CAN device driver
 *
 * Copyright (C) 2012 - 2014 Xilinx, Inc.
 * Copyright (C) 2009 PetaLogix. All rights reserved.
 *
 * Description:
 * This driver is developed for Axi CAN IP and for Zynq CANPS Controller.
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/clk.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/skbuff.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/can/dev.h>
#include <linux/can/error.h>
#include <linux/can/led.h>
#include <linux/pm_runtime.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <asm/mach/arch.h>

#define DRIVER_NAME	"led_drv"
#define LED_COM_STRING	"xlnx,led-ip-1.0"

/* Match table for OF platform binding */
static const struct of_device_id led_drv_of_match[] = {
	{ .compatible = LED_COM_STRING, },
	{ /* end of list */ },
};
MODULE_DEVICE_TABLE(of, led_drv_of_match);

static dev_t led_devt;

static const struct file_operations led_dev_fops = {
	.owner		= THIS_MODULE,
//	.llseek		= no_llseek,
//	.read		= rtc_dev_read,
//	.poll		= rtc_dev_poll,
//	.unlocked_ioctl	= rtc_dev_ioctl,
//	.open		= rtc_dev_open,
//	.release	= rtc_dev_release,
//	.fasync		= rtc_dev_fasync,
};


static int led_probe(struct platform_device *pdev)
{
	void __iomem *base;
	struct device_node *resetmgr;
	u32 tmp=0;
	int err;

	pr_info("enter led\n");

	resetmgr = of_find_compatible_node(NULL, NULL, LED_COM_STRING);
	if (!resetmgr) {
		pr_emerg("Couldn't find " LED_COM_STRING "\n");
		return -ENOMEM;
	}
	pr_info("find node \n");
	
	base = of_iomap(resetmgr, 0);
	if (!base) {
		pr_emerg("Couldn't map " LED_COM_STRING "\n");
		return -ENOMEM;
	}

	pr_info("base ptr %p \n", base);

	tmp = readl(base);
	tmp = ~tmp;
	writel(tmp, base); //+ RSTMGR_REG_CHIP_SOFT_RST_OFFSET);


	err = alloc_chrdev_region(&led_devt, 0, 10, "led");
if (err < 0)
	pr_err("failed to allocate char dev region\n");
	
	
	return 0;
}


static int led_remove(struct platform_device *pdev)
{
	pr_info("exit led\n");

	if (led_devt)
			unregister_chrdev_region(led_devt, 10);


	return 0;
}

static struct platform_driver led_drv = {
	.probe = led_probe,
	.remove	= led_remove,
	.driver	= {
		.name = DRIVER_NAME,
		.of_match_table	= led_drv_of_match,
	},
};

module_platform_driver(led_drv);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xilinx Inc");
MODULE_DESCRIPTION("Xilinx CAN interface");

