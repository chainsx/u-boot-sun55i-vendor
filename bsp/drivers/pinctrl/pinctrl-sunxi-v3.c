// SPDX-License-Identifier: GPL-2.0

#include <clk.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <dm/pinctrl.h>
#include <errno.h>
#include <malloc.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <sunxi_board.h>
#include <sunxi_gpio_ext.h>

extern U_BOOT_DRIVER(gpio_sunxi_v3);

/*
 * This structure implements a simplified view of the possible pinmux settings:
 * Each mux value is assumed to be the same for a given function, across the
 * pins in each group (almost universally true, with same rare exceptions not
 * relevant to U-Boot), but also across different ports (not true in many
 * cases). We ignore the first problem, and work around the latter by just
 * supporting one particular port for a each function. This works fine for all
 * board configurations so far. If this would need to be revisited, we could
 * add a "u8 port;" below and match that, with 0 encoding the "don't care" case.
 */
struct sunxi_pinctrl_function {
	const char name[sizeof("gpio_out")];
	u8 mux;
};

struct sunxi_pinctrl_hw_info {
	u32 initial_bank_offset;
	u32 mux_regs_offset;
	u32 data_regs_offset;
	u32 dlevel_regs_offset;
	u32 pull_regs_offset;
};

struct sunxi_pinctrl_desc {
	const struct sunxi_pinctrl_function *functions;
	u8 num_functions;
	u8 first_bank;
	u8 num_banks;
	struct sunxi_pinctrl_hw_info hw_info;
};

struct sunxi_pinctrl_plat {
	void __iomem *base; //pio or r_pio
	u32 initial_bank_offset;
	u32 mux_regs_offset;
	u32 data_regs_offset;
	u32 dlevel_regs_offset;
	u32 pull_regs_offset;
	u32 bank_mem_size;
	u32 data_mem_size;
};

static int sunxi_pinctrl_get_pins_count(struct udevice *dev)
{
	const struct sunxi_pinctrl_desc *desc = dev_get_priv(dev);

	return desc->num_banks * SUNXI_GPIOS_PER_BANK;
}

static const char *sunxi_pinctrl_get_pin_name(struct udevice *dev,
					      uint pin_selector)
{
	const struct sunxi_pinctrl_desc *desc = dev_get_priv(dev);
	static char pin_name[sizeof("PN31")];

	snprintf(pin_name, sizeof(pin_name), "P%c%d",
		 pin_selector / SUNXI_GPIOS_PER_BANK + desc->first_bank + 'A',
		 pin_selector % SUNXI_GPIOS_PER_BANK);

	return pin_name;
}

static int sunxi_pinctrl_get_functions_count(struct udevice *dev)
{
	const struct sunxi_pinctrl_desc *desc = dev_get_priv(dev);

	return desc->num_functions;
}

static const char *sunxi_pinctrl_get_function_name(struct udevice *dev,
						   uint func_selector)
{
	const struct sunxi_pinctrl_desc *desc = dev_get_priv(dev);

	return desc->functions[func_selector].name;
}

u32 *sunxi_pinctrl_get_bank_cfg(struct udevice *dev, u32 bank_num)
{
	struct sunxi_pinctrl_plat *plat = dev_get_plat(dev);
	//bank cfg addr
	return plat->base + plat->initial_bank_offset + plat->mux_regs_offset +
	       (plat->bank_mem_size * bank_num);
}

u32 *sunxi_pinctrl_get_bank_dat(struct udevice *dev, u32 bank_num)
{
	struct sunxi_pinctrl_plat *plat = dev_get_plat(dev);
	return plat->base + plat->initial_bank_offset + plat->data_regs_offset +
	       (plat->data_mem_size * bank_num);
}

u32 *sunxi_pinctrl_get_bank_drv(struct udevice *dev, u32 bank_num)
{
	struct sunxi_pinctrl_plat *plat = dev_get_plat(dev);
	return plat->base + plat->initial_bank_offset +
	       plat->dlevel_regs_offset + (plat->bank_mem_size * bank_num);
}

u32 *sunxi_pinctrl_get_bank_pll(struct udevice *dev, u32 bank_num)
{
	struct sunxi_pinctrl_plat *plat = dev_get_plat(dev);
	return plat->base + plat->initial_bank_offset + plat->pull_regs_offset +
	       (plat->bank_mem_size * bank_num);
}

