// SPDX-License-Identifier:	GPL-2.0+
/*
 * (C) Copyright 2007-2013
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Jerry Wang <wangflord@allwinnertech.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <part.h>

/* This function is not yet completed, wait for completion
 *	This thing was ported from u-boot-2018
 *	Mainly used to improve access efficiency
*/

int sunxi_probe_partition_map(void)
{
#ifndef CONFIG_ENABLE_MTD_CMDLINE_PARTS_BY_ENV
	struct blk_desc *desc;
	int ret = 0;
	//desc = blk_get_devnum_by_typename("sunxi_flash", 0);
	ret = blk_get_device_by_str("mmc", "0", &desc);
	if (ret < 0)
		return -1;
#if 0
	if (desc == NULL) {
		pr_err("%s: get desc fail\n", __func__);
		return -1;
	}
	if (part_init_info_map(desc) < 0)
	    return -1;
	else
#endif
#endif
	return 0;
}

