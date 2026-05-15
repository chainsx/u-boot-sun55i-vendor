// SPDX-License-Identifier: GPL-2.0
#include <common.h>
#include <errno.h>
#include <dm.h>
#include <adc.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <clk.h>
#include <reset.h>

/* gpadc has data */
#define GPADC_DAT_MASK         0xfff
#define GPADC_CONV_TIMEOUT_US  15
#define GPADC_MAX_CHANNEL      9

#define SUNXI_GPADC_CLK0 (0x02002fc0)
#define SUNXI_GPADC_GAT0 (0x02002fc4)
#define SUNXI_GPADC_CLK1 (0x02002fc8)
#define SUNXI_GPADC_GAT1 (0x02002fcc)
#define SUNXI_GPADC_CLK2 (0x02002fd0)
#define SUNXI_GPADC_GAT2 (0x02002fd4)
#define SUNXI_GPADC_CLK3 (0x02002fd8)
#define SUNXI_GPADC_GAT3 (0x02002fdc)

#define GP_CTRL        (0x04)
#define GP_CS_EN       (0x08)
#define GP_DATA_INTC   (0x28)
#define GP_DATA_INTS   (0x38)
#define GP_CH0_DATA    (0x80)

struct sunxi_gpadc_priv {
	void *base_addr;
	u32 active_channel;
	int channel_num;
	struct reset_ctl reset;
	struct clk clk;
};

int sunxi_gpadc_channel_data(struct udevice *dev, int channel,
			    unsigned int *data)
{
	struct sunxi_gpadc_priv *priv = dev_get_priv(dev);
	void *gp_data_ints = priv->base_addr + GP_DATA_INTS;
	u32 ints, i, snum = 0;

	if (channel != priv->active_channel) {
		pr_err("Requested channel is not active!");
		return -EINVAL;
	}

	ints = readl(gp_data_ints);
	/* clear the pending data */
	writel(readl(gp_data_ints)|(ints & (1 << channel)), gp_data_ints);
	/* if there is already data pending, read it */
	if (ints & (1 << channel)) {
		for (i = 0; i < 5; i++) {
			snum += readl(priv->base_addr + GP_CH0_DATA + (channel * 4));
			udelay(5);
		}
		snum = snum / i;
	}

	*data = snum & GPADC_DAT_MASK;

	return 0;
}

int sunxi_gpadc_start_channel(struct udevice *dev, int channel)
{
	struct sunxi_gpadc_priv *priv = dev_get_priv(dev);
	uint reg_val = 0;

	/*choose channel*/
	reg_val = readl(priv->base_addr + GP_CS_EN);
	reg_val |= (1 << channel);
	writel(reg_val, priv->base_addr + GP_CS_EN);

	/*choose continue work mode and enable ADC*/
	reg_val = readl(priv->base_addr + GP_CTRL);
	reg_val &= ~(1 << 18);
	reg_val |= ((1 << 19) | (1 << 16));
	writel(reg_val, priv->base_addr + GP_CTRL);

	/* disable all key irq */
	writel(0, priv->base_addr + GP_DATA_INTC);
	writel(1, priv->base_addr + GP_DATA_INTS);

	udelay(500);

	priv->active_channel = channel;

	return 0;
}

int sunxi_gpadc_stop(struct udevice *dev)
{
	struct sunxi_gpadc_priv *priv = dev_get_priv(dev);
	uint reg_val = 0;

	/* disable ADC */
	reg_val = readl(priv->base_addr + GP_CTRL);
	reg_val &= ~(1 << 16);
	writel(reg_val, priv->base_addr + GP_CTRL);

	priv->active_channel = -1;

	return 0;
}

int sunxi_gpadc_probe(struct udevice *dev)
{
	struct sunxi_gpadc_priv *priv = dev_get_priv(dev);
#if 0
	int ret;

	ret = reset_get_by_index(dev, 0, &priv->reset);
	if (ret)
		return ret;

	reset_assert(&priv->reset);
	udelay(2);
	reset_deassert(&priv->reset);

	ret = clk_get_by_index(dev, 0, &priv->clk);
	if (ret)
		return ret;

	ret = clk_enable(&priv->clk);
	if (ret)
		goto free_clk;

free_clk:
	clk_free(&priv->clk);
#else

	uint reg_val = 0;

	/* set factor_m */
	reg_val = readl(SUNXI_GPADC_CLK2);
	reg_val &= ~(0x1f << 0);
	writel(reg_val, SUNXI_GPADC_CLK2);

	/* set clk source 24M */
	reg_val = readl(SUNXI_GPADC_CLK2);
	reg_val &= ~(0x7 << 24);
	writel(reg_val, SUNXI_GPADC_CLK2);

	/* enable clk gating */
	reg_val = readl(SUNXI_GPADC_CLK2);
	reg_val |= (1 << 31);
	writel(reg_val, SUNXI_GPADC_CLK2);

	/* reset */
	reg_val = readl(SUNXI_GPADC_GAT2);
	reg_val &= ~(1 << 16);
	writel(reg_val, SUNXI_GPADC_GAT2);

	udelay(2);

	reg_val |= (1 << 16);
	writel(reg_val, SUNXI_GPADC_GAT2);

	/* enable ADC gating */
	reg_val = readl(SUNXI_GPADC_GAT2);
	reg_val |= (1 << 0);
	writel(reg_val, SUNXI_GPADC_GAT2);

#endif

	priv->active_channel = -1;

	return 0;
}

int sunxi_gpadc_of_to_plat(struct udevice *dev)
{
	struct adc_uclass_plat *uc_pdata = dev_get_uclass_plat(dev);
	struct sunxi_gpadc_priv *priv = dev_get_priv(dev);

	priv->base_addr = dev_read_addr_ptr(dev);
	if (priv->base_addr == (void *) FDT_ADDR_T_NONE) {
		pr_err("Dev: %s - can't get address!", dev->name);
		return -ENODATA;
	}

	priv->channel_num = dev_read_u32_default(dev, "channel_num", 10);

	uc_pdata->data_mask = GPADC_DAT_MASK;
	uc_pdata->data_format = ADC_DATA_FORMAT_BIN;
	uc_pdata->data_timeout_us = GPADC_CONV_TIMEOUT_US;

	/* Mask available channel bits: [0:9] */
	uc_pdata->channel_mask = (2 << (priv->channel_num - 1)) - 1;

	return 0;
}

static const struct adc_ops sunxi_gpadc_ops = {
	.start_channel = sunxi_gpadc_start_channel,
	.channel_data = sunxi_gpadc_channel_data,
	.stop = sunxi_gpadc_stop,
};

static const struct udevice_id sunxi_gpadc_ids[] = {
	{ .compatible = "allwinner,sunxi-gpadc" },
	{ }
};

U_BOOT_DRIVER(sunxi_gpadc) = {
	.name		= "sunxi-gpadc",
	.id		= UCLASS_ADC,
	.of_match	= sunxi_gpadc_ids,
	.ops		= &sunxi_gpadc_ops,
	.probe		= sunxi_gpadc_probe,
	.of_to_plat = sunxi_gpadc_of_to_plat,
	.priv_auto	= sizeof(struct sunxi_gpadc_priv),
};
