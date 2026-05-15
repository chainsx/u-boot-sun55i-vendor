// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2024 Allwinner
 */

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <errno.h>
#include <eth_phy.h>
#include <log.h>
#include <malloc.h>
#include <miiphy.h>
#include <net.h>
#include <netdev.h>
#include <phy.h>
#include <reset.h>
#include <cpu_func.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <dm/pinctrl.h>
#include <dm/device.h>
#include <dm/device_compat.h>
#include <power/regulator.h>
#include <linux/bitfield.h>
#include <linux/delay.h>

#include "dwc_eth_qos.h"

#define DWMAC_SUNXI_MODULE_VERSION	"0.1.0"

/* GMAC-200 Register */
#define SUNXI_DWMAC200_SYSCON_REG	(0x00)
	#define SUNXI_DWMAC200_SYSCON_BPS_EFUSE		GENMASK(31, 28)
	#define SUNXI_DWMAC200_SYSCON_XMII_SEL		BIT(27)
	#define SUNXI_DWMAC200_SYSCON_EPHY_MODE		GENMASK(26, 25)
	#define SUNXI_DWMAC200_SYSCON_PHY_ADDR		GENMASK(24, 20)
	#define SUNXI_DWMAC200_SYSCON_BIST_CLK_EN	BIT(19)
	#define SUNXI_DWMAC200_SYSCON_CLK_SEL		BIT(18)
	#define SUNXI_DWMAC200_SYSCON_LED_POL		BIT(17)
	#define SUNXI_DWMAC200_SYSCON_SHUTDOWN		BIT(16)
	#define SUNXI_DWMAC200_SYSCON_PHY_SEL		BIT(15)
	#define SUNXI_DWMAC200_SYSCON_ENDIAN_MODE	BIT(14)
	#define SUNXI_DWMAC200_SYSCON_RMII_EN		BIT(13)
	#define SUNXI_DWMAC200_SYSCON_ETXDC			GENMASK(12, 10)
	#define SUNXI_DWMAC200_SYSCON_ERXDC			GENMASK(9, 5)
	#define SUNXI_DWMAC200_SYSCON_ERXIE			BIT(4)
	#define SUNXI_DWMAC200_SYSCON_ETXIE			BIT(3)
	#define SUNXI_DWMAC200_SYSCON_EPIT			BIT(2)
	#define SUNXI_DWMAC200_SYSCON_ETCS			GENMASK(1, 0)

/* GMAC-210 Register */
#define SUNXI_DWMAC210_CFG_REG	(0x00)
	#define SUNXI_DWMAC210_CFG_ETXDC_H		GENMASK(17, 16)
	#define SUNXI_DWMAC210_CFG_PHY_SEL		BIT(15)
	#define SUNXI_DWMAC210_CFG_ENDIAN_MODE	BIT(14)
	#define SUNXI_DWMAC210_CFG_RMII_EN		BIT(13)
	#define SUNXI_DWMAC210_CFG_ETXDC_L		GENMASK(12, 10)
	#define SUNXI_DWMAC210_CFG_ERXDC		GENMASK(9, 5)
	#define SUNXI_DWMAC210_CFG_ERXIE		BIT(4)
	#define SUNXI_DWMAC210_CFG_ETXIE		BIT(3)
	#define SUNXI_DWMAC210_CFG_EPIT			BIT(2)
	#define SUNXI_DWMAC210_CFG_ETCS			GENMASK(1, 0)
#define SUNXI_DWMAC210_PTP_TIMESTAMP_L_REG	(0x40)
#define SUNXI_DWMAC210_PTP_TIMESTAMP_H_REG	(0x48)
#define SUNXI_DWMAC210_STAT_INT_REG		(0x4C)
	#define SUNXI_DWMAC210_STAT_PWR_DOWN_ACK	BIT(4)
	#define SUNXI_DWMAC210_STAT_SBD_TX_CLK_GATE	BIT(3)
	#define SUNXI_DWMAC210_STAT_LPI_INT			BIT(1)
	#define SUNXI_DWMAC210_STAT_PMT_INT			BIT(0)
#define SUNXI_DWMAC210_CLK_GATE_CFG_REG	(0x80)
	#define SUNXI_DWMAC210_CLK_GATE_CFG_RX		BIT(7)
	#define SUNXI_DWMAC210_CLK_GATE_CFG_PTP_REF	BIT(6)
	#define SUNXI_DWMAC210_CLK_GATE_CFG_CSR		BIT(5)
	#define SUNXI_DWMAC210_CLK_GATE_CFG_TX		BIT(4)
	#define SUNXI_DWMAC210_CLK_GATE_CFG_APP		BIT(3)

