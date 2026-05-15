// SPDX-License-Identifier: GPL-2.0+
/*
 * Derived from linux/drivers/watchdog/sunxi_wdt.c:
 *	Copyright (C) 2013 Carlo Caione
 *	Copyright (C) 2012 Henrik Nordstrom
 */

#include <dm.h>
#include <stdio.h>
#include <wdt.h>
#include <asm/io.h>
#include <linux/delay.h>

#define MSEC_PER_SEC		1000

#define WDT_MAX_TIMEOUT		16
#define WDT_TIMEOUT_MASK	0xf

#define WDT_CTRL_RELOAD		((1 << 0) | (0x0a57 << 1))

#define WDT_MODE_EN		BIT(0)

struct sunxi_wdt_reg {
	u8 wdt_ctrl;
	u8 wdt_cfg;
	u8 wdt_mode;
	u8 wdt_ocfg;
	u8 wdt_timeout_shift;
	u8 wdt_reset_mask;
	u8 wdt_reset_val;
	u32 wdt_out_mask;
	u32 wdt_out_val;
	u32 wdt_key_val;
	u32 wdt_keep_reg;
};

struct sunxi_wdt_priv {
	void __iomem			*base;
	struct sunxi_wdt_reg	*regs;
};

/* Map of timeout in seconds to register value */
static const u8 wdt_timeout_map[1 + WDT_MAX_TIMEOUT] = {
	[0]	= 0x0,
	[1]	= 0x1,
	[2]	= 0x2,
	[3]	= 0x3,
	[4]	= 0x4,
	[5]	= 0x5,
	[6]	= 0x6,
	[7]	= 0x7,
	[8]	= 0x7,
	[9]	= 0x8,
	[10]	= 0x8,
	[11]	= 0x9,
	[12]	= 0x9,
	[13]	= 0xa,
	[14]	= 0xa,
	[15]	= 0xb,
	[16]	= 0xb,
};

static int sunxi_wdt_reset(struct udevice *dev)
{
	struct sunxi_wdt_priv *priv = dev_get_priv(dev);
	const struct sunxi_wdt_reg *regs = priv->regs;
	void __iomem *base = priv->base;

	writel(WDT_CTRL_RELOAD, base + regs->wdt_ctrl);

	return 0;
}

static int sunxi_wdt_start(struct udevice *dev, u64 timeout, ulong flags)
{
	struct sunxi_wdt_priv *priv = dev_get_priv(dev);
	const struct sunxi_wdt_reg *regs = priv->regs;
	void __iomem *base = priv->base;
	u32 val;

	timeout /= MSEC_PER_SEC;
	if (timeout > WDT_MAX_TIMEOUT)
		timeout = WDT_MAX_TIMEOUT;

	/* Set out config */
	if (regs->wdt_out_mask) {
		val = readl(base + regs->wdt_ocfg);
		val &= ~regs->wdt_out_mask;
		val |= regs->wdt_out_val;
		val |= regs->wdt_key_val;
		writel(val, base + regs->wdt_ocfg);
	}

	/* Set system reset function */
	val = readl(base + regs->wdt_cfg);
	val &= ~regs->wdt_reset_mask;
	val |= regs->wdt_reset_val;
	val |= regs->wdt_key_val;
	writel(val, base + regs->wdt_cfg);

	/* Set timeout and enable watchdog */
	val = readl(base + regs->wdt_mode);
	val &= ~(WDT_TIMEOUT_MASK << regs->wdt_timeout_shift);
	val |= wdt_timeout_map[timeout] << regs->wdt_timeout_shift;
	val |= WDT_MODE_EN;
	val |= regs->wdt_key_val;
	writel(val, base + regs->wdt_mode);

	return sunxi_wdt_reset(dev);
}

static int sunxi_wdt_stop(struct udevice *dev)
{
	struct sunxi_wdt_priv *priv = dev_get_priv(dev);
	const struct sunxi_wdt_reg *regs = priv->regs;
	void __iomem *base = priv->base;

	writel(regs->wdt_key_val, base + regs->wdt_mode);

	return 0;
}

static int sunxi_wdt_expire_now(struct udevice *dev, ulong flags)
{
	int ret;

	ret = sunxi_wdt_start(dev, 0, flags);
	if (ret)
		return ret;

	mdelay(500);

	return 0;
}

static const struct wdt_ops sunxi_wdt_ops = {
	.reset		= sunxi_wdt_reset,
	.start		= sunxi_wdt_start,
	.stop		= sunxi_wdt_stop,
	.expire_now	= sunxi_wdt_expire_now,
};

#if !CONFIG_IS_ENABLED(AW_BSP)
static const struct sunxi_wdt_reg sun4i_wdt_reg = {
	.wdt_ctrl		= 0x00,
	.wdt_cfg		= 0x04,
	.wdt_mode		= 0x04,
	.wdt_timeout_shift	= 3,
	.wdt_reset_mask		= 0x2,
	.wdt_reset_val		= 0x2,
};

static const struct sunxi_wdt_reg sun6i_wdt_reg = {
	.wdt_ctrl		= 0x10,
	.wdt_cfg		= 0x14,
	.wdt_mode		= 0x18,
	.wdt_timeout_shift	= 4,
	.wdt_reset_mask		= 0x3,
	.wdt_reset_val		= 0x1,
};

