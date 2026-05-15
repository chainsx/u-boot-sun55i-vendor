// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2012-2013 Henrik Nordstrom <henrik@henriknordstrom.net>
 * (C) Copyright 2013 Luke Kenneth Casson Leighton <lkcl@lkcl.net>
 *
 * (C) Copyright 2007-2011
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Tom Cubie <tangliang@allwinnertech.com>
 *
 * Some board init for the Allwinner A10-evb board.
 */

#include <common.h>
#include <clock_legacy.h>
#include <dm.h>
#include <env.h>
#include <hang.h>
#include <image.h>
#include <init.h>
#include <log.h>
#include <mmc.h>
#include <axp_pmic.h>
#include <generic-phy.h>
#include <asm/arch/clock.h>
#include <asm/arch/cpu.h>
#include <asm/arch/display.h>
#include <asm/arch/dram.h>
#include <asm/arch/mmc.h>
#include <asm/arch/prcm.h>
#include <asm/arch/pmic_bus.h>
#include <asm/arch/spl.h>
#include <asm/arch/sys_proto.h>
#include <asm/global_data.h>
#include <linux/delay.h>
#include <u-boot/crc.h>
#include <sunxi_board.h>
#include <smc.h>
#ifndef CONFIG_ARM64
#include <asm/armv7.h>
#endif
#include <asm/gpio.h>
#include <asm/io.h>
#include <u-boot/crc.h>
#include <env_internal.h>
#include <linux/libfdt.h>
#include <fdt_support.h>
#include <nand.h>
#include <net.h>
#include <spl.h>
#include <sy8106a.h>
#include <asm/setup.h>
#include <status_led.h>
#include <private_uboot.h>
#include <asm/io.h>
#include <power_manage.h>
#include <sunxi_boot_param.h>
#include "board_helper.h"
#include <sunxi_flash.h>
#if CONFIG_IS_ENABLED(AW_REMOTEPROC)
#include <sunxi_rproc.h>
#endif
#include <sunxi-usb-phy.h>
#include <dm/uclass-internal.h>
#include <dm/device-internal.h>
#include <dm/root.h>

DECLARE_GLOBAL_DATA_PTR;

int  __attribute__((weak)) sunxi_set_sramc_mode(void)
{
	return 0;
}

void twi_init_board(void)
{
#ifdef CONFIG_I2C0_ENABLE
#if defined(CONFIG_MACH_SUN4I) || \
    defined(CONFIG_MACH_SUN5I) || \
    defined(CONFIG_MACH_SUN7I) || \
    defined(CONFIG_MACH_SUN8I_R40)
	sunxi_gpio_set_cfgpin(SUNXI_GPB(0), SUN4I_GPB_TWI0);
	sunxi_gpio_set_cfgpin(SUNXI_GPB(1), SUN4I_GPB_TWI0);
	clock_twi_onoff(0, 1);
#elif defined(CONFIG_MACH_SUN6I)
	sunxi_gpio_set_cfgpin(SUNXI_GPH(14), SUN6I_GPH_TWI0);
	sunxi_gpio_set_cfgpin(SUNXI_GPH(15), SUN6I_GPH_TWI0);
	clock_twi_onoff(0, 1);
#elif defined(CONFIG_MACH_SUN8I_V3S)
	sunxi_gpio_set_cfgpin(SUNXI_GPB(6), SUN8I_V3S_GPB_TWI0);
	sunxi_gpio_set_cfgpin(SUNXI_GPB(7), SUN8I_V3S_GPB_TWI0);
	clock_twi_onoff(0, 1);
#elif defined(CONFIG_MACH_SUN8I)
	sunxi_gpio_set_cfgpin(SUNXI_GPH(2), SUN8I_GPH_TWI0);
	sunxi_gpio_set_cfgpin(SUNXI_GPH(3), SUN8I_GPH_TWI0);
	clock_twi_onoff(0, 1);
#elif defined(CONFIG_MACH_SUN50I)
	sunxi_gpio_set_cfgpin(SUNXI_GPH(0), SUN50I_GPH_TWI0);
	sunxi_gpio_set_cfgpin(SUNXI_GPH(1), SUN50I_GPH_TWI0);
	clock_twi_onoff(0, 1);
#endif
#endif

#ifdef CONFIG_I2C1_ENABLE
#if defined(CONFIG_MACH_SUN4I) || \
    defined(CONFIG_MACH_SUN7I) || \
    defined(CONFIG_MACH_SUN8I_R40)
	sunxi_gpio_set_cfgpin(SUNXI_GPB(18), SUN4I_GPB_TWI1);
	sunxi_gpio_set_cfgpin(SUNXI_GPB(19), SUN4I_GPB_TWI1);
	clock_twi_onoff(1, 1);
#elif defined(CONFIG_MACH_SUN5I)
	sunxi_gpio_set_cfgpin(SUNXI_GPB(15), SUN5I_GPB_TWI1);
	sunxi_gpio_set_cfgpin(SUNXI_GPB(16), SUN5I_GPB_TWI1);
	clock_twi_onoff(1, 1);
#elif defined(CONFIG_MACH_SUN6I)
	sunxi_gpio_set_cfgpin(SUNXI_GPH(16), SUN6I_GPH_TWI1);
	sunxi_gpio_set_cfgpin(SUNXI_GPH(17), SUN6I_GPH_TWI1);
	clock_twi_onoff(1, 1);
#elif defined(CONFIG_MACH_SUN8I)
	sunxi_gpio_set_cfgpin(SUNXI_GPH(4), SUN8I_GPH_TWI1);
	sunxi_gpio_set_cfgpin(SUNXI_GPH(5), SUN8I_GPH_TWI1);
	clock_twi_onoff(1, 1);
#elif defined(CONFIG_MACH_SUN50I)
	sunxi_gpio_set_cfgpin(SUNXI_GPH(2), SUN50I_GPH_TWI1);
	sunxi_gpio_set_cfgpin(SUNXI_GPH(3), SUN50I_GPH_TWI1);
	clock_twi_onoff(1, 1);
#endif
#endif

#ifdef CONFIG_R_I2C_ENABLE
#ifdef CONFIG_MACH_SUN50I
	clock_twi_onoff(5, 1);
	sunxi_gpio_set_cfgpin(SUNXI_GPL(8), SUN50I_GPL_R_TWI);
	sunxi_gpio_set_cfgpin(SUNXI_GPL(9), SUN50I_GPL_R_TWI);
#elif CONFIG_MACH_SUN50I_H616
	clock_twi_onoff(5, 1);
	sunxi_gpio_set_cfgpin(SUNXI_GPL(0), SUN50I_H616_GPL_R_TWI);
	sunxi_gpio_set_cfgpin(SUNXI_GPL(1), SUN50I_H616_GPL_R_TWI);
#else
	clock_twi_onoff(5, 1);
	sunxi_gpio_set_cfgpin(SUNXI_GPL(0), SUN8I_H3_GPL_R_TWI);
	sunxi_gpio_set_cfgpin(SUNXI_GPL(1), SUN8I_H3_GPL_R_TWI);
#endif
#endif
}

