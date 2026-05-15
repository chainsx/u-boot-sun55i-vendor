/*
 * (C) Copyright 2019-2025
 * allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * xudongpdc <xudongpdc@allwinnertech.com>
 *
 * some simple description for this code
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#include <common.h>
#include <audio_codec.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <dm/device-internal.h>
#include <errno.h>
#include <log.h>
#include <sound.h>
#include <reset.h>
#include <dma.h>
#include <cpu_func.h>
#include <asm/gpio.h>
#include <asm-generic/gpio.h>
#include <clk.h>
#include <asm/arch/cpu.h>
#include <asm/arch/dma.h>
#include <linux/delay.h>
#include <linux/errno.h>

#include "sunxi-codec.h"

#include "sun55iw6-codec.h"
#include "sunxi_rw_func.h"


static int sunxi_codec_set_pll(struct sunxi_codec_priv *priv, int stream, u32 freq_in, u32 freq_out)
{
	if (stream == 0) {
		if (sunxi_codec_clk_rate(priv->dev, freq_in, freq_out)) {
			debug("codec clk set rate failed\n");
			return -EINVAL;
		}
	} else {
		return log_msg_ret("stream is invaild", -EINVAL);
	}
	return 0;
}

int sunxi_codec_playback_prepare(struct udevice *dev)
{
	struct sunxi_codec_priv *priv = dev_get_priv(dev);
	int ret = 0;

	ret = sunxi_codec_playback_startup(dev);
	if (ret) {
		dev_dbg(priv->dev, "sunxi_codec_playback_startup failed\n");
	}

	/* increase delay time, if Pop occured */
	mdelay(10);
	if (dm_gpio_is_valid(&priv->spk_gpio))
		dm_gpio_set_value(&priv->spk_gpio, priv->pa_ctl_level);

	return 0;
}


int sunxi_codec_playback_debug(struct udevice *dev)
{
	int ret = 0;
	struct sunxi_codec_priv *priv = dev_get_priv(dev);

	if (!dm_gpio_is_valid(&priv->spk_gpio))
		return -1;

	ret = dm_gpio_get_value(&priv->spk_gpio);
	printf("GPIO %s val:%d\n", priv->spk_gpio.dev->name, ret);
	printf("PI7 conf [%x], data [%x] \n", readl(0x03604480), readl(0x03604480 + 0x10));

	int i = 0;

	for (i = 0; i < 5; i++) {
#if 1
		/* ccu */
		printf("ccu:0x02002260 %x 0x02002268 %x 0x02002280 %x 0x02002288 %x 0x020032E0 %x 0x020032EC %x \n ", \
			readl(priv->addr_clkbase + 0x260), readl(priv->addr_clkbase + 0x268), readl(priv->addr_clkbase + 0x280), \
			readl(priv->addr_clkbase + 0x288), readl(priv->addr_clkbase + 0x12E0), readl(priv->addr_clkbase + 0x12EC));
#endif
		/* audiocodec */

		printf("0x02030000 [%x]  0x02030004 [%x] 0x02030010 [%x]  0x02030014 [%x] \n \
					0x02030024 [%x]  0x02030310 [%x] 0x0203031C [%x] 0x02030320 [%x]\n", \
				readl(priv->addr_base), readl(priv->addr_base + 0x04), readl(priv->addr_base + 0x10), readl(priv->addr_base + 0x14),\
				readl(priv->addr_base + 0x24), readl(priv->addr_base + 0x310), readl(priv->addr_base + 0x31C), readl(priv->addr_base + 0x320));

		mdelay(200);

	}
	return 0;
}

void sunxi_codec_fill_txfifo(struct udevice *dev, u32 *data)
{
	sunxi_codec_fill_txfifo_start(dev, data);
}

int sunxi_codec_playback_start(struct udevice *dev, ulong handle, u32 *srcBuf, u32 cnt)
{
	int ret = 0;

	flush_cache((unsigned long)srcBuf, cnt);

	sunxi_codec_playback_trigger_start(dev, handle, srcBuf, cnt);

	return ret;
}

int sunxi_codec_playback_stop(struct udevice *dev, ulong handle)
{

	return sunxi_codec_playback_trigger_stop(dev, handle);
}

static int sunxi_codec_of_to_plat(struct udevice *dev)
{
	struct sunxi_codec_priv *priv = dev_get_priv(dev);
	fdt_addr_t base;
	int ret = 0;
	uint tmp;

	base = devfdt_get_addr_name(dev, "audiocodec");
	if (base == FDT_ADDR_T_NONE) {
		debug("%s: Missing audiocodec base\n", __func__);
		return -EINVAL;
	}
	priv->addr_base = (void __iomem *)base;

#ifndef SUNXI_USE_CCU_DRV
	base = devfdt_get_addr_name(dev, "ccu");
	if (base == FDT_ADDR_T_NONE) {
		debug("%s: Missing ccu base\n", __func__);
		return -EINVAL;
	}
	priv->addr_clkbase = (void __iomem *)base;
#endif

	printf("codec: %s %d dev_name: %s base: 0x%x clkbase: 0x%x\n", __func__, __LINE__, priv->dev->name, \
			(unsigned int)priv->addr_base, (unsigned int)priv->addr_clkbase);

	ret = dev_read_u32u(dev, "dac_vol", &tmp);
	if (ret) {
		debug("dac_vol get failed\n");
		priv->dac_vol = 63;
	} else {
		priv->dac_vol = tmp;
	}
	printf("dac_vol = %d\n", priv->dac_vol);


	ret = dev_read_u32u(dev, "dacl_vol", &tmp);
	if (ret) {
		debug("dacl_vol get failed\n");
		priv->dacl_vol = 160;
	} else {
		priv->dacl_vol = tmp;
	}
	printf("dacl_vol = %d\n", priv->dacl_vol);


	ret = dev_read_u32u(dev, "lineout_vol", &tmp);
	if (ret) {
		debug("lineout_vol get failed\n");
		priv->lineout_vol = 31;
	} else {
		priv->lineout_vol = tmp;
	}
	printf("lineout_vol = %d\n", priv->lineout_vol);


	ret = dev_read_u32u(dev, "pa_level", &tmp);
	if (ret) {
		debug("pa_level get failed\n");
		priv->pa_ctl_level = 0;
	} else {
		if (tmp > 0)
			priv->pa_ctl_level = 1;
	}
	printf("pa_ctl_level = %d\n", priv->pa_ctl_level);


	ret = gpio_request_by_name(dev, "gpio-spk", 0, &priv->spk_gpio,
				   GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);
	if (ret) {
		return log_msg_ret("gpio", ret);
	}

	if (dm_gpio_is_valid(&priv->spk_gpio))
		dm_gpio_set_value(&priv->spk_gpio, !priv->pa_ctl_level);

	return 0;
}

