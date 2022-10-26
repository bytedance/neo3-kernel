// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022, Pico Immersive Pte. Ltd ("Pico"). All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
** ========================================================================
** File:
**     hardware_version.c
**
** Description:
**     Show hardware version
**     cat /sys/bus/platform/drivers/hw_version/soc:hw_version/version
**
** Bug 2774: add hardware version
** ========================================================================
*/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/spinlock.h>
#include <linux/pinctrl/consumer.h>
#include <linux/syscore_ops.h>
#include <linux/hardware_version.h>
/* Add by PICO_Driver angran.dong for Bug_32808 :[finch2]add hardware version --start-- */
#define		HW_VERSION_DEV_NAME  	"hw_version"
#define		MAX						MAX_SUPPORT_GPIO_NUMBER
//static struct gpio_keys_platform_data *public_data;
static int hw_gpio[MAX] = {0};
struct hw_version_data {
	const struct gpio_keys_button *button;
};

struct hw_version_drvdata {
	const struct gpio_keys_platform_data *pdata;
	struct pinctrl *key_pinctrl;
	struct hw_version_data data[0];
};

static ssize_t hw_version_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	unsigned int state;
	int i;
	state = 0;

	for(i=0; i< MAX; i++)
	{
		//const struct gpio_keys_button *button = &public_data->buttons[i];
		state |= (gpio_get_value(hw_gpio[i]) << i);
		printk("[%s]: hw_gpio[%d]=%d, value=%d state=0x%x\n", __func__, i, hw_gpio[i], gpio_get_value(hw_gpio[i]), state);
	}
	printk("hw_version_show! state=%x\n", state);
        /* Add by PICO_Driver gavin.zhao for Bug_28625 :[finch2]add battery select --start-- */
        #ifdef PICOVR_BATT_FINCH2PRO
	state = state & 0x7;
	#endif
        /* Add by PICO_Driver gavin.zhao for Bug_28625 :[finch2]add battery select --end-- */

	return sprintf(buf, "B%d\n", state);
}
/*added by webber.wang for Bug 19460 - FalconCV left and right panel adjustable--start*/
int hardware_version_num = 0;
/*Modify by PICO_Driver clark.tian for bug62692: Auto set qca,bt-vdd-dig --start--*/
int hw_version_get(void)
{
	int i;
	for(i = 0; i < MAX; i++)
	{
		//const struct gpio_keys_button *button = &public_data->buttons[i];
		hardware_version_num |= (gpio_get_value(hw_gpio[i]) << i);
    }
	printk("hw_version_get and version is %d\n", hardware_version_num);
	return hardware_version_num;
}
EXPORT_SYMBOL(hardware_version_num);
EXPORT_SYMBOL_GPL(hw_version_get);
/*Modify by PICO_Driver clark.tian for bug62692: Auto set qca,bt-vdd-dig --end--*/
/*added by webber.wang for Bug 19460 - FalconCV left and right panel adjustable--start*/

static ssize_t hw_version_store(struct device *dev,
				     struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}


static DEVICE_ATTR(version, 0664, hw_version_show,hw_version_store);

static struct attribute *hw_version_attrs[] = {
	&dev_attr_version.attr,
	NULL,
};

static struct attribute_group hw_version_attr_group = {
	.attrs = hw_version_attrs,
};

static int hw_version_pinctrl_configure(struct hw_version_drvdata *ddata,
							bool active)
{
	struct pinctrl_state *set_state;
	int retval;

	if (active) {
		set_state =
			pinctrl_lookup_state(ddata->key_pinctrl,
						"tlmm_hardware_version_active");
		if (IS_ERR(set_state)) {
			printk("%s:annot get ts pinctrl active state\n",__func__);
			return PTR_ERR(set_state);
		}

	} else {
		set_state =
			pinctrl_lookup_state(ddata->key_pinctrl,
						"tlmm_hardware_version_suspend");
		if (IS_ERR(set_state)) {
			printk("[%s]: cannot get gpiokey pinctrl sleep state\n",__func__);
			return PTR_ERR(set_state);
		}
	}
	retval = pinctrl_select_state(ddata->key_pinctrl, set_state);
	if (retval) {
		printk("[%s]: cannot set ts pinctrl active state\n",__func__);
		return retval;
	}

	return 0;
}