/* should use public env, don't use this function */
#if 0
/*
 * Try to use the environment from the boot source first.
 * For MMC, this means a FAT partition on the boot device (SD or eMMC).
 * If the raw MMC environment is also enabled, this is tried next.
 * When booting from NAND we try UBI first, then NAND directly.
 * SPI flash falls back to FAT (on SD card).
 */
enum env_location env_get_location(enum env_operation op, int prio)
{
	if (prio > 1)
		return ENVL_UNKNOWN;

	/* NOWHERE is exclusive, no other option can be defined. */
	if (IS_ENABLED(CONFIG_ENV_IS_NOWHERE))
		return ENVL_NOWHERE;

	switch (sunxi_get_boot_device()) {
	case BOOT_DEVICE_MMC1:
	case BOOT_DEVICE_MMC2:
		if (prio == 0 && IS_ENABLED(CONFIG_ENV_IS_IN_FAT))
			return ENVL_FAT;
		if (IS_ENABLED(CONFIG_ENV_IS_IN_MMC))
			return ENVL_MMC;
		break;
	case BOOT_DEVICE_NAND:
		if (prio == 0 && IS_ENABLED(CONFIG_ENV_IS_IN_UBI))
			return ENVL_UBI;
		if (IS_ENABLED(CONFIG_ENV_IS_IN_NAND))
			return ENVL_NAND;
		break;
	case BOOT_DEVICE_SPI:
		if (prio == 0 && IS_ENABLED(CONFIG_ENV_IS_IN_SPI_FLASH))
			return ENVL_SPI_FLASH;
		if (IS_ENABLED(CONFIG_ENV_IS_IN_FAT))
			return ENVL_FAT;
		break;
	case BOOT_DEVICE_BOARD:
		break;
	default:
		break;
	}

	/*
	 * If we come here for the first time, we *must* return a valid
	 * environment location other than ENVL_UNKNOWN, or the setup sequence
	 * in board_f() will silently hang. This is arguably a bug in
	 * env_init(), but for now pick one environment for which we know for
	 * sure to have a driver for. For all defconfigs this is either FAT
	 * or UBI, or NOWHERE, which is already handled above.
	 */
	if (prio == 0) {
		if (IS_ENABLED(CONFIG_ENV_IS_IN_FAT))
			return ENVL_FAT;
		if (IS_ENABLED(CONFIG_ENV_IS_IN_UBI))
			return ENVL_UBI;
	}

	return ENVL_UNKNOWN;
}
#endif

int sunxi_plat_init(void)
{
	sunxi_probe_securemode();

#ifdef CONFIG_ARM
	extern int secure_os_memory_init(void);
	if (sunxi_probe_secure_os()) {
		smc_tee_inform_fdt((uint64_t)(unsigned long)working_fdt, gd->fdt_size);
		secure_os_memory_init();
	}
#endif
	return 0;
}

static int sunxi_flash_init(void)
{
	int workmode     = 0;
	int storage_type = 0;
	workmode     = get_boot_work_mode();
	storage_type = get_boot_storage_type();

	pr_notice("workmode = %d,storage type = %s\n",
			workmode, sunxi_flash_to_storage_string(storage_type));

	switch (storage_type) {
	case STORAGE_NAND:
	case STORAGE_SPI_NAND:
#if defined(CONFIG_AW_MTD_SPINAND) || defined(CONFIG_AW_MTD_RAWNAND)
		sunxi_ubi_nand_init();
#else
		pr_err("UBI nand driver is not supported");
#endif
		break;
	case STORAGE_SD:
	case STORAGE_SD1:
	case STORAGE_EMMC:
	case STORAGE_EMMC0:
	case STORAGE_EMMC3:
#ifdef CONFIG_MMC
		mmc_initialize(gd->bd);
#else
		pr_err("eMMC driver is not supported");
#endif
	if (workmode == WORK_MODE_CARD_PRODUCT) {
#if defined(CONFIG_AW_MTD_SPINAND) || defined(CONFIG_AW_MTD_RAWNAND)
			sunxi_ubi_nand_init();
#elif defined(CONFIG_AW_SPINOR)
			sunxi_spinor_init();
#endif
	}
		break;
	case STORAGE_NOR:
#ifdef CONFIG_AW_SPINOR
		sunxi_spinor_init();
#else
		pr_err("NOR driver is not supported");
#endif
		break;
	case STORAGE_UFS:
		break;
	default:
		pr_err("This storage medium type %d is not supported\n", storage_type);
		return -1;
	}

	return 0;
}
#ifdef CONFIG_LOGF_MODULE
static void sunxi_set_log_cat_flag(void)
{
	gd->log_fmt |= 	BIT(LOGF_CAT);
}
#endif

#ifdef CONFIG_LOGF_LEVEL
static void sunxi_set_log_level_flag(void)
{
	gd->log_fmt |= 	BIT(LOGF_LEVEL);
}
#endif

#define PA_CFG0_ADDR 0x03604080
#define PA_DAT_ADDR  0x03604090