/**
 * sunxi_codec_device_init() - Initialise sunxi audio codec device
 *
 * @priv: sunxi audiocodec information
 *
 * Return: -EIO for error, 0 for success.
 */
static int sunxi_codec_device_init(struct sunxi_codec_priv *priv)
{
	int ret = 0;
	bool use_ccu = 0;;

	priv->clk = sunxi_codec_clk_init(priv->dev, &use_ccu);
	if (!priv->clk && use_ccu) {
		debug("clk init failed\n");
		ret = -EINVAL;
		goto err_codec_clk_init;
	}

	ret = sunxi_codec_clk_enable(priv->dev);
	if (ret) {
		debug("clk enable failed\n");
		ret = -EINVAL;
		goto err_clk_enable;
	}

	/* init audiocodec*/
	ret = sunxi_codec_init(priv->dev);
	if (ret != 0) {
		printf("init audiocodec failed\n");
		goto err_clk_enable;
	}

	/* initialize private data */
	priv->channels = -1U;
	priv->rate = -1U;
	priv->bits = -1U;

	return 0;


err_clk_enable:
	sunxi_codec_clk_disable(priv->clk);

err_codec_clk_init:
	sunxi_codec_clk_exit(priv->clk);

	return ret;

}

static int sunxi_codec_do_init(struct sunxi_codec_priv *priv, int rate,
			int bits_per_sample, uint channels)
{
	int ret = 0;
	unsigned int freq_point;

	switch (rate) {
	case 8000:
	case 12000:
	case 16000:
	case 24000:
	case 32000:
	case 48000:
	case 64000:
	case 96000:
	case 192000:
		freq_point = 24576000;
		break;
	case 11025:
	case 22050:
	case 44100:
	case 88200:
	case 176400:
		freq_point = 22579200;
		break;
	default:
		printf("Invalid rate %d\n", rate);
		return -EINVAL;
	}

	ret = sunxi_codec_set_pll(priv, 0, freq_point, freq_point);
	if (ret < 0) {
		printf("%s: sunxi codec set sys clock failed\n", __func__);
		return ret;
	}

	ret = sunxi_codec_hw_params(priv->dev, rate, bits_per_sample, channels);
	if (ret < 0) {
		printf("%s: sunxi codec set hw params failed\n", __func__);
		return ret;
	}

	debug("ccu:0x02002260 [%x] 0x02002268 [%x] 0x02002280 [%x] 0x02002288 [%x] 0x020032E0 [%x] 0x020032EC [%x]\n", \
				readl(priv->addr_clkbase + 0x260), readl(priv->addr_clkbase + 0x268), \
				readl(priv->addr_clkbase + 0x280), readl(priv->addr_clkbase + 0x288), \
				readl(priv->addr_clkbase + 0x12E0), readl(priv->addr_clkbase + 0x12EC));

	return ret;
}


static int sunxi_codec_set_params(struct udevice *dev, int interface, int rate,
			     int mclk_freq, int bits_per_sample, uint channels)
{
	struct sunxi_codec_priv *priv = dev_get_priv(dev);

	debug("%s: rate %d, bit %d, ch %d\n", __func__, rate, bits_per_sample, channels);

	return sunxi_codec_do_init(priv, rate, bits_per_sample, channels);
}

static int sunxi_codec_probe(struct udevice *dev)
{
	struct sunxi_codec_priv *priv = dev_get_priv(dev);
	int ret;

	priv->dev = dev;
	ret = sunxi_codec_device_init(priv);
	if (ret < 0) {
		debug("%s: sunxi codec init failed\n", __func__);
		return ret;
	}
	return 0;
}

static const struct audio_codec_ops sunxi_codec_ops = {
	.set_params	= sunxi_codec_set_params,
};

static const struct udevice_id sunxi_codec_ids[] = {
	{ .compatible = "allwinner,sun55iw6-snd-codec" },
	{ }
};

U_BOOT_DRIVER(audiocodec) = {
	.name		= "sunxi-codec",
	.id		= UCLASS_AUDIO_CODEC,
	.of_match	= sunxi_codec_ids,
	.probe		= sunxi_codec_probe,
	.of_to_plat	= sunxi_codec_of_to_plat,
	.ops		= &sunxi_codec_ops,
	.priv_auto	= sizeof(struct sunxi_codec_priv),
};
