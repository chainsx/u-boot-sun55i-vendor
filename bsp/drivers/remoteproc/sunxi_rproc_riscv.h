/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * drivers/remoteproc/sunxi_rproc_riscv.h
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

#ifndef __SUNXI_RPROC_RISCV_H__
#define __SUNXI_RPROC_RISCV_H__

#include "sunxi_rproc_internal.h"

extern struct sunxi_rproc_driver_data e907_driver_data;
#define SUNXI_RPROC_RISCV_IDS	\
	{.compatible = "allwinner,e907-rproc", .data = (ulong)&e907_driver_data }

#endif /* __SUNXI_RPROC_RISCV_H__ */