#define SUNXI_DWMAC_ETCS_MII		0x0
#define SUNXI_DWMAC_ETCS_EXT_GMII	0x1
#define SUNXI_DWMAC_ETCS_INT_GMII	0x2

/* MAC flags defined */
#define SUNXI_DWMAC_NSI_CLK_GATE	BIT(0)

struct sunxi_dwmac;

enum sunxi_dwmac_delaychain_dir {
	SUNXI_DWMAC_DELAYCHAIN_TX,
	SUNXI_DWMAC_DELAYCHAIN_RX,
};

struct sunxi_dwmac_variant {
	u32 flags;
	u32 default_syscon_value;
	u32 interface;
	u32 rx_delay_max;
	u32 tx_delay_max;
	int (*set_syscon)(struct sunxi_dwmac *chip);
	void (*unset_syscon)(struct sunxi_dwmac *chip);
	int (*set_delaychain)(struct sunxi_dwmac *chip, enum sunxi_dwmac_delaychain_dir dir, u32 delay);
	u32 (*get_delaychain)(struct sunxi_dwmac *chip, enum sunxi_dwmac_delaychain_dir dir);
};

struct sunxi_dwmac {
    struct udevice *dev;
    const struct sunxi_dwmac_variant *variant;
	void *syscfg_base;
    struct udevice *dwmac_supply;
    struct udevice *phy_supply;
	struct clk *pclk;
	struct clk *ahb_clk;
	struct clk *phy_clk;
	struct clk *mac_clk;
	struct clk *nsi_clk;
	struct reset_ctl *mac_rst;
	struct reset_ctl *ahb_rst;
    bool rgmii_clk_ext;
	bool soc_phy_clk_en;
	int bus_num;
    int interface;
	u32 tx_delay;
	u32 rx_delay;
};

static int sunxi_dwmac200_set_syscon(struct sunxi_dwmac *chip)
{
	u32 reg_val = chip->variant->default_syscon_value;

	/* Clear interface mode bits */
	reg_val &= ~(SUNXI_DWMAC200_SYSCON_ETCS | SUNXI_DWMAC200_SYSCON_EPIT);
	if (chip->variant->interface & PHY_INTERFACE_MODE_RMII)
		reg_val &= ~SUNXI_DWMAC200_SYSCON_RMII_EN;

	switch (chip->interface) {
	case PHY_INTERFACE_MODE_MII:
		/* default */
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_TXID:
		reg_val |= SUNXI_DWMAC200_SYSCON_EPIT;
		reg_val |= FIELD_PREP(SUNXI_DWMAC200_SYSCON_ETCS,
					chip->rgmii_clk_ext ? SUNXI_DWMAC_ETCS_EXT_GMII : SUNXI_DWMAC_ETCS_INT_GMII);
		break;
	case PHY_INTERFACE_MODE_RMII:
		reg_val |= SUNXI_DWMAC200_SYSCON_RMII_EN;
		reg_val &= ~SUNXI_DWMAC200_SYSCON_ETCS;
		break;
	default:
		dev_err(chip->dev, "Unsupported interface mode: %s\n", phy_string_for_interface(chip->interface));
		return -EINVAL;
	}

	writel(reg_val, chip->syscfg_base + SUNXI_DWMAC200_SYSCON_REG);
	return 0;
}

static void sunxi_dwmac200_unset_syscon(struct sunxi_dwmac *chip)
{
	writel(chip->variant->default_syscon_value, chip->syscfg_base + SUNXI_DWMAC200_SYSCON_REG);
}

static int sunxi_dwmac200_set_delaychain(struct sunxi_dwmac *chip, enum sunxi_dwmac_delaychain_dir dir, u32 delay)
{
	u32 reg_val = readl(chip->syscfg_base + SUNXI_DWMAC200_SYSCON_REG);
	int ret = -EINVAL;

	switch (dir) {
	case SUNXI_DWMAC_DELAYCHAIN_TX:
		if (delay <= chip->variant->tx_delay_max) {
			reg_val &= ~SUNXI_DWMAC200_SYSCON_ETXDC;
			reg_val |= FIELD_PREP(SUNXI_DWMAC200_SYSCON_ETXDC, delay);
			ret = 0;
		}
		break;
	case SUNXI_DWMAC_DELAYCHAIN_RX:
		if (delay <= chip->variant->rx_delay_max) {
			reg_val &= ~SUNXI_DWMAC200_SYSCON_ERXDC;
			reg_val |= FIELD_PREP(SUNXI_DWMAC200_SYSCON_ERXDC, delay);
			ret = 0;
		}
		break;
	}

	if (!ret)
		writel(reg_val, chip->syscfg_base + SUNXI_DWMAC200_SYSCON_REG);

	return ret;
}

