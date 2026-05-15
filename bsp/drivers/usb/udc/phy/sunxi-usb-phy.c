// SPDX-License-Identifier: GPL-2.0

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <log.h>
#include <dm/device.h>
#include <generic-phy.h>
#include <reset.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <dm/device_compat.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <power/regulator.h>
#include <sunxi-usb-phy.h>

#define REG_ISCR			0x00
#define REG_PHYBIST			0x08
#define REG_PHYTUNE			0x0c
#define REG_PHYCTL			0x10
#define REG_PHY_OTGCTL			0x20

#define REG_HCI_PHY_CTL			0x10

#define PHY_PLL_BW			0x03
#define PHY_RES45_CAL_EN		0x0c

#define PHY_TX_AMPLITUDE_TUNE		0x20
#define PHY_TX_SLEWRATE_TUNE		0x22
#define PHY_DISCON_TH_SEL		0x2a
#define PHY_SQUELCH_DETECT		0x3c

#define PHYCTL_DATA			BIT(7)
#define OTGCTL_ROUTE_USB		BIT(0)

#define PHY_TX_RATE			BIT(4)
#define PHY_TX_MAGNITUDE		BIT(2)
#define PHY_TX_AMPLITUDE_LEN		5

#define PHY_RES45_CAL_DATA		BIT(0)
#define PHY_RES45_CAL_LEN		1
#define PHY_DISCON_TH_LEN		2

#define SUNXI_AHB_ICHR8_EN		BIT(10)
#define SUNXI_AHB_INCR4_BURST_EN	BIT(9)
#define SUNXI_AHB_INCRX_ALIGN_EN	BIT(8)
#define SUNXI_ULPI_BYPASS_EN		BIT(0)

#define PHY_CTL_VBUSVLDEXT		BIT(5)
#define PHY_CTL_SIDDQ			BIT(3)
#define PHY_CTL_H3_SIDDQ		BIT(1)

#define SUNXI_EHCI_HS_FORCE		BIT(20)
#define SUNXI_HSIC_CONNECT_INT		BIT(16)
#define SUNXI_HSIC			BIT(1)

#define MAX_PHYS			4

enum sunxi_usb_phy_type {
	sunxi_phy,
};

struct sunxi_usb_phy_cfg {
	int num_phys;
	enum sunxi_usb_phy_type type;
	u32 disc_thresh;
	u32 hci_phy_ctl_clear;
	u8 phyctl_offset;
	bool dedicated_clocks;
	bool phy0_dual_route;
	int missing_phys;
};

struct sunxi_usb_phy_info {
	const char *gpio_vbus;
	const char *gpio_vbus_det;
	const char *gpio_id_det;
} phy_info[] = {
	{
		.gpio_vbus = CONFIG_USB0_VBUS_PIN,
		.gpio_vbus_det = CONFIG_USB0_VBUS_DET,
		.gpio_id_det = CONFIG_USB0_ID_DET,
	},
	{
		.gpio_vbus = CONFIG_USB1_VBUS_PIN,
		.gpio_vbus_det = NULL,
		.gpio_id_det = NULL,
	},
	{
		.gpio_vbus = CONFIG_USB2_VBUS_PIN,
		.gpio_vbus_det = NULL,
		.gpio_id_det = NULL,
	},
	{
		.gpio_vbus = CONFIG_USB3_VBUS_PIN,
		.gpio_vbus_det = NULL,
		.gpio_id_det = NULL,
	},
};

struct sunxi_usb_phy_plat {
	void __iomem *pmu;
	struct gpio_desc gpio_vbus;
	struct gpio_desc gpio_vbus_det;
	struct gpio_desc gpio_id_det;
	struct clk clocks;
	struct reset_ctl resets;
	int id;
};

struct sunxi_usb_phy_data {
	void __iomem *base;
	const struct sunxi_usb_phy_cfg *cfg;
	struct sunxi_usb_phy_plat *usb_phy;
	struct udevice *vbus_power_supply;
};

static int initial_usb_scan_delay = CONFIG_INITIAL_USB_SCAN_DELAY;

