/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2007
 * Gerald Van Baren, Custom IDEAS, vanbaren@cideas.com
 */

#ifndef __SUNXI_ARISC_H
#define __SUNXI_ARISC_H

/* #include <asm/u-boot.h>
#include <linux/libfdt.h>
#include <abuf.h> */

int fdt_getprop_u32_arisc(const void *fdt, int nodeoffset,
				const char *prop, uint32_t *val);

u32 __sunxi_smc_call(ulong arg0, ulong arg1, ulong arg2, ulong arg3);

u32 sunxi_smc_call_atf(ulong arg0, ulong arg1, ulong arg2, ulong arg3, ulong pResult);

#endif /* ifndef __SUNXI_ARISC_H */
