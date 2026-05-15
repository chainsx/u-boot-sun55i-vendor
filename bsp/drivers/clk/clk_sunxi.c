// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018 Amarula Solutions.
 * Author: Jagan Teki <jagan@amarulasolutions.com>
 */

#include <common.h>
#include <clk-uclass.h>
#include <dm.h>
#include <errno.h>
#include <log.h>
#include <reset.h>
#include <asm/io.h>
#include <dm/device-internal.h>
#include <dm/device_compat.h>
#include <linux/bitops.h>
#include <linux/clk-provider.h>
#include <linux/log2.h>
#include <div64.h>
#include <clk/sunxi.h>
#include "sunxi-clk.h"

extern U_BOOT_DRIVER(sunxi_reset);

struct _clk_nkmp {
	unsigned long	n, min_n, max_n;
	unsigned long	m, min_m, max_m;
	unsigned long	p, min_p, max_p;
};

int sunxi_clk_init(struct udevice *dev,
		   const struct sunxi_clock_match_data *data)
{
	int i;
	u8 *cpt;
	struct sunximp_rcc_priv *priv = dev_get_priv(dev);
	fdt_addr_t base = dev_read_addr(dev);
	const struct clk_sunxi_clock_data *clock_data = data->clock_data;

	if (base == FDT_ADDR_T_NONE) {
		return -EINVAL;
	}

	priv->base = (void __iomem *)(uintptr_t)base;

	debug("Core:  %s %d  dev_name: %s base: 0x%p\n", __func__, __LINE__, dev->name, priv->base);

	/* allocate the counter of user for internal RCC gates, common for several user */
	cpt = kzalloc(clock_data->num_gates, GFP_KERNEL);
	if (!cpt)
		return -ENOMEM;

	debug("Core:  %s %d num_clk: %d num_gates: %d\n", __func__, __LINE__, data->num_clocks, clock_data->num_gates);

	priv->gate_cpt = cpt;

	priv->data = clock_data;

	for (i = 0; i < data->num_clocks; i++) {
		const struct clock_config *cfg = &data->tab_clocks[i];
		struct clk *clk = ERR_PTR(-ENOENT);

		if (cfg->setup) {
			clk = cfg->setup(dev, cfg);
			clk->id = cfg->id;
		} else {
			dev_err(dev, "failed to register clock %s\n", cfg->name);
			return -ENOENT;
		}
	}

	return 0;
}

int sunxi_clk_bind(struct udevice *dev)
{
	/* Reuse the platform data for the reset driver. */
	struct ccu_plat *plat = dev_get_plat(dev);

	plat = malloc(sizeof(*plat));

	plat->base = dev_read_addr_ptr(dev);

	if (!plat->base) {
		free(plat);
		return -ENOMEM;
	}

	plat->desc = (const struct ccu_desc *)dev_get_driver_data(dev);
	debug("Core:  %s %d dev_name: %s base: 0x%p num: %d \n", __func__, __LINE__, dev->name, plat->base, plat->desc->num_resets);
	if (!plat->desc)
		return -EINVAL;


	debug("Core:  %s %d dev_name: %s \n", __func__, __LINE__, dev->name);

	return device_bind(dev, DM_DRIVER_REF(sunxi_reset), "reset",
			   plat, dev_ofnode(dev), NULL);
}

int sunxi_clk_of_to_plat(struct udevice *dev)
{
	return 0;
}

/* Used by platform driver, example: clk_sun8iw21.c */
const struct clk_ops sunxi_ccu_ops = {
	.enable = ccf_clk_enable,
	.disable = ccf_clk_disable,
	.get_rate = ccf_clk_get_rate,
	.set_rate = ccf_clk_set_rate,
	.set_parent = ccf_clk_set_parent,
};

static void clk_sunxi_gate_set_state(void __iomem *base,
				     const struct clk_sunxi_clock_data *data,
				     u8 *cpt, u16 gate_id, int enable)
{
	const struct sunxi_gate_cfg *gate_cfg = &data->gates[gate_id];
	void __iomem *addr = base + gate_cfg->reg_off;

	if (enable) {
		if (cpt[gate_id]++ > 0)
			return;
		writel(readl(addr) | (gate_cfg->bit_idx), addr);
	} else {
		if (--cpt[gate_id] > 0)
			return;

		writel(readl(addr) & ~(gate_cfg->bit_idx), addr);
	}
}