int sunxi_pinctrl_pinmux_set(struct udevice *dev, uint pin_selector,
			     uint func_selector)
{
	const struct sunxi_pinctrl_desc *desc = dev_get_priv(dev);
	// struct sunxi_pinctrl_plat *plat = dev_get_plat(dev);
	u32 bank_num = pin_selector / SUNXI_GPIOS_PER_BANK;
	u32 pin = pin_selector % SUNXI_GPIOS_PER_BANK;
	u32 *bank = sunxi_pinctrl_get_bank_cfg(dev, bank_num);
	pr_info("set mux: %-4s bank(%p) => %s (%d)\n",
		sunxi_pinctrl_get_pin_name(dev, pin_selector), bank,
		sunxi_pinctrl_get_function_name(dev, func_selector),
		desc->functions[func_selector].mux);
	// asm volatile("b .");
	sunxi_gpio_ext_set_bank_cfg(bank, pin,
				    desc->functions[func_selector].mux);

	return 0;
}

static const struct pinconf_param sunxi_pinctrl_pinconf_params[] = {
	{ "bias-disable", PIN_CONFIG_BIAS_DISABLE, 0 },
	{ "bias-pull-down", PIN_CONFIG_BIAS_PULL_DOWN, 2 },
	{ "bias-pull-up", PIN_CONFIG_BIAS_PULL_UP, 1 },
	{ "drive-strength", PIN_CONFIG_DRIVE_STRENGTH, 10 },
};

static int sunxi_pinctrl_pinconf_set_pull(struct udevice *dev, uint bank_num,
					  uint pin, uint bias)
{
	// struct sunxi_gpio *regs = &plat->base[bank];
	u32 *bank = sunxi_pinctrl_get_bank_pll(dev, bank_num);
	pr_info("set pull: P%c%d bank(%p) => (%d)\n", 'A' + bank_num, pin, bank, bias);
	// sunxi_gpio_ext_set_pull_bank(bank, pin, bias);
	sunxi_gpio_ext_set_bank_pull(bank, pin, bias);
	return 0;
}

static int sunxi_pinctrl_pinconf_set_drive(struct udevice *dev, uint bank_num,
					   uint pin, uint drive)
{
	u32 *bank = sunxi_pinctrl_get_bank_drv(dev, bank_num);
	if (drive < 10 || drive > 40)
		return -EINVAL;
	pr_info("set drv: P%c%d bank(%p) =>(%d)\n", 'A' + bank_num, pin, bank, drive / 10 - 1);
	sunxi_gpio_ext_set_bank_drv(bank, pin, drive / 10 - 1);
	return 0;
}

static int sunxi_pinctrl_pinconf_set(struct udevice *dev, uint pin_selector,
				     uint param, uint val)
{
	// struct sunxi_pinctrl_plat *plat = dev_get_plat(dev);
	int bank_num = pin_selector / SUNXI_GPIOS_PER_BANK;
	int pin = pin_selector % SUNXI_GPIOS_PER_BANK;

	switch (param) {
	case PIN_CONFIG_BIAS_DISABLE:
	case PIN_CONFIG_BIAS_PULL_DOWN:
	case PIN_CONFIG_BIAS_PULL_UP:
		return sunxi_pinctrl_pinconf_set_pull(dev, bank_num, pin, val);
	case PIN_CONFIG_DRIVE_STRENGTH:
		return sunxi_pinctrl_pinconf_set_drive(dev, bank_num, pin, val);
	}

	return -EINVAL;
}

static int sunxi_pinctrl_get_pin_muxing(struct udevice *dev, uint pin_selector,
					char *buf, int size)
{
	// struct sunxi_pinctrl_plat *plat = dev_get_plat(dev);
	int bank_num = pin_selector / SUNXI_GPIOS_PER_BANK;
	int pin = pin_selector % SUNXI_GPIOS_PER_BANK;
	u32 *bank = sunxi_pinctrl_get_bank_cfg(dev, bank_num);
	u32 mux = sunxi_gpio_ext_get_bank_cfg(bank, pin);

	switch (mux) {
	case SUNXI_GPIO_INPUT:
		strlcpy(buf, "gpio input", size);
		break;
	case SUNXI_GPIO_OUTPUT:
		strlcpy(buf, "gpio output", size);
		break;
	case SUNXI_GPIO_DISABLE:
		strlcpy(buf, "disabled", size);
		break;
	default:
		snprintf(buf, size, "function %d", mux);
		break;
	}

	return 0;
}