/* add board specific code here */
int board_init(void)
{
	struct udevice *dev;
	struct uclass *uc;
	__maybe_unused int id_pfr1, ret, satapwr_pin, macpwr_pin;
	u32 reg_val;
#ifdef CONFIG_LOGF_MODULE
	sunxi_set_log_cat_flag();
#endif

#ifdef CONFIG_LOGF_LEVEL
	sunxi_set_log_level_flag();
#endif

	gd->bd->bi_boot_params = (PHYS_SDRAM_0 + 0x100);

	/*fdt cmd must set fdt address, set it here*/
	set_working_fdt_addr((ulong)gd->fdt_blob);

#if !defined(CONFIG_ARM64) && !defined(CONFIG_MACH_SUNIV)
	asm volatile("mrc p15, 0, %0, c0, c1, 1" : "=r"(id_pfr1));
	debug("id_pfr1: 0x%08x\n", id_pfr1);
	/* Generic Timer Extension available? */
	if ((id_pfr1 >> CPUID_ARM_GENTIMER_SHIFT) & 0xf) {
		uint32_t freq;

		debug("Setting CNTFRQ\n");

		/*
		 * CNTFRQ is a secure register, so we will crash if we try to
		 * write this from the non-secure world (read is OK, though).
		 * In case some bootcode has already set the correct value,
		 * we avoid the risk of writing to it.
		 */
		asm volatile("mrc p15, 0, %0, c14, c0, 0" : "=r"(freq));
		if (freq != CONFIG_COUNTER_FREQUENCY) {
			debug("arch timer frequency is %d Hz, should be %d, fixing ...\n",
			      freq, CONFIG_COUNTER_FREQUENCY);
#ifdef CONFIG_NON_SECURE
			printf("arch timer frequency is wrong, but cannot adjust it\n");
#else
			asm volatile("mcr p15, 0, %0, c14, c0, 0"
				     : : "r"(CONFIG_COUNTER_FREQUENCY));
#endif
		}
	}
#endif /* !CONFIG_ARM64 && !CONFIG_MACH_SUNIV */

	sunxi_plat_init();

	ret = axp_gpio_init();
	if (ret)
		return ret;

	/* strcmp() would look better, but doesn't get optimised away. */
	if (CONFIG_SATAPWR[0]) {
		satapwr_pin = sunxi_name_to_gpio(CONFIG_SATAPWR);
		if (satapwr_pin >= 0) {
			gpio_request(satapwr_pin, "satapwr");
			gpio_direction_output(satapwr_pin, 1);

			/*
			 * Give the attached SATA device time to power-up
			 * to avoid link timeouts
			 */
			mdelay(500);
		}
	}

	if (CONFIG_MACPWR[0]) {
		macpwr_pin = sunxi_name_to_gpio(CONFIG_MACPWR);
		if (macpwr_pin >= 0) {
			gpio_request(macpwr_pin, "macpwr");
			gpio_direction_output(macpwr_pin, 1);
		}
	}

#if CONFIG_IS_ENABLED(DM_I2C)
	/*
	 * Temporary workaround for enabling I2C clocks until proper sunxi DM
	 * clk, reset and pinctrl drivers land.
	 */
	twi_init_board();
#endif

#if CONFIG_IS_ENABLED(DM_PMIC)
	/*
	 * Temporary workaround for enabling I2C clocks until proper sunxi DM
	 * clk, reset and pinctrl drivers land.
	 */
	if (!pmic_probe()) {
		pmic_set_dcdc_mode();
		pmic_set_power_supply_output();
	}
#endif
	sunxi_set_sramc_mode();
	eth_init_board();
	sunxi_flash_init();

	reg_val = readl(PA_CFG0_ADDR);
	reg_val &= ~(0xf << 24);
	reg_val |= (0x1 << 24);
	writel(reg_val, PA_CFG0_ADDR);

	/* Set led1 gpio output high */
	reg_val = readl(PA_DAT_ADDR);
	reg_val |= (0x1 << 6);
	writel(reg_val, PA_DAT_ADDR);

#if CONFIG_IS_ENABLED(AW_REMOTEPROC)
	sunxi_remoteproc_init();
#endif

	// probe i2c bus
	ret = uclass_get(UCLASS_I2C_GENERIC, &uc);
	if (ret) {
		printf("uclass get UCLASS_I2C_GENERIC fail. \n");
		return -1;
	}

	uclass_foreach_dev(dev, uc) {
		ret = device_probe(dev);
		if (ret) {
			printf("uclass probe fail. \n");
		}
	}

	return 0;
}

/*
 * On older SoCs the SPL is actually at address zero, so using NULL as
 * an error value does not work.
 */
#define INVALID_SPL_HEADER ((void *)~0UL)

static struct boot_file_head * get_spl_header(uint8_t req_version)
{
	struct boot_file_head *spl = (void *)(ulong)SPL_ADDR;
	uint8_t spl_header_version = spl->spl_signature[3];

	/* Is there really the SPL header (still) there? */
	if (memcmp(spl->spl_signature, SPL_SIGNATURE, 3) != 0)
		return INVALID_SPL_HEADER;

	if (spl_header_version < req_version) {
		printf("sunxi SPL version mismatch: expected %u, got %u\n",
		       req_version, spl_header_version);
		return INVALID_SPL_HEADER;
	}

	return spl;
}

static const char *get_spl_dt_name(void)
{
	struct boot_file_head *spl = get_spl_header(SPL_DT_HEADER_VERSION);

	/* Check if there is a DT name stored in the SPL header. */
	if (spl != INVALID_SPL_HEADER && spl->dt_name_offset)
		return (char *)spl + spl->dt_name_offset;

	return NULL;
}

int dram_init(void)
{
	uint dram_size = 0;

	dram_size = uboot_spare_head.boot_data.dram_scan_size;
	dram_size = dram_size > 2048 ? 2048 : dram_size;

	if (dram_size)
		gd->ram_size = dram_size * 1024 * 1024;
	else
		gd->ram_size =
			get_ram_size((long *)PHYS_SDRAM_0, PHYS_SDRAM_0_SIZE);

	return 0;
}

#if defined(CONFIG_NAND_SUNXI)
static void nand_pinmux_setup(void)
{
	unsigned int pin;

	for (pin = SUNXI_GPC(0); pin <= SUNXI_GPC(19); pin++)
		sunxi_gpio_set_cfgpin(pin, SUNXI_GPC_NAND);

#if defined CONFIG_MACH_SUN4I || defined CONFIG_MACH_SUN7I
	for (pin = SUNXI_GPC(20); pin <= SUNXI_GPC(22); pin++)
		sunxi_gpio_set_cfgpin(pin, SUNXI_GPC_NAND);
#endif
	/* sun4i / sun7i do have a PC23, but it is not used for nand,
	 * only sun7i has a PC24 */
#ifdef CONFIG_MACH_SUN7I
	sunxi_gpio_set_cfgpin(SUNXI_GPC(24), SUNXI_GPC_NAND);
#endif
}

