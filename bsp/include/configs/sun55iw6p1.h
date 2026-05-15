/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Configuration settings for the Allwinner A7 (sun8i) CPU
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#ifdef CONFIG_SUNXI_MALLOC_LEN
#define SUNXI_SYS_MALLOC_LEN	CONFIG_SUNXI_MALLOC_LEN
#else
#define SUNXI_SYS_MALLOC_LEN	(32 << 20)
#endif

/*
 * Include common sunxi configuration where most the settings are
 */
#include <configs/sunxi-common.h>

#endif /* __CONFIG_H */
