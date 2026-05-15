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
#include <dt-bindings/clock/sun8iw21p1-ccu.h>
#include <dt-bindings/reset/sun8iw21p1-ccu.h>
#include <linux/bitops.h>
#include <linux/clk-provider.h>
#include <clk/sunxi.h>
#include "sunxi-clk.h"

static struct ccu_reset sunxi_resets[] = {
	[RST_BUS_DE]		= RESET(0x60c, BIT(16)),
	[RST_BUS_PWM]		= RESET(0x7ac, BIT(16)),

	[RST_BUS_DMA]		= RESET(0x70c, BIT(16)),

	[RST_BUS_MMC0]		= RESET(0x84c, BIT(16)),
	[RST_BUS_MMC1]		= RESET(0x84c, BIT(17)),
	[RST_BUS_MMC2]		= RESET(0x84c, BIT(18)),
	[RST_BUS_UART0]		= RESET(0x90c, BIT(16)),
	[RST_BUS_UART1]		= RESET(0x90c, BIT(17)),
	[RST_BUS_UART2]		= RESET(0x90c, BIT(18)),
	[RST_BUS_UART3]		= RESET(0x90c, BIT(19)),

	[RST_BUS_I2C0]		= RESET(0x91c, BIT(16)),
	[RST_BUS_I2C1]		= RESET(0x91c, BIT(17)),
	[RST_BUS_I2C2]		= RESET(0x91c, BIT(18)),
	[RST_BUS_I2C3]		= RESET(0x91c, BIT(19)),

	[RST_BUS_SPI0]		= RESET(0x96c, BIT(16)),
	[RST_BUS_SPI1]		= RESET(0x96c, BIT(17)),
	[RST_BUS_SPIF]		= RESET(0x9fc, BIT(20)),

	[RST_BUS_EMAC]		= RESET(0x97c, BIT(16)),

	[RST_USB_PHY0]		= RESET(0xa70, BIT(30)),

	[RST_USB_PHY1]		= RESET(0xa74, BIT(30)),

	[RST_USB_HSIC]		= RESET(0xa7c, BIT(28)),
	[RST_USB_PHY3]		= RESET(0xa7c, BIT(30)),

	[RST_BUS_OHCI0]		= RESET(0xa8c, BIT(16)),
	[RST_BUS_OHCI3]		= RESET(0xa8c, BIT(19)),
	[RST_BUS_EHCI0]		= RESET(0xa8c, BIT(20)),
	[RST_BUS_XHCI]		= RESET(0xa8c, BIT(21)),
	[RST_BUS_EHCI3]		= RESET(0xa8c, BIT(23)),
	[RST_BUS_OTG]		= RESET(0xa8c, BIT(24)),

	[RST_BUS_THS]		= RESET(0x9fc, BIT(16)),
};

static const char * const apb1_parents[] = {
	"osc24M", "ext32k", "rc16m", "pll-peri-600m"
};
static const char * const mmc_parents[] = {
	"osc24M", "pll-peri-200m", "pll-peri-150m"
};

static const char * const dsi_parents[] = {
	"osc24M", "pll-peri-200m", "pll-peri-150m"
};
enum mux_cfg {
	MUX_APB1,
	MUX_CLK_MMC0,
	MUX_CLK_DSI,
};

static const struct sunxi_mux_cfg sun8iw21_muxes[] = {
	MUX_CFG(MUX_APB1, apb1_parents, 0x524, 24, 2),
	MUX_CFG(MUX_CLK_MMC0, mmc_parents, 0x1580, 24, 3),
	MUX_CFG(MUX_CLK_DSI, dsi_parents, 0xB24, 24, 3),
};

enum gate_cfg {
	GATE_CLK_MMC0,
	GATE_CLK_MMC1,
	GATE_CLK_MMC2,

	GATE_CLK_DSI,

	GATE_CLK_BUS_MMC0,
	GATE_CLK_BUS_MMC1,
	GATE_CLK_BUS_MMC2,

	GATE_CLK_SPIF,
	GATE_CLK_BUS_SPIF,

	GATE_CLK_I2C0,
	GATE_CLK_I2C1,
	GATE_CLK_I2C2,
	GATE_CLK_I2C3,

	GATE_CLK_BUS_UART0,
	GATE_CLK_BUS_UART1,
	GATE_CLK_BUS_UART2,
	GATE_CLK_BUS_UART3,
	GATE_CLK_PLL_PERIPH0,

	GATE_CLK_BUS_OTG,

	GATE_CLK_BUS_DMA,
	GATE_CLK_MBUS_DMA_GATE,
};