static void nand_clock_setup(void)
{
	struct sunxi_ccm_reg *const ccm =
		(struct sunxi_ccm_reg *)SUNXI_CCM_BASE;

	setbits_le32(&ccm->ahb_gate0, (CLK_GATE_OPEN << AHB_GATE_OFFSET_NAND0));
#if defined CONFIG_MACH_SUN6I || defined CONFIG_MACH_SUN8I || \
    defined CONFIG_MACH_SUN9I || defined CONFIG_MACH_SUN50I
	setbits_le32(&ccm->ahb_reset0_cfg, (1 << AHB_GATE_OFFSET_NAND0));
#endif
	setbits_le32(&ccm->nand0_clk_cfg, CCM_NAND_CTRL_ENABLE | AHB_DIV_1);
}

void board_nand_init(void)
{
	nand_pinmux_setup();
	nand_clock_setup();
#ifndef CONFIG_SPL_BUILD
	sunxi_nand_init();
#endif
}
#endif /* CONFIG_NAND_SUNXI */

#ifdef CONFIG_MMC
#if !CONFIG_IS_ENABLED(DM_MMC)
static void mmc_pinmux_setup(int sdc)
{
	unsigned int pin;

	switch (sdc) {
	case 0:
		/* SDC0: PF0-PF5 */
		for (pin = SUNXI_GPF(0); pin <= SUNXI_GPF(5); pin++) {
			sunxi_gpio_set_cfgpin(pin, SUNXI_GPF_SDC0);
			sunxi_gpio_set_pull(pin, SUNXI_GPIO_PULL_UP);
			sunxi_gpio_set_drv(pin, 2);
		}
		break;

	case 1:
#if defined(CONFIG_MACH_SUN4I) || defined(CONFIG_MACH_SUN7I) || \
    defined(CONFIG_MACH_SUN8I_R40)
		if (IS_ENABLED(CONFIG_MMC1_PINS_PH)) {
			/* SDC1: PH22-PH-27 */
			for (pin = SUNXI_GPH(22); pin <= SUNXI_GPH(27); pin++) {
				sunxi_gpio_set_cfgpin(pin, SUN4I_GPH_SDC1);
				sunxi_gpio_set_pull(pin, SUNXI_GPIO_PULL_UP);
				sunxi_gpio_set_drv(pin, 2);
			}
		} else {
			/* SDC1: PG0-PG5 */
			for (pin = SUNXI_GPG(0); pin <= SUNXI_GPG(5); pin++) {
				sunxi_gpio_set_cfgpin(pin, SUN4I_GPG_SDC1);
				sunxi_gpio_set_pull(pin, SUNXI_GPIO_PULL_UP);
				sunxi_gpio_set_drv(pin, 2);
			}
		}
#elif defined(CONFIG_MACH_SUN5I)
		/* SDC1: PG3-PG8 */
		for (pin = SUNXI_GPG(3); pin <= SUNXI_GPG(8); pin++) {
			sunxi_gpio_set_cfgpin(pin, SUN5I_GPG_SDC1);
			sunxi_gpio_set_pull(pin, SUNXI_GPIO_PULL_UP);
			sunxi_gpio_set_drv(pin, 2);
		}
#elif defined(CONFIG_MACH_SUN6I)
		/* SDC1: PG0-PG5 */
		for (pin = SUNXI_GPG(0); pin <= SUNXI_GPG(5); pin++) {
			sunxi_gpio_set_cfgpin(pin, SUN6I_GPG_SDC1);
			sunxi_gpio_set_pull(pin, SUNXI_GPIO_PULL_UP);
			sunxi_gpio_set_drv(pin, 2);
		}
#elif defined(CONFIG_MACH_SUN8I)
		/* SDC1: PG0-PG5 */
		for (pin = SUNXI_GPG(0); pin <= SUNXI_GPG(5); pin++) {
			sunxi_gpio_set_cfgpin(pin, SUN8I_GPG_SDC1);
			sunxi_gpio_set_pull(pin, SUNXI_GPIO_PULL_UP);
			sunxi_gpio_set_drv(pin, 2);
		}
#endif
		break;

	case 2:
#if defined(CONFIG_MACH_SUN4I) || defined(CONFIG_MACH_SUN7I)
		/* SDC2: PC6-PC11 */
		for (pin = SUNXI_GPC(6); pin <= SUNXI_GPC(11); pin++) {
			sunxi_gpio_set_cfgpin(pin, SUNXI_GPC_SDC2);
			sunxi_gpio_set_pull(pin, SUNXI_GPIO_PULL_UP);
			sunxi_gpio_set_drv(pin, 2);
		}
#elif defined(CONFIG_MACH_SUN5I)
		/* SDC2: PC6-PC15 */
		for (pin = SUNXI_GPC(6); pin <= SUNXI_GPC(15); pin++) {
			sunxi_gpio_set_cfgpin(pin, SUNXI_GPC_SDC2);
			sunxi_gpio_set_pull(pin, SUNXI_GPIO_PULL_UP);
			sunxi_gpio_set_drv(pin, 2);
		}
#elif defined(CONFIG_MACH_SUN6I)
		/* SDC2: PC6-PC15, PC24 */
		for (pin = SUNXI_GPC(6); pin <= SUNXI_GPC(15); pin++) {
			sunxi_gpio_set_cfgpin(pin, SUNXI_GPC_SDC2);
			sunxi_gpio_set_pull(pin, SUNXI_GPIO_PULL_UP);
			sunxi_gpio_set_drv(pin, 2);
		}

		sunxi_gpio_set_cfgpin(SUNXI_GPC(24), SUNXI_GPC_SDC2);
		sunxi_gpio_set_pull(SUNXI_GPC(24), SUNXI_GPIO_PULL_UP);
		sunxi_gpio_set_drv(SUNXI_GPC(24), 2);
#elif defined(CONFIG_MACH_SUN8I_R40)
		/* SDC2: PC6-PC15, PC24 */
		for (pin = SUNXI_GPC(6); pin <= SUNXI_GPC(15); pin++) {
			sunxi_gpio_set_cfgpin(pin, SUNXI_GPC_SDC2);
			sunxi_gpio_set_pull(pin, SUNXI_GPIO_PULL_UP);
			sunxi_gpio_set_drv(pin, 2);
		}

		sunxi_gpio_set_cfgpin(SUNXI_GPC(24), SUNXI_GPC_SDC2);
		sunxi_gpio_set_pull(SUNXI_GPC(24), SUNXI_GPIO_PULL_UP);
		sunxi_gpio_set_drv(SUNXI_GPC(24), 2);
#elif defined(CONFIG_MACH_SUN8I) || defined(CONFIG_MACH_SUN50I)
		/* SDC2: PC5-PC6, PC8-PC16 */
		for (pin = SUNXI_GPC(5); pin <= SUNXI_GPC(6); pin++) {
			sunxi_gpio_set_cfgpin(pin, SUNXI_GPC_SDC2);
			sunxi_gpio_set_pull(pin, SUNXI_GPIO_PULL_UP);
			sunxi_gpio_set_drv(pin, 2);
		}

		for (pin = SUNXI_GPC(8); pin <= SUNXI_GPC(16); pin++) {
			sunxi_gpio_set_cfgpin(pin, SUNXI_GPC_SDC2);
			sunxi_gpio_set_pull(pin, SUNXI_GPIO_PULL_UP);
			sunxi_gpio_set_drv(pin, 2);
		}
#elif defined(CONFIG_MACH_SUN50I_H6)
		/* SDC2: PC4-PC14 */
		for (pin = SUNXI_GPC(4); pin <= SUNXI_GPC(14); pin++) {
			sunxi_gpio_set_cfgpin(pin, SUNXI_GPC_SDC2);
			sunxi_gpio_set_pull(pin, SUNXI_GPIO_PULL_UP);
			sunxi_gpio_set_drv(pin, 2);
		}
#elif defined(CONFIG_MACH_SUN50I_H616)
		/* SDC2: PC0-PC1, PC5-PC6, PC8-PC11, PC13-PC16 */
		for (pin = SUNXI_GPC(0); pin <= SUNXI_GPC(16); pin++) {
			if (pin > SUNXI_GPC(1) && pin < SUNXI_GPC(5))
				continue;
			if (pin == SUNXI_GPC(7) || pin == SUNXI_GPC(12))
				continue;
			sunxi_gpio_set_cfgpin(pin, SUNXI_GPC_SDC2);
			sunxi_gpio_set_pull(pin, SUNXI_GPIO_PULL_UP);
			sunxi_gpio_set_drv(pin, 3);
		}
#elif defined(CONFIG_MACH_SUN9I)
		/* SDC2: PC6-PC16 */
		for (pin = SUNXI_GPC(6); pin <= SUNXI_GPC(16); pin++) {
			sunxi_gpio_set_cfgpin(pin, SUNXI_GPC_SDC2);
			sunxi_gpio_set_pull(pin, SUNXI_GPIO_PULL_UP);
			sunxi_gpio_set_drv(pin, 2);
		}
#else
		puts("ERROR: No pinmux setup defined for MMC2!\n");
#endif
		break;

	case 3:
#if defined(CONFIG_MACH_SUN4I) || defined(CONFIG_MACH_SUN7I) || \
    defined(CONFIG_MACH_SUN8I_R40)
		/* SDC3: PI4-PI9 */
		for (pin = SUNXI_GPI(4); pin <= SUNXI_GPI(9); pin++) {
			sunxi_gpio_set_cfgpin(pin, SUNXI_GPI_SDC3);
			sunxi_gpio_set_pull(pin, SUNXI_GPIO_PULL_UP);
			sunxi_gpio_set_drv(pin, 2);
		}
#elif defined(CONFIG_MACH_SUN6I)
		/* SDC3: PC6-PC15, PC24 */
		for (pin = SUNXI_GPC(6); pin <= SUNXI_GPC(15); pin++) {
			sunxi_gpio_set_cfgpin(pin, SUN6I_GPC_SDC3);
			sunxi_gpio_set_pull(pin, SUNXI_GPIO_PULL_UP);
			sunxi_gpio_set_drv(pin, 2);
		}

		sunxi_gpio_set_cfgpin(SUNXI_GPC(24), SUN6I_GPC_SDC3);
		sunxi_gpio_set_pull(SUNXI_GPC(24), SUNXI_GPIO_PULL_UP);
		sunxi_gpio_set_drv(SUNXI_GPC(24), 2);
#endif
		break;

	default:
		printf("sunxi: invalid MMC slot %d for pinmux setup\n", sdc);
		break;
	}
}