static const struct pinctrl_ops sunxi_pinctrl_ops = {
	.get_pins_count = sunxi_pinctrl_get_pins_count,
	.get_pin_name = sunxi_pinctrl_get_pin_name,
	.get_functions_count = sunxi_pinctrl_get_functions_count,
	.get_function_name = sunxi_pinctrl_get_function_name,
	.pinmux_set = sunxi_pinctrl_pinmux_set,
	.pinconf_num_params = ARRAY_SIZE(sunxi_pinctrl_pinconf_params),
	.pinconf_params = sunxi_pinctrl_pinconf_params,
	.pinconf_set = sunxi_pinctrl_pinconf_set,
	.set_state = pinctrl_generic_set_state,
	.get_pin_muxing = sunxi_pinctrl_get_pin_muxing,
};

static int sunxi_pinctrl_bind(struct udevice *dev)
{
	struct sunxi_pinctrl_plat *plat = dev_get_plat(dev);
	struct sunxi_pinctrl_desc *desc;
	struct sunxi_gpio_plat *gpio_plat;
	struct udevice *gpio_dev;
	int i, ret;

	desc = (void *)dev_get_driver_data(dev);
	if (!desc)
		return -EINVAL;
	dev_set_priv(dev, desc);

	plat->base = dev_read_addr_ptr(dev);

	plat->initial_bank_offset =
		dev_read_u32_default(dev, "initial_bank_offset", 0x0);
	plat->mux_regs_offset =
		dev_read_u32_default(dev, "mux_regs_offset", 0x0);
	plat->data_regs_offset =
		dev_read_u32_default(dev, "data_regs_offset", 0x10);
	plat->dlevel_regs_offset =
		dev_read_u32_default(dev, "dlevel_regs_offset", 0x20);
	plat->pull_regs_offset =
		dev_read_u32_default(dev, "pull_regs_offset", 0x30);
	plat->bank_mem_size = dev_read_u32_default(dev, "bank_mem_size", 0x30);
	plat->data_mem_size =
		dev_read_u32_default(dev, "data_mem_size", plat->bank_mem_size);

	pr_info("base : 0x%p\ninitial_bank_offset : 0x%x\n"
	       "mux_regs_offset : 0x%x\ndata_regs_offset : 0x%x\n"
	       "dlevel_regs_offset : 0x%x\npull_regs_offset : 0x%x\n"
	       "bank_mem_size : 0x%x\ndata_mem_size : 0x%x\n",
	       plat->base, plat->initial_bank_offset, plat->mux_regs_offset,
	       plat->data_regs_offset, plat->dlevel_regs_offset,
	       plat->pull_regs_offset, plat->bank_mem_size,
	       plat->data_mem_size);
	ret = device_bind_driver_to_node(dev, "gpio_sunxi_v3", dev->name,
					 dev_ofnode(dev), &gpio_dev);
	if (ret)
		return ret;

	for (i = 0; i < desc->num_banks; ++i) {
		gpio_plat = malloc(sizeof(*gpio_plat));
		if (!gpio_plat)
			return -ENOMEM;

		gpio_plat->bank_base = plat->base + plat->initial_bank_offset +
				       i * plat->bank_mem_size;
		gpio_plat->data_reg = plat->base + plat->initial_bank_offset +
				      plat->data_regs_offset +
				      i * plat->data_mem_size;

		gpio_plat->mux_reg =
			gpio_plat->bank_base + plat->mux_regs_offset;
		gpio_plat->dlevel_reg =
			gpio_plat->bank_base + plat->dlevel_regs_offset;
		gpio_plat->pull_reg =
			gpio_plat->bank_base + plat->pull_regs_offset;

		gpio_plat->bank_name[0] = 'P';
		gpio_plat->bank_name[1] = 'A' + desc->first_bank + i;
		gpio_plat->bank_name[2] = '\0';
		pr_debug("--------\n%s : 0x%p\ngpio data : 0x%p\n"
		       "mux_reg : 0x%p\ndlevel_reg : 0x%p\n"
		       "pull_reg : 0x%p\n",
		       gpio_plat->bank_name, gpio_plat->bank_base,
		       gpio_plat->data_reg, gpio_plat->mux_reg,
		       gpio_plat->dlevel_reg, gpio_plat->pull_reg);
		ret = device_bind(gpio_dev, DM_DRIVER_REF(gpio_sunxi_v3),
				  gpio_plat->bank_name, gpio_plat,
				  ofnode_null(), NULL);
		if (ret)
			return ret;
	}

	return 0;
}

