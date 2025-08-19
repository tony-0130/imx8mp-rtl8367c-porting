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
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/of_mdio.h>
#include <linux/mdio.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>

#include "rtk_switch.h"
#include "rtk_error.h"
#include "port.h"
#include "dal/dal_mgmt.h"
#include "dal/rtl8367c_asicdrv.h"

/* The MDIO bus */
struct mii_bus *rtl8367c_mdio_bus = NULL;

/* External read/write function from rtl8367c_smi.c */
extern int rtl8367c_smi_read(unsigned int mAddrs, unsigned int *rData);
extern int rtl8367c_smi_write(unsigned int mAddrs, unsigned int rData);

static int rtl8367c_mdio_init(struct platform_device *pdev)
{
	struct device_node *mdio_node;
	struct platform_device *mdio_pdev;

	// Check 'mdio-gpio' property exist
	mdio_node = of_parse_phandle(pdev->dev.of_node, "mdio-bus", 0);
	if (!mdio_node) {
		dev_err(&pdev->dev, "Failed to get mdio-bus phandle\n");
		return -ENODEV;
	}

	dev_info(&pdev->dev, "Found mdio node: %s\n", mdio_node->full_name);

	// Check MDIO platform device
	mdio_pdev = of_find_device_by_node(mdio_node);
	if (!mdio_pdev) {
		dev_err(&pdev->dev, "No platform device found for mdio mode\n");
	} else {
		dev_info(&pdev->dev, "Found MDIO platform device: %s\n", dev_name(&mdio_pdev->dev));
		put_device(&mdio_pdev->dev);
	}

	// Fetch MDIO platform device
	rtl8367c_mdio_bus = of_mdio_find_bus(mdio_node);
	of_node_put(mdio_node);

	if (!rtl8367c_mdio_bus) {
		dev_err(&pdev->dev, "Failed to find MDIO bus\n");
		return -EPROBE_DEFER;
	}

	dev_info(&pdev->dev, "Find MDIO bus: %s (id: %s)\n",
			rtl8367c_mdio_bus->name, rtl8367c_mdio_bus->id);
	
	return 0;
}

static int rtl8367c_gpio_init(struct platform_device *pdev)
{
	int ret;

	// Get 'reset-gpio' property from device tree
	int reset = of_get_named_gpio(pdev->dev.of_node, "reset-gpio", 0);

	ret = gpio_request_one(reset, GPIOF_OUT_INIT_HIGH, "rtl8367c reset");
	if (ret) {
		dev_err(&pdev->dev, "Failed to request reset GPIO: %d\n", ret);
		return ret;
	}

	msleep(20);
	gpio_direction_output(reset, 1);
	msleep(100);

	return 0;
}

static int rtl8367c_probe(struct platform_device *pdev)
{

	rtk_api_ret_t retVal;
	unsigned int ret;
	int retry_count = 3;

	// Set up MDIO infterface
	retVal = rtl8367c_mdio_init(pdev);
	if (retVal != RT_ERR_OK) {
		dev_err(&pdev->dev, "rtl8367c_mdio_init failed!\n");
		return -ENODEV;
	}

	// Set up reset gpio
	retVal = rtl8367c_gpio_init(pdev);
	if (retVal != RT_ERR_OK) {
		dev_err(&pdev->dev, "rtl8367c_gpio_init failed!\n");
	}

	// Check MDIO interface is working by read the register 0x1202 value
	for (int i = 0 ; i < retry_count ; i++ ) {
		
		msleep(100);
		retVal = rtl8367c_smi_read(0x1202, &ret);

		if (retVal == RT_ERR_OK) {
			dev_info(&pdev->dev, "Read 0x1202 value = 0x%04x, retry %d\n", ret, i);

			if (ret == 0x88a8) break;
		} else {
			dev_err(&pdev->dev, "Read 0x1202 value failed!, fail count = %d\n", i);
		}
	}

	// Run realtek switch init process
	retVal = rtk_switch_init();
	if (retVal == RT_ERR_OK) {
		dev_info(&pdev->dev, "rtk_switch_init success!\n");
	} else {
		dev_err(&pdev->dev, "rtk_switch_init failed!\n");
	}

	// Try to read the ethernet switch chip id
	retVal = rtl8367c_smi_write(0x13C2, 0x0249);
	if (retVal != RT_ERR_OK) {
		dev_err(&pdev->dev, "Write 0x13C2 value 0x0249 failed!\n");
	}

	retVal = rtl8367c_smi_read(0x1300, &ret);
	if (retVal == RT_ERR_OK) {
		dev_info(&pdev->dev, "Read 0x1300 value = 0x%04x\n", ret);
	} else {
		dev_err(&pdev->dev, "Read 0x1300 value failed!\n");
	}

	// Enable all UTP ports ethernet switch
	retVal = rtk_port_phyEnableAll_set(ENABLED);
	if (retVal != RT_ERR_OK) {
		dev_err(&pdev->dev, "Enable all UTP ports failed!\n");
	}

	// Status LED configuration 
	// Set LED_GROUP_0 blinking when have some activities
	retVal = rtk_led_groupConfig_set(LED_GROUP_0, LED_CONFIG_ACT);
	if (retVal != RT_ERR_OK) {
		dev_err(&pdev->dev, "Config LED_GROUP_0 to LED_CONFIG_ACT failed!\n");
	}
	
	// Set LED_GROUP_0 blinking rate to 64ms
	retVal = rtk_led_blinkRate_set(LED_BLINKRATE_64MS);
	if (retVal != RT_ERR_OK) {
		dev_err(&pdev-dev, "Config LED_GROUP_0 blinking rate failed!\n");
	}

	// Set up ability of each speed and activity
	rtk_led_ability_t ability = {
		.link_10m = DISABLED,
		.link_100m = ENABLED,
		.link_500m = DISABLED,
		.link_1000m = ENABLED,
		.link_2500m = DISABLED,
		.act_rx = ENABLED,
		.act_tx = ENABLED
	};

	// Set customize ability to LED_GROUP_0
	retVal = rtk_led_groupAbility_set(LED_GROUP_0, &ability);
	if (retVal !=RT_ERR_OK) {
		dev_err(&pdev->dev, "Set up LED_GROUP_0 ability failed!\n");
	}

	// Apply LED_GROUP_0 setting to all ports
	rtk_portmask_t portmask;
	portmask.bits[0] = 0xE;
	retVal = rtk_led_enable_set(LED_GROUP_0, &portmask);
	if (retVal != RT_ERR_OK) {
		dev_err(&pdev->dev, "Apply LED_GROUP_0 setting to ports failed!\n");
	}

	// Set LED_GROUP_1 and LED_GROUP_2 to LED_CONFIG_LEDOFF due to unuse
	retVal = rtk_led_groupConfig_set(LED_GROUP_1, LED_CONFIG_LEDOFF);
	if (retVal != RT_ERR_OK) {
		dev_err(&pdev->dev, "Set LED_GROUP_1 to LED_CONFIG_LEDOFF failed!\n");
	}

	retVal = rtk_led_groupConfig_set(LED_GROUP_2, LED_CONFIG_LEDOFF);
	if (retVal != RT_ERR_OK) {
		dev_err(&pdev->dev, "Set LED_GROUP_2 to LED_CONFIG_LEDOFF failed!\n");
	}
	
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

static int __exit rtl8367c_module_exit(void)
{
	return platform_driver_unregister(&rtl8367c_driver)
}
module_exit(rtl8367c_module_exit);

MODULE_DESCRIPTION("RTL8367C Switch Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Tony Shih <sky111144@gmail.com>");