int board_mmc_init(struct bd_info *bis)
{
	/*
	 * The BROM always accesses MMC port 0 (typically an SD card), and
	 * most boards seem to have such a slot. The others haven't reported
	 * any problem with unconditionally enabling this in the SPL.
	 */
	if (!IS_ENABLED(CONFIG_UART0_PORT_F)) {
		mmc_pinmux_setup(0);
		if (!sunxi_mmc_init(0))
			return -1;
	}

	if (CONFIG_MMC_SUNXI_SLOT_EXTRA != -1) {
		mmc_pinmux_setup(CONFIG_MMC_SUNXI_SLOT_EXTRA);
		if (!sunxi_mmc_init(CONFIG_MMC_SUNXI_SLOT_EXTRA))
			return -1;
	}

	return 0;
}
#endif //!CONFIG_IS_ENABLED(DM_MMC)
#if CONFIG_MMC_SUNXI_SLOT_EXTRA != -1
int mmc_get_env_dev(void)
{
	switch (sunxi_get_boot_device()) {
	case BOOT_DEVICE_MMC1:
		return 0;
	case BOOT_DEVICE_MMC2:
		return 1;
	default:
		return CONFIG_SYS_MMC_ENV_DEV;
	}
}
#endif
#endif /* CONFIG_MMC */

#ifdef CONFIG_SPL_BUILD

static void sunxi_spl_store_dram_size(phys_addr_t dram_size)
{
	struct boot_file_head *spl = get_spl_header(SPL_DT_HEADER_VERSION);

	if (spl == INVALID_SPL_HEADER)
		return;

	/* Promote the header version for U-Boot proper, if needed. */
	if (spl->spl_signature[3] < SPL_DRAM_HEADER_VERSION)
		spl->spl_signature[3] = SPL_DRAM_HEADER_VERSION;

	spl->dram_size = dram_size >> 20;
}

