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

//#define SUNXI_USE_CCU_DRV

struct sunxi_codec_clk {
	/* parent clk */
	struct clk clk_pll_audio0_4x;
	struct clk clk_pll_audio1_4x;

	/* module clk */
	struct clk clk_audio_dac;

	struct clk clk_bus;
	struct reset_ctl resets;
};

sunxi_codec_clk_t *sunxi_codec_clk_init(struct udevice *dev, bool *use_ccu)
{
	struct sunxi_codec_clk *clk = NULL;

	debug("\n");

#ifdef SUNXI_USE_CCU_DRV
	int ret;

	clk = calloc(1, sizeof(*clk));
	if (!clk) {
		printf("can't allocate sunxi_codec_clk memory\n");
		return NULL;
	}

	ret = reset_get_by_name(dev, "rst", &clk->resets);
	if (ret) {
		debug("failed to get reset phandle\n");
		goto free_clk_ret;
	}

	reset_assert(&clk->resets);
	udelay(10);
	reset_deassert(&clk->resets);

	ret = clk_get_by_name(dev, "clk_bus_audio", &clk->clk_bus);
	if (ret) {
		debug("failed to get clk bus phandle\n");
		goto free_clk_bus;
	}

	ret = clk_get_by_name(dev, "clk_pll_audio0_4x", &clk->clk_pll_audio0_4x);
	if (ret) {
		debug("failed to get clk bus phandle\n");
		goto free_clk_pll_audio0_4x;
	}

	ret = clk_get_by_name(dev, "clk_pll_audio1_4x", &clk->clk_pll_audio1_4x);
	if (ret) {
		debug("failed to get clk bus phandle\n");
		goto free_clk_pll_audio1_4x;
	}

	ret = clk_get_by_name(dev, "clk_audio_dac", &clk->clk_audio_dac);
	if (ret) {
		debug("failed to get clk bus phandle\n");
		goto free_clk_audio_dac;
	}

	*use_ccu = 1;

#else

	*use_ccu = 0;

#endif

	return clk;

#ifdef SUNXI_USE_CCU_DRV

free_clk_audio_dac:
	clk_free(&clk->clk_audio_dac);

free_clk_pll_audio1_4x:
	clk_free(&clk->clk_pll_audio1_4x);

free_clk_pll_audio0_4x:
	clk_free(&clk->clk_pll_audio0_4x);

free_clk_bus:
	clk_free(&clk->clk_bus);

free_clk_ret:
	clk_free(&clk->resets);

	free(clk);
#endif

	return NULL;


}

void sunxi_codec_clk_exit(void *clk_orig)
{
	struct sunxi_codec_clk *clk = (struct sunxi_codec_clk *)clk_orig;

	debug("\n");

	clk_free(&clk->clk_audio_dac);
	clk_free(&clk->clk_pll_audio1_4x);
	clk_free(&clk->clk_pll_audio0_4x);
	clk_free(&clk->clk_bus);

	free(clk);
}

