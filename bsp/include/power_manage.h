/*
 * Copyright (C) 2019 Allwinner.
 * weidonghui <weidonghui@allwinnertech.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <asm/global_data.h>
#include <linux/libfdt.h>
#include <fdt_support.h>
#include <axp_pmic.h>

#define PMU_FASTBOOT_MODE		(0x0c)
#define PMU_FACTORY_MODE		(0x0d)
#define PMU_SYS_MODE			(0x0e)
#define PMU_CHARGE_MODE			(0x0f)
#define PMU_RECOVERY_MODE		(0x10)
#define PMU_EFEX_MODE			(0x11)
#define PMU_UBOOT_MODE			(0x02)
#define PMU_BOOT_MODE			(0x02)
#define FDT_PATH_CHARGER0		"/soc/charger0"
#define FDT_PATH_POWER_DELAY	"/soc/power_delay"
#define FDT_PATH_POWER_SPLY		"/soc/power_sply"
#define FDT_PATH_GPIO_BIAS		"/soc/gpio_bias"

int pmic_probe(void);
int pmic_set_power_supply_output(void);
int pmic_set_dcdc_mode(void);




