/*
 * (C) Copyright 2013-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <clk-uclass.h>
#include <dm.h>
#include <errno.h>
#include <dt-bindings/clock/sun55iw6p1-ccu.h>
#include <dt-bindings/reset/sun55iw6p1-ccu.h>
#include <linux/bitops.h>
#include <linux/clk-provider.h>
#include <clk/sunxi.h>
#include "sunxi-clk.h"

static struct ccu_reset sunxi_resets[] = {
	[RST_BUS_DE0]		= RESET(0x0A04, BIT(16)),
	[RST_BUS_DE_SYS]	= RESET(0x0A74, BIT(16)),
	[RST_BUS_VO0_TCONLCD0]	= RESET(0x1504, BIT(16)),
	[RST_BUS_LVDS0]		= RESET(0x1544, BIT(16)),
	[RST_BUS_DSI0]		= RESET(0x1584, BIT(16)),
	[RST_BUS_DPSS]		= RESET(0x16C4, BIT(16)),
	[RST_BUS_VIDEO_OUT0]	= RESET(0x16E4, BIT(16)),
	[RST_BUS_MMC0]		= RESET(0xd0c, BIT(16)),
	[RST_BUS_MMC1]		= RESET(0xd1c, BIT(16)),
	[RST_BUS_MMC2]		= RESET(0xd2c, BIT(16)),
	[RST_BUS_DMA]		= RESET(0x704, BIT(16)),
	[RST_BUS_OTG]		= RESET(0x1304, BIT(24)),
	[RST_USB_PHY0]		= RESET(0x1300, BIT(30)),
	[RST_USB_20_0_HOST_EHCI]	= RESET(0x1304, BIT(20)),
	[RST_USB_20_1_HOST_EHCI]	= RESET(0x130c, BIT(20)),
	[RST_USB_PHY0_RSTN]		= RESET(0x1300, BIT(30)),
	[RST_USB_PHY1_RSTN]		= RESET(0x1308, BIT(30)),
	[RST_BUS_GMAC0_AXI]		= RESET(0x140c, BIT(17)),
	[RST_BUS_GMAC0]			= RESET(0x140c, BIT(16)),
	[RST_BUS_GMAC1_AXI]		= RESET(0x141c, BIT(17)),
	[RST_BUS_GMAC1]			= RESET(0x141c, BIT(16)),
	[RST_BUS_PCIE]			= RESET(0x138c, BIT(17)),
	[RST_BUS_PCIE_PWRUP]	= RESET(0x138c, BIT(16)),
	[RST_BUS_SERDES_NOPPU]	= RESET(0x13cc, BIT(17)),
	[RST_BUS_SERDE]			= RESET(0x13cc, BIT(16)),
	[RST_BUS_PWM0]			= RESET(0x0784, BIT(16)),
	[RST_BUS_PWM1]			= RESET(0x078C, BIT(16)),
	[RST_BUS_PWM2]			= RESET(0x0794, BIT(16)),
};

static const char * const apb1_parents[] = {
	"osc24M", "ext32k", "rc16m", "pll-peri0-600m"
};
static const char * const dsi0_parents[] = {
	"osc24M", "pll-peri0-200m", "pll-peri0-150m"
};

static const char * const de0_parents[] = {
	"pll-peri0-600m", "pll-peri0-400m"
};

static const char * const mmc0_parents[] = {
	"osc24M", "pll-peri0-400m", "pll-peri0-300m", "pll-peri1-400m", "pll-peri1-300m"
};

static const char * const mmc1_parents[] = {
	"osc24M", "pll-peri0-400m", "pll-peri0-300m", "pll-peri1-400m", "pll-peri1-300m"
};

static const char * const mmc2_parents[] = {
	"osc24M", "pll-peri0-800m", "pll-peri0-600m", "pll-peri1-800m", "pll-peri1-600m"
};

static const char * const gmac_nsi_parents[] = {
	"osc24M", "pll-peri0-480m", "pll-peri0-400m", "pll-peri0-300m"
};

static const char * const serdes_phy_ref_parents[] = {
	"osc24M", "pll-peri0-600m"
};

static const char * const serdes_phy_cfg_parents[] = {
	"pll-peri0-600m", "pll-peri0-400m"
};

static const char * const serdes_axi_parents[] = {
	"osc24M", "pll-peri0-480m", "pll-peri0-400m", "pll-peri0-300m"
};

enum enum_mux_cfg {
	MUX_APB1,
	MUX_CLK_DSI0,
	MUX_CLK_MMC0,
	MUX_CLK_MMC1,
	MUX_CLK_MMC2,
	MUX_CLK_DE0,
	MUX_CLK_GMAC_NSI,
	MUX_SERDES_PHY_REF,
	MUX_SERDES_PHY_CFG,
	MUX_SERDES_PHY_AXI,
};

static const struct sunxi_mux_cfg sun55iw6_muxes[] = {
	MUX_CFG(MUX_APB1, apb1_parents, 0x518, 24, 2),
	MUX_CFG(MUX_CLK_DSI0, dsi0_parents, 0x1580, 24, 3),
	MUX_CFG(MUX_CLK_MMC0, mmc0_parents, 0xd00, 24, 3),
	MUX_CFG(MUX_CLK_MMC1, mmc1_parents, 0xd10, 24, 3),
	MUX_CFG(MUX_CLK_MMC2, mmc2_parents, 0xd20, 24, 3),
	MUX_CFG(MUX_CLK_DE0, de0_parents, 0xA00, 24, 1),
	MUX_CFG(MUX_CLK_GMAC_NSI, gmac_nsi_parents, 0x1420, 24, 2),
	MUX_CFG(MUX_SERDES_PHY_REF, serdes_phy_ref_parents, 0x13C4, 24, 1),
	MUX_CFG(MUX_SERDES_PHY_CFG, serdes_phy_cfg_parents, 0x13C0, 24, 1),
	MUX_CFG(MUX_SERDES_PHY_AXI, serdes_axi_parents, 0x13E0, 24, 2),
};

enum enum_gate_cfg {
	GATE_CLK_BUS_DPSS,
	GATE_CLK_BUS_DSI0,
	GATE_CLK_BUS_DE0,
	GATE_CLK_BUS_VO0_TCONLCD0,
	GATE_CLK_BUS_DMA,
	GATE_CLK_MBUS_DMA_GATE,
	GATE_CLK_DSI0,
	GATE_CLK_DE0,
	GATE_CLK_PLL_PERIPH0,
	GATE_CLK_BUS_MMC0,
	GATE_CLK_BUS_MMC1,
	GATE_CLK_BUS_MMC2,
	GATE_CLK_MMC0,
	GATE_CLK_MMC1,
	GATE_CLK_MMC2,
	GATE_CLK_BUS_OTG,
	GATE_CLK_USB20_0_HOST_EHCI,
	GATE_CLK_USB20_1_HOST_EHCI,
	GATE_CLK_MBUS_GMAC0,
	GATE_CLK_MBUS_GMAC1,
	GATE_CLK_AHB_GMAC0,
	GATE_CLK_AHB_GMAC1,
	GATE_CLK_GMAC0,
	GATE_CLK_GMAC1,
	GATE_CLK_GMAC0_PHY,
	GATE_CLK_GMAC1_PHY,
	GATE_CLK_GMAC_NSI,
	GATE_CLK_PCIE_REF_AUX,
	GATE_CLK_PCIE_SLV,
	GATE_CLK_SERDES_PHY_REF,
	GATE_CLK_SERDES_PHY_CFG,
	GATE_CLK_BUS_SERDES,
	GATE_CLK_AHB_SERDES,
	GATE_CLK_SERDES_AXI,
	GATE_CLK_PWM0,
	GATE_CLK_PWM1,
	GATE_CLK_PWM2,
};

static const struct sunxi_gate_cfg sun55iw6_gates[] = {
	GATE_CFG(GATE_CLK_BUS_DPSS, 0x16C4, BIT(0)),
	GATE_CFG(GATE_CLK_BUS_DSI0, 0x1584, BIT(0)),
	GATE_CFG(GATE_CLK_BUS_DE0, 0xA04, BIT(0)),
	GATE_CFG(GATE_CLK_BUS_VO0_TCONLCD0, 0x1504, BIT(0)),
	GATE_CFG(GATE_CLK_BUS_DMA, 0x704, BIT(0)),
	GATE_CFG(GATE_CLK_MBUS_DMA_GATE, 0x5e0, BIT(0)),
	GATE_CFG(GATE_CLK_DSI0, 0x1580, BIT(31)),
	GATE_CFG(GATE_CLK_DE0, 0xA00, BIT(31)),
	GATE_CFG(GATE_CLK_PLL_PERIPH0, 0xA0, BIT(31)),
	GATE_CFG(GATE_CLK_MMC0, 0x0d00, BIT(31)),
	GATE_CFG(GATE_CLK_MMC1, 0x0d10, BIT(31)),
	GATE_CFG(GATE_CLK_MMC2, 0x0d20, BIT(31)),
	GATE_CFG(GATE_CLK_BUS_MMC0, 0x0d04, BIT(0)),
	GATE_CFG(GATE_CLK_BUS_MMC1, 0x0d1c, BIT(0)),
	GATE_CFG(GATE_CLK_BUS_MMC2, 0x0d2c, BIT(0)),
	GATE_CFG(GATE_CLK_BUS_OTG, 0x1304, BIT(8)),
	GATE_CFG(GATE_CLK_USB20_0_HOST_EHCI, 0x1304, BIT(4)),
	GATE_CFG(GATE_CLK_USB20_1_HOST_EHCI, 0x130C, BIT(4)),
	GATE_CFG(GATE_CLK_MBUS_GMAC0, 0x05E0, BIT(11)),
	GATE_CFG(GATE_CLK_MBUS_GMAC1, 0x05E0, BIT(12)),
	GATE_CFG(GATE_CLK_AHB_GMAC0, 0x05C0, BIT(13)),
	GATE_CFG(GATE_CLK_AHB_GMAC1, 0x05C0, BIT(14)),
	GATE_CFG(GATE_CLK_GMAC0, 0x140C, BIT(0)),
	GATE_CFG(GATE_CLK_GMAC1, 0x141C, BIT(0)),
	GATE_CFG(GATE_CLK_GMAC0_PHY, 0x1400, BIT(31)),
	GATE_CFG(GATE_CLK_GMAC1_PHY, 0x1410, BIT(31)),
	GATE_CFG(GATE_CLK_GMAC_NSI, 0x1420, BIT(31)),
	GATE_CFG(GATE_CLK_PCIE_REF_AUX, 0x1380, BIT(31)),
	GATE_CFG(GATE_CLK_PCIE_SLV, 0x1384, BIT(31)),
	GATE_CFG(GATE_CLK_SERDES_PHY_REF, 0x13c4, BIT(31)),
	GATE_CFG(GATE_CLK_SERDES_PHY_CFG, 0x13c0, BIT(31)),
	GATE_CFG(GATE_CLK_BUS_SERDES, 0x13cc, BIT(1)),
	GATE_CFG(GATE_CLK_AHB_SERDES, 0x13cc, BIT(0)),
	GATE_CFG(GATE_CLK_SERDES_AXI, 0x13e0, BIT(31)),
	GATE_CFG(GATE_CLK_PWM0, 0x784, BIT(0)),
	GATE_CFG(GATE_CLK_PWM1, 0x78C, BIT(0)),
	GATE_CFG(GATE_CLK_PWM2, 0x794, BIT(0)),
};

enum enum_div_cfg {
	DIV_CLK_DSI0,
	DIV_CLK_DE0,
	DIV_CLK_MMC0,
	DIV_CLK_MMC1,
	DIV_CLK_MMC2,
	DIV_CLK_PLL_PERIPH0,
	DIV_CLK_PLL_PERIPH0_800M,
	DIV_CLK_PLL_PERIPH0_480M,
	DIV_CLK_APB1,
	DIV_CLK_GMAC0_PHY,
	DIV_CLK_GMAC1_PHY,
	DIV_CLK_GMAC_NSI,
	DIV_CLK_SERDES_PHY_REF,
	DIV_CLK_SERDES_PHY_CFG,
	DIV_CLK_SERDES_AXI,
};

static const struct sunxi_div_cfg sun55iw6_dividers[] = {
	DIV_CFG(DIV_CLK_DSI0, 0x1580, 0, 5, 0, 0, 0, 0, 0, NULL),
	DIV_CFG(DIV_CLK_DE0, 0xA00, 0, 5, 0, 0, 0, 0, 0, NULL),
	DIV_CFG(DIV_CLK_MMC0, 0xD00, 0, 5, 8, 5, 0, 0, 0, NULL),
	DIV_CFG(DIV_CLK_MMC1, 0xD10, 0, 5, 8, 5, 0, 0, 0, NULL),
	DIV_CFG(DIV_CLK_MMC2, 0xD20, 0, 5, 8, 5, 0, 0, 0, NULL),
	DIV_CFG(DIV_CLK_PLL_PERIPH0, 0xa0, 1, 1, 16, 3, 8, 8, 0, NULL),
	DIV_CFG(DIV_CLK_PLL_PERIPH0_800M, 0xa0, 1, 1, 20, 3, 8, 8, 0, NULL),
	DIV_CFG(DIV_CLK_PLL_PERIPH0_480M, 0xa0, 1, 1, 2, 3, 8, 8, 0, NULL),
	DIV_CFG(DIV_CLK_APB1, 0x518, 0, 5, 0, 0, 0, 0, 0, NULL),
	DIV_CFG(DIV_CLK_GMAC0_PHY, 0x1400, 0, 5, 0, 0, 0, 0, 0, NULL),
	DIV_CFG(DIV_CLK_GMAC1_PHY, 0x1410, 0, 5, 0, 0, 0, 0, 0, NULL),
	DIV_CFG(DIV_CLK_GMAC_NSI, 0x1420, 0, 5, 0, 0, 0, 0, 0, NULL),
	DIV_CFG(DIV_CLK_SERDES_PHY_REF, 0x13C4, 0, 5, 0, 0, 0, 0, 0, NULL),
	DIV_CFG(DIV_CLK_SERDES_PHY_CFG, 0x13C0, 0, 5, 0, 0, 0, 0, 0, NULL),
	DIV_CFG(DIV_CLK_SERDES_AXI, 0x13E0, 0, 5, 0, 0, 0, 0, 0, NULL),
};

static const struct clock_config sun55iw6_clock_cfg[] = {

	SUNXI_CLK_COMPOSITE_FACTOR(CLK_PLL_PERIPH0, "pll-peri0-parent", "osc24M", 0,
			GATE_CLK_PLL_PERIPH0, DIV_CLK_PLL_PERIPH0),

	SUNXI_CLK_COMPOSITE_FACTOR(CLK_PLL_PERIPH0_800M, "pll-peri0-800m", "osc24M", 0,
			GATE_CLK_PLL_PERIPH0, DIV_CLK_PLL_PERIPH0_800M),

	SUNXI_CLK_COMPOSITE_FACTOR(CLK_PLL_PERIPH0_480M, "pll-peri0-480m", "osc24M", 0,
			GATE_CLK_PLL_PERIPH0, DIV_CLK_PLL_PERIPH0_480M),

	SUNXI_CLK_FIXED(CLK_PLL_PERIPH0_600M, "pll-peri0-600m", "pll-peri0-parent", 2, 1, 0),
	SUNXI_CLK_FIXED(CLK_PLL_PERIPH0_300M, "pll-peri0-300m", "pll-peri0-600m", 2, 1, 0),
	SUNXI_CLK_FIXED(CLK_PLL_PERIPH0_150M, "pll-peri0-150m", "pll-peri0-300m", 2, 1, 0),

	SUNXI_CLK_FIXED(CLK_PLL_PERIPH0_400M, "pll-peri0-400m", "pll-peri0-parent", 3, 1, 0),
	SUNXI_CLK_FIXED(CLK_PLL_PERIPH0_200M, "pll-peri0-200m", "pll-peri0-400m", 2, 1, 0),
	SUNXI_CLK_FIXED(CLK_PLL_PERIPH0_160M, "pll-peri0-160m", "pll-peri0-480m", 3, 1, 0),

	SUNXI_CLK_COMPOSITE(CLK_APB1, "apb1", CLK_SET_RATE_PARENT,
			NO_SUNXI_GATE, MUX_APB1, DIV_CLK_APB1),

	SUNXI_CLK_GATE(CLK_BUS_DPSS, "dpss", "osc24M", 0, GATE_CLK_BUS_DPSS),
	SUNXI_CLK_GATE(CLK_BUS_DSI0, "dsi0-bus", "osc24M", 0, GATE_CLK_BUS_DSI0),
	SUNXI_CLK_GATE(CLK_BUS_DE0, "de0-bus", "osc24M", 0, GATE_CLK_BUS_DE0),
	SUNXI_CLK_GATE(CLK_BUS_VO0_TCONLCD0, "vo0-tconlcd0", "osc24M", 0, GATE_CLK_BUS_VO0_TCONLCD0),
	SUNXI_CLK_GATE(CLK_BUS_DMA, "dma-bus", "osc24M", 0, GATE_CLK_BUS_DMA),
	SUNXI_CLK_GATE(CLK_MBUS_DMA_GATE, "dma-mbus", "osc24M", 0, GATE_CLK_MBUS_DMA_GATE),
	SUNXI_CLK_GATE(CLK_BUS_OTG, "otg", "osc24M", 0, GATE_CLK_BUS_OTG),
	SUNXI_CLK_GATE(CLK_USB20_0_HOST_EHCI, "otg", "osc24M", 0, GATE_CLK_USB20_0_HOST_EHCI),
	SUNXI_CLK_GATE(CLK_USB20_1_HOST_EHCI, "otg", "osc24M", 0, GATE_CLK_USB20_1_HOST_EHCI),

	SUNXI_CLK_COMPOSITE(CLK_DSI0, "dsi0", 0,
			GATE_CLK_DSI0, MUX_CLK_DSI0, DIV_CLK_DSI0),

	SUNXI_CLK_COMPOSITE(CLK_DE0, "de0", 0,
			GATE_CLK_DE0, MUX_CLK_DE0, DIV_CLK_DE0),

	SUNXI_CLK_COMPOSITE(CLK_MMC0, "mmc0", 0,
			GATE_CLK_MMC0, MUX_CLK_MMC0, DIV_CLK_MMC0),
	SUNXI_CLK_COMPOSITE(CLK_MMC1, "mmc1", 0,
			GATE_CLK_MMC1, MUX_CLK_MMC1, DIV_CLK_MMC1),
	SUNXI_CLK_COMPOSITE(CLK_MMC2, "mmc2", 0,
			GATE_CLK_MMC2, MUX_CLK_MMC2, DIV_CLK_MMC2),
	SUNXI_CLK_GATE(CLK_BUS_MMC0, "mmc0-bus", "osc24M", 0, GATE_CLK_BUS_MMC0),
	SUNXI_CLK_GATE(CLK_BUS_MMC1, "mmc1-bus", "osc24M", 0, GATE_CLK_BUS_MMC1),
	SUNXI_CLK_GATE(CLK_BUS_MMC2, "mmc2-bus", "osc24M", 0, GATE_CLK_BUS_MMC2),

	SUNXI_CLK_GATE(CLK_MBUS_GMAC0_GATE, "gmac0-mbus", "osc24M", 0, GATE_CLK_MBUS_GMAC0),
	SUNXI_CLK_GATE(CLK_MBUS_GMAC1_GATE, "gmac1-mbus", "osc24M", 0, GATE_CLK_MBUS_GMAC1),
	SUNXI_CLK_GATE(CLK_AHB_GMAC0_GATE, "gmac0-ahb", "osc24M", 0, GATE_CLK_AHB_GMAC0),
	SUNXI_CLK_GATE(CLK_AHB_GMAC1_GATE, "gmac1-ahb", "osc24M", 0, GATE_CLK_AHB_GMAC1),
	SUNXI_CLK_GATE(CLK_GMAC0, "gmac0", "osc24M", 0, GATE_CLK_GMAC0),
	SUNXI_CLK_GATE(CLK_GMAC1, "gmac1", "osc24M", 0, GATE_CLK_GMAC1),
	SUNXI_CLK_COMPOSITE_FACTOR(CLK_GMAC0_PHY, "gmac0-phy", "pll-peri0-150m", 0, GATE_CLK_GMAC0_PHY, DIV_CLK_GMAC0_PHY),
	SUNXI_CLK_COMPOSITE_FACTOR(CLK_GMAC1_PHY, "gmac1-phy", "pll-peri0-150m", 0, GATE_CLK_GMAC1_PHY, DIV_CLK_GMAC1_PHY),
	SUNXI_CLK_COMPOSITE(CLK_GMAC_NSI, "gmac-nsi", 0, GATE_CLK_GMAC_NSI, MUX_CLK_GMAC_NSI, DIV_CLK_GMAC_NSI),

	SUNXI_CLK_GATE(CLK_PCIE_REF_AUX, "pcie-ref-aux", "osc24M", 0, GATE_CLK_PCIE_REF_AUX),
	SUNXI_CLK_GATE(CLK_PCIE_SLV, "pcie-slv", "pll-peri0-300m", 0, GATE_CLK_PCIE_SLV),
	SUNXI_CLK_COMPOSITE(CLK_SERDES_PHY_REF, "serdes-phy-ref", 0, GATE_CLK_SERDES_PHY_REF, MUX_SERDES_PHY_REF, DIV_CLK_SERDES_PHY_REF),
	SUNXI_CLK_COMPOSITE(CLK_SERDES_PHY_CFG, "serdes-phy-cfg", 0, GATE_CLK_SERDES_PHY_CFG, MUX_SERDES_PHY_CFG, DIV_CLK_SERDES_PHY_CFG),
	SUNXI_CLK_GATE(CLK_BUS_SERDES, "serdes-bus", "osc24M", 0, GATE_CLK_BUS_SERDES),
	SUNXI_CLK_GATE(CLK_AHB_SERDES, "serdes-ahb", "osc24M", 0, GATE_CLK_AHB_SERDES),
	SUNXI_CLK_COMPOSITE(CLK_SERDES_AXI, "serdes-axi", 0, GATE_CLK_SERDES_AXI, MUX_SERDES_PHY_AXI, DIV_CLK_SERDES_AXI),
	SUNXI_CLK_GATE(CLK_PWM0, "pwm0", "osc24M", 0, GATE_CLK_PWM0),
	SUNXI_CLK_GATE(CLK_PWM1, "pwm1", "osc24M", 0, GATE_CLK_PWM1),
	SUNXI_CLK_GATE(CLK_PWM2, "pwm2", "osc24M", 0, GATE_CLK_PWM2),

};

const struct ccu_desc sunxi_ccu_desc = {
	.resets = sunxi_resets,
	.num_resets = ARRAY_SIZE(sunxi_resets),
};

static const struct sunxi_clock_match_data sun55iw6_data = {
	.tab_clocks	= sun55iw6_clock_cfg,
	.num_clocks	= ARRAY_SIZE(sun55iw6_clock_cfg),
	.clock_data = &(const struct clk_sunxi_clock_data) {
		.num_gates	= ARRAY_SIZE(sun55iw6_gates),
		.gates		= sun55iw6_gates,
		.muxes		= sun55iw6_muxes,
		.dividers	= sun55iw6_dividers,
	},
};

static int sun55iw6_clk_probe(struct udevice *dev)
{
	int err;

	err = sunxi_clk_init(dev, &sun55iw6_data);
	if (err)
		return err;

	return 0;
}

static const struct udevice_id sun55iw6_clk_ids[] = {
	{ .compatible = "allwinner,sunxi-ccu",
	  .data = (ulong)&sunxi_ccu_desc },
	{ }
};

U_BOOT_DRIVER(sun55iw6_clk) = {
	.name		= "sun55iw6_clk",
	.id		= UCLASS_CLK,
	.of_match	= sun55iw6_clk_ids,
	.bind		= sunxi_clk_bind,
	.probe		= sun55iw6_clk_probe,
	.of_to_plat	= sunxi_clk_of_to_plat,
	.ops 		= &sunxi_ccu_ops,
	.priv_auto 	= sizeof(struct sunximp_rcc_priv),
};