void sunxi_board_init(void)
{
	int power_failed = 0;

#ifdef CONFIG_LED_STATUS
	if (IS_ENABLED(CONFIG_SPL_DRIVERS_MISC))
		status_led_init();
#endif

#ifdef CONFIG_SY8106A_POWER
	power_failed = sy8106a_set_vout1(CONFIG_SY8106A_VOUT1_VOLT);
#endif

#if defined CONFIG_AXP152_POWER || defined CONFIG_AXP209_POWER || \
	defined CONFIG_AXP221_POWER || defined CONFIG_AXP305_POWER || \
	defined CONFIG_AXP809_POWER || defined CONFIG_AXP818_POWER
	power_failed = axp_init();

	if (IS_ENABLED(CONFIG_AXP_DISABLE_BOOT_ON_POWERON) && !power_failed) {
		u8 boot_reason;

		pmic_bus_read(AXP_POWER_STATUS, &boot_reason);
		if (boot_reason & AXP_POWER_STATUS_ALDO_IN) {
			printf("Power on by plug-in, shutting down.\n");
			pmic_bus_write(0x32, BIT(7));
		}
	}

#if defined CONFIG_AXP221_POWER || defined CONFIG_AXP809_POWER || \
	defined CONFIG_AXP818_POWER
	power_failed |= axp_set_dcdc1(CONFIG_AXP_DCDC1_VOLT);
#endif
#if !defined(CONFIG_AXP305_POWER)
	power_failed |= axp_set_dcdc2(CONFIG_AXP_DCDC2_VOLT);
	power_failed |= axp_set_dcdc3(CONFIG_AXP_DCDC3_VOLT);
#endif
#if !defined(CONFIG_AXP209_POWER) && !defined(CONFIG_AXP818_POWER)
	power_failed |= axp_set_dcdc4(CONFIG_AXP_DCDC4_VOLT);
#endif
#if defined CONFIG_AXP221_POWER || defined CONFIG_AXP809_POWER || \
	defined CONFIG_AXP818_POWER
	power_failed |= axp_set_dcdc5(CONFIG_AXP_DCDC5_VOLT);
#endif

#if defined CONFIG_AXP221_POWER || defined CONFIG_AXP809_POWER || \
	defined CONFIG_AXP818_POWER
	power_failed |= axp_set_aldo1(CONFIG_AXP_ALDO1_VOLT);
#endif
#if !defined(CONFIG_AXP305_POWER)
	power_failed |= axp_set_aldo2(CONFIG_AXP_ALDO2_VOLT);
#endif
#if !defined(CONFIG_AXP152_POWER) && !defined(CONFIG_AXP305_POWER)
	power_failed |= axp_set_aldo3(CONFIG_AXP_ALDO3_VOLT);
#endif
#ifdef CONFIG_AXP209_POWER
	power_failed |= axp_set_aldo4(CONFIG_AXP_ALDO4_VOLT);
#endif

#if defined(CONFIG_AXP221_POWER) || defined(CONFIG_AXP809_POWER) || \
	defined(CONFIG_AXP818_POWER)
	power_failed |= axp_set_dldo(1, CONFIG_AXP_DLDO1_VOLT);
	power_failed |= axp_set_dldo(2, CONFIG_AXP_DLDO2_VOLT);
#if !defined CONFIG_AXP809_POWER
	power_failed |= axp_set_dldo(3, CONFIG_AXP_DLDO3_VOLT);
	power_failed |= axp_set_dldo(4, CONFIG_AXP_DLDO4_VOLT);
#endif
	power_failed |= axp_set_eldo(1, CONFIG_AXP_ELDO1_VOLT);
	power_failed |= axp_set_eldo(2, CONFIG_AXP_ELDO2_VOLT);
	power_failed |= axp_set_eldo(3, CONFIG_AXP_ELDO3_VOLT);
#endif

#ifdef CONFIG_AXP818_POWER
	power_failed |= axp_set_fldo(1, CONFIG_AXP_FLDO1_VOLT);
	power_failed |= axp_set_fldo(2, CONFIG_AXP_FLDO2_VOLT);
	power_failed |= axp_set_fldo(3, CONFIG_AXP_FLDO3_VOLT);
#endif

#if defined CONFIG_AXP809_POWER || defined CONFIG_AXP818_POWER
	power_failed |= axp_set_sw(IS_ENABLED(CONFIG_AXP_SW_ON));
#endif
#endif
	printf("DRAM:");
	gd->ram_size = sunxi_dram_init();
	printf(" %d MiB\n", (int)(gd->ram_size >> 20));
	if (!gd->ram_size)
		hang();

	sunxi_spl_store_dram_size(gd->ram_size);

	/*
	 * Only clock up the CPU to full speed if we are reasonably
	 * assured it's being powered with suitable core voltage
	 */
	if (!power_failed)
		clock_set_pll1(get_board_sys_clk());
	else
		printf("Failed to set core voltage! Can't set CPU frequency\n");
}
#endif /* CONFIG_SPL_BUILD */

#ifdef CONFIG_USB_GADGET
int g_dnl_board_usb_cable_connected(void)
{
	struct udevice *dev;
	struct phy phy;
	int ret;

	ret = uclass_get_device(UCLASS_USB_GADGET_GENERIC, 0, &dev);
	if (ret) {
		pr_err("%s: Cannot find USB device\n", __func__);
		return ret;
	}

	ret = generic_phy_get_by_name(dev, "usb", &phy);
	if (ret) {
		pr_err("failed to get %s USB PHY\n", dev->name);
		return ret;
	}

	ret = generic_phy_init(&phy);
	if (ret) {
		pr_debug("failed to init %s USB PHY\n", dev->name);
		return ret;
	}

#ifdef CONFIG_AW_USB_PHY
	return sunxi_usb_phy_vbus_detect(&phy);
#endif
	return 0;
}
#endif /* CONFIG_USB_GADGET */

#ifdef CONFIG_SERIAL_TAG
void get_board_serial(struct tag_serialnr *serialnr)
{
	char *serial_string;
	unsigned long long serial;

	serial_string = env_get("serial#");

	if (serial_string) {
		serial = simple_strtoull(serial_string, NULL, 16);

		serialnr->high = (unsigned int) (serial >> 32);
		serialnr->low = (unsigned int) (serial & 0xffffffff);
	} else {
		serialnr->high = 0;
		serialnr->low = 0;
	}
}
#endif

/*
 * Check the SPL header for the "sunxi" variant. If found: parse values
 * that might have been passed by the loader ("fel" utility), and update
 * the environment accordingly.
 */
