#include <asm-generic/gpio.h>
#include <sunxi_gpadc.h>
#include <dm/device.h>
#include "cubie_board.h"

unsigned int board_index = 0;
static const struct hw_info_def hw_info[] = {
	{
		.compatible = "allwinner,a733",
		.fdtfile = "allwinner/sun60i-a733-cubie-a7z.dtb",
		.pcie_power_gpio = "PD20",
		.pcie_wake_gpio = "PD21",
		.pcie_reset_gpio = "PD22",
		.hw_id_level = 1,
		.hw_id_lower_bound = 1750,
		.hw_id_upper_bound = 1850, //1800mv +/- 50mv
	},
	{
		.compatible = "allwinner,a733",
		.fdtfile = "allwinner/sun60i-a733-cubie-a7a.dtb",
		.pcie_power_gpio = "PE11",
		.pcie_wake_gpio = "PE12",
		.pcie_reset_gpio = "PE13",
		.hw_id_level = 1,
		.hw_id_lower_bound = 1519,
		.hw_id_upper_bound = 1619, //1569mv +/- 50mv
	},
	{
		.compatible = "allwinner,a733",
		.fdtfile = "allwinner/sun60i-a733-cubie-a7s.dtb",
		.pcie_power_gpio = "PD20",
		.pcie_wake_gpio = "PD21",
		.pcie_reset_gpio = "PD22",
		.hw_id_level = 1,
		.hw_id_lower_bound = 1265,
		.hw_id_upper_bound = 1365, //1315mv +/- 50mv
	},
};

void radxa_set_board_type(void)
{
	int i, vol, level;

	if (gpio_request(HW_ID_GPIO, "hw_id_level") != 0) {
		debug("Failed to request GPIO %d for hardware ID detection\n", HW_ID_GPIO);
		return;
	}
	gpio_direction_input(HW_ID_GPIO);
	level = gpio_get_value(HW_ID_GPIO);
	vol = sunxi_get_gpadc_vol(HW_ID_ADC_CHANNEL);
	gpio_free(HW_ID_GPIO);

	debug("%s vol=%d, level=%d\n", __func__, vol, level);
	for (i = 0; i < countof(hw_info); i++) {
		if (of_machine_is_compatible(hw_info[i].compatible) &&
			vol >= hw_info[i].hw_id_lower_bound &&
			vol <= hw_info[i].hw_id_upper_bound &&
			level == hw_info[i].hw_id_level) {
			board_index = i;
			return;
		}
	}
	printf("No compatible board type found for vol=%d, level=%d\n", vol, level);
	return;
}

void radxa_set_compat_fdt(void)
{
	env_set("fdtfile", hw_info[board_index].fdtfile);
	printf("Override default fdtfile to %s\n", hw_info[board_index].fdtfile);
	return;
}

struct hw_info_def radxa_get_hw_info(void)
{
	return hw_info[board_index];
}
