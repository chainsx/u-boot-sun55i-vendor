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
#include <dt-bindings/clock/sun65iw1p1-ccu.h>
#include <dt-bindings/reset/sun65iw1p1-ccu.h>
#include <linux/bitops.h>
#include <linux/clk-provider.h>
#include <clk/sunxi.h>
#include "sunxi-clk.h"

static struct ccu_reset sunxi_resets[] = {
	[RST_BUS_MMC0]		= RESET(0xd0c, BIT(16)),
	[RST_BUS_MMC1]		= RESET(0xd1c, BIT(16)),
	[RST_BUS_MMC2]		= RESET(0xd2c, BIT(16)),
};

static const char * const mmc_parents[] = {
	"osc24M", "pll-peri-200m", "pll-peri-150m"
};

enum enum_mux_cfg {
	MUX_CLK_MMC0,
};

static const struct sunxi_mux_cfg sun65iw1_muxes[] = {
	MUX_CFG(MUX_CLK_MMC0, mmc_parents, 0xd00, 24, 3),
};

enum enum_gate_cfg {
	GATE_CLK_BUS_MMC0,
	GATE_CLK_BUS_MMC1,
	GATE_CLK_BUS_MMC2,
	GATE_CLK_MMC0,
	GATE_CLK_MMC1,
	GATE_CLK_MMC2,
	GATE_CLK_PLL_PERIPH0,
};

enum enum_div_cfg {
	DIV_CLK_PLL_PERIPH0,
	DIV_CLK_PLL_PERIPH0_800M,
	DIV_CLK_PLL_PERIPH0_480M,
};

static const struct sunxi_gate_cfg sun65iw1_gates[] = {
	GATE_CFG(GATE_CLK_PLL_PERIPH0, 0xA0, BIT(31)),
	GATE_CFG(GATE_CLK_MMC0, 0x0d00, BIT(31)),
	GATE_CFG(GATE_CLK_MMC1, 0x0d10, BIT(31)),
	GATE_CFG(GATE_CLK_MMC2, 0x0d20, BIT(31)),
	GATE_CFG(GATE_CLK_BUS_MMC0, 0x0d04, BIT(0)),
	GATE_CFG(GATE_CLK_BUS_MMC1, 0x0d1c, BIT(1)),
	GATE_CFG(GATE_CLK_BUS_MMC2, 0x0d2c, BIT(2)),
};

static const struct sunxi_div_cfg sun65iw1_dividers[] = {
	DIV_CFG(DIV_CLK_PLL_PERIPH0, 0xa0, 1, 1, 16, 3, 8, 8, 0, NULL),
	DIV_CFG(DIV_CLK_PLL_PERIPH0_800M, 0xa0, 1, 1, 20, 3, 8, 8, 0, NULL),
	DIV_CFG(DIV_CLK_PLL_PERIPH0_480M, 0xa0, 1, 1, 2, 3, 8, 8, 0, NULL),
};

static const struct clock_config sun65iw1_clock_cfg[] = {

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

	SUNXI_CLK_COMPOSITE(CLK_MMC0, "mmc0", 0,
			GATE_CLK_MMC0, MUX_CLK_MMC0, NO_SUNXI_DIV),
	SUNXI_CLK_COMPOSITE(CLK_MMC1, "mmc1", 0,
			GATE_CLK_MMC1, MUX_CLK_MMC0, NO_SUNXI_DIV),
	SUNXI_CLK_COMPOSITE(CLK_MMC2, "mmc2", 0,
			GATE_CLK_MMC2, MUX_CLK_MMC0, NO_SUNXI_DIV),
	SUNXI_CLK_GATE(CLK_BUS_MMC0, "mmc0-bus", "osc24M", 0, GATE_CLK_BUS_MMC0),
	SUNXI_CLK_GATE(CLK_BUS_MMC1, "mmc1-bus", "osc24M", 0, GATE_CLK_BUS_MMC1),
	SUNXI_CLK_GATE(CLK_BUS_MMC2, "mmc2-bus", "osc24M", 0, GATE_CLK_BUS_MMC2),
};

const struct ccu_desc sunxi_ccu_desc = {
	.resets = sunxi_resets,
	.num_resets = ARRAY_SIZE(sunxi_resets),
};

static const struct sunxi_clock_match_data sun65iw1_data = {
	.tab_clocks	= sun65iw1_clock_cfg,
	.num_clocks	= ARRAY_SIZE(sun65iw1_clock_cfg),
	.clock_data = &(const struct clk_sunxi_clock_data) {
		.num_gates	= ARRAY_SIZE(sun65iw1_gates),
		.gates		= sun65iw1_gates,
		.muxes		= sun65iw1_muxes,
		.dividers	= sun65iw1_dividers,
	},
};

static int sun65iw1_clk_probe(struct udevice *dev)
{
	int err;

	err = sunxi_clk_init(dev, &sun65iw1_data);
	if (err)
		return err;

	return 0;
}

static const struct udevice_id sun65iw1_clk_ids[] = {
	{ .compatible = "allwinner,sunxi-ccu",
	  .data = (ulong)&sunxi_ccu_desc },
	{ }
};

U_BOOT_DRIVER(sun65iw1_clk) = {
	.name		= "sun65iw1_clk",
	.id		= UCLASS_CLK,
	.of_match	= sun65iw1_clk_ids,
	.bind		= sunxi_clk_bind,
	.probe		= sun65iw1_clk_probe,
	.of_to_plat	= sunxi_clk_of_to_plat,
	.ops 		= &sunxi_ccu_ops,
	.priv_auto 	= sizeof(struct sunximp_rcc_priv),
};