static void sunxi_usb_phy_write(struct phy *phy, u32 addr, u32 data, int len)
{
	struct sunxi_usb_phy_data *phy_data = dev_get_priv(phy->dev);
	struct sunxi_usb_phy_plat *usb_phy = &phy_data->usb_phy[phy->id];
	u32 temp, usbc_bit = BIT(usb_phy->id * 2);
	void __iomem *phyctl = phy_data->base + phy_data->cfg->phyctl_offset;
	int i;

	if (phy_data->cfg->phyctl_offset == REG_PHYCTL) {
		/* need us to set phyctl to 0 explicitly */
		writel(0, phyctl);
	}

	for (i = 0; i < len; i++) {
		temp = readl(phyctl);

		/* clear the address portion */
		temp &= ~(0xff << 8);

		/* set the address */
		temp |= ((addr + i) << 8);
		writel(temp, phyctl);

		/* set the data bit and clear usbc bit*/
		temp = readb(phyctl);
		if (data & 0x1)
			temp |= PHYCTL_DATA;
		else
			temp &= ~PHYCTL_DATA;
		temp &= ~usbc_bit;
		writeb(temp, phyctl);

		/* pulse usbc_bit */
		temp = readb(phyctl);
		temp |= usbc_bit;
		writeb(temp, phyctl);

		temp = readb(phyctl);
		temp &= ~usbc_bit;
		writeb(temp, phyctl);

		data >>= 1;
	}
}

static void sunxi_usb_phy_passby(struct phy *phy, bool enable)
{
	struct sunxi_usb_phy_data *data = dev_get_priv(phy->dev);
	struct sunxi_usb_phy_plat *usb_phy = &data->usb_phy[phy->id];
	u32 bits, reg_value;

	if (!usb_phy->pmu)
		return;

	bits = SUNXI_AHB_ICHR8_EN | SUNXI_AHB_INCR4_BURST_EN |
		SUNXI_AHB_INCRX_ALIGN_EN | SUNXI_ULPI_BYPASS_EN;

	reg_value = readl(usb_phy->pmu);

	if (enable)
		reg_value |= bits;
	else
		reg_value &= ~bits;

	writel(reg_value, usb_phy->pmu);
}

static int sunxi_usb_phy_power_on(struct phy *phy)
{
	struct sunxi_usb_phy_data *data = dev_get_priv(phy->dev);
	struct sunxi_usb_phy_plat *usb_phy = &data->usb_phy[phy->id];

	if (initial_usb_scan_delay) {
		mdelay(initial_usb_scan_delay);
		initial_usb_scan_delay = 0;
	}

	if (dm_gpio_is_valid(&usb_phy->gpio_vbus))
		dm_gpio_set_value(&usb_phy->gpio_vbus, 1);

	return 0;
}

static int sunxi_usb_phy_power_off(struct phy *phy)
{
	struct sunxi_usb_phy_data *data = dev_get_priv(phy->dev);
	struct sunxi_usb_phy_plat *usb_phy = &data->usb_phy[phy->id];

	if (dm_gpio_is_valid(&usb_phy->gpio_vbus))
		dm_gpio_set_value(&usb_phy->gpio_vbus, 0);

	return 0;
}

static void sunxi_usb_phy0_reroute(struct sunxi_usb_phy_data *data, bool id_det)
{
	u32 regval;

	regval = readl(data->base + REG_PHY_OTGCTL);
	if (!id_det) {
		regval &= ~OTGCTL_ROUTE_USB;
	} else {
		regval |= OTGCTL_ROUTE_USB;
	}
	writel(regval, data->base + REG_PHY_OTGCTL);
}