static void parse_spl_header(const uint32_t spl_addr)
{
	struct boot_file_head *spl = get_spl_header(SPL_ENV_HEADER_VERSION);

	if (spl == INVALID_SPL_HEADER)
		return;

	if (!spl->fel_script_address)
		return;

	if (spl->fel_uEnv_length != 0) {
		/*
		 * data is expected in uEnv.txt compatible format, so "env
		 * import -t" the string(s) at fel_script_address right away.
		 */
		himport_r(&env_htab, (char *)(uintptr_t)spl->fel_script_address,
			  spl->fel_uEnv_length, '\n', H_NOCLEAR, 0, 0, NULL);
		return;
	}
	/* otherwise assume .scr format (mkimage-type script) */
	env_set_hex("fel_scriptaddr", spl->fel_script_address);
}

static bool get_unique_sid(unsigned int *sid)
{
	if (sunxi_get_sid(sid) != 0)
		return false;

	if (!sid[0])
		return false;

	/*
	 * The single words 1 - 3 of the SID have quite a few bits
	 * which are the same on many models, so we take a crc32
	 * of all 3 words, to get a more unique value.
	 *
	 * Note we only do this on newer SoCs as we cannot change
	 * the algorithm on older SoCs since those have been using
	 * fixed mac-addresses based on only using word 3 for a
	 * long time and changing a fixed mac-address with an
	 * u-boot update is not good.
	 */
#if !defined(CONFIG_MACH_SUN4I) && !defined(CONFIG_MACH_SUN5I) && \
    !defined(CONFIG_MACH_SUN6I) && !defined(CONFIG_MACH_SUN7I) && \
    !defined(CONFIG_MACH_SUN8I_A23) && !defined(CONFIG_MACH_SUN8I_A33)
	sid[3] = crc32(0, (unsigned char *)&sid[1], 12);
#endif

	/* Ensure the NIC specific bytes of the mac are not all 0 */
	if ((sid[3] & 0xffffff) == 0)
		sid[3] |= 0x800000;

	return true;
}

/*
 * Note this function gets called multiple times.
 * It must not make any changes to env variables which already exist.
 */
static void setup_environment(const void *fdt)
{
	char serial_string[17] = { 0 };
	unsigned int sid[4];
	uint8_t mac_addr[6];
	char ethaddr[16];
	int i;

	if (!get_unique_sid(sid))
		return;

	for (i = 0; i < 4; i++) {
		sprintf(ethaddr, "ethernet%d", i);
		if (!fdt_get_alias(fdt, ethaddr))
			continue;

		if (i == 0)
			strcpy(ethaddr, "ethaddr");
		else
			sprintf(ethaddr, "eth%daddr", i);

		if (env_get(ethaddr))
			continue;

		/* Non OUI / registered MAC address */
		mac_addr[0] = (i << 4) | 0x02;
		mac_addr[1] = (sid[0] >>  0) & 0xff;
		mac_addr[2] = (sid[3] >> 24) & 0xff;
		mac_addr[3] = (sid[3] >> 16) & 0xff;
		mac_addr[4] = (sid[3] >>  8) & 0xff;
		mac_addr[5] = (sid[3] >>  0) & 0xff;

		eth_env_set_enetaddr(ethaddr, mac_addr);
	}

	if (!env_get("serial#")) {
		snprintf(serial_string, sizeof(serial_string),
			"%08x%08x", sid[0], sid[3]);

		env_set("serial#", serial_string);
	}
}

int misc_init_r(void)
{
	const char *spl_dt_name;
	uint boot;

	env_set("fel_booted", NULL);
	env_set("fel_scriptaddr", NULL);
	env_set("mmc_bootdev", NULL);

	boot = sunxi_get_boot_device();
	/* determine if we are running in FEL mode */
	if (boot == BOOT_DEVICE_BOARD) {
		env_set("fel_booted", "1");
		parse_spl_header(SPL_ADDR);
	/* or if we booted from MMC, and which one */
	} else if (boot == BOOT_DEVICE_MMC1) {
		env_set("mmc_bootdev", "0");
	} else if (boot == BOOT_DEVICE_MMC2) {
		env_set("mmc_bootdev", "1");
	}

	/* Set fdtfile to match the FIT configuration chosen in SPL. */
	spl_dt_name = get_spl_dt_name();
	if (spl_dt_name) {
		char *prefix = IS_ENABLED(CONFIG_ARM64) ? "allwinner/" : "";
		char str[64];

		snprintf(str, sizeof(str), "%s%s.dtb", prefix, spl_dt_name);
		env_set("fdtfile", str);
	}

	setup_environment(gd->fdt_blob);

	return 0;
}

int board_late_init(void)
{
#ifdef CONFIG_USB_ETHER
	usb_ether_init();
#endif

#if CONFIG_IS_ENABLED(SUNXI_BOOT_PARAM)
	sunxi_bootparam_down();
#endif

#ifdef CONFIG_SUNXI_BOOT_ENV_INLINE
	int sunxi_boot_env_inline_init(void);
	if (sunxi_boot_env_inline_init())
		return -1;

	int sunxi_check_setargs(void);
	if (sunxi_check_setargs())
		return -1;
#endif

#ifdef CONFIG_SUNXI_SWITCH_SYSTEM
	int sunxi_auto_switch_system(void);
	sunxi_auto_switch_system();
#endif
	sunxi_update_bootcmd();

	return 0;
}

static void bluetooth_dt_fixup(void *blob)
{
	/* Some devices ship with a Bluetooth controller default address.
	 * Set a valid address through the device tree.
	 */
	uchar tmp[ETH_ALEN], bdaddr[ETH_ALEN];
	unsigned int sid[4];
	int i;

	if (!CONFIG_BLUETOOTH_DT_DEVICE_FIXUP[0])
		return;

	if (eth_env_get_enetaddr("bdaddr", tmp)) {
		/* Convert between the binary formats of the corresponding stacks */
		for (i = 0; i < ETH_ALEN; ++i)
			bdaddr[i] = tmp[ETH_ALEN - i - 1];
	} else {
		if (!get_unique_sid(sid))
			return;

		bdaddr[0] = ((sid[3] >>  0) & 0xff) ^ 1;
		bdaddr[1] = (sid[3] >>  8) & 0xff;
		bdaddr[2] = (sid[3] >> 16) & 0xff;
		bdaddr[3] = (sid[3] >> 24) & 0xff;
		bdaddr[4] = (sid[0] >>  0) & 0xff;
		bdaddr[5] = 0x02;
	}

	do_fixup_by_compat(blob, CONFIG_BLUETOOTH_DT_DEVICE_FIXUP,
			   "local-bd-address", bdaddr, ETH_ALEN, 1);
}

