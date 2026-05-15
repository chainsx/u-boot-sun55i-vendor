/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * drivers/remoteproc/sunxi_rproc_misc.h
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

#ifndef __SUNXI_RPROC_MISC_H__
#define __SUNXI_RPROC_MISC_H__

void hex_dump_wapper(unsigned long addr, unsigned long len);
void flush_cache_wapper(unsigned long addr, unsigned long size);
int load_from_partition(const char *name, void *dst, size_t bytes);

#endif /* __SUNXI_RPROC_MISC_H__ */