static int clk_sunxi_gate_enable(struct clk *clk)
{
	struct clk_sunxi_gate *sunxi_gate = to_clk_sunxi_gate(clk);
	struct sunximp_rcc_priv *priv = sunxi_gate->priv;

	clk_sunxi_gate_set_state(priv->base, priv->data, priv->gate_cpt,
				 sunxi_gate->gate_id, 1);

	return 0;
}

static int clk_sunxi_gate_disable(struct clk *clk)
{
	struct clk_sunxi_gate *sunxi_gate = to_clk_sunxi_gate(clk);
	struct sunximp_rcc_priv *priv = sunxi_gate->priv;

	clk_sunxi_gate_set_state(priv->base, priv->data, priv->gate_cpt,
				 sunxi_gate->gate_id, 0);

	return 0;
}

static const struct clk_ops clk_sunxi_gate_ops = {
	.enable = clk_sunxi_gate_enable,
	.disable = clk_sunxi_gate_disable,
	.get_rate = clk_generic_get_rate,
};

#define UBOOT_DM_CLK_SUNXI_GATE "clk_sunxi_gate"

U_BOOT_DRIVER(clk_sunxi_gate) = {
	.name	= UBOOT_DM_CLK_SUNXI_GATE,
	.id	= UCLASS_CLK,
	.ops	= &clk_sunxi_gate_ops,
};

static void _clk_div_find_best(unsigned long parent_rate, unsigned long rate, unsigned int max_m, unsigned int *m)
{
	unsigned long best_rate = 0;
	unsigned int best_m = 0;
	unsigned int _m;

	for (_m = 1; _m <= max_m; _m++) {
		unsigned long tmp_rate = parent_rate /  _m;
		debug("Core:  %s %d m: %d\n", __func__, __LINE__, best_m);

		if (tmp_rate > rate)
			continue;

		if ((rate - tmp_rate) < (rate - best_rate)) {
			best_rate = tmp_rate;
			best_m = _m;
		}
	}
	debug("Core:  %s %d m: %d\n", __func__, __LINE__, best_m);
	*m = best_m;
}

static ulong clk_sunxi_div_set_rate(struct clk *clk, ulong _rate)
{
	struct clk_divider *divider = to_clk_divider(clk);
	unsigned long parent_rate = clk_get_parent_rate(clk);
	unsigned long cur_rate = clk_get_rate(clk);
	unsigned int max_m;
	unsigned int m;
	u32 reg;

	if (cur_rate == _rate)
		return 0;

	max_m = 1 << divider->width;
	_clk_div_find_best(parent_rate, _rate, max_m, &m);

	reg = readl(divider->reg);
	reg &= ~GENMASK(divider->width + divider->shift - 1, divider->shift);
	reg |= (m - 1) << divider->shift;

	debug("Core:  %s %d clk_name: %s reg: 0x%x addr: 0x%p\n", __func__, __LINE__, clk->dev->name, reg, divider->reg);

	writel(reg, divider->reg);

	return 0;
}

static ulong clk_sunxi_div_get_rate(struct clk *clk)
{
	struct clk_divider *divider = to_clk_divider(clk);
	unsigned long parent_rate = clk_get_parent_rate(clk);
	u32 reg;
	unsigned long val;

	reg = readl(divider->reg);
	val = reg >> divider->shift;
	val &= (1 << divider->width) - 1;

	val = divider_recalc_rate(clk, parent_rate, val, divider->table,
				  divider->flags, divider->width);

	debug("Core:  %s %d clk_name: %s val: %ld\n", __func__, __LINE__, clk->dev->name, val);

	return val;
}
static const struct clk_ops clk_sunxi_divider_ops = {
	.set_rate = clk_sunxi_div_set_rate,
	.get_rate = clk_sunxi_div_get_rate,
};

