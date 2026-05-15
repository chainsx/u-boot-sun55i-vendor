/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * drivers/riscv/sun21iw1/riscv_reg.h
 *
 * Copyright (c) 2007-2025 Allwinnertech Co., Ltd.
 * Author: wujiayi <wujiayi@allwinnertech.com>
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

#ifndef __RISCV_REG_H
#define __RISCV_REG_H

/*
 * RISCV CFG BASE
 */
#define RISCV_CFG_BASE		(0x01A00000)

/*
 * RISCV CCMU BASE
 */
#define RISCV_CCMU_BASE		(0x02002000)

/*
 * RISCV_CFG Register define
 */
#define RISCV_VER_REG				(0x0000) /* RISCV Version Register */
#define RISCV_RF1P_CFG_REG			(0x0010) /* RISCV Control Register0 */
#define RISCV_TS_TMODE_SEL_REG		(0x0040) /* RISCV TEST MODE SELETE Register */
#define RISCV_STA_ADD_REG			(0x0204) /* RISCV STAT Register */
#define RISCV_WAKEUP_EN_REG			(0x0220) /* RISCV WakeUp Enable Register */
#define RISCV_WAKEUP_MASK0_REG		(0x0224) /* RISCV WakeUp Mask0 Register */
#define RISCV_WAKEUP_MASK1_REG		(0x0228) /* RISCV WakeUp Mask1 Register */
#define RISCV_WAKEUP_MASK2_REG		(0x022C) /* RISCV WakeUp Mask2 Register */
#define RISCV_WAKEUP_MASK3_REG		(0x0230) /* RISCV WakeUp Mask3 Register */
#define RISCV_WAKEUP_MASK4_REG		(0x0234) /* RISCV WakeUp Mask4 Register */
#define RISCV_WORK_MODE_REG			(0x0248) /* RISCV Worke Mode Register */

/*
 * RISCV Version Register
 */
#define SMALL_VER_MASK		(0x1f << 0)
#define LARGE_VER_MASK		(0x1f << 16)

/*
 * RISCV PRID Register
 */
#define BIT_TEST_MODE		(1 << 1)

/*
 * RISCV WakeUp Enable Register
 */
#define BIT_WAKEUP_EN		(1 << 0)

/*
 * RISCV Worke Mode Register
 */
#define BIT_LOCK_STA		(1 << 3)
#define BIT_DEBUG_MODE		(1 << 2)
#define BIT_LOW_POWER_MASK	(0x3)
#define BIT_DEEP_SLEEP_MODE	(0x0)
#define BIT_LIGHT_SLEEP_MODE	(0x1)

/*
 * CCMU related
 */
#define CCMU_RV_CORE_CLK_REG		(0x0b80)
#define BIT_RISCV_CORE_CLK_FATOR_M	(0)
#define BIT_RISCV_AXI_CLK_FATOR_N	(8)
#define BIT_RISCV_CORE_CLK_SRC		(24)
#define BIT_RISCV_CORE_CLK_GAIING	(1 << 31)


#define CCMU_RV_TS_CLK_REG			(0x0b88)
#define BIT_RISCV_TS_CLK_SEL		(0 << 24)
#define BIT_RISCV_TS_CLK_GAIING		(1 << 31)


#define CCMU_RV_SYS_BGR_REG			(0x0b94)
#define BIT_RISCV_CORE_RST			(1 << 16)
#define BIT_RISCV_SYS_RST			(1 << 17)

#define CCMU_RV_CFG_BGR_REG			(0x0b9C)
#define BIT_RISCV_CFG_RST			(1 << 16)
#define BIT_RISCV_CFG_GATEING		(1 << 0)



#endif /* __RISCV_I_H */