static int sunxi_pinctrl_probe(struct udevice *dev)
{
	struct clk *apb_clk;

	apb_clk = devm_clk_get(dev, "apb");
	if (!IS_ERR(apb_clk))
		clk_enable(apb_clk);

	return 0;
}

static const struct sunxi_pinctrl_function sun55iw6_pinctrl_plat_functions[] = {
	{ "gpio_in", 0 }, /* in */
	{ "gpio_out", 1 }, /* out */
	{ "mmc0", 2 }, /* PF0-PF5 */
	{ "mmc1", 2 }, /* PG0-PG5 */
	{ "mmc2", 3 }, /* PC0-PC16 */
	{ "spi0", 4 }, /* PC0-PC6 */
	{ "nand0", 2 }, /*PC0-PC15*/
	{ "spi1", 4 },
	{ "spi2", 6 },
	{ "spi2_pi", 3 },
	{ "spi3", 6},
	{ "spi3_ph", 3 },
	{ "spi4_pb", 3 },
	{ "spi4_pf", 6 },
	{ "r_spi", 6 },
	{ "uart0", 2 }, /* PB9-PB10 */
	{ "rgmii0",	5 },	/* PJ0-PJ15 */
	{ "rgmii1",	5 },	/* PJ16-PJ31 */
	{ "lvds0",	3 },
	{ "lvds1",	3 },
	{ "dsi",	4 },
	{ "lcd",	2 },
	{ "twi2",	3 },
	{ "pwm0_0",   3}, /* PD23 */
	{ "pwm1_0",   5}, /* PB6 */
	{ "pwm0_3", 2}, /* J3 */
};

static const struct sunxi_pinctrl_function sun55iw6_r_pinctrl_plat_functions[] = {
	{ "gpio_in", 0 },
	{ "gpio_out", 1 },
	{ "s_twi0", 2 }, /* PL0-PL1 */
	/* TODO */
};

static const struct sunxi_pinctrl_desc __maybe_unused
	sun55iw6_pinctrl_plat_desc = {
		.functions = sun55iw6_pinctrl_plat_functions,
		.num_functions = ARRAY_SIZE(sun55iw6_pinctrl_plat_functions),
		.first_bank = SUNXI_GPIO_A,
		.num_banks = 11,

	};

static const struct sunxi_pinctrl_desc __maybe_unused
	sun55iw6_r_pinctrl_plat_desc = {
		.functions = sun55iw6_r_pinctrl_plat_functions,
		.num_functions = ARRAY_SIZE(sun55iw6_r_pinctrl_plat_functions),
		.first_bank = SUNXI_GPIO_L,
		.num_banks = 2,
	};

static const struct udevice_id sunxi_pinctrl_ids[] = {
	{
		.compatible = "allwinner,sun55iw6-pinctrl",
		.data = (ulong)&sun55iw6_pinctrl_plat_desc,
	},
	{
		.compatible = "allwinner,sun55iw6-r-pinctrl",
		.data = (ulong)&sun55iw6_r_pinctrl_plat_desc,
	},
	{}
};

U_BOOT_DRIVER(sunxi_pinctrl) = {
	.name = "sunxi-pinctrl",
	.id = UCLASS_PINCTRL,
	.of_match = sunxi_pinctrl_ids,
	.bind = sunxi_pinctrl_bind,
	.probe = sunxi_pinctrl_probe,
	.plat_auto = sizeof(struct sunxi_pinctrl_plat),
	.ops = &sunxi_pinctrl_ops,
};
