// SPDX-License-Identifier: GPL-2.0+

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <fdtdec.h>
#include <malloc.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <dt-bindings/gpio/gpio.h>
#include <sunxi_gpio_ext.h>

int sunxi_name_to_gpio(const char *name)
{
	unsigned int gpio;
	int ret;
	ret = gpio_lookup_name(name, NULL, NULL, &gpio);

	return ret ? ret : gpio;
}

enum io_pow_mode_e sunxi_gpio_get_volt_val(int port_group)
{
	uint32_t reg;
	uint8_t group_bit_offset = port_group;
	if (group_bit_offset < 0)
		return IO_MODE_DEFAULT;

	reg = readl(PIOC_REG_POW_VAL);
	return ((reg & (1 << (group_bit_offset * 2))) != PIOC_VAL_Px_1_8V_VOL) ?
		       IO_MODE_3_3_V :
		       IO_MODE_1_8_V;
}

static int sunxi_gpio_get_value(struct udevice *dev, unsigned offset)
{
	struct sunxi_gpio_plat *plat = dev_get_plat(dev);
	unsigned dat;

	dat = readl(plat->data_reg);
	pr_debug(
		"%s...%d bank_base = 0x%p plat->data_reg = 0x%p offset = 0x%x dat = 0x%x\n",
		__func__, __LINE__, plat->bank_base, plat->data_reg, offset,
		dat);
	dat >>= offset;

	return dat & 0x1;
}

static int sunxi_gpio_set_value(struct udevice *dev, unsigned offset, int value)
{
	struct sunxi_gpio_plat *plat = dev_get_plat(dev);

	pr_debug("%s...%d bank_base = 0x%p data_reg = 0x%p num = 0x%x\n",
		 __func__, __LINE__, plat->bank_base, plat->data_reg, offset);
	if (value) {
		setbits_le32(plat->data_reg, BIT(offset));

	} else {
		clrbits_le32(plat->data_reg, BIT(offset));
	}

	return 0;
}

static int sunxi_gpio_get_function(struct udevice *dev, unsigned offset)
{
	struct sunxi_gpio_plat *plat = dev_get_plat(dev);
	int func;

	func = sunxi_gpio_ext_get_bank_cfg(plat->mux_reg, offset);
	pr_debug(
		"%s...%d bank_base = 0x%p mux_reg = 0x%p offset = 0x%x func = 0x%x\n",
		__func__, __LINE__, plat->bank_base, plat->mux_reg, offset,
		func);
	if (func == SUNXI_GPIO_OUTPUT)
		return GPIOF_OUTPUT;
	else if (func == SUNXI_GPIO_INPUT)
		return GPIOF_INPUT;
	else
		return GPIOF_FUNC;
}

static int sunxi_gpio_xlate(struct udevice *dev, struct gpio_desc *desc,
			    struct ofnode_phandle_args *args)
{
	int ret;

	ret = device_get_child(dev, args->args[0], &desc->dev);
	if (ret)
		return ret;
	desc->offset = args->args[1];
	desc->flags = gpio_flags_xlate(args->args[2]);

	return 0;
}

static int sunxi_gpio_set_flags(struct udevice *dev, unsigned int offset,
				ulong flags)
{
	struct sunxi_gpio_plat *plat = dev_get_plat(dev);

	pr_debug("%s...%d bank_base = 0x%p offset = 0x%x flags = 0x%lx\n",
		 __func__, __LINE__, plat->bank_base, offset, flags);
	if (flags & GPIOD_IS_OUT) {
		u32 value = !!(flags & GPIOD_IS_OUT_ACTIVE);
		sunxi_gpio_ext_set_bank_cfg(plat->mux_reg, offset,
					    SUNXI_GPIO_OUTPUT);
		clrsetbits_le32(plat->data_reg, 1 << offset, value << offset);
	} else if (flags & GPIOD_IS_IN) {
		u32 pull = 0;

		if (flags & GPIOD_PULL_UP)
			pull = 1;
		else if (flags & GPIOD_PULL_DOWN)
			pull = 2;
		sunxi_gpio_ext_set_bank_pull(plat->pull_reg, offset, pull);
		sunxi_gpio_ext_set_bank_cfg(plat->mux_reg, offset,
					    SUNXI_GPIO_INPUT);
	}

	return 0;
}

static const struct dm_gpio_ops gpio_sunxi_ops = {
	.get_value		= sunxi_gpio_get_value,
	.set_value		= sunxi_gpio_set_value,
	.get_function		= sunxi_gpio_get_function,
	.xlate			= sunxi_gpio_xlate,
	.set_flags		= sunxi_gpio_set_flags,
};

static int gpio_sunxi_probe(struct udevice *dev)
{
	struct sunxi_gpio_plat *plat = dev_get_plat(dev);
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);

	/* Tell the uclass how many GPIOs we have */
	if (plat) {
		uc_priv->gpio_count = SUNXI_GPIOS_PER_BANK;
		uc_priv->bank_name = plat->bank_name;
	}

	return 0;
}

U_BOOT_DRIVER(gpio_sunxi_v3) = {
	.name	= "gpio_sunxi_v3",
	.id	= UCLASS_GPIO,
	.probe	= gpio_sunxi_probe,
	.ops	= &gpio_sunxi_ops,
};