static struct gpio_keys_platform_data *
hw_version_get_devtree_pdata(struct device *dev)
{
	struct device_node *node, *pp;
	struct gpio_keys_platform_data *pdata;
	struct gpio_keys_button *button;
	int nbuttons;
	int i,error;

	node = dev->of_node;
	if (!node)
		return ERR_PTR(-ENODEV);

	nbuttons = of_get_child_count(node);
	pr_err("DAR %s nbuttons = %d\n",__func__,nbuttons);
	if (nbuttons == 0)
		return ERR_PTR(-ENODEV);

	pdata = devm_kzalloc(dev,
			     sizeof(*pdata) + nbuttons * sizeof(*button),
			     GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->buttons = (struct gpio_keys_button *)(pdata + 1);
	pdata->nbuttons = nbuttons;
	i=0;
	for_each_child_of_node(node, pp)
	{
		int gpio;
		enum of_gpio_flags flags;

		if (!of_find_property(pp, "gpios", NULL))
		{
			pdata->nbuttons--;
			dev_warn(dev, "Found button without gpios\n");
			continue;
		}

		gpio = of_get_gpio_flags(pp, 0, &flags);
        	pr_err("DAR %s gpio = %d\n",__func__,gpio);
		if (gpio < 0)
		{
			error = gpio;
			if (error != -EPROBE_DEFER)
				dev_err(dev,
					"Failed to get gpio flags, error: %d\n",
					error);
			return ERR_PTR(error);
		}
		//button = &pdata->buttons[i++];
		if(i < MAX){
			hw_gpio[i] = gpio;
			pr_err("CK %s hw_gpio[%d] = %d\n",__func__,i ,hw_gpio[i]);
		}
		//button->gpio = gpio;
		//button->desc = of_get_property(pp, "label", NULL);
        	//pr_err("DAR %s label = %s\n",__func__,button->desc);
		//if (of_property_read_u32(pp, "linux,input-type", &button->type))
			//pr_err("linux,input-type = %d",button->type);
		//pr_err("linux,input-type = %d",button->type);
		i++;
	}

	if (pdata->nbuttons == 0)
		return ERR_PTR(-EINVAL);

	return pdata;
}



static int hw_version_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gpio_keys_platform_data *pdata = dev_get_platdata(dev);
	struct hw_version_drvdata *ddata;
	int error;
	size_t size;
	if (!pdata) {
		pdata = hw_version_get_devtree_pdata(dev);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);
	}

	size = sizeof(struct hw_version_drvdata) +
			pdata->nbuttons * sizeof(struct hw_version_data);
	ddata = devm_kzalloc(dev, size, GFP_KERNEL);

	if (!ddata) {
		dev_err(dev, "failed to allocate state\n");
		return -ENOMEM;
	}
	ddata->pdata = pdata;
	platform_set_drvdata(pdev, ddata);
	ddata->key_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(ddata->key_pinctrl)) {
		if (PTR_ERR(ddata->key_pinctrl) == -EPROBE_DEFER)
			return -EPROBE_DEFER;

		pr_debug("Target does not use pinctrl\n");
		ddata->key_pinctrl = NULL;
	}

	if (ddata->key_pinctrl) {
		error = hw_version_pinctrl_configure(ddata, true);
		if (error) {
			dev_err(dev, "cannot set ts pinctrl active state\n");
			return error;
		}
	}
	//public_data = pdata;

	error = sysfs_create_group(&pdev->dev.kobj, &hw_version_attr_group);
	if (error)
	{
		printk("[%s]: Unable to export keys/switches, error: %d\n", __func__, error);
	}
	hw_version_get();//added by webber.wang for Bug 19460 - FalconCV left and right panel adjustable
	return 0;


}
static int hw_version_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &hw_version_attr_group);
	return 0;
}


static const struct of_device_id hw_version_of_match[] = {
	{ .compatible = "hw_version", },
	{ },
};
MODULE_DEVICE_TABLE(of, hw_version_of_match);


static struct platform_driver hw_version_device_driver = {
	.probe		= hw_version_probe,
	.remove		= hw_version_remove,
	.driver		= {
		.name	= "hw_version",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(hw_version_of_match),
	}
};

static int __init hw_version_init(void)
{
	return platform_driver_register(&hw_version_device_driver);
}

static void __exit hw_version_exit(void)
{
	platform_driver_unregister(&hw_version_device_driver);
}

/* Add by PICO_Driver angran.dong for Bug_32808 :[finch2]add hardware version --start-- */

arch_initcall(hw_version_init);//modified by webber.wang for Bug 19460 - FalconCV left and right panel adjustable
module_exit(hw_version_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Clark Tian");
MODULE_DESCRIPTION("hw_version driver for hardware version");
MODULE_ALIAS("platform:hw_version");

