/*
 * Copyright 2000-2009
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * SPDX-License-Identifier:	GPL-2.0+
*/

#include <common.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <asm/arch/cpu.h>
#include <asm/arch/clock.h>
#include <asm/arch/timer.h>
#include <asm/arch/prcm.h>
#include "private_uboot.h"

void clock_open_timer(int timernum)
{
	u32 reg_value = 0;
	reg_value = readl(SUNXI_CCM_BASE + 0x800 + timernum * 4);
	reg_value |= (0 <<24);
	reg_value |= (2 << 0);
	writel(reg_value, (SUNXI_CCM_BASE + 0x800 + timernum * 4));

	/*enable timer*/
	reg_value = readl(SUNXI_CCM_BASE + 0x800 + timernum * 4);
	reg_value |= (1 << 31);
	writel(reg_value, (SUNXI_CCM_BASE + 0x800 + timernum * 4));

	reg_value = readl(SUNXI_CCM_BASE + 0x850);
	reg_value |= (1 << 16);
	writel(reg_value, (SUNXI_CCM_BASE + 0x850));

	reg_value = readl(SUNXI_CCM_BASE + 0x850);
	reg_value |= (1 << 0);
	writel(reg_value, (SUNXI_CCM_BASE + 0x850));
	__msdelay(1);

}

void clock_close_timer(int timernum)
{
	u32 reg_value = 0;
	/*disable timer*/
	reg_value = readl(SUNXI_CCM_BASE + 0x800 + timernum * 4);
	reg_value |= (0 << 31);
	writel(reg_value, (SUNXI_CCM_BASE + 0x800 + timernum * 4));

	reg_value = readl(SUNXI_CCM_BASE + 0x850);
	reg_value |= (0 << 16);
	writel(reg_value, (SUNXI_CCM_BASE + 0x850));

	reg_value = readl(SUNXI_CCM_BASE + 0x850);
	reg_value |= (0 << 0);
	writel(reg_value, (SUNXI_CCM_BASE + 0x850));
	__msdelay(1);
}

void clock_set_gic(void)
{
	u32 reg_value = 0;

	reg_value = readl(SUNXI_CCM_BASE + GIC_CLK_REG);
	reg_value |= ((1 << 31) | (3 << 24) | (0 << 0));
	writel(reg_value, SUNXI_CCM_BASE + GIC_CLK_REG);
}

int usb_open_clock(void)
{
	//usb 0 md 0x02002000 + 1300
	setbits_le32(SUNXI_CCM_BASE + USB0_CLK_REG,
		     BIT(USB0_CLK_REG_USBPHY0_RSTN_OFFSET));
	setbits_le32(SUNXI_CCM_BASE + USB0_BGR_REG,
		     BIT(USB0_BGR_REG_USB20_0_DEVICE_GATING_OFFSET));
	clrbits_le32(SUNXI_CCM_BASE + USB0_BGR_REG,
		     BIT(USB0_BGR_REG_USB20_0_DEVICE_RST_OFFSET));
	setbits_le32(SUNXI_CCM_BASE + USB0_BGR_REG,
		     BIT(USB0_BGR_REG_USB20_0_DEVICE_RST_OFFSET));
	//usb 1
	setbits_le32(SUNXI_CCM_BASE + USB1_CLK_REG,
		     BIT(USB0_CLK_REG_USBPHY0_RSTN_OFFSET));
	setbits_le32(SUNXI_CCM_BASE + USB1_BGR_REG,
		     BIT(USB0_BGR_REG_USB20_0_DEVICE_GATING_OFFSET));
	clrbits_le32(SUNXI_CCM_BASE + USB1_BGR_REG,
		     BIT(USB0_BGR_REG_USB20_0_DEVICE_RST_OFFSET));
	setbits_le32(SUNXI_CCM_BASE + USB1_BGR_REG,
		     BIT(USB0_BGR_REG_USB20_0_DEVICE_RST_OFFSET));

	return 0;
}


int usb_close_clock(void)
{
	clrbits_le32(SUNXI_CCM_BASE + USB0_BGR_REG,
		     BIT(USB0_BGR_REG_USB20_0_DEVICE_RST_OFFSET) |
			     BIT(USB0_BGR_REG_USB20_0_HOST_EHCI_RST_OFFSET));
	__msdelay(1);

	clrbits_le32(SUNXI_CCM_BASE + USB0_BGR_REG,
		     BIT(USB0_BGR_REG_USB20_0_DEVICE_GATING_OFFSET) |
			     BIT(USB0_BGR_REG_USB20_0_HOST_EHCI_GATING_OFFSET));
	__msdelay(1);

	clrbits_le32(SUNXI_CCM_BASE + USB0_CLK_REG,
		     BIT(USB0_CLK_REG_USBPHY0_RSTN_OFFSET) |
			     BIT(USB0_CLK_REG_USB0_CLKEN_OFFSET));
	__msdelay(1);

	clrbits_le32(SUNXI_CCM_BASE + USB1_BGR_REG,
		     BIT(USB0_BGR_REG_USB20_0_DEVICE_RST_OFFSET) |
			     BIT(USB0_BGR_REG_USB20_0_HOST_EHCI_RST_OFFSET));
	__msdelay(1);

	clrbits_le32(SUNXI_CCM_BASE + USB1_BGR_REG,
		     BIT(USB0_BGR_REG_USB20_0_DEVICE_GATING_OFFSET) |
			     BIT(USB0_BGR_REG_USB20_0_HOST_EHCI_GATING_OFFSET));
	__msdelay(1);

	clrbits_le32(SUNXI_CCM_BASE + USB1_CLK_REG,
		     BIT(USB0_CLK_REG_USBPHY0_RSTN_OFFSET) |
			     BIT(USB0_CLK_REG_USB0_CLKEN_OFFSET));
	__msdelay(1);

	return 0;
}

uint clock_get_pll6(void)
{
	struct sunxi_ccm_reg *const ccm =
		(struct sunxi_ccm_reg *)SUNXI_CCM_BASE;
	uint reg_val;
	uint factor_n, factor_p0, factor_m1, pll6;

	reg_val = readl(&ccm->pll6_cfg);

	factor_n = ((reg_val >> 8) & 0xff) + 1;
	factor_p0 = ((reg_val >> 16) & 0x03) + 1;
	factor_m1 = ((reg_val >> 1) & 0x01) + 1;
	pll6 = (24 * factor_n /factor_p0/factor_m1)>>1;


	return pll6;
}
