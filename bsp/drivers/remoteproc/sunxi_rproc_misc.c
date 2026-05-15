// SPDX-License-Identifier: GPL-2.0+
/*
 * drivers/remoteproc/sunxi_rproc_misc.c
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

#include <cpu_func.h>
#include <sunxi_board.h>
#include <sunxi_flash.h>
#include <sunxi_rproc.h>
#include "sunxi_rproc_internal.h"

#define BYTES_PER_BLOCK		(512)

#ifdef RPROC_DEBUG
#define MISC_OPS_DEBUG
#endif

#define MISC_OPS_PRINTF(_cond, _title, fmt, arg...) \
	do { \
		if (_cond) \
			printf(_title "%s " fmt "\n", __func__, ##arg); \
	} while (0)

#ifdef MISC_OPS_DEBUG
#define MISC_OPS_TRACE(fmt, arg...) MISC_OPS_PRINTF(1, "trace ", fmt, ##arg)
#else
#define MISC_OPS_TRACE(fmt, arg...)
#endif

#ifndef ALIGN_UP
#define ALIGN_UP(size, align)	(((size) + (align) - 1) & ~((align) - 1))
#endif

#ifndef ALIGN_DOWN
#define ALIGN_DOWN(size, align)	((size) & ~((align) - 1))
#endif

void hex_dump_wapper(unsigned long addr, unsigned long len)
{
	int i = 0;

	MISC_OPS_TRACE("");

	printf("addr:0x%08lx, len: 0x%lx", addr, len);
	for (i = 0; i < (len / 4); i++) {
		unsigned long cur_addr = addr + i * 4;

		if (!(i % 4))
			printf("\n0x%08lx: %08lx ", cur_addr, *(unsigned long *)cur_addr);
		else
			printf("%08lx ", *(unsigned long *)cur_addr);
	}
	printf("\n");
}

void flush_cache_wapper(unsigned long addr, unsigned long size)
{
	unsigned long aligned_addr, aligned_size;

	MISC_OPS_TRACE("");

	aligned_addr = ALIGN_DOWN(addr, CONFIG_SYS_CACHELINE_SIZE);
	aligned_size = ALIGN_UP(addr + size - aligned_addr, CONFIG_SYS_CACHELINE_SIZE);
#ifdef RPROC_DEBUG
	printf("aligned_addr: 0x%08lx, aligned_size: 0x%08lx\n", aligned_addr, aligned_size);
#endif
	flush_cache(aligned_addr, aligned_size);
}

int load_from_partition(const char *name, void *dst, size_t bytes)
{
	int ret;
	struct blk_desc *desc;
	lbaint_t start_block, read_block;
	struct disk_partition info = { 0 };

	MISC_OPS_TRACE("");

	sunxi_flash_dev_get_blk(&desc);
	if (!desc) {
		printf("can not get blk\r\n");
		ret = -ENODEV;
		goto err_out;
	}

	ret = sunxi_flash_try_partition(desc, name, &info);
	if (ret < 0) {
		printf("can not find %s partition\r\n", name);
		goto err_out;
	}

	start_block = info.start;
	read_block = bytes / BYTES_PER_BLOCK;
	if (read_block) {
		ret = blk_dread(desc, start_block, read_block, dst);
		if (ret != read_block) {
			printf("can not read %lu - %lu block\r\n", (unsigned long)start_block,
			       (unsigned long)(start_block + read_block - 1));
			goto err_out;
		}
		start_block += read_block;
	}
	if (bytes % BYTES_PER_BLOCK) {
		static char buf[BYTES_PER_BLOCK];

		ret = blk_dread(desc, start_block, 1, buf);
		if (ret != 1) {
			printf("can not read %lu block\r\n", (unsigned long)start_block);
			goto err_out;
		}
		memcpy(dst + read_block * BYTES_PER_BLOCK, buf, bytes % BYTES_PER_BLOCK);
	}
	ret = 0;

err_out:
	if (ret) {
		memset(dst, 0, bytes);
		return (ret > 0) ? -ret : ret;
	} else {
		return bytes;
	}
}
