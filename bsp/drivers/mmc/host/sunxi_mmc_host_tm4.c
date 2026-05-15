// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2007-2011
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Aaron <leafy.myeh@allwinnertech.com>
 *
 * MMC driver for allwinner sunxi platform.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 */
#include <common.h>
#include <asm/io.h>
#include <asm/arch-sunxi/clock.h>
#include <asm/arch-sunxi/cpu.h>
#include <malloc.h>
#include <linux/libfdt.h>
#include <fdt_support.h>
#include <private_uboot.h>
#include <linux/delay.h>
#include <sunxi_mmc.h>
#include <mmc_def.h>

#include "sunxi_mmc_host_common.h"
#include "sunxi_mmc_host_tm4.h"

#define  SMC_DATA_TIMEOUT     0xffffffU
#define  SMC_RESP_TIMEOUT     0xff

#define SUNXI_DMA_TL_TM4_V4P5X                ((0x3<<28)|(15<<16)|240)
extern char *spd_name[];

static int mmc_init_default_timing_para(struct sunxi_mmc_priv *priv)
{
	struct sunxi_mmc_priv *mmcpriv = priv;
	struct mmc_config *cfg = mmcpriv->cfg;

	/* timing mode 4 */
	mmcpriv->tm4.cur_spd_md = DS26_SDR12;
	mmcpriv->tm4.cur_freq = CLK_400K;
	mmcpriv->tm4.sample_point_cnt = MMC_CLK_SAMPLE_POINIT_MODE_4;

	mmcpriv->tm4.def_odly[DS26_SDR12*MAX_CLK_FREQ_NUM+CLK_400K] = TM4_OUT_PH180;
	mmcpriv->tm4.def_sdly[DS26_SDR12*MAX_CLK_FREQ_NUM+CLK_400K] = 0x0;
	mmcpriv->tm4.def_odly[DS26_SDR12*MAX_CLK_FREQ_NUM+CLK_25M] = TM4_OUT_PH180;
	mmcpriv->tm4.def_sdly[DS26_SDR12*MAX_CLK_FREQ_NUM+CLK_25M] = 0x0;

	mmcpriv->tm4.def_odly[HSSDR52_SDR25*MAX_CLK_FREQ_NUM+CLK_400K] = TM4_OUT_PH180;
	mmcpriv->tm4.def_sdly[HSSDR52_SDR25*MAX_CLK_FREQ_NUM+CLK_400K] = 0;
	mmcpriv->tm4.def_odly[HSSDR52_SDR25*MAX_CLK_FREQ_NUM+CLK_25M] = TM4_OUT_PH180;
	mmcpriv->tm4.def_sdly[HSSDR52_SDR25*MAX_CLK_FREQ_NUM+CLK_25M] = 0;
	mmcpriv->tm4.def_odly[HSSDR52_SDR25*MAX_CLK_FREQ_NUM+CLK_50M] = TM4_OUT_PH180;
	mmcpriv->tm4.def_sdly[HSSDR52_SDR25*MAX_CLK_FREQ_NUM+CLK_50M] = 0;

	if (cfg->host_caps & MMC_MODE_8BIT) {
		mmcpriv->tm4.def_odly[HSDDR52_DDR50*MAX_CLK_FREQ_NUM+CLK_400K] = TM4_OUT_PH180;
		mmcpriv->tm4.def_sdly[HSDDR52_DDR50*MAX_CLK_FREQ_NUM+CLK_400K] = 0xe;
		mmcpriv->tm4.def_odly[HSDDR52_DDR50*MAX_CLK_FREQ_NUM+CLK_25M] = TM4_OUT_PH180;
		mmcpriv->tm4.def_sdly[HSDDR52_DDR50*MAX_CLK_FREQ_NUM+CLK_25M] = 0xe;
		mmcpriv->tm4.def_odly[HSDDR52_DDR50*MAX_CLK_FREQ_NUM+CLK_50M] = TM4_OUT_PH180;
		mmcpriv->tm4.def_sdly[HSDDR52_DDR50*MAX_CLK_FREQ_NUM+CLK_50M] = 0xe;
	} else if (cfg->host_caps & MMC_MODE_4BIT) {
		mmcpriv->tm4.def_odly[HSDDR52_DDR50*MAX_CLK_FREQ_NUM+CLK_400K] = TM4_OUT_PH90;
		mmcpriv->tm4.def_sdly[HSDDR52_DDR50*MAX_CLK_FREQ_NUM+CLK_400K] = 0xe;
		mmcpriv->tm4.def_odly[HSDDR52_DDR50*MAX_CLK_FREQ_NUM+CLK_25M] = TM4_OUT_PH90;
		mmcpriv->tm4.def_sdly[HSDDR52_DDR50*MAX_CLK_FREQ_NUM+CLK_25M] = 0xe;
		mmcpriv->tm4.def_odly[HSDDR52_DDR50*MAX_CLK_FREQ_NUM+CLK_50M] = TM4_OUT_PH90;
		mmcpriv->tm4.def_sdly[HSDDR52_DDR50*MAX_CLK_FREQ_NUM+CLK_50M] = 0xe;
	}

	mmcpriv->tm4.def_odly[HS200_SDR104*MAX_CLK_FREQ_NUM+CLK_400K] = TM4_OUT_PH180;
	mmcpriv->tm4.def_sdly[HS200_SDR104*MAX_CLK_FREQ_NUM+CLK_400K] = 0x0;
	mmcpriv->tm4.def_odly[HS200_SDR104*MAX_CLK_FREQ_NUM+CLK_25M] = TM4_OUT_PH90;
	mmcpriv->tm4.def_sdly[HS200_SDR104*MAX_CLK_FREQ_NUM+CLK_25M] = 0x0;
	mmcpriv->tm4.def_odly[HS200_SDR104*MAX_CLK_FREQ_NUM+CLK_50M] = TM4_OUT_PH90;
	mmcpriv->tm4.def_sdly[HS200_SDR104*MAX_CLK_FREQ_NUM+CLK_50M] = 0x11;
	mmcpriv->tm4.def_odly[HS200_SDR104*MAX_CLK_FREQ_NUM+CLK_100M] = TM4_OUT_PH90;
	mmcpriv->tm4.def_sdly[HS200_SDR104*MAX_CLK_FREQ_NUM+CLK_100M] = 0x12;
	mmcpriv->tm4.def_odly[HS200_SDR104*MAX_CLK_FREQ_NUM+CLK_150M] = TM4_OUT_PH90;
	mmcpriv->tm4.def_sdly[HS200_SDR104*MAX_CLK_FREQ_NUM+CLK_150M] = 0x13;
	mmcpriv->tm4.def_odly[HS200_SDR104*MAX_CLK_FREQ_NUM+CLK_200M] = TM4_OUT_PH90;
	mmcpriv->tm4.def_sdly[HS200_SDR104*MAX_CLK_FREQ_NUM+CLK_200M] = 0x6;

	mmcpriv->tm4.def_odly[HS400*MAX_CLK_FREQ_NUM+CLK_400K] = TM4_OUT_PH180;
	mmcpriv->tm4.def_sdly[HS400*MAX_CLK_FREQ_NUM+CLK_400K] = 0x0;
	mmcpriv->tm4.def_odly[HS400*MAX_CLK_FREQ_NUM+CLK_25M] = TM4_OUT_PH90;
	mmcpriv->tm4.def_sdly[HS400*MAX_CLK_FREQ_NUM+CLK_25M] = 0x0;
	mmcpriv->tm4.def_odly[HS400*MAX_CLK_FREQ_NUM+CLK_50M] = TM4_OUT_PH90;
	mmcpriv->tm4.def_sdly[HS400*MAX_CLK_FREQ_NUM+CLK_50M] = 0x11;
	mmcpriv->tm4.def_odly[HS400*MAX_CLK_FREQ_NUM+CLK_100M] = TM4_OUT_PH90;
	mmcpriv->tm4.def_sdly[HS400*MAX_CLK_FREQ_NUM+CLK_100M] = 0x12;
	mmcpriv->tm4.def_odly[HS400*MAX_CLK_FREQ_NUM+CLK_150M] = TM4_OUT_PH90;
	mmcpriv->tm4.def_sdly[HS400*MAX_CLK_FREQ_NUM+CLK_150M] = 0x13;
	mmcpriv->tm4.def_odly[HS400*MAX_CLK_FREQ_NUM+CLK_200M] = TM4_OUT_PH90;
	mmcpriv->tm4.def_sdly[HS400*MAX_CLK_FREQ_NUM+CLK_200M] = 0x6;

	mmcpriv->tm4.def_dsdly[CLK_25M] = 0x0;
	mmcpriv->tm4.def_dsdly[CLK_50M] = 0x18;
	mmcpriv->tm4.def_dsdly[CLK_100M] = 0xd;
	mmcpriv->tm4.def_dsdly[CLK_150M] = 0x6;
	mmcpriv->tm4.def_dsdly[CLK_200M] = 0x3;

	return 0;
}

