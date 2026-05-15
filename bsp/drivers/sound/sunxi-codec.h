/* u-boot-bsp/drivers/sound/sunxi-codec.h
 * (C) Copyright 2021-2025
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * xudongpdc <xudongpdc@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef _SUNXI_AUDIOCODEC_H
#define _SUNXI_AUDIOCODEC_H

#include <asm/gpio.h>
#include <asm-generic/gpio.h>

struct udevice;

typedef void sunxi_codec_clk_t;

struct sample_rate {
	unsigned int samplerate;
	unsigned int rate_bit;
};

/* codec private data */
struct sunxi_codec_priv {
	void __iomem *addr_base;
	void __iomem *addr_clkbase;
	unsigned int channels;
	unsigned int rate;
	unsigned int bits;
	u32 dac_vol;
	u32 dacl_vol;
	u32 lineout_vol;
	bool pa_ctl_level;
	sunxi_codec_clk_t *clk;
	struct gpio_desc spk_gpio;
	struct udevice *dev;
};


sunxi_codec_clk_t *sunxi_codec_clk_init(struct udevice *dev, bool *use_ccu);
void sunxi_codec_clk_exit(void *clk_orig);
int sunxi_codec_clk_enable(struct udevice *dev);
void sunxi_codec_clk_disable(void *clk_orig);
int sunxi_codec_clk_rate(struct udevice *dev, unsigned int freq_in, unsigned int freq_out);
int sunxi_codec_hw_params(struct udevice *dev, u32 rate, u32 bits_per_sample, u32 channels);
int sunxi_codec_init(struct udevice *dev);
int sunxi_codec_playback_startup(struct udevice *dev);
void sunxi_codec_fill_txfifo_start(struct udevice *dev, u32 *data);
int sunxi_codec_playback_trigger_start(struct udevice *dev, ulong handle, u32 *srcBuf, u32 cnt);
int sunxi_codec_playback_trigger_stop(struct udevice *dev, ulong handle);


#endif /* _SUNXI_AUDIOCODEC_H */
