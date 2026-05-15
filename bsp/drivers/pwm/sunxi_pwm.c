// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2017-2018 Vasily Khoruzhick <anarsoul@gmail.com>
 */

#include <common.h>
#include <div64.h>
#include <dm.h>
#include <log.h>
#include <pwm.h>
#include <regmap.h>
#include <syscon.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/arch/pwm.h>
#include <power/regulator.h>
#include <clk.h>
#include <reset.h>

DECLARE_GLOBAL_DATA_PTR;

#define OSC_24MHZ 24000000

#define sunxi_pwm_debug 0
#undef  sunxi_pwm_debug

#ifdef sunxi_pwm_debug
	#define pwm_debug(fmt, args...)	printf(fmt, ##args)
#else
	#define pwm_debug(fmt, args...)
#endif

#define PWM_PIN_STATE_ACTIVE "active"
#define PWM_PIN_STATE_SLEEP "sleep"

#define PWM_CCU_OFFSET		0x7ac
#define PWM_RCM_OFFSET		0x13c

#define SETMASK(width, shift)   ((width ? ((-1U) >> (32 - width)) : 0)  << (shift))
#define CLRMASK(width, shift)   (~(SETMASK(width, shift)))
#define GET_BITS(shift, width, reg)     \
	    (((reg) & SETMASK(width, shift)) >> (shift))
#define SET_BITS(shift, width, reg, val) \
	    (((reg) & CLRMASK(width, shift)) | (val << (shift)))

#define CLK_CCU_SUPPORT
uint clk_count;
#if !defined CONFIG_ARCH_SUN8IW20 && !defined CONFIG_ARCH_SUN20IW1
#define CLK_RCM_SUPPORT
uint sclk_count;
#endif

struct sunxi_pwm_hw_data {
	u32 pwm_reg_uniform_offset;
};

struct sunxi_pwm_priv {
	u32 pwm_base;
	bool invert;
	u32 prescaler;
	struct reset_ctl reset;
	struct clk clk;
	const struct sunxi_pwm_hw_data *data;
};

static inline u32 sunxi_pwm_readl(u32 base, u32 offset)
{
	unsigned int value;

	value = readl((void *)(unsigned long)(base + offset));

	return value;
}

static inline u32 sunxi_pwm_writel(u32 base, u32 offset, u32 value)
{
	writel(value, (void *)(unsigned long)(base + offset));

	return 0;
}

static int sunxi_pwm_set_invert(struct udevice *dev, uint channel,
				bool polarity)
{
	uint sel;
	uint temp;

	unsigned int reg_offset, reg_shift;
	struct sunxi_pwm_priv *priv = dev_get_plat(dev);
	u32 reg_base = priv->pwm_base;

	sel = channel;
	reg_offset = PWM_PCR_BASE + sel * priv->data->pwm_reg_uniform_offset;
	reg_shift = PWM_ACT_STA_SHIFT;

	temp = sunxi_pwm_readl(reg_base, reg_offset);
	temp = SET_BITS(reg_shift, 1, temp, polarity);

	sunxi_pwm_writel(reg_base, reg_offset, temp);

	return 0;
}