static void sunxi_mmc_clk_io_onoff(struct sunxi_mmc_priv *priv, int onoff, int reset_clk)
{

	int rval;

	if (onoff) {
		reset_deassert_bulk(&priv->reset_bulk);
		clk_enable(&(priv->gate_clk_ahb));
		clk_enable(&(priv->gate_clk_mmc));
	} else {
		clk_disable(&(priv->gate_clk_mmc));
		clk_disable(&(priv->gate_clk_ahb));
		reset_assert_bulk(&priv->reset_bulk);
	}

	/* config mod clock */
	if (reset_clk) {
		rval = readl(priv->mclkreg);
		/*set to 24M default value*/
		rval &= ~(0x7fffffff);
		sunxi_r_op(priv, writel(rval, priv->mclkreg));
		priv->mod_clk = 24000000;
	}

	/*
   dumphex32("ccmu", (char *)SUNXI_CCM_BASE, 0x100);
   dumphex32("gpio", (char *)SUNXI_PIO_BASE, 0x100);
   dumphex32("mmc", (char *)priv->reg, 0x100);
   */

}

static int mmc_config_delay(struct sunxi_mmc_priv *mmcpriv)
{
	unsigned int rval = 0;
	unsigned int spd_md, freq;
	u8 odly, sdly, dsdly = 0;

	spd_md = mmcpriv->tm4.cur_spd_md;
	freq = mmcpriv->tm4.cur_freq;

	if (mmcpriv->tm4.sdly[spd_md*MAX_CLK_FREQ_NUM+freq] != 0xFF)
		sdly = mmcpriv->tm4.sdly[spd_md*MAX_CLK_FREQ_NUM+freq];
	else
		sdly = mmcpriv->tm4.def_sdly[spd_md*MAX_CLK_FREQ_NUM+freq];

	if (mmcpriv->tm4.odly[spd_md*MAX_CLK_FREQ_NUM+freq] != 0xFF)
		odly = mmcpriv->tm4.odly[spd_md*MAX_CLK_FREQ_NUM+freq];
	else
		odly = mmcpriv->tm4.def_odly[spd_md*MAX_CLK_FREQ_NUM+freq];

	mmcpriv->tm4.cur_odly = odly;
	mmcpriv->tm4.cur_sdly = sdly;

	rval = readl(&mmcpriv->reg->drv_dl);
	rval &= (~(0x3<<16));
	rval |= (((odly&0x1)<<16) | ((odly&0x1)<<17));
	sunxi_r_op(mmcpriv, writel(rval, &mmcpriv->reg->drv_dl));

	rval = readl(&mmcpriv->reg->samp_dl);
	rval &= (~SDXC_CfgDly);
	rval |= ((sdly&SDXC_CfgDly) | SDXC_EnableDly);
	writel(rval, &mmcpriv->reg->samp_dl);

	if (spd_md == HS400) {
		if (mmcpriv->tm4.dsdly[freq] != 0xFF)
			dsdly = mmcpriv->tm4.dsdly[freq];
		else
			dsdly = mmcpriv->tm4.def_dsdly[freq];
		mmcpriv->tm4.cur_dsdly = dsdly;

		rval = readl(&mmcpriv->reg->ds_dl);
		rval &= (~SDXC_CfgDly);
		rval |= ((dsdly&SDXC_CfgDly) | SDXC_EnableDly);
#ifdef FPGA_PLATFORM
		rval &= (~0x7);
#endif
		writel(rval, &mmcpriv->reg->ds_dl);
	}
#if defined(CONFIG_MACH_SUN8IW15) || defined(CONFIG_MACH_SUN8IW16)
	rval = readl(&mmcpriv->reg->sfc);
	rval |= 0x1;
	writel(rval, &mmcpriv->reg->sfc);
	MMCDBG("sfc 0x%x\n", readl(&mmcpriv->reg->sfc));
#endif
	MMCDBG("%s: spd_md:%d, freq:%d, odly: %d; sdly: %d; dsdly: %d\n", __FUNCTION__, spd_md, freq, odly, sdly, dsdly);

#if defined(CONFIG_MACH_SUN55IW6)
	/* the platform need set SKEW, because the sample point is offset from the normal interval */
	if (mmcpriv->mmc_no == 2) {
		writel(0x8f, &mmcpriv->reg->skew_dat0_dl);
		writel(0x8f, &mmcpriv->reg->skew_dat1_dl);
		writel(0x8f, &mmcpriv->reg->skew_dat2_dl);
		writel(0x8f, &mmcpriv->reg->skew_dat3_dl);
		writel(0x8f, &mmcpriv->reg->skew_dat4_dl);
		writel(0x8f, &mmcpriv->reg->skew_dat5_dl);
		writel(0x8f, &mmcpriv->reg->skew_dat6_dl);
		writel(0x8f, &mmcpriv->reg->skew_dat7_dl);
		writel(0x8f, &mmcpriv->reg->skew_dat7_dl);
		writel(0xc, &mmcpriv->reg->skew_ctrl);
	}
#endif
	return 0;
}

