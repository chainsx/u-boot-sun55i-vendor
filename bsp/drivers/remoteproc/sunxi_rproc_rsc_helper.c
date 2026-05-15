// SPDX-License-Identifier: GPL-2.0+
/*
 * drivers/remoteproc/sunxi_rproc_rsc_helper.c
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

#include "sunxi_rproc_rsc_helper.h"
#include "sunxi_rproc_internal.h"

#ifdef RPROC_DEBUG
#define RSC_HELPER_DEBUG
#endif

#define RSC_HELPER_PRINTF(_cond, _title, fmt, arg...) \
	do { \
		if (_cond) \
			printf(_title "%s " fmt "\n", __func__, ##arg); \
	} while (0)

#ifdef RSC_HELPER_DEBUG
#define RSC_HELPER_TRACE(fmt, arg...) RSC_HELPER_PRINTF(1, "trace ", fmt, ##arg)
#else
#define RSC_HELPER_TRACE(fmt, arg...)
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr)		(sizeof(arr) / sizeof((arr)[0]))
#endif

static const struct rsc_ops * const rsc_ops_table[] = {
#if CONFIG_IS_ENABLED(AW_RPROC_USER_RESOURCE)
	&user_resource_ops,
#endif
};

static const struct rsc_ops *rsc_type_match(u32 rsc_type, int avail)
{
	const struct rsc_ops *ops;
	int i;

	RSC_HELPER_TRACE("");

	for (i = 0; i < ARRAY_SIZE(rsc_ops_table); i++) {
		ops = rsc_ops_table[i];
		if (ops->type == rsc_type && avail >= ops->rsc_size)
			return ops;
	}

	RSC_HELPER_TRACE("not found %u %d %d", rsc_type, avail, i);
	return NULL;
}

int sunxi_rproc_rsc_helper_handle_rsc(struct udevice *dev, u32 rsc_type, void *info, int offset,
				      int avail)
{
	const struct rsc_ops *ops;
	struct rsc_node rsc;

	RSC_HELPER_TRACE("");

	// not match
	ops = rsc_type_match(rsc_type, avail);
	if (!dev || !ops)
		return RSC_IGNORED;

	rsc.ops = ops;
	rsc.cache = NULL;
	memcpy(&rsc.info, info, ops->rsc_size);

	if (ops->prepare_rsc)
		return ops->prepare_rsc(dev, &rsc);
	else
		return RSC_IGNORED;
}