int sunxi_codec_clk_enable(struct udevice *dev)
{
	struct sunxi_codec_priv *priv = dev_get_priv(dev);
	int ret = -1;

	debug("\n");

#ifdef SUNXI_USE_CCU_DRV
	struct sunxi_codec_clk *clk = (struct sunxi_codec_clk *)priv->clk;

	if (clk == NULL) {
		debug("Error, CLK is NULl\n");
		return -EINVAL;
	}

	ret = clk_enable(&clk->clk_bus);
	if (ret) {
		debug("failed to enable clk bus\n");
		goto err_enable_clk_bus;
	}

	ret = clk_enable(&clk->clk_pll_audio0_4x);
	if (ret) {
		debug("failed to enable clk_pll_audio0_4x bus\n");
		goto err_enable_clk_pll_audio0_4x;
	}

	ret = clk_enable(&clk->clk_pll_audio1_4x);
	if (ret) {
		debug("failed to enable clk_pll_audio1_4x bus\n");
		goto err_enable_clk_pll_audio1_4x;
	}

	ret = clk_enable(&clk->clk_audio_dac);
	if (ret) {
		debug("failed to enable clk_audio_dac bus\n");
		goto err_enable_clk_audio_dac;
	}

#else
	if (priv == NULL) {
		return log_msg_ret("codec priv is invaild", -EINVAL);
	}

	/*ret audiocodec clk*/
	sunxi_codec_update_bits(priv->addr_clkbase, 0x12EC, 0x1 << 16, 0x0 << 16);
	udelay(10);
	sunxi_codec_update_bits(priv->addr_clkbase, 0x12EC, 0x1 << 16, 0x1 << 16);

	/*enable audiocodec bus clk*/
	sunxi_codec_update_bits(priv->addr_clkbase, 0x12EC, 0x1 << 0, 0x1 << 0);

	/* enable pll_audio0_4x */
	sunxi_codec_update_bits(priv->addr_clkbase, 0x260, 0x1 << 30, 0x1 << 30);  /* LDO_EN */
	sunxi_codec_update_bits(priv->addr_clkbase, 0x260, 0x1 << 27, 0x1 << 27);  /* OUTPUT GATE */
	sunxi_codec_update_bits(priv->addr_clkbase, 0x260, 0x1 << 31, 0x1 << 31);  /* PLLEN */
	sunxi_codec_update_bits(priv->addr_clkbase, 0x260, 0x1 << 29, 0x1 << 29);  /* LOCK ENABLE */

	/* enable pll_audio1_4x */
	sunxi_codec_update_bits(priv->addr_clkbase, 0x280, 0x1 << 30, 0x1 << 30);
	sunxi_codec_update_bits(priv->addr_clkbase, 0x280, 0x1 << 27, 0x1 << 27);
	sunxi_codec_update_bits(priv->addr_clkbase, 0x280, 0x1 << 31, 0x1 << 31);
	sunxi_codec_update_bits(priv->addr_clkbase, 0x280, 0x1 << 29, 0x1 << 29);

	/* enable clk_audio_dac gate */
	sunxi_codec_update_bits(priv->addr_clkbase, 0x12E0, 0x1 << 31, 0x1 << 31);

#endif

	return 0;

#ifdef SUNXI_USE_CCU_DRV
err_enable_clk_audio_dac:
	clk_disable(&clk->clk_audio_dac);
err_enable_clk_pll_audio1_4x:
	clk_disable(&clk->clk_pll_audio1_4x);
err_enable_clk_pll_audio0_4x:
	clk_disable(&clk->clk_pll_audio0_4x);
err_enable_clk_bus:
	clk_disable(&clk->clk_bus);
#endif
	return ret;

}

void sunxi_codec_clk_disable(void *clk_orig)
{
	struct sunxi_codec_clk *clk = (struct sunxi_codec_clk *)clk_orig;

	debug("\n");

	clk_disable(&clk->clk_audio_dac);
	clk_disable(&clk->clk_pll_audio0_4x);
	clk_disable(&clk->clk_pll_audio1_4x);
	clk_disable(&clk->clk_bus);
}

int sunxi_codec_clk_rate(struct udevice *dev, unsigned int freq_in, unsigned int freq_out)
{
	struct sunxi_codec_priv *priv = dev_get_priv(dev);

	debug("\n");
#ifdef SUNXI_USE_CCU_DRV
	struct sunxi_codec_clk *clk = (struct sunxi_codec_clk *)priv->clk;

	if (freq_in % 24576000 == 0) {
		if (clk_set_parent(&clk->clk_audio_dac, &clk->clk_pll_audio0_4x)) {
				debug("set dmic parent clk failed\n");
				return -EINVAL;
		}
	} else {
		if (clk_set_parent(&clk->clk_audio_dac, &clk->clk_pll_audio1_4x)) {
				debug("set dmic parent clk failed\n");
				return -EINVAL;
		}
	}

	if (clk_set_rate(&clk->clk_audio_dac, freq_out)) {
			debug("set dmic rate failed, rate: %u\n", freq_out);
			return -EINVAL;
	}
#else

	/* clk_audio_dac sel */
	if (freq_in % 24576000 == 0) {
		sunxi_codec_update_bits(priv->addr_clkbase, 0x12E0, 0x1 << 24, 0x0 << 24);

		/* disable sdm */
		sunxi_codec_update_bits(priv->addr_clkbase, 0x260, 0x1 << 24, 0x0 << 24);
		/* set PLL AUDIO0 P and N  PLL AUDIO0 = 24 *N / M1 / M0 / P */
		/* 24 * 128 / 25 = 122.88M = 24.576 * 5M */
		sunxi_codec_update_bits(priv->addr_clkbase, 0x260, 0x3F << 16, 0x18 << 16);  /* P = 25 */
		sunxi_codec_update_bits(priv->addr_clkbase, 0x260, 0xFF << 8, 0x7F << 8);  /* N = 128 */

		/* set audiocodec dac div M = 5 */
		sunxi_codec_update_bits(priv->addr_clkbase, 0x12E0, 0x1F << 0, 0x4 << 0);
	} else {
		sunxi_codec_update_bits(priv->addr_clkbase, 0x12E0, 0x1 << 24, 0x1 << 24);

		/* enable sdm */
		sunxi_codec_update_bits(priv->addr_clkbase, 0x280, 0x1 << 24, 0x1 << 24);
		/* set PLL AUDIO1 P and N  PLL AUDIO1 = 24 *N / M1 / M0 / P */
		/* 24 * 94 / 50 = 45.12M */
		sunxi_codec_update_bits(priv->addr_clkbase, 0x280, 0x3F << 16, 0x31 << 16);  /* P = 50 */
		sunxi_codec_update_bits(priv->addr_clkbase, 0x280, 0xFF << 8, 0x5D << 8);  /* N = 94 */

		sunxi_codec_write(priv->addr_clkbase, 0x288, 0xc00028f5);

		/* set audiocodec dac div M = 0 */
		sunxi_codec_update_bits(priv->addr_clkbase, 0x12E0, 0x1F << 0, 0x0 <<0);

	}

#endif

	return 0;
}

