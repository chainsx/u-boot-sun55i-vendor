// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2024-2027
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Tom Cubie <huangrongcun@allwinnertech.com>
 */

#include <common.h>
#include <stdio.h>
#include <asm/io.h>
#include <asm/arch/usb.h>

#define SRAM_CTRL_REG2 (SUNXI_SYSCTRL_BASE + 0x8)
int sunxi_set_sramc_mode(void)
{
	u32 reg_val;

	/* SRAM:set sram to npu, default boot mode */
	reg_val = readl(SRAM_CTRL_REG2);
	reg_val &= ~(0x1 << 1);
	writel(reg_val, SRAM_CTRL_REG2);
	debug("set sram to npu\n");
	return 0;
}

void sunxi_usb_otg_phy_config(void)
{
	u32 reg_val;
	reg_val = readl((const volatile void __iomem *)(SUNXI_USBOTG_BASE+USBC_REG_o_PHYCTL));
	reg_val &= ~(0x01<<USBC_PHY_CTL_SIDDQ);
	reg_val |= 0x01<<USBC_PHY_CTL_VBUSVLDEXT;
	writel(reg_val, (volatile void __iomem *)(SUNXI_USBOTG_BASE+USBC_REG_o_PHYCTL));
}
