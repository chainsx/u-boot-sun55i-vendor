// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018 Amarula Solutions.
 * Author: Jagan Teki <jagan@amarulasolutions.com>
 */

#ifndef _CCU_SUNXI_H
#define _CCU_SUNXI_H

#include <linux/bitops.h>
#include <clk.h>

struct sunxi_clock_match_data;

/**
 * struct sunxi_mux_cfg - multiplexer configuration
 *
 * @parent_names:	array of string names for all possible parents
 * @num_parents:	number of possible parents
 * @reg_off:		register controlling multiplexer
 * @shift:		shift to multiplexer bit field
 * @width:		width of the multiplexer bit field
 * @mux_flags:		hardware-specific flags
 * @table:		array of register values corresponding to the parent
 *			index
 */
struct sunxi_mux_cfg {
	const char * const *parent_names;
	u8 num_parents;
	u32 reg_off;
	u8 shift;
	u8 width;
	u8 mux_flags;
	u32 *table;
};

#define MUX_CFG(id, src, _offset, _shift, _witdh) \
	[id] = { \
		.num_parents	= ARRAY_SIZE(src), \
		.parent_names	= (src), \
		.reg_off	= (_offset), \
		.shift		= (_shift), \
		.width		= (_witdh), \
	}
/**
 * struct sunxi_gate_cfg - gating configuration
 *
 * @reg_off:	register controlling gate
 * @bit_idx:	single bit controlling gate
 * @gate_flags:	hardware-specific flags
 * @set_clr:	0 : normal gate, 1 : has a register to clear the gate
 */
struct sunxi_gate_cfg {
	u32 reg_off;
	u32 bit_idx;
	u8 gate_flags;
};

#define GATE_CFG(id, _offset, _bit_idx) \
	[id] = { \
		.reg_off	= (_offset), \
		.bit_idx	= (_bit_idx), \
	}

/**
 * struct sunxi_div_cfg - divider configuration
 *
 * @reg_off:	register containing the divider
 * @shift:	shift to the divider bit field
 * @width:	width of the divider bit field
 * @table:	array of value/divider pairs, last entry should have div = 0
 */
struct sunxi_div_cfg {
	u32 reg_off;
	u8 shift;
	u8 width;
	u8 shift2;
	u8 width2;
	u8 n_shift;
	u8 n_width;
	u8 div_flags;
	const struct clk_div_table *table;
};

#define DIV_CFG(id, _offset, _shift, _width, _shift2, _width2, _n_shift, _n_width, _flags, _table) \
	[id] = { \
		.reg_off	= _offset, \
		.shift	= _shift, \
		.width	= _width, \
		.shift2	= _shift2, \
		.width2	= _width2, \
		.n_shift = _n_shift, \
		.n_width = _n_width, \
		.div_flags	= _flags, \
		.table	= _table, \
	}

struct sunxi_mult_cfg {
	u32 reg_off;
	u8 shift;
	u8 width;
	u8 mult_flags;
};
#define NO_SUNXI_MUX	-1
#define NO_SUNXI_DIV	-1
#define NO_SUNXI_GATE	-1
#define NO_SUNXI_MULT	-1

/**
 * struct sunxi_composite_cfg - composite configuration
 *
 * @mux:	index of a multiplexer
 * @gate:	index of a gate
 * @div:	index of a divider
 */
struct sunxi_composite_cfg {
	int mux;
	int gate;
	int div;
	int mult;
};

/**
 * struct clock_config - clock configuration
 *
 * @id:			binding id of the clock
 * @name:		clock name
 * @parent_name:	name of the clock parent
 * @flags:		framework-specific flags
 * @sec_id:		secure id (use to known if the clock is secured or not)
 * @clock_cfg:		specific clock data configuration
 * @setup:		specific call back to reister the clock (will use
 *			clock_cfg data as input)
 */
struct clock_config {
	unsigned long id;
	const char *name;
	const char *parent_name;
	unsigned long flags;
	void *clock_cfg;

	struct clk *(*setup)(struct udevice *dev,
			     const struct clock_config *cfg);
};

/**
 * struct clk_sunxi_clock_data - clock data
 *
 * @num_gates:		number of defined gates
 * @gates:		array of gate configuration
 * @muxes:		array of multiplexer configuration
 * @dividers:		array of divider configuration
 */
struct clk_sunxi_clock_data {
	unsigned int num_gates;
	const struct sunxi_gate_cfg *gates;
	const struct sunxi_mux_cfg *muxes;
	const struct sunxi_div_cfg *dividers;
	const struct sunxi_mult_cfg *mults;
};

struct sunximp_rcc_priv {
	void __iomem *base;
	u8 *gate_cpt;
	const struct clk_sunxi_clock_data *data;
};

