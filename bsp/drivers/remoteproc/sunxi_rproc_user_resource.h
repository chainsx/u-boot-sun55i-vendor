/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * drivers/remoteproc/sunxi_rproc_user_resource.h
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

#ifndef __SUNXI_RPROC_USER_RESOURE_H__
#define __SUNXI_RPROC_USER_RESOURE_H__

struct fw_rsc_user_resource {
	//u32 type; // useless for helper
	u32 da;
	u32 len;
	u32 reserved;
	u32 src_type;
	u8 src_name[32];
} __packed;

extern const struct rsc_ops user_resource_ops;

#endif /* __SUNXI_RPROC_USER_RESOURE_H__ */
