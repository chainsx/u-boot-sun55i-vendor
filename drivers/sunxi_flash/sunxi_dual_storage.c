// SPDX-License-Identifier: GPL-2.0+
/*
 * sunxi dual storage.
 *
 * Copyright (C) 2024
 * 2024.12.27 linzibo <linzibo@allwinnertech.com>
 */

#include <common.h>
#include <sunxi_board.h>
#include <sunxi_flash.h>
#include <sunxi_dual_storage.h>
#include <fdt_support.h>

#define nodeoffset    fdt_path_offset(working_fdt, FDT_PATH_TARGET)

int sunxi_fdt_get_boot_storage_type(int *storage_type)
{
	return fdt_getprop_u32(working_fdt, nodeoffset, "boot_storage_type",
					(uint32_t *)storage_type);
}

int sunxi_fdt_get_system_storage_type(int *storage_type)
{
	return fdt_getprop_u32(working_fdt, nodeoffset, "system_storage_type",
					(uint32_t *)storage_type);
}

int sunxi_dual_storage_handle(int stage, int workmode)
{
	int ret = 0;
	int storage_type;

	switch (stage) {
	case SUNXI_DUAL_STORAGE_BOOT: {
		ret = sunxi_fdt_get_system_storage_type(&storage_type);
		if (ret < 0) {
			pr_err("FDT ERROR:%s:get property system_storage_type error\n", __func__);
			return -1;
		}
		pr_info("system storage type = %d\n", storage_type);
		ret = sunxi_flash_boot_init(storage_type, workmode);
	} break;
	case SUNXI_DUAL_STORAGE_SPRITE: {
		ret = sunxi_fdt_get_boot_storage_type(&storage_type);
		if (ret < 0) {
			pr_err("FDT ERROR:%s:get property boot_storage_type error\n", __func__);
			return -1;
		}
		pr_info("boot storage type = %d\n", storage_type);
		ret = sunxi_flash_sprite_init(storage_type, workmode);
		if (ret < 0) {
			pr_err("%s:init boot storage fail\n", __func__);
			return -1;
		}

		ret = sunxi_fdt_get_system_storage_type(&storage_type);
		if (ret < 0) {
			pr_err("FDT ERROR:%s:get property system_storage_type error\n", __func__);
			return -1;
		}
		pr_info("system storage type = %d\n", storage_type);
		ret = sunxi_flash_sprite_init(storage_type, workmode);
	} break;
	case SUNXI_DUAL_STORAGE_SWITCH: {
		ret = sunxi_fdt_get_boot_storage_type(&storage_type);
		if (ret < 0) {
			pr_err("FDT ERROR:%s:get property boot_storage_type error\n", __func__);
			return -1;
		}
		pr_info("boot storage type = %d\n", storage_type);
		ret = sunxi_flash_sprite_switch(storage_type, workmode);
	} break;
	default: {
		pr_err("not support\n");
		return -1;
	} break;
	}

	if (ret != 0) {
		pr_err("dual storage handle fail, stage = %d, workmode = %d\n", stage, workmode);
		return -1;
	}
	set_boot_storage_type(storage_type);

	return 0;
}
