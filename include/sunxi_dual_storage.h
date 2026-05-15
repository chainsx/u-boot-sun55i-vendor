/*
 * SPDX-License-Identifier: GPL-2.0+
 *
 * (C) Copyright 2024
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * linzibo <linzibo@allwinnertech.com>
 *
 */

#ifndef __SUNXI_DUAL_STORAGE_H__
#define __SUNXI_DUAL_STORAGE_H__

#define SUNXI_DUAL_STORAGE_BOOT			(0)
#define SUNXI_DUAL_STORAGE_SPRITE		(1)
#define SUNXI_DUAL_STORAGE_SWITCH		(2)
int sunxi_fdt_get_boot_storage_type(int *storage_type);
int sunxi_fdt_get_system_storage_type(int *storage_type);
int sunxi_dual_storage_handle(int stage, int workmode);

#endif