struct clk *clk_sunxi_gate_register(struct udevice *dev,
				    const struct clock_config *cfg)
{
	struct sunximp_rcc_priv *priv = dev_get_priv(dev);
	struct sunxi_clk_gate_cfg *clk_cfg = cfg->clock_cfg;
	struct clk_sunxi_gate *sunxi_gate;
	struct clk *clk;
	int ret;

	sunxi_gate = kzalloc(sizeof(*sunxi_gate), GFP_KERNEL);
	if (!sunxi_gate)
		return ERR_PTR(-ENOMEM);

	sunxi_gate->priv = priv;
	sunxi_gate->gate_id = clk_cfg->gate_id;

	clk = &sunxi_gate->clk;
	clk->flags = cfg->flags;

	debug("Core:  %s %d clk_name: %s\n", __func__, __LINE__, cfg->name);
	ret = clk_register(clk, UBOOT_DM_CLK_SUNXI_GATE,
			   cfg->name, cfg->parent_name);
	if (ret) {
		kfree(sunxi_gate);
		return ERR_PTR(ret);
	}

	return clk;
}

static ulong clk_sunxi_nkmp_round_rate(struct clk *clk, unsigned long rate)
{
/*TODO*/
	debug("Core:  %s %d clk_name: %s\n", __func__, __LINE__, clk->dev->name);
	return rate;
}

static ulong _clk_sunxi_nkmp_calc_rate(unsigned long parent,	unsigned long n,
						unsigned long m, unsigned long p)
{
	u64 rate = parent;

	debug("Core:  %s %d rate: %lld m: %ld p: %ld n: %ld\n", __func__, __LINE__, rate, m, p, n);
	rate *= n;
	do_div(rate, m * p);

	return rate;
}

/* PLL_RATE = 24*N/M/P */
static ulong _clk_sunxi_nkmp_recalc_rate(ulong reg_val, struct clk_sunxi_divider *divider, unsigned long p_rate)
{
	unsigned long n, m, p;
	u64 rate = p_rate;

	n = reg_val >> divider->n_shift;
	n &= (1 << divider->n_width) - 1;
	n += 1;

	m = reg_val >> divider->shift;
	m &= (1 << divider->width) - 1;
	m += 1;

	p = reg_val >> divider->shift2;
	p &= (1 << divider->width2) - 1;
	p += 1;

	debug("Core:  %s %d rate: %lld m: %ld p: %ld n: %ld\n", __func__, __LINE__, rate, m, p, n);
	rate = _clk_sunxi_nkmp_calc_rate(p_rate, n, m, p);
	return rate;
}

static ulong clk_sunxi_nkmp_get_rate(struct clk *clk)
{
	struct clk_sunxi_divider *divider = to_clk_sunxi_divider(clk);
	unsigned long parent_rate = clk_get_parent_rate(clk);
	u32 reg_val;

	reg_val = readl(divider->reg);
	debug("Core:  %s %d clk_name: %s reg: 0x%x w1: %d w2: %d n: %hd\n", __func__, __LINE__, clk->dev->name, reg_val, divider->shift, divider->shift2, divider->n_shift);

	return _clk_sunxi_nkmp_recalc_rate(reg_val, divider, parent_rate);
}

static void _clk_sunxi_nkmp_find_best(struct clk_sunxi_divider *divider, unsigned long p_rate, u64 rate, struct _clk_nkmp *nkmp)
{
	u64 best_rate = 0;
	unsigned long best_n = 0, best_m = 0, best_p = 0;
	unsigned long _n,  _m, _p;

	for (_n = nkmp->min_n; _n <= nkmp->max_n; _n++) {
		for (_m = nkmp->min_m; _m <= nkmp->max_m; _m++) {
			for (_p = nkmp->min_p; _p <= nkmp->max_p; _p <<= 1) {
				u64 tmp_rate;

				tmp_rate = _clk_sunxi_nkmp_calc_rate(p_rate,
						_n, _m, _p);

				if (tmp_rate > rate)
					continue;

				if ((rate - tmp_rate) < (rate - best_rate)) {
					best_rate = tmp_rate;
					best_n = _n;
					best_m = _m;
					best_p = _p;
				}
			}
		}
	}

	nkmp->n = best_n;
	nkmp->m = best_m;
	nkmp->p = best_p;
}