static const struct sunxi_gate_cfg sun8iw21_gates[] = {
	GATE_CFG(GATE_CLK_MMC0, 0x0830, BIT(31)),
	GATE_CFG(GATE_CLK_MMC1, 0x0834, BIT(31)),
	GATE_CFG(GATE_CLK_MMC2, 0x0838, BIT(31)),

	GATE_CFG(GATE_CLK_DSI, 0x0B24, BIT(31)),

	GATE_CFG(GATE_CLK_BUS_MMC0, 0x084c, BIT(0)),
	GATE_CFG(GATE_CLK_BUS_MMC1, 0x084c, BIT(1)),
	GATE_CFG(GATE_CLK_BUS_MMC2, 0x084c, BIT(2)),

	GATE_CFG(GATE_CLK_SPIF, 0x0950, BIT(31)),
	GATE_CFG(GATE_CLK_BUS_SPIF, 0x096c, BIT(4)),

	GATE_CFG(GATE_CLK_I2C0, 0x091c, BIT(0)),
	GATE_CFG(GATE_CLK_I2C1, 0x091c, BIT(1)),
	GATE_CFG(GATE_CLK_I2C2, 0x091c, BIT(2)),
	GATE_CFG(GATE_CLK_I2C3, 0x091c, BIT(3)),

	GATE_CFG(GATE_CLK_BUS_UART0, 0x90c, BIT(0)),
	GATE_CFG(GATE_CLK_BUS_UART1, 0x90c, BIT(1)),
	GATE_CFG(GATE_CLK_BUS_UART2, 0x90c, BIT(2)),
	GATE_CFG(GATE_CLK_BUS_UART3, 0x90c, BIT(3)),
	GATE_CFG(GATE_CLK_PLL_PERIPH0, 0x20, BIT(31) | BIT(27) | BIT(30)),

	GATE_CFG(GATE_CLK_BUS_OTG, 0xa8c, BIT(8)),

	GATE_CFG(GATE_CLK_BUS_DMA, 0x70c, BIT(0)),
	GATE_CFG(GATE_CLK_MBUS_DMA_GATE, 0x804, BIT(0)),
};

enum div_cfg {
	DIV_CLK_PLL_PERIPH0,
	DIV_CLK_PLL_PERIPH0_800M,
	DIV_CLK_PLL_PERIPH0_480M,
	DIV_CLK_APB1,
	DIV_CLK_DSI,
};

static const struct sunxi_div_cfg sun8iw21_dividers[] = {
	DIV_CFG(DIV_CLK_PLL_PERIPH0, 0x20, 1, 1, 16, 3, 8, 8, 0, NULL),
	DIV_CFG(DIV_CLK_PLL_PERIPH0_800M, 0x20, 1, 1, 20, 3, 8, 8, 0, NULL),
	DIV_CFG(DIV_CLK_PLL_PERIPH0_480M, 0x20, 1, 1, 2, 3, 8, 8, 0, NULL),
	DIV_CFG(DIV_CLK_APB1, 0x524, 0, 5, 0, 0, 0, 0, 0, NULL),
	DIV_CFG(DIV_CLK_DSI, 0xB24, 0, 4, 0, 0, 0, 0, 0, NULL),
};