static int sunxi_usb_phy_init(struct phy *phy)
{
	struct sunxi_usb_phy_data *data = dev_get_priv(phy->dev);
	struct sunxi_usb_phy_plat *usb_phy = &data->usb_phy[phy->id];
	u32 val;
	int ret;

	ret = clk_enable(&usb_phy->clocks);
	if (ret) {
		dev_dbg(phy->dev, "failed to enable usb_%ldphy clock\n",
			phy->id);
		//return ret;
	}

	ret = reset_deassert(&usb_phy->resets);
	if (ret) {
		dev_err(phy->dev, "failed to deassert usb_%ldreset reset\n",
			phy->id);
		return ret;
	}

	if (usb_phy->pmu && data->cfg->hci_phy_ctl_clear) {
		val = readl(usb_phy->pmu + REG_HCI_PHY_CTL);
		val &= ~data->cfg->hci_phy_ctl_clear;
		writel(val, usb_phy->pmu + REG_HCI_PHY_CTL);
	}

	val = readl(usb_phy->pmu + REG_HCI_PHY_CTL);
	val |= BIT(5);
	writel(val, usb_phy->pmu + REG_HCI_PHY_CTL);

#ifdef CONFIG_AW_UDC
	if (usb_phy->id != 0)
		sunxi_usb_phy_passby(phy, true);

	if (data->cfg->phy0_dual_route)
		sunxi_usb_phy0_reroute(data, true);
#else
	sunxi_usb_phy_passby(phy, true);

	if (data->cfg->phy0_dual_route)
		sunxi_usb_phy0_reroute(data, false);
#endif

	return 0;
}

static int sunxi_usb_phy_exit(struct phy *phy)
{
	struct sunxi_usb_phy_data *data = dev_get_priv(phy->dev);
	struct sunxi_usb_phy_plat *usb_phy = &data->usb_phy[phy->id];
	int ret;

	sunxi_usb_phy_passby(phy, false);


	ret = clk_disable(&usb_phy->clocks);
	if (ret) {
		dev_dbg(phy->dev, "failed to disable usb_%ldphy clock\n",
			phy->id);
		//return ret;
	}


	ret = reset_assert(&usb_phy->resets);
	if (ret) {
		dev_err(phy->dev, "failed to assert usb_%ldreset reset\n",
			phy->id);
		return ret;
	}

	return 0;
}

static int sunxi_usb_phy_xlate(struct phy *phy,
			       struct ofnode_phandle_args *args)
{
	struct sunxi_usb_phy_data *data = dev_get_priv(phy->dev);

	if (args->args_count >= data->cfg->num_phys)
		return -EINVAL;

	if (data->cfg->missing_phys & BIT(args->args[0]))
		return -ENODEV;

	if (args->args_count)
		phy->id = args->args[0];
	else
		phy->id = 0;

	debug("%s: phy_id = %ld\n", __func__, phy->id);
	return 0;
}

int sunxi_usb_phy_vbus_detect(struct phy *phy)
{
	struct sunxi_usb_phy_data *data = dev_get_priv(phy->dev);
	struct sunxi_usb_phy_plat *usb_phy = &data->usb_phy[phy->id];
	int err = 1, retries = 3;

	if (dm_gpio_is_valid(&usb_phy->gpio_vbus_det)) {
		err = dm_gpio_get_value(&usb_phy->gpio_vbus_det);
		while (err > 0 && retries--) {
			mdelay(100);
			err = dm_gpio_get_value(&usb_phy->gpio_vbus_det);
		}
	} else if (data->vbus_power_supply) {
		err = regulator_get_enable(data->vbus_power_supply);
	}

	return err;
}

int sunxi_usb_phy_id_detect(struct phy *phy)
{
	struct sunxi_usb_phy_data *data = dev_get_priv(phy->dev);
	struct sunxi_usb_phy_plat *usb_phy = &data->usb_phy[phy->id];

	if (!dm_gpio_is_valid(&usb_phy->gpio_id_det))
		return -1;

	return dm_gpio_get_value(&usb_phy->gpio_id_det);
}

void sunxi_usb_phy_set_squelch_detect(struct phy *phy, bool enabled)
{
	sunxi_usb_phy_write(phy, PHY_SQUELCH_DETECT, enabled ? 0 : 2, 2);
}

static struct phy_ops sunxi_usb_phy_ops = {
	.of_xlate = sunxi_usb_phy_xlate,
	.init = sunxi_usb_phy_init,
	.power_on = sunxi_usb_phy_power_on,
	.power_off = sunxi_usb_phy_power_off,
	.exit = sunxi_usb_phy_exit,
};