static const struct sunxi_wdt_reg sun20i_wdt_reg = {
	.wdt_ctrl		= 0x10,
	.wdt_cfg		= 0x14,
	.wdt_mode		= 0x18,
	.wdt_timeout_shift	= 4,
	.wdt_reset_mask		= 0x03,
	.wdt_reset_val		= 0x01,
	.wdt_key_val		= 0x16aa0000,
};
#endif
static const struct sunxi_wdt_reg sunxi_common_wdt_reg = {
	.wdt_ctrl		= 0x10, /*ctrl reg offset*/
	.wdt_cfg		= 0x14, /*cfg reg offset*/
	.wdt_mode		= 0x18, /*mode reg offset*/
	.wdt_timeout_shift	= 0x4,
	.wdt_reset_mask		= 0x03,
	.wdt_reset_val		= 0x01,
	.wdt_key_val		= 0x16aa0000, /* defult value of disable wdt*/
};

static const struct udevice_id sunxi_wdt_ids[] = {
#if !CONFIG_IS_ENABLED(AW_BSP)
	{ .compatible = "allwinner,sun4i-a10-wdt", .data = (ulong)&sun4i_wdt_reg },
	{ .compatible = "allwinner,sun6i-a31-wdt", .data = (ulong)&sun6i_wdt_reg },
	{ .compatible = "allwinner,sun20i-d1-wdt", .data = (ulong)&sun20i_wdt_reg },
#endif
	{ .compatible = "allwinner,sunxi-wdt", .data = (ulong)&sunxi_common_wdt_reg },
	{ /* sentinel */ }
};

static int update_wdt_reg_from_fdt(struct udevice *dev,
				   struct sunxi_wdt_reg *regs)
{
	regs->wdt_ctrl = dev_read_u32_default(dev, "wdt_ctrl", -ENODATA);
	regs->wdt_cfg = dev_read_u32_default(dev, "wdt_cfg", -ENODATA);
	regs->wdt_mode = dev_read_u32_default(dev, "wdt_mode", -ENODATA);
	regs->wdt_ocfg = dev_read_u32_default(dev, "wdt_ocfg", -ENODATA);
	regs->wdt_timeout_shift =
		dev_read_u32_default(dev, "wdt_timeout_shift", -ENODATA);
	regs->wdt_reset_mask =
		dev_read_u32_default(dev, "wdt_reset_mask", -ENODATA);
	regs->wdt_reset_val =
		dev_read_u32_default(dev, "wdt_reset_val", -ENODATA);
	regs->wdt_out_mask =
		dev_read_u32_default(dev, "wdt_out_mask", -ENODATA);
	regs->wdt_out_val =
		dev_read_u32_default(dev, "wdt_out_val", -ENODATA);
	regs->wdt_key_val = dev_read_u32_default(dev, "wdt_key_val", -ENODATA);
	regs->wdt_keep_reg = dev_read_u32_default(dev, "wdt_keep_reg", -ENODATA);

	if ((regs->wdt_ctrl == -ENODATA) || (regs->wdt_cfg == -ENODATA) ||
	    (regs->wdt_mode == -ENODATA) ||
	    (regs->wdt_timeout_shift == -ENODATA) ||
	    (regs->wdt_reset_mask == -ENODATA) ||
	    (regs->wdt_reset_val == -ENODATA) ||
	    (regs->wdt_key_val == -ENODATA))
		return -ENODATA;

	if ((regs->wdt_ocfg == -ENODATA) || (regs->wdt_out_mask == -ENODATA) ||
	    (regs->wdt_out_val == -ENODATA)) {
		regs->wdt_ocfg = 0x0;
		regs->wdt_out_mask = 0x0;
		regs->wdt_out_val = 0x0;
	}

	if (regs->wdt_keep_reg == -ENODATA)
		regs->wdt_keep_reg = 0;

	debug("regs->wdt_ctrl = 0x%x\nregs->wdt_cfg = 0x%x\n"
	       "regs->wdt_mode = 0x%x\nregs->wdt_timeout_shift = 0x%x\n"
	       "regs->wdt_ocfg = 0x%x\n"
	       "regs->wdt_out_mask = 0x%x\nregs->wdt_out_val = 0x%x\n"
	       "regs->wdt_reset_mask = 0x%x\nregs->wdt_reset_val = 0x%x\n"
	       "regs->wdt_key_val = 0x%x \n"
		   "regs->wdt_keep_reg = 0x%x \n",
	       regs->wdt_ctrl, regs->wdt_cfg, regs->wdt_mode,
	       regs->wdt_ocfg,
	       regs->wdt_out_mask, regs->wdt_out_val,
	       regs->wdt_timeout_shift, regs->wdt_reset_mask,
	       regs->wdt_reset_val, regs->wdt_key_val,
		   regs->wdt_keep_reg);
	return 0;
}

static int sunxi_wdt_probe(struct udevice *dev)
{
	struct sunxi_wdt_priv *priv = dev_get_priv(dev);

	priv->base = dev_remap_addr(dev);
	if (!priv->base)
		return -EINVAL;

	priv->regs = (void *)dev_get_driver_data(dev);
	if (!priv->regs)
		return -EINVAL;

	if (update_wdt_reg_from_fdt(dev, priv->regs) < 0) {
		pr_err("%s:probe wdt failed", __func__);
		return -ENODATA;
	}

	if (priv->regs->wdt_keep_reg == 0)
		sunxi_wdt_stop(dev);
	else
		pr_notice("sunxi wdt keep config\n");

	return 0;
}

U_BOOT_DRIVER(sunxi_wdt) = {
	.name		= "sunxi_wdt",
	.id		= UCLASS_WDT,
	.of_match	= sunxi_wdt_ids,
	.probe		= sunxi_wdt_probe,
	.priv_auto	= sizeof(struct sunxi_wdt_priv),
	.ops		= &sunxi_wdt_ops,
};