static int mmc_set_mod_clk(struct sunxi_mmc_priv *priv, unsigned int hz)
{
	unsigned int mod_hz, freq_id;
	struct mmc *mmc = priv->mmc;
	u32 val = 0;
	mod_hz = 0;
	int mmc_speed_mode = mmc_speedmode_convert(mmc->selected_mode);

	/*
	 * The MMC clock has an extra /2 post-divider when operating in the new
	 * mode.
	 */
	if ((mmc_speed_mode == HSDDR52_DDR50) && (mmc->bus_width == 8))
		mod_hz = hz * 4;/* 4xclk: DDR8(HS) */
	else
		mod_hz = hz * 2;/* 2xclk: SDR 1/4/8; DDR4(HS); DDR8(HS400) */

#if defined(CONFIG_MACH_SUN55IW6)
	unsigned int pll, pll_hz, div, m, n;

	if (mod_hz <= 24000000) {
		pll = CCM_MMC_CTRL_OSCM24;
		pll_hz = 24000000;
	} else {
		sunxi_mmc_get_src_clk_calc(priv->mmc_no, mod_hz, &pll, &pll_hz);
	}

	MMCDBG("pll config :%d : %d\n", pll, pll_hz);

	div = pll_hz / mod_hz;
	if (pll_hz % mod_hz)
		div += 1;
	for (n = 1; n <= 32; n++) {
		for (m = n; m <= 32; m++) {
			if (n * m == div) {
				MMCDBG("div=%d n=%d m=%d\n", div, n, m);
				goto freq_out;
			}
		}
	}
	MMCINFO("%s: wrong clock source, div=%d n = %d m=%d\n", __func__, div, n, m);
	return -1;
freq_out:
	MMCDBG("Calculate frequency division success! div=%d n=%d m=%d\n", div, n, m);

#elif !defined(CONFIG_MACH_SUN55IW3)
	unsigned int pll, pll_hz, div, n;

	if (mod_hz <= 24000000) {
		pll = CCM_MMC_CTRL_OSCM24;
		pll_hz = CCM_MMC_FREQ_OSCM24;
	} else {
		pll = CCM_MMC_CTRL_600M;
		pll_hz = CCM_MMC_FREQ_600M;
	}

	MMCDBG("pll config pll:%x pll_hz:%d mod_hz:%d\n", pll, pll_hz, mod_hz);

	div = pll_hz / mod_hz;
	if (pll_hz % mod_hz)
		div++;

	n = 0;
	while (div > 16) {
		n++;
		div = (div + 1) / 2;
	}

	if (n > 3) {
		MMCINFO("mmc %u error cannot set clock to %u\n", priv->mmc_no,
		       hz);
		return -1;
	}
#else
#ifdef CONFIG_AW_CLK
	int err = 0;
	u32 rate = 0;
	u32 rate_2 = 0;
	u32 retry_time = 0;
	struct clk *mclk = priv->cfg.clk_mmc;
	struct clk *sclk;
	struct clk *sclk_2;
	u32 src_clk = 0;

	/* hosc */
	sclk = clk_get(NULL, priv->cfg.pll0);
	if (IS_ERR_OR_NULL(sclk)) {
		MMCINFO("Error to get source clock %s\n", priv->cfg.pll0);
		return -1;
	}

	src_clk = clk_get_rate(sclk);
	if (mod_hz > src_clk) {
		clk_put(sclk);
		sclk = clk_get(NULL, priv->cfg.pll1);
	}
	if (IS_ERR_OR_NULL(sclk)) {
		MMCINFO("Error to get source clock %s\n", priv->cfg.pll1);
		return -1;
	}

clk_set_retry:
	err = clk_set_parent(mclk, sclk);
	if (err) {
		MMCINFO("set parent failed\n");
		clk_put(sclk);
		return -1;
	}

	rate = clk_round_rate(mclk, mod_hz);

	MMCDBG("get round rate %d\n", rate);

	if ((rate != mod_hz) && (mod_hz > src_clk) && (retry_time == 0)) {
		sclk_2 = clk_get(NULL, priv->cfg.pll2);
		if (IS_ERR_OR_NULL(sclk_2)) {
			MMCINFO("Error to get source clock pll_periph_another\n");
		} else {
			err = clk_set_parent(mclk, sclk_2);
			if (err) {
				MMCINFO("%s: set parent failed\n", __func__);
				clk_put(sclk_2);
				retry_time++;
				goto clk_set_retry;
			}

			rate_2 = clk_round_rate(mclk, mod_hz);

			MMCDBG("get round rate_2 = %d\n", rate_2);

			if (abs(mod_hz - rate_2) > abs(mod_hz - rate)) {
				MMCDBG("another SourceClk is worse\n");
				clk_put(sclk_2);
				retry_time++;
				goto clk_set_retry;
			} else {
				MMCDBG("another SourceClk is better and choose it\n");
				clk_put(sclk);
				sclk = sclk_2;
				rate = rate_2;
			}
		}
	}

	err = clk_disable(mclk);
	if (err) {
		MMCINFO("disable mmc clk err\n");
		return -1;
	}

	err = clk_set_rate(mclk, rate);
	if (err) {
		MMCINFO("set mclk rate error, rate %dHz\n",
			rate);
		clk_put(sclk);
		return -1;
	}

	err = clk_prepare_enable(mclk);
	if (err) {
		MMCINFO("enable mmc clk err\n");
		return -1;
	}

	src_clk = clk_get_rate(sclk);
	clk_put(sclk);

	MMCDBG("set round clock %d, soure clk is %d, mod_hz is %d\n", rate, src_clk, mod_hz);
#else
	MMCINFO("%s: need ccu config open, set clk = %d\n", __func__, mod_hz);
	return -1;
#endif
#endif
	freq_id = CLK_50M;
	/* determine delays */
	if (hz <= 400000)
		freq_id = CLK_400K;
	else if (hz <= 25000000)
		freq_id = CLK_25M;
	else if (hz <= 52000000)
		freq_id = CLK_50M;
	else if (hz <= 100000000)
		freq_id = CLK_100M;
	else if (hz <= 150000000)
		freq_id = CLK_150M;
	else if (hz <= 200000000)
		freq_id = CLK_200M;
	else
		/* hz > 52000000 */
		freq_id = CLK_50M;

	MMCDBG("freq_id:%d\n", freq_id);

/* used for some host which NTSR default use 2x mode */
	if (priv->version >= 0x50300) {
		val = readl(&priv->reg->ntsr);
		val &= ~SUNXI_MMC_NTSR_MODE_SEL_NEW;
		writel(val, &priv->reg->ntsr);
		MMCDBG("Clear NTSR bit 31 to shutdown 2x mode!\n");
	}

#ifdef FPGA_PLATFORM
	if (mod_hz > (400000 * 2)) {
		sunxi_r_op(priv, writel(CCM_MMC_CTRL_ENABLE,  priv->mclkreg));
	} else {
#if !defined CONFIG_MACH_SUN55IW3
		sunxi_r_op(priv, writel(pll | CCM_MMC_CTRL_N(n) |
			CCM_MMC_CTRL_M(div), priv->mclkreg));
#endif
	}
	if (hz <= 400000) {
		sunxi_r_op(priv, writel(readl(&priv->reg->drv_dl) & ~(0x1 << 7), &priv->reg->drv_dl));
	} else {
		sunxi_r_op(priv, writel(readl(&priv->reg->drv_dl) | (0x1 << 7), &priv->reg->drv_dl));
	}

#else
#if defined(CONFIG_MACH_SUN55IW6)
		sunxi_r_op(priv, writel(pll | ((n-1) << 8) |
			(m - 1), priv->mclkreg));
#elif !defined(CONFIG_MACH_SUN55IW3)
		sunxi_r_op(priv, writel(pll | CCM_MMC_CTRL_N(n) |
			CCM_MMC_CTRL_M(div), priv->mclkreg));
#endif
#endif
	val = readl(&priv->reg->clkcr);
	val &= ~0xff;
	if (mmc_speed_mode == HSDDR52_DDR50 && (mmc->bus_width == 8))
		val |= 0x1;
	writel(val, &priv->reg->clkcr);

	priv->tm4.cur_spd_md = mmc_speed_mode;
	priv->tm4.cur_freq = freq_id;

	mmc_config_delay(priv);

	debug("mclk reg***%x\n", readl(priv->mclkreg));
	debug("clkcr reg***%x\n", readl(&priv->reg->clkcr));
#if !defined CONFIG_MACH_SUN55IW3
	debug("mmc %u set mod-clk req %u parent %u n %u m %u rate %u\n",
	      priv->mmc_no, mod_hz, pll_hz, 1u << n, div, pll_hz / (1u << n) / div);
#endif
	return 0;
}

