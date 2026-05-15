/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * drivers/remoteproc/sunxi_rproc_internal.h
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

#ifndef __SUNXI_RPROC_INTERNAL_H__
#define __SUNXI_RPROC_INTERNAL_H__

#include <linux/list.h>
#include <remoteproc.h>
#include <dm/device.h>

//#define RPROC_DEBUG

struct firmware {
	size_t size;
	const u8 *data;

	/* firmware loader private fields */
	void *priv;
};

#define RSC_VENDOR_START	(128)
#define RSC_VENDOR_END		(512)

enum rsc_handling_status {
	RSC_HANDLED	= 0,
	RSC_IGNORED	= 1,
};

struct sunxi_rproc_driver_data {
	int (*find_rsc_table)(const void *fw_addr, size_t fw_size, const void **table_ptr,
			      size_t *table_sz);
};

#endif /* __SUNXI_RPROC_INTERNAL_H__ */
