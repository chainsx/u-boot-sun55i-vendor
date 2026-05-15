/*
 * Copyright (C) 2019 Allwinner.
 * weidonghui <weidonghui@allwinnertech.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <power_manage.h>
#include <power/regulator.h>
#include <spare_head.h>
#include <console.h>
#include <axp_pmic.h>
#include <dm.h>
#include <dm/lists.h>
#include <dm/device-internal.h>

/*
 * Global data (for the gd->bd)
 */
DECLARE_GLOBAL_DATA_PTR;

int pmic_set_power_supply_output(void)
{
	int nodeoffset = -1;
	int ret, i;
	struct udevice *dev;
	char power_name[32];
	int power_vol, power_vol_d, onoff;
	ulong id;

	nodeoffset = fdt_path_offset(working_fdt, FDT_PATH_POWER_SPLY);
	if (nodeoffset < 0)
		return nodeoffset;

	for (i = 0; ; i++) {
		ret = uclass_get_device_by_seq(UCLASS_REGULATOR, i, &dev);
		if (ret == -ENODEV)
			break;
		id = dev_get_driver_data(dev->parent);
		memset(power_name, 0, sizeof(power_name));
		strcpy(power_name, axp_model_names[id]);
		strcat(power_name, "_");
		strcat(power_name, dev->name);
		strcat(power_name, "_vol");
		power_vol = fdt_getprop_u32_default_node(working_fdt, nodeoffset, 0, power_name, -1);
		if (power_vol < 0) {
			continue;
		}
		onoff       = -1;
		power_vol_d = 0;

		if (power_vol > 10000) {
			onoff       = 1;
			power_vol_d = power_vol % 10000;

		} else if (power_vol >= 0) {
			onoff       = 0;
			power_vol_d = power_vol;
		}
		regulator_set_enable(dev, onoff);
		regulator_set_value(dev, power_vol_d * 1000);
		pr_notice("%s = %d[now:%d], onoff=%d[now:%d]\n", power_name, power_vol_d, regulator_get_value(dev) / 1000, onoff, regulator_get_enable(dev));
	}

	return 0;
}

/* set dcdc pwm mode */
int pmic_set_dcdc_mode(void)
{
	int nodeoffset = -1;
	int ret, i;
	struct udevice *dev;
	char power_name[32];
	int dcdc_mode;
	ulong id;

	nodeoffset = fdt_path_offset(working_fdt, FDT_PATH_POWER_SPLY);
	if (nodeoffset < 0)
		return nodeoffset;

	for (i = 0; ; i++) {
		ret = uclass_get_device_by_seq(UCLASS_REGULATOR, i, &dev);
		if (ret == -ENODEV)
			break;

		if (strstr(dev->name, "dcdc") == NULL) {
			continue;
		}

		id = dev_get_driver_data(dev->parent);
		memset(power_name, 0, sizeof(power_name));
		strcpy(power_name, axp_model_names[id]);
		strcat(power_name, "_");
		strcat(power_name, dev->name);
		strcat(power_name, "_mode");

		dcdc_mode = fdt_getprop_u32_default_node(working_fdt, nodeoffset, 0, power_name, -1);
		if (dcdc_mode < 0) {
			continue;
		}
		if (regulator_set_dcdc_mode(dev, dcdc_mode) < 0)
			pr_err("set %s to %d fail!\n", power_name, dcdc_mode);
		else
			pr_notice("%s = %d[now:%d]\n", power_name, dcdc_mode, regulator_get_dcdc_mode(dev));
	}
	return 0;
}

int pmic_probe(void)
{
	int ret, i;
	struct uclass *uc;
	struct udevice *dev;

	ret = uclass_get(UCLASS_PMIC, &uc);
	if (ret)
		return ret;

	for (i = 0; ; i++) {
		ret = uclass_get_device_by_seq(UCLASS_PMIC, i, &dev);
		debug("%s:%d dev:%s\n", __func__, __LINE__, dev->name);
		if (ret == -ENODEV)
			break;
	}

	uclass_foreach_dev(dev, uc) {
		ret = device_probe(dev);
		if (ret)
			pr_err("%s - probe failed: %d\n", dev->name, ret);
		else
			axp_start_up_debug(dev);
	}

	return ret;
}
