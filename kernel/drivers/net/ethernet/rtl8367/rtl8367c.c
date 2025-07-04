/*
 * Platform driver for the Realtek RTL8367C series Ethernet Switchs
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation
 * 
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>


static int rtl8367c_probe(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "RTL8367C driver probe\n");

	return 0;
}

static int rtl8367c_remove(struct platform_device* pdev)
{
	dev_info(&pdev->dev, "RTL8367C driver unloaded!\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id rtl8367c_match[] = {
	{ .compatible = "realtek,rtl8367c" },
	{},
};
#endif

static struct platform_driver rtl8367c_driver = {
	.driver = {
		.name = "rtl8367c",
		.owner = THIS_MODULE,
#ifdef
		.of_match_table = of_match_ptr(rtl8367c_match),
#endif
	},
	.probe = rtl8367c_probe,
	.remove = rtl8367c_remove,
};

static int __init rtl8367c_module_init(void)
{
	return platform_driver_register(&rtl8367c_driver);
}
late_initcall(rtl8367c_module_init);

static int __init rtl8367c_module_exit(void)
{
	return platform_driver_unregister(&rtl8367c_driver)
}
module_exit(rtl8367c_module_exit);

MODULE_DESCRIPTION("RTL8367C Switch Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Tony Shih <sky111144@gmail.com>");