static int sunxi_pwm_set_config(struct udevice *dev, uint channel,
				uint period_ns, uint duty_ns)
{
#if defined(CONFIG_MACH_SUN8IW20) || defined(CONFIG_MACH_SUN20IW1) || defined(CONFIG_MACH_SUN55IW3) || defined(CONFIG_MACH_SUN55IW6)
	uint pre_scal[][2] = {
		{0, 1},
		{1, 2},
		{2, 4},
		{3, 8},
		{4, 16},
		{5, 32},
		{6, 64},
		{7, 128},
		{8, 256},
	};
#else
	uint pre_scal[11][2] = {{15, 1}, {0, 120}, {1, 180}, {2, 240}, {3, 360}, {4, 480},
		{8, 12000}, {9, 24000}, {10, 36000}, {11, 48000}, {12, 72000} };
#endif
	uint freq;
	uint pre_scal_id = 0;
	uint entire_cycles = 256;
	uint active_cycles = 192;
	uint entire_cycles_max = 65536;
	uint temp;
	uint sel = channel;
	unsigned int reg_offset, reg_shift, reg_width;

	struct sunxi_pwm_priv *priv = dev_get_plat(dev);

	u32 reg_base = priv->pwm_base;
	u32 uniform_offset = priv->data->pwm_reg_uniform_offset;
	reg_offset = PCGR;
	reg_shift = sel + PWM_CLK_BYPASS_SHIFT;

	if (period_ns < 42) {
		/* if freq lt 24M, then direct output 24M clock */
		temp = sunxi_pwm_readl(reg_base, reg_offset);
		//temp |= (0x1 << reg_shift);//pwm bypass

		temp = SET_BITS(reg_shift, 1, temp, 1);
		sunxi_pwm_writel(reg_base, reg_offset, temp);

		return 0;
	}

	/* disable bypass function */
	temp = sunxi_pwm_readl(reg_base, reg_offset);
	temp = SET_BITS(reg_shift, 1, temp, 0);
	sunxi_pwm_writel(reg_base, reg_offset, temp);

	if (period_ns < 10667)
		freq = 93747;
	else if (period_ns > 1000000000)
		freq = 1;
	else
		freq = 1000000000 / period_ns;

	entire_cycles = 24000000 / freq / pre_scal[pre_scal_id][1];

	while (entire_cycles > entire_cycles_max) {
		pre_scal_id++;

		if (pre_scal_id > (ARRAY_SIZE(pre_scal) - 1))
			break;

		entire_cycles = 24000000 / freq / pre_scal[pre_scal_id][1];
	}

	if (period_ns < 5 * 100 * 1000)
		active_cycles = (duty_ns * entire_cycles + (period_ns / 2)) / period_ns;
	else if (period_ns >= 5 * 100 * 1000 && period_ns < 6553500)
		active_cycles = ((duty_ns / 100) * entire_cycles + (period_ns / 2 / 100)) / (period_ns / 100);
	else
		active_cycles = ((duty_ns / 10000) * entire_cycles + (period_ns / 2 / 10000)) / (period_ns / 10000);

	reg_offset = PWM_PCR_BASE + uniform_offset * sel;
	reg_shift = PWM_PRESCAL_SHIFT;
	reg_width = PWM_PRESCAL_WIDTH;
	temp = sunxi_pwm_readl(reg_base, reg_offset);

	temp = SET_BITS(reg_shift, reg_width, temp, (pre_scal[pre_scal_id][0]));

	pwm_debug("%s: reg_shift = %d, reg_width = %d, prescale temp = %x, pres=%d\n",
		  __func__, reg_shift, reg_width, temp, pre_scal[pre_scal_id][0]);
	sunxi_pwm_writel(reg_base, reg_offset, temp);

	/*config active cycles*/
	reg_offset = PWM_PPR_BASE + uniform_offset * sel;
	reg_shift = PWM_ACT_CYCLES_SHIFT;
	reg_width = PWM_ACT_CYCLES_WIDTH;
	temp = sunxi_pwm_readl(reg_base, reg_offset);
	temp = SET_BITS(reg_shift, reg_width, temp, active_cycles);
	sunxi_pwm_writel(reg_base, reg_offset, temp);

	/*config period cycles*/
	reg_offset = PWM_PPR_BASE + uniform_offset * sel;
	reg_shift = PWM_PERIOD_CYCLES_SHIFT;
	reg_width = PWM_PERIOD_CYCLES_WIDTH;
	temp = sunxi_pwm_readl(reg_base, reg_offset);
	temp = SET_BITS(reg_shift, reg_width, temp, (entire_cycles - 1));
	sunxi_pwm_writel(reg_base, reg_offset, temp);

	pwm_debug("PWM %s: duty_ns=%d, period_ns=%d, freq=%d, per_scal=%d, period_reg=0x%x\n",
		  __func__, duty_ns, period_ns, freq, pre_scal_id, temp);

	return 0;
}

static int sunxi_pwm_set_enable(struct udevice *dev, uint channel, bool enable)
{
	int value;
	unsigned int reg_offset, reg_shift;
	struct sunxi_pwm_priv *priv = dev_get_plat(dev);
	u32 reg_base = priv->pwm_base;

	/* enable clk for pwm controller. */
	reg_offset = PCGR;
	reg_shift = channel;
	value = sunxi_pwm_readl(reg_base, reg_offset);
#ifdef	CONFIG_SUNXI_EPHY_AC300
	value = SET_BITS(reg_shift + PWM_CLK_BYPASS_SHIFT, 1, value, 1);
#endif
	value = SET_BITS(reg_shift, 1, value, 1);
	sunxi_pwm_writel(reg_base, reg_offset, value);

	/* enable pwm controller. */
	reg_offset = PWM_PER;
	reg_shift = channel;
	value = sunxi_pwm_readl(reg_base, reg_offset);
	value = SET_BITS(reg_shift, 1, value, 1);
	sunxi_pwm_writel(reg_base, reg_offset, value);
	value = sunxi_pwm_readl(reg_base, reg_offset);

	return 0;
}

static int sunxi_pwm_of_to_plat(struct udevice *dev)
{
	struct sunxi_pwm_priv *priv = dev_get_plat(dev);

	priv->pwm_base = dev_read_addr(dev);

	return 0;
}

static int sunxi_pwm_probe(struct udevice *dev)
{
	struct sunxi_pwm_priv *priv = dev_get_plat(dev);

	int ret;

	ret = reset_get_by_name(dev, "rst", &priv->reset);
	if (ret)
		return ret;

	reset_assert(&priv->reset);
	udelay(2);
	reset_deassert(&priv->reset);

	ret = clk_get_by_name(dev, "gate", &priv->clk);
	if (ret)
		return ret;

	ret = clk_enable(&priv->clk);
	if (ret)
		return ret;

	return 0;
}

static const struct pwm_ops sunxi_pwm_ops = {
	.set_invert	= sunxi_pwm_set_invert,
	.set_config	= sunxi_pwm_set_config,
	.set_enable	= sunxi_pwm_set_enable,
};


static const struct sunxi_pwm_hw_data sunxi_pwm_data = {
	.pwm_reg_uniform_offset = 0x40,
};

static const struct udevice_id sunxi_pwm_ids[] = {
	{ .compatible = "allwinner,sunxi-pwm", .data = (ulong)&sunxi_pwm_data },
	{ }
};

U_BOOT_DRIVER(sunxi_pwm) = {
	.name	= "sunxi_pwm",
	.id	= UCLASS_PWM,
	.of_match = sunxi_pwm_ids,
	.ops	= &sunxi_pwm_ops,
	.of_to_plat	= sunxi_pwm_of_to_plat,
	.probe		= sunxi_pwm_probe,
	.plat_auto	= sizeof(struct sunxi_pwm_priv),
};
