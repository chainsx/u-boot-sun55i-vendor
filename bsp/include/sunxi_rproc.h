/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2023-2027
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * shihongfu <shihongfu@allwinnertech.com>
 *
 * sunxi remoteproc init for older sunxi SoCs.
 */

#ifndef __SUNXI_RPROC_H__
#define __SUNXI_RPROC_H__

#include <compiler.h>

struct resource_table {
	u32 ver;
	u32 num;
	u32 reserved[2];
	u32 offset[];
} __packed;

int sunxi_remoteproc_init(void);
int sunxi_remoteproc_get_id_by_name(const char *name);
int sunxi_remoteproc_set_fw_partitions(int id, const char **partitions, int count, int sectors);

#endif //__SUNXI_RPROC_H__
