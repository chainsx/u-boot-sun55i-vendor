/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * drivers/remoteproc/sunxi_rproc_rsc_helper.h
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

#ifndef __SUNXI_RPROC_RSC_HELPER_H__
#define __SUNXI_RPROC_RSC_HELPER_H__

#include <sunxi_rproc.h>
#include "sunxi_rproc_internal.h"

#if CONFIG_IS_ENABLED(AW_RPROC_USER_RESOURCE)
#include "sunxi_rproc_user_resource.h"
#endif

union rsc_info {
#if CONFIG_IS_ENABLED(AW_RPROC_USER_RESOURCE)
	struct fw_rsc_user_resource user_resource;
#endif
};

struct rsc_node {
	const struct rsc_ops *ops;
	void *cache;
	union rsc_info info;
};

struct rsc_ops {
	const char *name;
	u32 type;
	u32 rsc_size;
	void (*show_rsc)(void *_info);
	int (*prepare_rsc)(struct udevice *dev, struct rsc_node *rsc);
};

int sunxi_rproc_rsc_helper_handle_rsc(struct udevice *dev, u32 rsc_type, void *rsc, int offset,
				      int avail);

#endif /* __SUNXI_RPROC_RSC_HELPER_H__ */