int ft_board_setup(void *blob, struct bd_info *bd)
{
	int __maybe_unused r;

	/*
	 * Call setup_environment and fdt_fixup_ethernet again
	 * in case the boot fdt has ethernet aliases the u-boot
	 * copy does not have.
	 */
	setup_environment(blob);
	fdt_fixup_ethernet(blob);

	bluetooth_dt_fixup(blob);
	while(uboot_spare_head.boot_head.reserved[0] && uboot_spare_head.boot_data.work_mode)
	;
#ifdef CONFIG_VIDEO_DT_SIMPLEFB
	r = sunxi_simplefb_setup(blob);
	if (r)
		return r;
#endif
	return 0;
}

#ifdef CONFIG_SPL_LOAD_FIT
static void set_spl_dt_name(const char *name)
{
	struct boot_file_head *spl = get_spl_header(SPL_ENV_HEADER_VERSION);

	if (spl == INVALID_SPL_HEADER)
		return;

	/* Promote the header version for U-Boot proper, if needed. */
	if (spl->spl_signature[3] < SPL_DT_HEADER_VERSION)
		spl->spl_signature[3] = SPL_DT_HEADER_VERSION;

	strcpy((char *)&spl->string_pool, name);
	spl->dt_name_offset = offsetof(struct boot_file_head, string_pool);
}

int board_fit_config_name_match(const char *name)
{
	const char *best_dt_name = get_spl_dt_name();
	int ret;

#ifdef CONFIG_DEFAULT_DEVICE_TREE
	if (best_dt_name == NULL)
		best_dt_name = CONFIG_DEFAULT_DEVICE_TREE;
#endif

	if (best_dt_name == NULL) {
		/* No DT name was provided, so accept the first config. */
		return 0;
	}
#ifdef CONFIG_PINE64_DT_SELECTION
	if (strstr(best_dt_name, "-pine64-plus")) {
		/* Differentiate the Pine A64 boards by their DRAM size. */
		if ((gd->ram_size == 512 * 1024 * 1024))
			best_dt_name = "sun50i-a64-pine64";
	}
#endif
#ifdef CONFIG_PINEPHONE_DT_SELECTION
	if (strstr(best_dt_name, "-pinephone")) {
		/* Differentiate the PinePhone revisions by GPIO inputs. */
		prcm_apb0_enable(PRCM_APB0_GATE_PIO);
		sunxi_gpio_set_pull(SUNXI_GPL(6), SUNXI_GPIO_PULL_UP);
		sunxi_gpio_set_cfgpin(SUNXI_GPL(6), SUNXI_GPIO_INPUT);
		udelay(100);

		/* PL6 is pulled low by the modem on v1.2. */
		if (gpio_get_value(SUNXI_GPL(6)) == 0)
			best_dt_name = "sun50i-a64-pinephone-1.2";
		else
			best_dt_name = "sun50i-a64-pinephone-1.1";

		sunxi_gpio_set_cfgpin(SUNXI_GPL(6), SUNXI_GPIO_DISABLE);
		sunxi_gpio_set_pull(SUNXI_GPL(6), SUNXI_GPIO_PULL_DISABLE);
		prcm_apb0_disable(PRCM_APB0_GATE_PIO);
	}
#endif

	ret = strcmp(name, best_dt_name);

	/*
	 * If one of the FIT configurations matches the most accurate DT name,
	 * update the SPL header to provide that DT name to U-Boot proper.
	 */
	if (ret == 0)
		set_spl_dt_name(best_dt_name);

	return ret;
}
#endif /* CONFIG_SPL_LOAD_FIT */

#if defined(CONFIG_BOARD_EARLY_INIT_F)
static int sunxi_set_log_level(void)
{
	int level;

	level = fdt_getprop_u32_default((struct fdt_header *)gd->fdt_blob,
			"/soc/platform", "debug_mode", CONFIG_LOG_DEFAULT_LEVEL);
	debug("debug_mode:%d\n", level);

	gd->default_log_level = level;

	return 0;
}

int board_early_init_f(void)
{
	sunxi_set_log_level();
	return 0;
}
#endif /* CONFIG_BOARD_EARLY_INIT_F */

int aw_bsp_reserve_board_f(void)
{
#ifdef CONFIG_SUNXI_BOOT_PARAM
	reserve_sunxi_boot_param();
#endif
	return 0;
}

static void sunxi_storage_type_node_enable(void)
{
	uint storage_type = get_boot_storage_type_ext();
	uint workmode = get_boot_work_mode();

	pr_debug("gd->fdt_blob = 0x%p\n", gd->fdt_blob);
	void *fdt_blob = (void *)gd->fdt_blob;
	switch (storage_type) {
	case STORAGE_NAND:
		fdt_status_okay_by_alias(fdt_blob, "rawnand");
		break;
	case STORAGE_NOR:
		fdt_status_okay_by_alias(fdt_blob, "spi-nor0");
		break;
	case STORAGE_SPI_NAND:
		fdt_status_okay_by_alias(fdt_blob, "spi-nand0");
		break;
	case STORAGE_SD:
		fdt_status_okay_by_alias(fdt_blob, "sunxi-mmc0");
		if (workmode == WORK_MODE_BOOT)
			break;
#if defined(CONFIG_AW_MTD_SPINAND) || defined(CONFIG_AW_MTD_RAWNAND)
		fdt_status_okay_by_alias(fdt_blob, "spi-nand0");
#else
		fdt_status_okay_by_alias(fdt_blob, "spi-nor0");
		fdt_status_okay_by_alias(fdt_blob, "sunxi-mmc2");
#endif
		break;
	case STORAGE_EMMC:
	case STORAGE_EMMC0:
		fdt_status_okay_by_alias(fdt_blob, "sunxi-mmc2");
		break;
	default:
		break;
	}
}

int aw_bsp_board_f_init(void)
{
	sunxi_storage_type_node_enable();

#if defined(CONFIG_OPTEE25)
	smc_init();
#endif
	return 0;
}