static u32 sunxi_dwmac200_get_delaychain(struct sunxi_dwmac *chip, enum sunxi_dwmac_delaychain_dir dir)
{
	u32 delay = 0;
	u32 reg_val = readl(chip->syscfg_base + SUNXI_DWMAC200_SYSCON_REG);

	switch (dir) {
	case SUNXI_DWMAC_DELAYCHAIN_TX:
		delay = FIELD_GET(SUNXI_DWMAC200_SYSCON_ETXDC, reg_val);
		break;
	case SUNXI_DWMAC_DELAYCHAIN_RX:
		delay = FIELD_GET(SUNXI_DWMAC200_SYSCON_ERXDC, reg_val);
		break;
	default:
		dev_err(chip->dev, "Unknow delaychain dir %d\n", dir);
	}

	return delay;
}

static int sunxi_dwmac210_set_delaychain(struct sunxi_dwmac *chip, enum sunxi_dwmac_delaychain_dir dir, u32 delay)
{
	u32 reg_val = readl(chip->syscfg_base + SUNXI_DWMAC210_CFG_REG);
	int ret = -EINVAL;

	switch (dir) {
	case SUNXI_DWMAC_DELAYCHAIN_TX:
		if (delay <= chip->variant->tx_delay_max) {
			reg_val &= ~(SUNXI_DWMAC210_CFG_ETXDC_H | SUNXI_DWMAC210_CFG_ETXDC_L);
			reg_val |= FIELD_PREP(SUNXI_DWMAC210_CFG_ETXDC_H, delay >> 3);
			reg_val |= FIELD_PREP(SUNXI_DWMAC210_CFG_ETXDC_L, delay);
			ret = 0;
		}
		break;
	case SUNXI_DWMAC_DELAYCHAIN_RX:
		if (delay <= chip->variant->rx_delay_max) {
			reg_val &= ~SUNXI_DWMAC210_CFG_ERXDC;
			reg_val |= FIELD_PREP(SUNXI_DWMAC210_CFG_ERXDC, delay);
			ret = 0;
		}
		break;
	}

	if (!ret)
		writel(reg_val, chip->syscfg_base + SUNXI_DWMAC210_CFG_REG);

	return ret;
}

static u32 sunxi_dwmac210_get_delaychain(struct sunxi_dwmac *chip, enum sunxi_dwmac_delaychain_dir dir)
{
	u32 delay = 0;
	u32 tx_l, tx_h;
	u32 reg_val = readl(chip->syscfg_base + SUNXI_DWMAC210_CFG_REG);

	switch (dir) {
	case SUNXI_DWMAC_DELAYCHAIN_TX:
		tx_h = FIELD_GET(SUNXI_DWMAC210_CFG_ETXDC_H, reg_val);
		tx_l = FIELD_GET(SUNXI_DWMAC210_CFG_ETXDC_L, reg_val);
		delay = (tx_h << 3 | tx_l);
		break;
	case SUNXI_DWMAC_DELAYCHAIN_RX:
		delay = FIELD_GET(SUNXI_DWMAC210_CFG_ERXDC, reg_val);
		break;
	}

	return delay;
}

static const struct sunxi_dwmac_variant dwmac200_variant = {
	.default_syscon_value = 0,
	.interface = PHY_INTERFACE_MODE_RMII | PHY_INTERFACE_MODE_RGMII,
	.rx_delay_max = 31,
	.tx_delay_max = 7,
	.set_syscon = sunxi_dwmac200_set_syscon,
	.unset_syscon = sunxi_dwmac200_unset_syscon,
	.set_delaychain = sunxi_dwmac200_set_delaychain,
	.get_delaychain = sunxi_dwmac200_get_delaychain,
};

static const struct sunxi_dwmac_variant dwmac210_variant = {
	.default_syscon_value = 0,
	.interface = PHY_INTERFACE_MODE_RMII | PHY_INTERFACE_MODE_RGMII,
	.rx_delay_max = 31,
	.tx_delay_max = 31,
	.set_syscon = sunxi_dwmac200_set_syscon,
	.unset_syscon = sunxi_dwmac200_unset_syscon,
	.set_delaychain = sunxi_dwmac210_set_delaychain,
	.get_delaychain = sunxi_dwmac210_get_delaychain,
};

