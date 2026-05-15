// SPDX-License-Identifier: GPL-2.0+
/*
 * Allwinner USB HOST EHCI Controller
 *
 * Copyright (C) 2024 Allwinner Technology Co.Ltd. All rights reserved.
 *
 * SoftWinner EHCI Driver
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <common.h>
#include <dm.h>
#include <linux/delay.h>
#include <usb.h>
#include <clk.h>
#include <reset.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include "ehci.h"


DECLARE_GLOBAL_DATA_PTR;

struct sunxi_ehci_plat {
	struct usb_plat usb_plat;
	fdt_addr_t device_base;
	fdt_addr_t hcd_base;
	struct gpio_desc vbus_gpio;
	struct clk clk_bus_hci;
	struct reset_ctl reset_hci;
	struct reset_ctl reset_phy;
};

struct sunxi_ehci {
	struct ehci_ctrl ctrl;
	struct ehci_hccr *hcd;
};

struct sunxi_ehci_ops {
	bool otg_mode;
	bool host_mode;
	bool device_reg;
	bool host_reg;
};

#define USB_CTRL		0x800
#define  ULPI_BYPASS		BIT(0) /* 1: enable UTMI, disable ULPI;
					* 0: enable ULPI, disable UTML. */
#define  AHB_INCRX_ENABLE	BIT(8)
#define  AHB_INCR4_ENABLE	BIT(9)
#define  AHB_INCR8_ENABLE	BIT(10)
#define USB_CTRL3		0x808
#define PHY_CTRL		0x810
#define  SIDDQ			BIT(3) /* 1: disable phy; 0: enable phy */

#define USB_PHY_SEL		0x420
#define  OTG_SEL		BIT(0) /* 1: phy is OTG;  0: phy is HCI */

static int sunxi_ehci_usb_of_to_plat(struct udevice *dev)
{
	struct sunxi_ehci_plat *plat = dev_get_plat(dev);
	struct sunxi_ehci_ops *ops = (struct sunxi_ehci_ops *)dev_get_driver_data(dev);
	int ret;

	if (ops && ops->host_mode) {
		plat->hcd_base = dev_read_addr(dev);
		if (plat->hcd_base == FDT_ADDR_T_NONE) {
			debug("Can't get the XHCI register base address\n");
			return -ENXIO;
		}
	}

	if (ops && ops->otg_mode) {
		plat->device_base = dev_read_addr_index(dev, 0);
		if (plat->device_base == FDT_ADDR_T_NONE) {
			debug("Can't get the device register base address\n");
			return -ENXIO;
		}

		plat->hcd_base = dev_read_addr_index(dev, 1);
		if (plat->hcd_base == FDT_ADDR_T_NONE) {
			debug("Can't get the EHCI register base address\n");
			return -ENXIO;
		}
	}

	ret = clk_get_by_name(dev, "bus_hci", &plat->clk_bus_hci);
	if (ret) {
		printf("get clk bus_hci failed %d.\n", ret);
		return -ENXIO;
	}

	ret = reset_get_by_name(dev, "hci", &plat->reset_hci);
	if (ret) {
		printf("get reset hci failed %d.\n", ret);
		return -ENXIO;
	}

	ret = reset_get_by_name(dev, "phy", &plat->reset_phy);
	if (ret) {
		printf("get reset phy failed %d.\n", ret);
		return -ENXIO;
	}

	if (!dm_gpio_is_valid(&plat->vbus_gpio)) {
		ret = gpio_request_by_name(dev, "vbus-gpio", 0,
		&plat->vbus_gpio, GPIOD_IS_OUT);
		if (ret)
			printf("ehci get vbus-gpio failed %d, use default gpio.\n", ret);
	}

	return 0;
}