static void mmc_ddr_mode_onoff(struct sunxi_mmc_priv *priv, int on)
{
	u32 rval = 0;

	rval = readl(&priv->reg->gctrl);
	rval &= (~(1U << 10));

	if (on) {
		rval |= (1U << 10);
		sunxi_r_op(priv, writel(rval, &priv->reg->gctrl));
		MMCDBG("set %d rgctrl 0x%x to enable ddr mode\n",
				priv->mmc_no, readl(&priv->reg->gctrl));
	} else {
		sunxi_r_op(priv, writel(rval, &priv->reg->gctrl));
		MMCDBG("set %d rgctrl 0x%x to disable ddr mode\n",
				priv->mmc_no, readl(&priv->reg->gctrl));
	}
}

static void mmc_hs400_mode_onoff(struct sunxi_mmc_priv *priv, int on)
{
	u32 rval = 0;

	rval = readl(&priv->reg->dsbd);
	rval &= (~(1U << 31));

	if (on) {
		rval |= (1U << 31);
		writel(rval, &priv->reg->dsbd);
		rval = readl(&priv->reg->csdc);
		rval &= ~0xF;
		rval |= 0x6;
		writel(rval, &priv->reg->csdc);
		MMCDBG("set %d dsbd 0x%x to enable hs400 mode\n",
				priv->mmc_no, readl(&priv->reg->dsbd));
	} else {
		writel(rval, &priv->reg->dsbd);
		rval = readl(&priv->reg->csdc);
		rval &= ~0xF;
		rval |= 0x3;
		writel(rval, &priv->reg->csdc);
		MMCDBG("set %d dsbd 0x%x to disable hs400 mode\n",
				priv->mmc_no, readl(&priv->reg->dsbd));
	}
}