static const struct sunxi_dwmac_variant dwmac220_variant = {
	.default_syscon_value = 0,
	.interface = PHY_INTERFACE_MODE_RMII | PHY_INTERFACE_MODE_RGMII,
	.flags = SUNXI_DWMAC_NSI_CLK_GATE,
	.rx_delay_max = 31,
	.tx_delay_max = 31,
	.set_syscon = sunxi_dwmac200_set_syscon,
	.unset_syscon = sunxi_dwmac200_unset_syscon,
	.set_delaychain = sunxi_dwmac210_set_delaychain,
	.get_delaychain = sunxi_dwmac210_get_delaychain,
};

static int sunxi_dwmac_power_on(struct sunxi_dwmac *chip)
{
	int ret;

	if (chip->dwmac_supply) {
		ret = regulator_set_enable(chip->dwmac_supply, true);
		if (ret) {
			dev_err(chip->dev, "Error enabling dwmac3v3 supply\n");
			goto err_dwmac3v3;
		}
	}

	if (chip->phy_supply) {
		ret = regulator_set_enable(chip->phy_supply, true);
		if (ret) {
			dev_err(chip->dev, "Error enabling phy3v3 supply\n");
			goto err_phy3v3;
		}
	}

	return 0;
err_dwmac3v3:
err_phy3v3:
	if (chip->dwmac_supply)
		regulator_set_enable(chip->dwmac_supply, false);
	return ret;
}

static void sunxi_dwmac_power_off(struct sunxi_dwmac *chip)
{
	if (chip->phy_supply)
		regulator_set_enable(chip->phy_supply, false);
	if (chip->dwmac_supply)
		regulator_set_enable(chip->dwmac_supply, false);
}

static int sunxi_dwmac_hw_init(struct sunxi_dwmac *chip)
{
	int ret;

#if defined(CONFIG_DM_REGULATOR)
	sunxi_dwmac_power_on(chip);
#endif

	ret = chip->variant->set_syscon(chip);
	if (ret < 0) {
		dev_err(chip->dev, "Set syscon failed\n");
		goto err;
	}

	ret = chip->variant->set_delaychain(chip, SUNXI_DWMAC_DELAYCHAIN_TX, chip->tx_delay);
	if (ret < 0) {
		dev_err(chip->dev, "Invalid TX clock delay: %d\n", chip->tx_delay);
		goto err;
	}

	ret = chip->variant->set_delaychain(chip, SUNXI_DWMAC_DELAYCHAIN_RX, chip->rx_delay);
	if (ret < 0) {
		dev_err(chip->dev, "Invalid RX clock delay: %d\n", chip->rx_delay);
		goto err;
	}

	return 0;
err:
#if defined(CONFIG_DM_REGULATOR)
	sunxi_dwmac_power_off(chip);
#endif
	return ret;
}

static void sunxi_dwmac_hw_exit(struct sunxi_dwmac *chip)
{
	chip->variant->unset_syscon(chip);
#if defined(CONFIG_DM_REGULATOR)
	sunxi_dwmac_power_off(chip);
#endif
}