/**
 * struct sunxi_clock_match_data - clock match data
 *
 * @num_gates:		number of clocks
 * @tab_clocks:		array of clock configuration
 * @clock_data:		definition of all gates / dividers / multiplexers
 * @check_security:	call back to check if clock is secured or not
 */
struct sunxi_clock_match_data {
	unsigned int num_clocks;
	const struct clock_config *tab_clocks;
	const struct clk_sunxi_clock_data *clock_data;
	int (*check_security)(void __iomem *base,
			      const struct clock_config *cfg);
};

int sunxi_clk_init(struct udevice *dev,
		   const struct sunxi_clock_match_data *data);

struct clk_sunxi_gate {
	struct clk clk;
	struct sunximp_rcc_priv *priv;
	int gate_id;
};

struct clk_sunxi_divider {
	struct clk      clk;
	void __iomem    *reg;
	u8              shift;
	u8              width;
	u8              shift2;
	u8              width2;
	u8              n_shift;
	u8              n_width;
	u8              flags;
	const struct clk_div_table      *table;
};

#define to_clk_sunxi_gate(_clk) container_of(_clk, struct clk_sunxi_gate, clk)
#define to_clk_sunxi_divider(_clk) container_of(_clk, struct clk_sunxi_divider, clk)
#define to_clk_sunxi_mult(_clk) container_of(_clk, struct clk_mult_cfg, clk)

struct clk *
clk_sunxi_register_composite_factor(struct udevice *dev,
			     const struct clock_config *cfg);
struct clk *
clk_sunxi_register_composite(struct udevice *dev,
			     const struct clock_config *cfg);

struct clk *
clk_sunxi_gate_register(struct udevice *dev,
			     const struct clock_config *cfg);

struct clk *
clk_sunxi_register_fixed(struct udevice *dev,
			     const struct clock_config *cfg);

struct sunxi_clk_gate_cfg {
	int gate_id;
};

struct sunxi_clk_composite_cfg {
	int	gate_id;
	int	mux_id;
	int	div_id;
	int	mult_id;
};

#define SUNXI_CLK_GATE(_id, _name, _parent, _flags, _gate_id) \
{ \
	.id		= _id, \
	.name		= _name, \
	.parent_name	= _parent, \
	.flags		= _flags, \
	.clock_cfg	= &(struct sunxi_clk_gate_cfg) { \
		.gate_id	= _gate_id, \
	}, \
	.setup		= clk_sunxi_gate_register, \
}

#define SUNXI_CLK_COMPOSITE(_id, _name, _flags, \
			_gate_id, _mux_id, _div_id) \
{ \
	.id		= _id, \
	.name		= _name, \
	.flags		= _flags, \
	.clock_cfg	= &(struct sunxi_clk_composite_cfg) { \
		.gate_id	= _gate_id, \
		.mux_id		= _mux_id, \
		.div_id		= _div_id, \
	}, \
	.setup		= clk_sunxi_register_composite, \
}

#define SUNXI_CLK_COMPOSITE_FACTOR(_id, _name, _parent, _flags, \
			_gate_id, _div_id) \
{ \
	.id		= _id, \
	.name		= _name, \
	.parent_name	= _parent, \
	.flags		= _flags, \
	.clock_cfg	= &(struct sunxi_clk_composite_cfg) { \
		.gate_id	= _gate_id, \
		.div_id		= _div_id, \
	}, \
	.setup		= clk_sunxi_register_composite_factor, \
}

#define SUNXI_CLK_FIXED(_id, _name, _parent, _div_id, _mult_id, _flags) \
{ \
	.id		= _id, \
	.name		= _name, \
	.parent_name	= _parent, \
	.flags		= _flags, \
	.clock_cfg	= &(struct sunxi_clk_composite_cfg) { \
		.div_id		= _div_id, \
		.mult_id	= _mult_id, \
	}, \
	.setup		= clk_sunxi_register_fixed, \
}
extern const struct clk_ops sunxi_ccu_ops;
extern const int sunxi_clk_of_to_plat(struct udevice *dev);
extern const int sunxi_clk_bind(struct udevice *dev);

/**
 * struct sunxi_priv - private struct for sunximp clocks
 *
 * @base:	base register of RCC driver
 * @gate_cpt:	array of refcounting for gate with more than one
 *		clocks as input. See explanation of Peripheral clock enabling
 *              below.
 * @data:	data for gate / divider / multiplexer configuration
 */
struct sunxi_priv {
	void __iomem *base;
	u8 *gate_cpt;
	const struct clk_sunxi_clock_data *data;
};

#endif /* _CCU_SUNXI_H */