static const struct clock_config sun8iw21_clock_cfg[] = {

	SUNXI_CLK_COMPOSITE(CLK_APB1, "apb1", CLK_SET_RATE_PARENT,
			NO_SUNXI_GATE, MUX_APB1, DIV_CLK_APB1),

	SUNXI_CLK_COMPOSITE(CLK_MMC0, "mmc0", 0,
			GATE_CLK_MMC0, MUX_CLK_MMC0, NO_SUNXI_DIV),
	SUNXI_CLK_COMPOSITE(CLK_MMC1, "mmc1", 0,
			GATE_CLK_MMC1, MUX_CLK_MMC0, NO_SUNXI_DIV),
	SUNXI_CLK_COMPOSITE(CLK_MMC2, "mmc2", 0,
			GATE_CLK_MMC2, MUX_CLK_MMC0, NO_SUNXI_DIV),

	SUNXI_CLK_COMPOSITE(CLK_DSI, "dsi", 0,
			GATE_CLK_DSI, MUX_CLK_DSI, DIV_CLK_DSI),

	SUNXI_CLK_GATE(CLK_BUS_I2C0, "twi0", "apb1", 0, GATE_CLK_I2C0),
	SUNXI_CLK_GATE(CLK_BUS_I2C1, "twi1", "apb1", 0, GATE_CLK_I2C1),
	SUNXI_CLK_GATE(CLK_BUS_I2C2, "twi2", "apb1", 0, GATE_CLK_I2C2),
	SUNXI_CLK_GATE(CLK_BUS_I2C3, "twi3", "apb1", 0, GATE_CLK_I2C3),

	SUNXI_CLK_GATE(CLK_BUS_MMC0, "mmc0-bus", "osc24M", 0, GATE_CLK_BUS_MMC0),
	SUNXI_CLK_GATE(CLK_BUS_MMC1, "mmc1-bus", "osc24M", 0, GATE_CLK_BUS_MMC1),
	SUNXI_CLK_GATE(CLK_BUS_MMC2, "mmc2-bus", "osc24M", 0, GATE_CLK_BUS_MMC2),

	SUNXI_CLK_GATE(CLK_BUS_SPIF, "spif-bus", "osc24M", 0, GATE_CLK_BUS_SPIF),
	SUNXI_CLK_GATE(CLK_SPIF, "spif", "osc24M", 0, GATE_CLK_SPIF),

	SUNXI_CLK_GATE(CLK_BUS_UART0, "uart0", "apb1", 0, GATE_CLK_BUS_UART0),
	SUNXI_CLK_GATE(CLK_BUS_UART1, "uart1", "apb1", 0, GATE_CLK_BUS_UART1),
	SUNXI_CLK_GATE(CLK_BUS_UART2, "uart2", "apb1", 0, GATE_CLK_BUS_UART2),
	SUNXI_CLK_GATE(CLK_BUS_UART3, "uart3", "apb1", 0, GATE_CLK_BUS_UART3),

	SUNXI_CLK_GATE(CLK_BUS_DMA, "dma-bus", "osc24M", 0, GATE_CLK_BUS_DMA),
	SUNXI_CLK_GATE(CLK_MBUS_DMA_GATE, "dma-mbus", "osc24M", 0, GATE_CLK_MBUS_DMA_GATE),

	SUNXI_CLK_GATE(CLK_BUS_OTG, "otg", "osc24M", 0, GATE_CLK_BUS_OTG),

	SUNXI_CLK_COMPOSITE_FACTOR(CLK_PLL_PERIPH0, "pll-peri-parent", "osc24M", 0,
			GATE_CLK_PLL_PERIPH0, DIV_CLK_PLL_PERIPH0),

	SUNXI_CLK_COMPOSITE_FACTOR(CLK_PLL_PERIPH0_800M, "pll-peri-800m", "osc24M", 0,
			GATE_CLK_PLL_PERIPH0, DIV_CLK_PLL_PERIPH0_800M),

	SUNXI_CLK_COMPOSITE_FACTOR(CLK_PLL_PERIPH0_480M, "pll-peri-480m", "osc24M", 0,
			GATE_CLK_PLL_PERIPH0, DIV_CLK_PLL_PERIPH0_480M),

	SUNXI_CLK_FIXED(CLK_PLL_PERIPH0_600M, "pll-peri-600m", "pll-peri-parent", 2, 1, 0),
	SUNXI_CLK_FIXED(CLK_PLL_PERIPH0_300M, "pll-peri-300m", "pll-peri-600m", 2, 1, 0),
	SUNXI_CLK_FIXED(CLK_PLL_PERIPH0_150M, "pll-peri-150m", "pll-peri-300m", 2, 1, 0),

	SUNXI_CLK_FIXED(CLK_PLL_PERIPH0_400M, "pll-peri-400m", "pll-peri-parent", 3, 1, 0),
	SUNXI_CLK_FIXED(CLK_PLL_PERIPH0_200M, "pll-peri-200m", "pll-peri-400m", 2, 1, 0),
	SUNXI_CLK_FIXED(CLK_PLL_PERIPH0_160M, "pll-peri-160m", "pll-peri-480m", 3, 1, 0),
};

const struct ccu_desc sunxi_ccu_desc = {
	.resets = sunxi_resets,
	.num_resets = ARRAY_SIZE(sunxi_resets),
};

static const struct sunxi_clock_match_data sun8iw21_data = {
	.tab_clocks	= sun8iw21_clock_cfg,
	.num_clocks	= ARRAY_SIZE(sun8iw21_clock_cfg),
	.clock_data = &(const struct clk_sunxi_clock_data) {
		.num_gates	= ARRAY_SIZE(sun8iw21_gates),
		.gates		= sun8iw21_gates,
		.muxes		= sun8iw21_muxes,
		.dividers	= sun8iw21_dividers,
	},
};

static int sun8iw21_clk_probe(struct udevice *dev)
{
	int err;

	err = sunxi_clk_init(dev, &sun8iw21_data);
	if (err)
		return err;

	return 0;
}

static const struct udevice_id sun8iw21_clk_ids[] = {
	{ .compatible = "allwinner,sunxi-ccu",
	  .data = (ulong)&sunxi_ccu_desc },
	{ }
};

U_BOOT_DRIVER(sun8iw21_clk) = {
	.name		= "sun8iw21_clk",
	.id		= UCLASS_CLK,
	.of_match	= sun8iw21_clk_ids,
	.bind		= sunxi_clk_bind,
	.probe		= sun8iw21_clk_probe,
	.of_to_plat	= sunxi_clk_of_to_plat,
	.ops 		= &sunxi_ccu_ops,
	.priv_auto 	= sizeof(struct sunximp_rcc_priv),
};