static void sunxi_mmc_set_speed_mode(struct sunxi_mmc_priv *priv,
		struct mmc *mmc)
{
	int mmc_speed_mode = mmc_speedmode_convert(mmc->selected_mode);
	/* set speed mode */
	if (mmc_speed_mode == HSDDR52_DDR50) {
		mmc_ddr_mode_onoff(priv, 1);
		mmc_hs400_mode_onoff(priv, 0);
	} else if (mmc_speed_mode == HS400) {
		mmc_ddr_mode_onoff(priv, 0);
		mmc_hs400_mode_onoff(priv, 1);
	} else {
		mmc_ddr_mode_onoff(priv, 0);
		mmc_hs400_mode_onoff(priv, 0);
	}
}

extern int mmc_update_clk(struct sunxi_mmc_priv *priv);
static int mmc_calibrate_delay_unit(struct sunxi_mmc_priv *mmchost)
{
 #ifndef FPGA_PLATFORM
	struct sunxi_mmc *reg = mmchost->reg;
	unsigned rval = 0;
	unsigned clk[4] = {50*1000*1000, 100*1000*1000, 150*1000*1000, 200*1000*1000};
	/*ps, module clk is 2xclk at init phase.*/
	unsigned period[4] = {10*1000, 5*1000, 3333, 2500};
	unsigned result = 0;
	int i = 0;

	if (mmchost->tm4.dsdly_unit_ps != 0) {
		MMCDBG("%s: don't need calibrate delay unit\n", __FUNCTION__);
		return 0;
	}
	MMCDBG("start %s, don't access device...\n", __FUNCTION__);

	for (i = 3; i < 4; i++) {
		MMCINFO("%d MHz...\n", clk[i]/1000/1000);
		/* close card clock */
		rval = readl(&reg->clkcr);
		rval &= ~(1 << 16);
		writel(rval, &reg->clkcr);
		if (mmc_update_clk(mmchost))
			return -1;

		/* set card clock to 100MHz */
		if ((mmchost->timing_mode == SUNXI_MMC_TIMING_MODE_1)
			|| (mmchost->timing_mode == SUNXI_MMC_TIMING_MODE_3)
			|| (mmchost->timing_mode == SUNXI_MMC_TIMING_MODE_4))
			mmc_set_mod_clk(mmchost, clk[i]);
		else {
			MMCINFO("%s: mmc %d wrong timing mode: 0x%x\n",
				__FUNCTION__, mmchost->mmc_no, mmchost->timing_mode);
			return -1;
		}

		/* start carlibrate delay unit */
		writel(0xA0, &reg->samp_dl);
		writel(0x0, &reg->samp_dl);
		rval = SDXC_StartCal;
		writel(rval, &reg->samp_dl);
		writel(0x0, &reg->samp_dl);
		while (!(readl(&reg->samp_dl) & SDXC_CalDone)) {
			;
		}

		if (mmchost->mmc_no == 2) {
			writel(0xA0, &reg->ds_dl);
			writel(0x0, &reg->ds_dl);
			rval = SDXC_StartCal;
			writel(rval, &reg->ds_dl);
			writel(0x0, &reg->ds_dl);
			while (!(readl(&reg->ds_dl) & SDXC_CalDone)) {
				;
			}
		}

		/* update result */
		rval = readl(&reg->samp_dl);
		result = (rval & SDXC_CalDly) >> 8;
		MMCDBG("samp_dl result: 0x%x\n", result);
		if (result) {
			rval = period[i] / result;
			mmchost->tm3.sdly_unit_ps = rval;
			mmchost->tm3.dly_calibrate_done = 1;
			mmchost->tm4.sdly_unit_ps = rval;
			MMCINFO("sample: %d - %d(ps)\n", result, mmchost->tm3.sdly_unit_ps);
		} else {
			MMCINFO("%s: cal sample delay fail\n", __FUNCTION__);
		}
		if (mmchost->mmc_no == 2) {
			rval = readl(&reg->ds_dl);
			result = (rval & SDXC_CalDly) >> 8;
			MMCDBG("ds_dl result: 0x%x\n", result);
			if (result) {
				rval = period[i] / result;
				mmchost->tm4.dsdly_unit_ps = rval;
				mmchost->tm4.dly_calibrate_done = 1;
				MMCINFO("ds: %d - %d(ps)\n", result, mmchost->tm4.dsdly_unit_ps);
			} else {
				MMCINFO("%s: cal data strobe delay fail\n", __FUNCTION__);
			}
		}
	}

#endif	/* FPGA_PLATFORM */
	return 0;
}