static const struct sample_rate sample_rate_conv[] = {
	{44100, 0},
	{48000, 0},
	{8000, 5},
	{32000, 1},
	{22050, 2},
	{24000, 2},
	{16000, 3},
	{11025, 4},
	{12000, 4},
	{192000, 6},
	{96000, 7},
};

/*
 * sample bits
 * sample rate
 * channels
 *
 */

int sunxi_codec_hw_params(struct udevice *dev, u32 rate, u32 bits_per_sample, u32 channels)
{

	struct sunxi_codec_priv *priv = dev_get_priv(dev);
	int i;

	/*
	 * Audio codec
	 * SUNXI_DAC_FIFO_CTL		0xF0
	 * SUNXI_DAC_DPC		0x00
	 *
	 */
	/* set playback sample resolution, only little endian*/
	switch (bits_per_sample) {
	case 16:
		sunxi_codec_update_bits(priv->addr_base, SUNXI_DAC_FIFO_CTL,
					   0x3 << DAC_FIFO_MODE, 0x3 << DAC_FIFO_MODE);
		sunxi_codec_update_bits(priv->addr_base, SUNXI_DAC_FIFO_CTL,
					   0x1 << TX_SAMPLE_BITS, 0x0 << TX_SAMPLE_BITS);
		break;
	case 24:
	case 32:
		sunxi_codec_update_bits(priv->addr_base, SUNXI_DAC_FIFO_CTL,
				   0x3 << DAC_FIFO_MODE, 0x0 << DAC_FIFO_MODE);
		sunxi_codec_update_bits(priv->addr_base, SUNXI_DAC_FIFO_CTL,
				   0x1 << TX_SAMPLE_BITS, 0x1 << TX_SAMPLE_BITS);
		break;
	default:
		return -1;
	}
	priv->bits = bits_per_sample;

	/* set playback sample rate */
	for (i = 0; i < ARRAY_SIZE(sample_rate_conv); i++) {
		if (sample_rate_conv[i].samplerate == rate) {
			sunxi_codec_update_bits(priv->addr_base, SUNXI_DAC_FIFO_CTL, 0x7 << DAC_FS,
					   (sample_rate_conv[i].rate_bit << DAC_FS));
		}
	}
	priv->rate = rate;

	/* set playback channels */
	if (channels == 1) {
		sunxi_codec_update_bits(priv->addr_base, SUNXI_DAC_FIFO_CTL,
					(1 << DAC_MONO_EN), (1 << DAC_MONO_EN));
	} else if (channels == 2) {
		sunxi_codec_update_bits(priv->addr_base, SUNXI_DAC_FIFO_CTL,
					(1 << DAC_MONO_EN), (0 << DAC_MONO_EN));
	} else {
		debug("channel:%u is not support!\n", channels);
	}
	priv->channels = channels;

	return 0;
}

int sunxi_codec_init(struct udevice *dev)
{
	struct sunxi_codec_priv *priv = dev_get_priv(dev);

	if (priv == NULL) {
		return log_msg_ret("codec priv is invaild", -EINVAL);
	}

	/* Enable DAC DAP */
	sunxi_codec_update_bits(priv->addr_base, SUNXI_DAC_DAP_CTL, 0x1 << DDAP_EN, 0x1 << DDAP_EN);
	/* set lineout as differential output */
	sunxi_codec_update_bits(priv->addr_base, SUNXI_DAC_AN_REG, 0x1 << LINEOUT_DIFFEN, 0x1 << LINEOUT_DIFFEN);
	/* Open DACL\R Volume Setting */
	sunxi_codec_update_bits(priv->addr_base, SUNXI_DAC_VOL_CTL, 0x1 << DAC_VOL_SEL, 0x1 << DAC_VOL_SEL);

	/* set digital volume 0x0, 0*-1.16 = 0dB */
	sunxi_codec_update_bits(priv->addr_base, SUNXI_DAC_DPC, (0x3F << DVOL), (priv->dac_vol << DVOL));
	/* set digital L/R volume */
	sunxi_codec_update_bits(priv->addr_base, SUNXI_DAC_VOL_CTL, 0xFF << DAC_VOL_L, (priv->dacl_vol << DAC_VOL_L));
	sunxi_codec_update_bits(priv->addr_base, SUNXI_DAC_VOL_CTL, 0xFF << DAC_VOL_R, (priv->dacl_vol << DAC_VOL_R));
	/* set LINEOUT volume, such as 0x19, -(31-0x19)*1.5 = -9dB */
	sunxi_codec_update_bits(priv->addr_base, SUNXI_DAC_AN_REG, 0x1F << LINEOUT_VOL_CTRL, (priv->lineout_vol << LINEOUT_VOL_CTRL));

	return 0;
}

