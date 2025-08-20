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

/* For debugfs use*/
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

/* The MDIO bus */
struct mii_bus *rtl8367c_mdio_bus = NULL;

/* External read/write function from rtl8367c_smi.c */
extern int rtl8367c_smi_read(unsigned int mAddrs, unsigned int *rData);
extern int rtl8367c_smi_write(unsigned int mAddrs, unsigned int rData);

/* Debugfs related variables & functions */
static struct dentry *rtl8367c_debugfs_root;

static const char *rtk_testmode_names[] = {
	"normal",
	"test1",
	"test2",
	"test3",
	"test4",
};

static int rtl8367c_set_port_testmode_rtk(int port, rtk_port_phy_test_mode_t test_mode)
{
	rtk_api_ret_t retVal;

	if (port < 0 || port > RTK_PHY_ID_MAX) {
		printk(KERN_ERR "RTL8367C: Invalid port %d, port range is (0-%d)\n", port, RTK_PHY_ID_MAX);
		return -EINVAL;
	}

	if (test_mode < PHY_TEST_MODE_NORMAL || test_mode >= PHY_TEST_MODE_END) {
		printk(KERN_ERR "RTL8367C: Invalid test mode %d\n", test_mode);
		return -EINVAL;
	}

	retVal = rtk_port_phyTestMode_set((rtk_port_t)port, test_mode);
	if (retVal != RT_ERR_OK) {
		printk(KERN_ERR "RTL8367C: Fail to set test mode %d for port %d (retVal=%d)\n",
				test_mode, port, retVal);
		return -EIO;
	}

	printk(KERN_INFO "RTL8367C: Port %d test mode set to %s (%d)\n",
			port,
			test_mode < ARRAY_SIZE(rtk_testmode_names) ? rtk_testmode_names[test_mode] : "unknown",
			test_mode);

	return 0;
}

static int rtl8367c_get_port_testmode_rtk(int port, rtk_port_phy_test_mode_t *test_mode)
{
	rtk_api_ret_t retVal;

	if (port < 0 || port > RTK_PHY_ID_MAX)
		return -EINVAL;

	retVal = rtk_port_phyTestMode_get((rtk_port_t)port, test_mode);
	if (retVal != RT_ERR_OK) {
		printk(KERN_ERR "RTL8367C: Fail to get test mode for port %d (retVal=%d)\n", port, retVal);
		return -EIO;
	}

	return 0;
}

static ssize_t rtl8367c_port_testmode_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	char command[128];
	rtk_uint32 port, mode_num;
	rkt_port_phy_test_mode_t test_mode = PHY_TEST_MODE_NORMAL;
	int ret;

	if (count >= sizeof(command))
		return -EINVAL;
	
	if (copy_from_user(command, buf, count))
		return -EFAULT;

	command[count] = '\0';

	if (command[count-1]=='\n')
		command[count-1] = '\0';

	// set command
	if (sscanf(command, "set %d %d", &port, &mode_num) == 2) {

		ret = rtl8367c_set_port_testmode_rtk(port, test_mode);
		if (ret != 0)
			return ret;

	} else if (sscanf(command, "get %d", &port) == 1) {

		ret = rtl8367c_get_port_testmode_rtk(port, &test_mode);

		if (ret == 0) {
			printk(KERN_INFO "RTL8367C: Port %d : mode %d\n", port, test_mode);
		} else {
			return ret;
		}

	} else if (strcmp(command, "status") == 0) {

		printk(KERN_INFO "RTL8367C: Test mode status for all ports:\n");

		for (int i = 0 ; i <= RTK_PHY_ID_MAX ; i++) {
			ret = rtl8367c_get_port_testmode_rtk(i, &test_mode);
			if (ret == 0) {
				printk(KERN_INFO "Port %d : mode %d\n", i, test_mode);
			}
		}

	} else if (strcmp(command, "reset") == 0) {

		printk(KERN_INFO "RTL8367C: Resetting all ports to normal mode ...\n");

		for (int i = 0 ; i <= RTK_PHY_ID_MAX ; i++) {
			ret = rtl8367c_set_port_testmode_rtk(i, PHY_TEST_MODE_NORMAL);
			if (ret != 0)
				printk(KERN_ERR "Fail to reset port %d\n", i);
		}

		printk(KERN_INFO "RTL8367C: Resetting ports done.\n");

	} else {

		printk(KERN_INFO "RTL8367C test mode commands:\n");
		printk(KERN_INFO "  set PORT MODE     - Set MODE of PORT\n");
		printk(KERN_INFO "  get PORT          - Get MODE of PORT\n");
		printk(KERN_INFO "  status            - Show status of all ports\n");
		printk(KERN_INFO "\nExamples:\n");
		printk(KERN_INFO "  echo 'set 0 1' > testmode\n");
		printk(KERN_INFO "  echo 'get 2' > testmode\n");
		printk(KERN_INFO "  echo 'status' > testmode\n");
		printk(KERN_INFO "  echo 'reset' > testmode\n");

	}

	return count;
};

static int rtl8367c_testmode_show(struct seq_file *s, void *unused)
{
	int port;
	rtk_port_phy_test_mode_t test_mode;
	int ret;

	seq_printf(s, "RTL8367C Port Test Mode Status (Using RTK API)\n");
	seq_printf(s, "===============================================\n\n");

	for (port = 0 ; port <= RTK_PHY_ID_MAX ; port++) {

		ret = rtl8367c_get_port_testmode_rtk(port, &test_mode);
		if (ret == 0) {
			seq_printf(s, "Port %d : test mode %d\n", port, test_mode);
		} else {
			seq_printf(s, "Port %d : READ ERROR\n", port);
		}
	}

	printk(KERN_INFO "\n");

	return 0;
}

static int rtl8367c_testmode_open(struct inode *inode, struct file *file)
{
	return single_open(file, rtl8367c_testmode_show, NULL);
}

static const struct file_operations rtl8367c_testmode_fops = {
	.open = rtl8367c_testmode_open,
	.read = seq_read,
	.write = rtl8367c_port_testmode_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static void rtl8367c_debugfs_init(struct platform_device *pdev)
{
	// Create the main directory
	rtl8367c_debugfs_root = debugfs_create_dir("rtl8367c", NULL);
	if (!rtl8367c_debugfs_root) {
		dev_err(&pdev->dev, "Fail to create debugfs directory\n");
		return;
	}

	// Create files
	debugfs_create_file("testmode", 0644, rtl8367c_debugfs_root, NULL,
			&rtl8367c_testmode_fops);

	dev_info(&pdev->dev, "Debugfs created at /sys/kernel/debug/rtl8367c/\n");
	dev_info(&pdev->dev, "Available files: testmode\n");
}

static void rtl8367c_debugfs_cleanup(void)
{
	debugfs_remove_recursive(rtl8367c_debugfs_root);
	rtl8367c_debugfs_root = NULL;
}

/* Switch init related functions */

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

	rtl8367c_debugfs_init(pdev);

	return 0;
}

static int rtl8367c_remove(struct platform_device* pdev)
{
	rtl8367c_debugfs_cleanup();

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