static void sunxi_mmc_core_init(struct mmc *mmc)
{
	struct sunxi_mmc_priv *priv = mmc->priv;

	/* Reset controller */
	writel(SUNXI_MMC_GCTRL_RESET, &priv->reg->gctrl);
	udelay(1000);
	/* release eMMC reset signal */
	writel(1, &priv->reg->hwrst);
	writel(0, &priv->reg->hwrst);
	udelay(1000);
	writel(1, &priv->reg->hwrst);
	udelay(1000);
	/* Set Data & Response Timeout Value */
	writel((SMC_DATA_TIMEOUT<<8)|SMC_RESP_TIMEOUT, &priv->reg->timeout);

	writel((512<<16)|(1U<<2)|(1U<<0), &priv->reg->thldc);
	writel(3, &priv->reg->csdc);
	writel(0xdeb, &priv->reg->dbgc);
	mmc_calibrate_delay_unit(priv);
}

void sunxi_mmc_host_tm4_init(struct sunxi_mmc_priv *priv)
{
	struct sunxi_mmc_priv *mmcpriv = priv;
	struct mmc_config *cfg = mmcpriv->cfg;

	cfg->f_max = 200000000; /* 200M */

	mmcpriv->dma_tl = SUNXI_DMA_TL_TM4_V4P5X;
	mmcpriv->timing_mode = SUNXI_MMC_TIMING_MODE_4;
	mmcpriv->mmc_init_default_timing_para = mmc_init_default_timing_para;
	mmcpriv->mmc_set_mod_clk = mmc_set_mod_clk;
	mmcpriv->sunxi_mmc_set_speed_mode = sunxi_mmc_set_speed_mode;
	mmcpriv->sunxi_mmc_core_init = sunxi_mmc_core_init;
	mmcpriv->sunxi_mmc_clk_io_onoff = sunxi_mmc_clk_io_onoff;
}