int sunxi_codec_playback_startup(struct udevice *dev)
{
	struct sunxi_codec_priv *priv = dev_get_priv(dev);

	if (priv == NULL) {
		return log_msg_ret("codec priv is invaild", -EINVAL);
	}

	/* RMC Enable */
	sunxi_codec_update_bits(priv->addr_base, SUNXI_RAMP, 0x1 << RMC_EN, 0x1 << RMC_EN);


	/* FIFO flush, clear pending, clear sample counter */
	sunxi_codec_update_bits(priv->addr_base, SUNXI_DAC_FIFO_CTL,
			   0x1 << DAC_FIFO_FLUSH, 0x1 << DAC_FIFO_FLUSH);
	sunxi_codec_write(priv->addr_base, SUNXI_DAC_FIFO_STA,
			 0x1 << DAC_TXE_INT | 0x1 << DAC_TXU_INT | 0x1 << DAC_TXO_INT);
	sunxi_codec_write(priv->addr_base, SUNXI_DAC_CNT, 0x0);

	/* enable FIFO empty DRQ */
	sunxi_codec_update_bits(priv->addr_base, SUNXI_DAC_FIFO_CTL,
				(1 << DAC_DRQ_EN), (1 << DAC_DRQ_EN));

	/* Enable DAC analog left channel */
	sunxi_codec_update_bits(priv->addr_base, SUNXI_DAC_AN_REG,
				(0x1 << DACL_EN), (0x1 << DACL_EN));

	/* enable DAC digital part */
	sunxi_codec_update_bits(priv->addr_base, SUNXI_DAC_DPC,
				(0x1 << DAC_DIG_EN), (0x1 << DAC_DIG_EN));

	/* enable left and right LINEOUT */
	sunxi_codec_update_bits(priv->addr_base, SUNXI_DAC_AN_REG, 0x1 << LMUTE, 0x1 << LMUTE);
	sunxi_codec_update_bits(priv->addr_base, SUNXI_DAC_AN_REG,
			   (0x1 << LINEOUTL_EN), (0x1 << LINEOUTL_EN));

	//sunxi_codec_write(codec, SUNXI_DAC_DEBUG, 0xB00);

	debug("0x02030000 [%x]  0x02030004 [%x] 0x02030010 [%x]  0x02030014 [%x] \n \
			0x02030024 [%x]  0x02030310 [%x] 0x0203031C [%x] 0x02030320 [%x] \n", \
			readl(priv->addr_base), readl(priv->addr_base + 0x04), readl(priv->addr_base + 0x10), readl(priv->addr_base + 0x14),\
			readl(priv->addr_base + 0x24), readl(priv->addr_base + 0x310), readl(priv->addr_base + 0x31C), readl(priv->addr_base + 0x320));

	return 0;
}

void sunxi_codec_fill_txfifo_start(struct udevice *dev, u32 *data)
{
	struct sunxi_codec_priv *priv = dev_get_priv(dev);

	sunxi_codec_write(priv, SUNXI_DAC_TXDATA, *data);
}

int sunxi_codec_playback_trigger_start(struct udevice *dev, ulong handle, u32 *srcBuf, u32 cnt)
{
	int ret = 0;
	struct sunxi_codec_priv *priv = dev_get_priv(dev);

	printf("start dma: from 0x%p to 0x%p  total 0x%x(%u)byte\n",
	       srcBuf, priv->addr_base + SUNXI_DAC_TXDATA, cnt, cnt);

	ret = sunxi_dma_start(handle, (uint)srcBuf,
			      (uint)(priv->addr_base + SUNXI_DAC_TXDATA), cnt);

	return ret;
}

int sunxi_codec_playback_trigger_stop(struct udevice *dev, ulong handle)
{
	int ret = 0;
	struct sunxi_codec_priv *priv = dev_get_priv(dev);

	ret = sunxi_dma_stop(handle);
	if (ret) {
		dev_dbg(priv->dev, "dma stop failed\n");
	}

	return ret;
}