static ulong clk_sunxi_nkmp_set_rate(struct clk *clk, ulong _rate)
{
/*TODO*/
	struct clk_sunxi_divider *divider = to_clk_sunxi_divider(clk);
	unsigned long parent_rate = clk_get_parent_rate(clk);
	u32 n_mask = 0, k_mask = 0, m_mask = 0, p_mask = 0;
	struct _clk_nkmp _nkmp;
	u64 rate = _rate;
	u32 reg;

	_nkmp.min_n = 1;
	_nkmp.max_n = 1 << divider->n_width;
	_nkmp.min_m = 1;
	_nkmp.max_m = 1 << divider->width;
	_nkmp.min_p = 1;
	_nkmp.max_p = 1 << divider->width2;

	_clk_sunxi_nkmp_find_best(divider, parent_rate, rate, &_nkmp);
	debug("Core:  %s %d clk_name: %s\n", __func__, __LINE__, clk->dev->name);

	/*
	 * If width is 0, GENMASK() macro may not generate expected mask (0)
	 * as it falls under undefined behaviour by C standard due to shifts
	 * which are equal or greater than width of left operand. This can
	 * be easily avoided by explicitly checking if width is 0.
	 */
	if (divider->n_width)
		n_mask = GENMASK(divider->n_width + divider->n_shift - 1,
				 divider->n_shift);
	if (divider->width)
		m_mask = GENMASK(divider->width + divider->shift - 1,
				 divider->shift);
	if (divider->width2)
		p_mask = GENMASK(divider->width2 + divider->shift2 - 1,
				 divider->shift2);

	reg = readl(divider->reg);
	reg &= ~(n_mask | k_mask | m_mask | p_mask);

	reg |= ((_nkmp.n - 1) << divider->n_shift) & n_mask;
	reg |= ((_nkmp.m - 1) << divider->shift) & m_mask;
	reg |= ((_nkmp.p - 1) << divider->shift2) & p_mask;

	writel(reg, divider->reg);
	return 0;
}

static const struct clk_ops clk_nkmp_ops = {
	.round_rate = clk_sunxi_nkmp_round_rate,
	.set_rate = clk_sunxi_nkmp_set_rate,
	.get_rate = clk_sunxi_nkmp_get_rate,
};

struct clk *
clk_sunxi_register_composite(struct udevice *dev,
			     const struct clock_config *cfg)
{
	struct sunxi_clk_composite_cfg *composite = cfg->clock_cfg;
	const char *const *parent_names;
	int num_parents;
	struct clk *clk = ERR_PTR(-ENOMEM);
	struct clk_mux *mux = NULL;
	struct clk_sunxi_gate *gate = NULL;
	struct clk_divider *div = NULL;
	struct clk *mux_clk = NULL;
	const struct clk_ops *mux_ops = NULL;
	struct clk *gate_clk = NULL;
	const struct clk_ops *gate_ops = NULL;
	struct clk *div_clk = NULL;
	const struct clk_ops *div_ops = NULL;
	struct sunximp_rcc_priv *priv = dev_get_priv(dev);
	const struct clk_sunxi_clock_data *data = priv->data;

	debug("Core:  %s %d clk_name: %s dev_name: %s \n", __func__, __LINE__, cfg->name, dev->name);
	debug("Core:  %s %d base: 0x%p \n", __func__, __LINE__, priv->base);

	if  (composite->mux_id != NO_SUNXI_MUX) {
		const struct sunxi_mux_cfg *mux_cfg;

		mux = kzalloc(sizeof(*mux), GFP_KERNEL);
		if (!mux)
			return ERR_CAST(clk);

		mux_cfg = &data->muxes[composite->mux_id];

		mux->reg = priv->base + mux_cfg->reg_off;
		mux->shift = mux_cfg->shift;
		mux->mask = BIT(mux_cfg->width) - 1;
		mux->num_parents = mux_cfg->num_parents;
		mux->flags = 0;
		mux->parent_names = mux_cfg->parent_names;

		mux_clk = &mux->clk;
		mux_ops = &clk_mux_ops;

		parent_names = mux_cfg->parent_names;
		num_parents = mux_cfg->num_parents;
	} else {
		parent_names = &cfg->parent_name;
		num_parents = 1;
	}

	if  (composite->gate_id != NO_SUNXI_GATE) {
		gate = kzalloc(sizeof(*gate), GFP_KERNEL);
		if (!gate)
			goto fail1;

		gate->priv = priv;
		gate->gate_id = composite->gate_id;

		gate_clk = &gate->clk;
		gate_ops = &clk_sunxi_gate_ops;
	}

	if  (composite->div_id != NO_SUNXI_DIV) {
		const struct sunxi_div_cfg *div_cfg;

		div = kzalloc(sizeof(*div), GFP_KERNEL);
		if (!div)
			goto fail2;

		div_cfg = &data->dividers[composite->div_id];

		div->reg = priv->base + div_cfg->reg_off;
		div->shift = div_cfg->shift;
		div->width = div_cfg->width;
		div->flags = div_cfg->div_flags;
		div->table = div_cfg->table;

		div_clk = &div->clk;
		div_ops = &clk_sunxi_divider_ops;
	}

	clk = clk_register_composite(NULL, cfg->name,
				     parent_names, num_parents,
				     mux_clk, mux_ops,
				     div_clk, div_ops,
				     gate_clk, gate_ops,
				     cfg->flags);
	if (IS_ERR(clk))
		goto fail3;

	return clk;

fail3:
	kfree(div);
fail2:
	kfree(gate);
fail1:
	kfree(mux);
	return ERR_CAST(clk);
}

