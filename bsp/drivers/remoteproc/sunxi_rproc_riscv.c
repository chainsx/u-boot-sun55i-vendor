// SPDX-License-Identifier: GPL-2.0+
/*
 * drivers/remoteproc/sunxi_rproc_riscv.c
 *
 * Copyright (c) 2007-2025 Allwinnertech Co., Ltd.
 * Author: shihongfu <shihongfu@allwinnertech.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details
 *
 */

#include "sunxi_rproc_riscv.h"
#if CONFIG_IS_ENABLED(BOOT_RISCV)
#include "../riscv/common/riscv_img.h"
#endif

struct sunxi_rproc_driver_data e907_driver_data = {
	.find_rsc_table = riscv_find_rsc_table,
};
