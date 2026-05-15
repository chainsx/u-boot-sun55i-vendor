// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2023
 * allwinnertech.
 */

#include <common.h>
#include <sunxi_efuse_map.h>
#include <log.h>
#include <dm.h>
#include <misc.h>
#include <sunxi_board.h>
#include <asm/io.h>

int sunxi_efuse_usr_write(void *key_buf)
{
	int fret = -1;
	efuse_key_info_t *p_key_info = key_buf;
	struct udevice *dev;

	fret = uclass_get_device_by_driver(UCLASS_MISC,
					   DM_DRIVER_GET(sunxi_efuse), &dev);
	if (fret) {
		pr_err("Unable to find device: sunxi_efuse\n");
		return fret;
	}

	debug("p_key_info :%s\n", p_key_info->name);

	fret = misc_ioctl(dev, SUNXI_EFUSE_IOCTL_WRITE, p_key_info);
	if (fret) {
		pr_err("%s...%d:write efuse failed\n", __func__, __LINE__);
		sunxi_dump(p_key_info, sizeof(efuse_key_info_t));
	}

	return fret;
}

int sunxi_efuse_usr_read(void *key_name, void *read_buf, int *len)
{
	int fret = -1;
	efuse_key_info_t key_info = { 0 };
	struct udevice *dev;

	fret = uclass_get_device_by_driver(UCLASS_MISC,
					   DM_DRIVER_GET(sunxi_efuse), &dev);
	if (fret) {
		pr_err("Unable to find device: sunxi_efuse\n");
		return fret;
	}

	strcpy(key_info.name, key_name);
	key_info.key_data = read_buf;

	fret = misc_ioctl(dev, SUNXI_EFUSE_IOCTL_READ, (void *)&key_info);
	if (fret) {
		pr_err("request: %d read key: %s failed!\n",
		       SUNXI_EFUSE_IOCTL_READ, key_info.name);
	}

	*len = key_info.len;
	return fret;
}