static int eqos_probe_resources_aw(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	struct sunxi_dwmac *chip;
	const char *variant;
	int reset_flags = GPIOD_IS_OUT;
	int ret;

	chip = malloc(sizeof(*chip));
	if (!chip)
		return -ENOMEM;
	variant = dev_read_string(dev, "aw,gmac-version");
	if (!strcmp(variant, "200"))
		chip->variant = &dwmac200_variant;
	else if (!strcmp(variant, "210"))
		chip->variant = &dwmac210_variant;
	else if (!strcmp(variant, "220"))
		chip->variant = &dwmac220_variant;
	else
		goto err;
	chip->dev = dev;
	chip->bus_num = dev_seq(chip->dev);
	chip->syscfg_base = dev_read_addr_index_ptr(dev, 1);
	chip->interface = eqos->config->interface(dev);
	chip->rgmii_clk_ext = dev_read_bool(dev, "aw,rgmii-clk-ext");
	chip->soc_phy_clk_en = dev_read_bool(dev, "aw,soc-phy-clk-en");
	ret = dev_read_u32(dev, "tx-delay", &chip->tx_delay);
	if (ret)
		chip->tx_delay = 0;
	ret = dev_read_u32(dev, "rx-delay", &chip->rx_delay);
	if (ret)
		chip->rx_delay = 0;
#if defined(CONFIG_DM_REGULATOR)
	device_get_supply_regulator(dev, "dwmac-supply", &chip->dwmac_supply);
	device_get_supply_regulator(dev, "phy-supply", &chip->phy_supply);
#endif
	eqos->bsp_priv = chip;

	chip->mac_rst = devm_reset_control_get(dev, "stmmaceth");
	if (IS_ERR(chip->mac_rst)) {
		ret = PTR_ERR(chip->mac_rst);
		dev_err(dev, "get mac_rst failed: %d\n", ret);
		goto err;
	}

	chip->ahb_rst = devm_reset_control_get(dev, "ahb");
	if (IS_ERR(chip->ahb_rst)) {
		ret = PTR_ERR(chip->ahb_rst);
		dev_err(dev, "get ahb_rst failed: %d\n", ret);
		goto err;
	}

	chip->pclk = devm_clk_get(dev, "pclk");
	if (IS_ERR(chip->pclk)) {
		ret = PTR_ERR(chip->pclk);
		dev_err(dev, "get pclk failed: %d\n", ret);
		goto err;
	}

	chip->ahb_clk = devm_clk_get(dev, "ahb");
	if (IS_ERR(chip->ahb_clk)) {
		ret = PTR_ERR(chip->ahb_clk);
		dev_err(dev, "get ahb_clk failed: %d\n", ret);
		goto err;
	}

	chip->phy_clk = devm_clk_get(dev, "phy");
	if (IS_ERR(chip->phy_clk)) {
		ret = PTR_ERR(chip->phy_clk);
		dev_err(dev, "get phy_clk failed: %d\n", ret);
		goto err;
	}

	chip->mac_clk = devm_clk_get(dev, "stmmaceth");
	if (IS_ERR(chip->mac_clk)) {
		ret = PTR_ERR(chip->mac_clk);
		dev_err(dev, "get mac_clk failed: %d\n", ret);
		goto err;
	}

	if (chip->variant->flags & SUNXI_DWMAC_NSI_CLK_GATE) {
		chip->nsi_clk = devm_clk_get(dev, "nsi");
		if (IS_ERR(chip->nsi_clk)) {
			ret = PTR_ERR(chip->nsi_clk);
			dev_err(dev, "get nsi_clk failed: %d\n", ret);
			goto err;
		}
	}

	if (dev_read_bool(dev, "snps,reset-active-low"))
		reset_flags |= GPIOD_ACTIVE_LOW;
	gpio_request_by_name(dev, "snps,reset-gpio", 0, &eqos->phy_reset_gpio, reset_flags);
	dev_read_u32_array(dev, "snps,reset-delays-us", eqos->reset_delays, 3);

	dev_dbg(dev, "gmac%s eth%d interface %s with speed %d\n", variant, chip->bus_num, phy_string_for_interface(chip->interface), eqos->max_speed);
	dev_dbg(dev, "delaychain tx:%d rx:%d\n", chip->tx_delay, chip->rx_delay);
	dev_dbg(dev, "clk dir %s with phy25m %s\n", chip->rgmii_clk_ext ? "external" : "internal", chip->soc_phy_clk_en ? "soc" : "osc");

	return 0;
err:
	if (chip)
		free(chip);
	return ret;
}

static int eqos_remove_resources_aw(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	struct sunxi_dwmac *chip = eqos->bsp_priv;

	dm_gpio_free(dev, &eqos->phy_reset_gpio);

	if (chip)
		free(chip);

	return 0;
}

static int eqos_start_resets_aw(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	struct sunxi_dwmac *chip = eqos->bsp_priv;
	int ret;

	if (eqos->phy)
		return 0;

	ret = dm_gpio_set_value(&eqos->phy_reset_gpio, 0);
	if (ret < 0) {
		dev_err(chip->dev, "set phy rst stage0 failed %d\n", ret);
		return ret;
	}

	udelay(eqos->reset_delays[0]);

	ret = dm_gpio_set_value(&eqos->phy_reset_gpio, 1);
	if (ret < 0) {
		dev_err(chip->dev, "set phy rst stage1 failed %d\n", ret);
		return ret;
	}

	udelay(eqos->reset_delays[1]);

	ret = dm_gpio_set_value(&eqos->phy_reset_gpio, 0);
	if (ret < 0) {
		dev_err(chip->dev, "set phy rst stage2 failed %d\n", ret);
		return ret;
	}

	udelay(eqos->reset_delays[2]);

	return 0;
}

