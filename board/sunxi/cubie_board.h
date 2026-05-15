#include <asm-generic/gpio.h>
#include <sunxi_gpadc.h>
#include <dm/device.h>

#define countof(x) (sizeof(x) / sizeof(x[0]))
#define HW_ID_ADC_CHANNEL	2
/*PK 24 */
#define HW_ID_GPIO			344

struct hw_info_def {
	char *compatible;
	char *fdtfile;
	const char* pcie_power_gpio;
	const char* pcie_wake_gpio;
	const char* pcie_reset_gpio;
	unsigned int hw_id_level;
	unsigned int hw_id_lower_bound;
	unsigned int hw_id_upper_bound;
};

void radxa_set_board_type(void);
void radxa_set_compat_fdt(void);
struct hw_info_def radxa_get_hw_info(void);