struct clk *
clk_sunxi_register_composite_factor(struct udevice *dev,
			     const struct clock_config *cfg)
{
	struct sunxi_clk_composite_cfg *composite = cfg->clock_cfg;
	const char *const *parent_names;
	int num_parents;
	struct clk *clk = ERR_PTR(-ENOMEM);
	struct clk_sunxi_gate *gate = NULL;
	struct clk_sunxi_divider *div = NULL;
	struct clk *gate_clk = NULL;
	const struct clk_ops *gate_ops = NULL;
	struct clk *div_clk = NULL;
	struct clk *mux_clk = NULL;
	const struct clk_ops *mux_ops = NULL;
	const struct clk_ops *div_ops = NULL;
	struct sunximp_rcc_priv *priv = dev_get_priv(dev);
	const struct clk_sunxi_clock_data *data = priv->data;

	debug("Core:  %s %d clk_name: %s dev_name: %s \n", __func__, __LINE__, cfg->name, dev->name);
	debug("Core:  %s %d base: 0x%p \n", __func__, __LINE__, priv->base);

	parent_names = &cfg->parent_name;
	num_parents = 1;

	if  (composite->gate_id != NO_SUNXI_GATE) {
		gate = kzalloc(sizeof(*gate), GFP_KERNEL);
		if (!gate)
			return ERR_CAST(clk);

		gate->priv = priv;
		gate->gate_id = composite->gate_id;

		gate_clk = &gate->clk;
		gate_ops = &clk_sunxi_gate_ops;
	}

	if  (composite->div_id != NO_SUNXI_DIV) {
		const struct sunxi_div_cfg *div_cfg;

		div = kzalloc(sizeof(*div), GFP_KERNEL);
		if (!div)
			goto fail1;

		div_cfg = &data->dividers[composite->div_id];

		div->reg = priv->base + div_cfg->reg_off;
		div->shift = div_cfg->shift;
		div->width = div_cfg->width;
		div->shift2 = div_cfg->shift2;
		div->width2 = div_cfg->width2;

		div->n_width = div_cfg->n_width;
		div->n_shift = div_cfg->n_shift;
		div->flags = div_cfg->div_flags;
		div->table = div_cfg->table;

		div_clk = &div->clk;
		div_ops = &clk_nkmp_ops;
	}

	clk = clk_register_composite(NULL, cfg->name,
				     parent_names, num_parents,
				     mux_clk, mux_ops,
				     div_clk, div_ops,
				     gate_clk, gate_ops,
				     cfg->flags);
	if (IS_ERR(clk))
		goto fail2;

	return clk;

fail2:
	kfree(div);
fail1:
	kfree(gate);
	return ERR_CAST(clk);
}

struct clk *
clk_sunxi_register_fixed(struct udevice *dev,
			     const struct clock_config *cfg)
{
	struct sunxi_clk_composite_cfg *composite = cfg->clock_cfg;
	struct clk *clk;

	clk = clk_register_fixed_factor(NULL, cfg->name, cfg->parent_name, 0, composite->mult_id, composite->div_id);

	return clk;
}
