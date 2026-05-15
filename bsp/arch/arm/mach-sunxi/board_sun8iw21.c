// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2023-2026
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Tom Cubie <tangliang@allwinnertech.com>
 */

#include <common.h>
#include <power/regulator.h>
#include <linux/delay.h>
#include <linux/libfdt.h>
#include <fdt_support.h>

/* set efuse voltage when burn efuse */
void set_efuse_voltage(int status)
{
	struct udevice *dev;
	int nodeoffset, len;
	int vol;
	const char *power_supply;
	const char *vol_value = "voltage";
	const char *power_name = "power_supply";

	nodeoffset = fdt_path_offset(working_fdt, "/soc/sid");
	if (nodeoffset < 0) {
		 printf ("libfdt fdt_path_offset() returned %s\n",
				 fdt_strerror(nodeoffset));
		 return ;
	}
	vol = fdt_getprop_u32_default_node(working_fdt, nodeoffset, 0, vol_value, -1);
	power_supply = fdt_getprop(working_fdt, nodeoffset, power_name, &len);
	regulator_get_by_devname(power_supply, &dev);

	if (status) {
		regulator_set_value(dev, vol);
		regulator_set_enable(dev, true);
		mdelay(20);
	} else {
		mdelay(20);
		regulator_set_enable(dev, false);
	}
}