static int sunxi_usb_phy_probe(struct udevice *dev)
{
	struct sunxi_usb_phy_plat *plat = dev_get_plat(dev);
	struct sunxi_usb_phy_data *data = dev_get_priv(dev);
	int i, ret;

	data->cfg = (const struct sunxi_usb_phy_cfg *)dev_get_driver_data(dev);
	if (!data->cfg)
		return -EINVAL;

	data->base = (void __iomem *)devfdt_get_addr_name(dev, "phy_ctrl");
	if (IS_ERR(data->base))
		return PTR_ERR(data->base);

	device_get_supply_regulator(dev, "usb0_vbus_power-supply",
				    &data->vbus_power_supply);

	data->usb_phy = plat;
	for (i = 0; i < data->cfg->num_phys; i++) {
		struct sunxi_usb_phy_plat *phy = &plat[i];
		struct sunxi_usb_phy_info *info = &phy_info[i];
		char name[16];

		if (data->cfg->missing_phys & BIT(i))
			continue;

		ret = dm_gpio_lookup_name(info->gpio_vbus, &phy->gpio_vbus);
		if (ret == 0) {
			ret = dm_gpio_request(&phy->gpio_vbus, "usb_vbus");
			if (ret)
				return ret;
			ret = dm_gpio_set_dir_flags(&phy->gpio_vbus,
						    GPIOD_IS_OUT);
			if (ret)
				return ret;
			ret = dm_gpio_set_value(&phy->gpio_vbus, 0);
			if (ret)
				return ret;
		}

		ret = dm_gpio_lookup_name(info->gpio_vbus_det,
					  &phy->gpio_vbus_det);
		if (ret == 0) {
			ret = dm_gpio_request(&phy->gpio_vbus_det,
					      "usb_vbus_det");
			if (ret)
				return ret;
			ret = dm_gpio_set_dir_flags(&phy->gpio_vbus_det,
						    GPIOD_IS_IN);
			if (ret)
				return ret;
		}

		ret = dm_gpio_lookup_name(info->gpio_id_det, &phy->gpio_id_det);
		if (ret == 0) {
			ret = dm_gpio_request(&phy->gpio_id_det, "usb_id_det");
			if (ret)
				return ret;
			ret = dm_gpio_set_dir_flags(&phy->gpio_id_det,
						GPIOD_IS_IN | GPIOD_PULL_UP);
			if (ret)
				return ret;
		}

		if (data->cfg->dedicated_clocks)
			snprintf(name, sizeof(name), "usb%d_phy", i);
		else
			strlcpy(name, "usb_phy", sizeof(name));

		ret = clk_get_by_name(dev, name, &phy->clocks);
		if (ret) {
			dev_dbg(dev, "failed to get usb%d_phy clock phandle\n", i);
			//return ret;
		}

		snprintf(name, sizeof(name), "usb%d_reset", i);
		ret = reset_get_by_name(dev, name, &phy->resets);
		if (ret) {
			dev_err(dev, "failed to get usb%d_reset reset phandle\n", i);
			return ret;
		}

		if (i || data->cfg->phy0_dual_route) {
			snprintf(name, sizeof(name), "pmu%d", i);
			phy->pmu = (void __iomem *)devfdt_get_addr_name(dev, name);
			if (IS_ERR(phy->pmu)) {
				return PTR_ERR(phy->pmu);
			}
		}

		phy->id = i;
	};

	debug("Allwinner Sunxi USB PHY driver loaded\n");
	return 0;
}
static const struct sunxi_usb_phy_cfg sunxi_usb_phy = {
	.num_phys = 1,
	.type = sunxi_phy,
	.disc_thresh = 1,
	.phyctl_offset = REG_PHYCTL,
	.dedicated_clocks = true,
	.hci_phy_ctl_clear = PHY_CTL_SIDDQ,
	.phy0_dual_route = true,
};

static const struct udevice_id sunxi_usb_phy_ids[] = {
	{ .compatible = "allwinner,sunxi-usb-phy", .data = (ulong)&sunxi_usb_phy },
	{ }
};

U_BOOT_DRIVER(sunxi_usb_phy) = {
	.name	= "sunxi_usb_phy",
	.id	= UCLASS_PHY,
	.of_match = sunxi_usb_phy_ids,
	.ops = &sunxi_usb_phy_ops,
	.probe = sunxi_usb_phy_probe,
	.plat_auto	= sizeof(struct sunxi_usb_phy_plat[MAX_PHYS]),
	.priv_auto	= sizeof(struct sunxi_usb_phy_data),
};