static int sunxi_enable_usb(struct udevice *dev)
{
	struct sunxi_ehci_ops *ops = (struct sunxi_ehci_ops *)dev_get_driver_data(dev);
	struct sunxi_ehci_plat *plat = dev_get_plat(dev);
	u32 value;
	int ret;

	ret = clk_enable(&plat->clk_bus_hci);
	if (ret) {
		printf("clk enable failed %d\n", ret);
		return ret;
	}

	ret = reset_deassert(&plat->reset_hci);
	if (ret) {
		printf("reset deassert failed %d\n", ret);
		return ret;
	}

	ret = reset_deassert(&plat->reset_phy);
	if (ret) {
		printf("reset deassert failed %d\n", ret);
		return ret;
	}

	/* It’s not necessary, but it’s artificial to ensure stability. */
	udelay(10);



	if (ops && ops->otg_mode) {
		value = readl(plat->device_base + USB_PHY_SEL);
		value &= ~OTG_SEL;
		writel(value, plat->device_base + USB_PHY_SEL);
	}

	value = ULPI_BYPASS | AHB_INCRX_ENABLE | AHB_INCR4_ENABLE | AHB_INCR8_ENABLE;
	writel(value, plat->hcd_base + USB_CTRL);

	/* enable the PHY. */
	value = readl(plat->hcd_base + PHY_CTRL);
	value &= ~SIDDQ;
	writel(value, plat->hcd_base + PHY_CTRL);

	/* It’s not necessary, but it’s artificial to ensure stability. */
	udelay(10);

	return 0;
}

static int sunxi_disable_usb(struct sunxi_ehci_plat *plat)
{
	int ret;

	ret = clk_disable(&plat->clk_bus_hci);
	if (ret) {
		printf("clk disable failed %d\n", ret);
		return ret;
	}

	ret = reset_assert(&plat->reset_phy);
	if (ret) {
		printf("reset assert failed %d\n", ret);
		return ret;
	}

	ret = reset_assert(&plat->reset_hci);
	if (ret) {
		printf("reset assert failed %d\n", ret);
		return ret;
	}

	return 0;
}

static int sunxi_ehci_usb_probe(struct udevice *dev)
{
	struct sunxi_ehci_plat *plat = dev_get_plat(dev);
	struct sunxi_ehci *ctx = dev_get_priv(dev);
	struct ehci_hcor *hcor;
	int ret;

	ret = sunxi_enable_usb(dev);
	if (ret) {
		printf("enable usb failed %d\n", ret);
		return ret;
	}

	ctx->hcd = (struct ehci_hccr *)plat->hcd_base;

	if (dm_gpio_is_valid(&plat->vbus_gpio)) {
		dm_gpio_set_value(&plat->vbus_gpio, 1);
	}

	mdelay(1);

	hcor = (struct ehci_hcor *)((ulong)ctx->hcd +
			HC_LENGTH(ehci_readl(&ctx->hcd->cr_capbase)));

	return ehci_register(dev, ctx->hcd, hcor, NULL, 0, USB_INIT_HOST);
}

static int sunxi_ehci_usb_remove(struct udevice *dev)
{
	struct sunxi_ehci_plat *plat = dev_get_plat(dev);
	int ret;

	ret = ehci_deregister(dev);
	if (ret)
		return ret;

	sunxi_disable_usb(plat);

	return 0;
}

struct sunxi_ehci_ops otg_data = {
	.otg_mode	= true,
	.device_reg	= true,
	.host_reg	= true,
};

struct sunxi_ehci_ops ehci_data = {
	.host_mode	= true,
	.host_reg	= true,
};

static const struct udevice_id sunxi_ehci_usb_ids[] = {
	{ .compatible = "allwinner,sunxi-ehci0",
	  .data = (ulong)&otg_data },
	{ .compatible = "allwinner,sunxi-ehci1",
	  .data = (ulong)&ehci_data },
	{ .compatible = "allwinner,sunxi-ehci2",
	  .data = (ulong)&ehci_data },
	{ .compatible = "allwinner,sunxi-ehci3",
	  .data = (ulong)&ehci_data },
	{ }
};

U_BOOT_DRIVER(usb_ehci) = {
	.name		= "ehci_sunxi",
	.id		= UCLASS_USB,
	.of_match	= sunxi_ehci_usb_ids,
	.of_to_plat	= sunxi_ehci_usb_of_to_plat,
	.probe		= sunxi_ehci_usb_probe,
	.remove		= sunxi_ehci_usb_remove,
	.ops		= &ehci_usb_ops,
	.priv_auto	= sizeof(struct sunxi_ehci),
	.plat_auto	= sizeof(struct sunxi_ehci_plat),
	.flags		= DM_FLAG_ALLOC_PRIV_DMA,
};