static int eqos_start_clks_aw(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	struct sunxi_dwmac *chip = eqos->bsp_priv;
	int ret;

	reset_assert(chip->ahb_rst);
	reset_assert(chip->mac_rst);
	udelay(2);
	reset_deassert(chip->ahb_rst);
	reset_deassert(chip->mac_rst);

	if (chip->variant->flags & SUNXI_DWMAC_NSI_CLK_GATE) {
		ret = clk_enable(chip->nsi_clk);
		if (ret < 0) {
			dev_err(dev, "clk_enable(nsi_clk) failed: %d\n", ret);
			goto err_nsi_clk;
		}
	}

	ret = clk_enable(chip->pclk);
	if (ret < 0) {
		dev_err(dev, "clk_enable(pclk) failed: %d\n", ret);
		goto err_pclk;
	}

	ret = clk_enable(chip->ahb_clk);
	if (ret < 0) {
		dev_err(dev, "clk_enable(ahb_clk) failed: %d\n", ret);
		goto err_ahb_clk;
	}

	ret = clk_enable(chip->mac_clk);
	if (ret < 0) {
		dev_err(dev, "clk_enable(mac_clk) failed: %d\n", ret);
		goto err_mac_clk;
	}

	if (chip->soc_phy_clk_en) {
		ret = clk_enable(chip->phy_clk);
		if (ret < 0) {
			dev_err(dev, "clk_enable(phy_clk) failed: %d\n", ret);
			goto err_phy_clk;
		}
	}

	ret = sunxi_dwmac_hw_init(chip);
	if (ret < 0) {
		dev_err(dev, "sunxi_dwmac_hw_init failed: %d\n", ret);
		goto err_hw_init;
	}

	return 0;
err_hw_init:
	if (chip->soc_phy_clk_en)
		clk_enable(chip->phy_clk);
err_phy_clk:
	clk_enable(chip->mac_clk);
err_mac_clk:
	clk_enable(chip->ahb_clk);
err_ahb_clk:
	clk_enable(chip->pclk);
err_pclk:
	if (chip->variant->flags & SUNXI_DWMAC_NSI_CLK_GATE)
		clk_enable(chip->nsi_clk);
err_nsi_clk:
	reset_assert(chip->ahb_rst);
	reset_assert(chip->mac_rst);
	return ret;
}

static int eqos_stop_clks_aw(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	struct sunxi_dwmac *chip = eqos->bsp_priv;

	sunxi_dwmac_hw_exit(chip);

	if (chip->soc_phy_clk_en)
		clk_enable(chip->phy_clk);
	clk_enable(chip->mac_clk);
	clk_enable(chip->ahb_clk);
	clk_enable(chip->pclk);
	if (chip->variant->flags & SUNXI_DWMAC_NSI_CLK_GATE)
		clk_enable(chip->nsi_clk);
	reset_assert(chip->ahb_rst);
	reset_assert(chip->mac_rst);

	return 0;
}

static struct eqos_ops eqos_aw_ops = {
	.eqos_inval_desc = eqos_inval_desc_generic,
	.eqos_flush_desc = eqos_flush_desc_generic,
	.eqos_inval_buffer = eqos_inval_buffer_generic,
	.eqos_flush_buffer = eqos_flush_buffer_generic,
	.eqos_probe_resources = eqos_probe_resources_aw,
	.eqos_remove_resources = eqos_remove_resources_aw,
	.eqos_stop_resets = eqos_null_ops,
	.eqos_start_resets = eqos_start_resets_aw,
	.eqos_stop_clks = eqos_stop_clks_aw,
	.eqos_start_clks = eqos_start_clks_aw,
	.eqos_calibrate_pads = eqos_null_ops,
	.eqos_disable_calibration = eqos_null_ops,
	.eqos_set_tx_clk_speed = eqos_null_ops,
	.eqos_get_enetaddr = eqos_null_ops,
};

struct eqos_config __maybe_unused eqos_aw_config = {
	.reg_access_always_ok = false,
	.mdio_wait = 10000,
	.swr_wait = 5000,
	.config_mac = EQOS_MAC_RXQ_CTRL0_RXQ0EN_ENABLED_DCB,
	.config_mac_mdio = EQOS_MAC_MDIO_ADDRESS_CR_150_250,
	.axi_bus_width = EQOS_AXI_WIDTH_64,
	.interface = dev_read_phy_mode,
	.ops = &eqos_aw_ops
};
