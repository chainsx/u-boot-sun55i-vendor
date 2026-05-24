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
#include <dm.h>
#include <mmc.h>
#include <axp_pmic.h>
#include <generic-phy.h>
#include <phy-sun4i-usb.h>
#include <asm/arch/clock.h>
#include <asm/arch/cpu.h>
#include <asm/arch/display.h>
#include <asm/arch/dram.h>
#include <asm/arch/gpio.h>
#include <asm/arch/mmc.h>
#include <asm/arch/spl.h>
//#include <asm/arch/usb_phy.h>
#ifdef CONFIG_ARM
#include <asm/setup.h>
#ifndef CONFIG_ARM64
#include <asm/armv7.h>
#endif
#endif
#include <asm/gpio.h>
#include <asm/io.h>
#include <crc.h>
#include <environment.h>
#include <linux/libfdt.h>
#include <nand.h>
#include <net.h>
#include <spl.h>
#include <sy8106a.h>
#include <private_uboot.h>
#include <sys_config.h>
#include <sunxi_board.h>
#ifdef CONFIG_SUNXI_POWER
#include <sunxi_power/axp.h>
#include <sunxi_power/power_manage.h>
#endif
#ifdef CONFIG_TCS4838_POWER
#include <sunxi_power/pmu_tcs4838.h>
#endif
#ifdef CONFIG_SUNXI_DMA
#include <asm/arch/dma.h>
#endif
#include <mapmem.h>
#include <smc.h>

int __attribute__((weak)) sunxi_set_sramc_mode(void)
{
	return 0;
}

int __attribute__((weak)) clock_set_corepll(int frequency)
{
	return 0;
}

void __attribute__((weak)) rtc_set_vccio_det_spare(void)
{
	return;
}

int __attribute__((weak)) rtc_set_dcxo_off(void)
{
	return 0;
}

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_SUNXI_OVERLAY
__weak int sunxi_overlay_apply_merged(void *dtb_base, void *dtbo_base)
{
	return fdt_check_header(dtbo_base);
}
#endif

void i2c_init_board(void)
{
	__maybe_unused char fdt_node_str[8] = {0};

#ifdef CONFIG_I2C0_ENABLE
#if defined(CONFIG_MACH_SUN8IW18)
#if 0 /* twi0 & uart0 use the same pin */
	sunxi_gpio_set_cfgpin(SUNXI_GPH(0), SUN8I_GPH_TWI0);
	sunxi_gpio_set_cfgpin(SUNXI_GPH(1), SUN8I_GPH_TWI0);
	clock_twi_onoff(0, 1);
#endif
#else
	sprintf(fdt_node_str, "twi0");
	fdt_set_all_pin(fdt_node_str, "pinctrl-0");
#endif
#endif

#ifdef CONFIG_I2C1_ENABLE
#if defined(CONFIG_MACH_SUN8IW18)
	sunxi_gpio_set_cfgpin(SUNXI_GPH(2), SUN8I_GPH_TWI1);
	sunxi_gpio_set_cfgpin(SUNXI_GPH(3), SUN8I_GPH_TWI1);
	/* clock_twi_onoff(1, 1); */
#else
	sprintf(fdt_node_str, "twi1");
	fdt_set_all_pin(fdt_node_str, "pinctrl-0");
#endif
#endif

#ifdef CONFIG_I2C2_ENABLE
	sprintf(fdt_node_str, "twi2");
	fdt_set_all_pin(fdt_node_str, "pinctrl-0");
#endif

#ifdef CONFIG_I2C3_ENABLE
	sprintf(fdt_node_str, "twi3");
	fdt_set_all_pin(fdt_node_str, "pinctrl-0");
#endif

#ifdef CONFIG_I2C4_ENABLE
	sprintf(fdt_node_str, "twi4");
	fdt_set_all_pin(fdt_node_str, "pinctrl-0");
#endif

#ifdef CONFIG_I2C5_ENABLE
	sprintf(fdt_node_str, "twi5");
	fdt_set_all_pin(fdt_node_str, "pinctrl-0");
#endif

#ifdef CONFIG_R_I2C0_ENABLE
#if defined(CONFIG_MACH_SUN50IW11)
	sunxi_gpio_set_cfgpin(SUNXI_GPL(5), SUN8I_H3_GPL_R_TWI);
	sunxi_gpio_set_cfgpin(SUNXI_GPL(6), SUN8I_H3_GPL_R_TWI);
#else
#if defined(CONFIG_MACH_SUN50IW9)
	sprintf(fdt_node_str, "twi5");
#elif defined(CONFIG_MACH_SUN50IW10) || defined(CONFIG_MACH_SUN55IW3) || defined(CONFIG_MACH_SUN60IW2)
	sprintf(fdt_node_str, "twi6");
#else
	sprintf(fdt_node_str, "twi4");
#endif
	fdt_set_all_pin(fdt_node_str, "pinctrl-0");
#endif
#ifdef CONFIG_R_I2C1_ENABLE
	sprintf(fdt_node_str, "twi7");
	fdt_set_all_pin(fdt_node_str, "pinctrl-0");
#endif
#endif
}

#ifdef CONFIG_ARM
void enable_smp(void)
{
	/* SMP status is controlled by bit 6 of the CP15 Aux Ctrl Reg:ACTLR */
	asm volatile("MRC p15, 0, r0, c1, c0, 1");
	asm volatile("ORR r0, r0, #0x040");
	asm volatile("MCR p15, 0, r0, c1, c0, 1");
}
#else
void __attribute__((weak)) enable_smp(void)
{
	return;
}
#endif

void smp_init(void)
{
	int cpu_status = 0;

#if defined(CONFIG_SUNXI_NCAT) || defined(CONFIG_SUNXI_NCAT_V2)
#ifdef CONFIG_MACH_SUN300IW1
	cpu_status = 1;
#else
	cpu_status = readl(IOMEM_ADDR(SUNXI_CPUXCFG_BASE + 0x80));
	cpu_status &= (0xf << 24);
#endif
#else
	/* old platform enable smp unconditionally */
	cpu_status = 1;
#endif
	/*
	 * note: sbrom will enable smp bit when jmp to non-secure fel,
	 * but normal brom not do this operation. so should enable smp
	 * when run uboot by normal fel mode.
	 */
	if (!cpu_status)
		enable_smp();
}

int sunxi_plat_init(void)
{
	sunxi_probe_securemode();
#ifdef CONFIG_SUNXI_DMA
	sunxi_dma_init();
#endif
#ifdef CONFIG_ARM
	extern int secure_os_memory_init(void);

	if (sunxi_probe_secure_os()) {
		smc_tee_inform_fdt((uint64_t)(unsigned long)working_fdt, gd->fdt_size);
		secure_os_memory_init();
	}
#endif
	return 0;
}

/* add board specific code here */
int board_init(void)
{
	__maybe_unused int id_pfr1, ret, satapwr_pin, macpwr_pin;
	int work_mode;
	int boot_clock;

	gd->bd->bi_boot_params = (PHYS_SDRAM_0 + 0x100);
	sunxi_plat_init();

	work_mode = get_boot_work_mode();
	ret = axp_gpio_init();
	if (ret)
		return ret;

#ifdef CONFIG_SUNXI_BMU_EXT
	bmu_ext_probe();
#endif
#ifdef CONFIG_SUNXI_POWER
	axp_probe();
#endif
#ifdef CONFIG_SUNXI_PMU_EXT
	if (!pmu_ext_probe()) {
		pmu_ext_set_dcdc_mode();
		pmu_ext_set_power_supply_output();
	}
#endif
#ifdef CONFIG_SUNXI_PMU_GENERAL
	pmu_general_probe();
#endif
	rtc_set_dcxo_off();

	if ((work_mode == WORK_MODE_BOOT) ||
	    (work_mode == WORK_MODE_CARD_PRODUCT) ||
	    (work_mode == WORK_MODE_CARD_UPDATE))
		sunxi_set_sramc_mode();

	script_parser_fetch(FDT_PATH_TARGET, "boot_clock", &boot_clock,
			    uboot_spare_head.boot_data.run_clock);
	clock_set_corepll(boot_clock);

	/* fix reset circuit detection threshold */
	rtc_set_vccio_det_spare();
	tick_printf("CPU=%d MHz,PLL6=%d Mhz,AHB=%d Mhz, APB1=%dMhz MBus=%dMhz\n",
		    clock_get_corepll(), clock_get_pll6(), clock_get_ahb(),
		    clock_get_apb1(), clock_get_mbus());

#ifdef CONFIG_SUNXI_OVERLAY
	if (get_boot_work_mode() != WORK_MODE_BOOT)
		return 0;

	if (!sunxi_overlay_apply_merged(working_fdt, gd->new_dtbo)) {
		pr_err("sunxi overlay merged %sqv\n",
		       (fdt_overlay_apply_verbose(working_fdt, gd->new_dtbo) ? "fail" : "ok"));
	} else {
		pr_msg("not need merged sunxi overlay\n");
	}
#endif

	return 0;
}

int dram_init(void)
{
	uint dram_size = 0;

	dram_size = uboot_spare_head.boot_data.dram_scan_size;
	dram_size = dram_size > 2048 ? 2048 : dram_size;
	if (dram_size)
		gd->ram_size = dram_size * 1024 * 1024;
	else
		gd->ram_size = get_ram_size((long *)PHYS_SDRAM_0, PHYS_SDRAM_0_SIZE);

#ifdef CONFIG_ARM
	ulong drm_base = 0, drm_size = 0;

	if (sunxi_probe_secure_os()) {
		if (!smc_tee_probe_drm_configure(&drm_base, &drm_size)) {
			pr_msg("drm_base=0x%lx\n", drm_base);
			pr_msg("drm_size=0x%lx\n", drm_size);
			pr_msg("dram_base=0x%lx\n", CONFIG_SYS_SDRAM_BASE);
			pr_msg("dram_size=0x%lx\n", gd->ram_size);
			if (drm_base + drm_size == CONFIG_SYS_SDRAM_BASE + gd->ram_size) {
				/* drm region resides in end of memory, do not relocate to that area */
				gd->ram_size -= drm_size;
			}
		}
	}
#endif
	return 0;
}

#ifdef CONFIG_MMC
static void mmc_pinmux_setup(int sdc)
{
#if 0
	unsigned int pin;
	__maybe_unused int pins;

	switch (sdc) {
	case 0:
		/* SDC0: PF0-PF5 */
		for (pin = SUNXI_GPF(0); pin <= SUNXI_GPF(5); pin++) {
			sunxi_gpio_set_cfgpin(pin, SUNXI_GPF_SDC0);
			sunxi_gpio_set_pull(pin, SUNXI_GPIO_PULL_UP);
			sunxi_gpio_set_drv(pin, 2);
		}
		break;
	case 2:
		pins = sunxi_name_to_gpio_bank(CONFIG_MMC2_PINS);
#if defined(CONFIG_MACH_SUN50IW6)
		/* SDC2: PC4-PC14 */
		for (pin = SUNXI_GPC(4); pin <= SUNXI_GPC(14); pin++) {
			sunxi_gpio_set_cfgpin(pin, SUNXI_GPC_SDC2);
			sunxi_gpio_set_pull(pin, SUNXI_GPIO_PULL_UP);
			sunxi_gpio_set_drv(pin, 2);
		}
#endif
		break;
	default:
		printf("sunxi: invalid MMC slot %d for pinmux setup\n", sdc);
		break;
	}
#endif
}

/*
int board_mmc_init(bd_t *bis)
{
	sunxi_mmc_init(bis->bi_card_num);
	return 0;
}
*/

void board_mmc_pre_init(int card_num)
{
	bd_t *bd;

	bd = gd->bd;
	/* gd->bd->bi_card_num = card_num; */
	mmc_initialize(bd);
}

int board_mmc_get_num(void)
{
	return gd->boot_card_num;
}

void board_mmc_set_num(int num)
{
	gd->boot_card_num = num;
}

int board_mmc_init(bd_t *bis)
{
	__maybe_unused struct mmc *mmc0, *mmc1;
	__maybe_unused char buf[512];

	mmc_pinmux_setup(board_mmc_get_num());
	mmc0 = sunxi_mmc_init(board_mmc_get_num());
	if (!mmc0)
		return -1;

#if 0
#if CONFIG_MMC_SUNXI_SLOT_EXTRA != -1
	mmc_pinmux_setup(CONFIG_MMC_SUNXI_SLOT_EXTRA);
	mmc1 = sunxi_mmc_init(CONFIG_MMC_SUNXI_SLOT_EXTRA);
	if (!mmc1)
		return -1;
#endif
#endif
	return 0;
}
#endif

#ifdef CONFIG_USB_GADGET
int g_dnl_board_usb_cable_connected(void)
{
	struct udevice *dev;
	struct phy phy;
	int ret;

	ret = uclass_get_device(UCLASS_USB_DEV_GENERIC, 0, &dev);
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
		pr_err("failed to init %s USB PHY\n", dev->name);
		return ret;
	}

	ret = sun4i_usb_phy_vbus_detect(&phy);
	if (ret == 1) {
		pr_err("A charger is plugged into the OTG\n");
		return -ENODEV;
	}

	return ret;
}
#endif

static void sunxi_mac_addr_inc(uint8_t *mac)
{
	mac[5]++;
	if (mac[5] == 0)
		mac[4]++;

	/* Keep the address locally administered and unicast. */
	mac[0] &= 0xfe;
	mac[0] |= 0x02;
}

/* Some vendor trees provide this helper, some do not.  Use it as a
 * weak optional fallback because sunxi_get_sid() returns -ENODEV on A527 in
 * this boot flow.
 */
extern int sunxi_get_soc_chipid(uint8_t *chipid) __attribute__((weak));

static int sunxi_seed_is_valid(const uint32_t seed[4])
{
	return seed[0] || seed[1] || seed[2] || seed[3];
}

static void sunxi_dump_seed(const char *src, const uint32_t seed[4])
{
	printf("sunxi: ethernet mac seed from %s: %08x %08x %08x %08x\n",
	       src, seed[0], seed[1], seed[2], seed[3]);
}

static int sunxi_get_eth_mac_seed(uint32_t seed[4], const char **src)
{
	unsigned int sid[4] = { 0 };
	int ret;

	memset(seed, 0, sizeof(uint32_t) * 4);

	/* Preferred path: traditional sunxi SID.  It may return -ENODEV on A527. */
	ret = sunxi_get_sid(sid);
	if (!ret && (sid[0] || sid[1] || sid[2] || sid[3])) {
		seed[0] = sid[0];
		seed[1] = sid[1];
		seed[2] = sid[2];
		seed[3] = sid[3];
		if (src)
			*src = "sid";
		return 0;
	}
	printf("sunxi: sunxi_get_sid failed for ethernet mac, ret=%d sid=%08x %08x %08x %08x\n",
	       ret, sid[0], sid[1], sid[2], sid[3]);

	/* Optional vendor/kernel-style helper.  Some Allwinner trees expose this. */
	if (sunxi_get_soc_chipid) {
		uint8_t chipid[16] = { 0 };

		ret = sunxi_get_soc_chipid(chipid);
		memcpy(seed, chipid, sizeof(uint32_t) * 4);
		if (sunxi_seed_is_valid(seed)) {
			if (src)
				*src = "chipid";
			return 0;
		}
		printf("sunxi: sunxi_get_soc_chipid failed for ethernet mac, ret=%d chipid=%08x %08x %08x %08x\n",
		       ret, seed[0], seed[1], seed[2], seed[3]);
	}

#ifdef CONFIG_MMC
	/* Last-resort stable fallback: boot-card CID.  This is stable for the
	 * installed card/eMMC and avoids a random MAC on every reboot if SID access
	 * is unavailable in U-Boot.
	 */
	{
		struct mmc *mmc;
		int dev = board_mmc_get_num();

		mmc = find_mmc_device(dev);
		if (!mmc)
			mmc = find_mmc_device(0);

		if (mmc) {
			mmc_init(mmc);
			seed[0] = mmc->cid[0];
			seed[1] = mmc->cid[1];
			seed[2] = mmc->cid[2];
			seed[3] = mmc->cid[3];
			if (sunxi_seed_is_valid(seed)) {
				if (src)
					*src = "mmc-cid";
				return 0;
			}
		}
	}
#endif

	return -ENODEV;
}

static int sunxi_generate_eth_mac(unsigned int index, uint8_t *mac)
{
	uint32_t seed[4];
	uint32_t hash;
	const char *src = "unknown";
	int ret;

	ret = sunxi_get_eth_mac_seed(seed, &src);
	if (ret)
		return ret;

	sunxi_dump_seed(src, seed);

	/* Derive a stable locally administered unicast MAC from the best available
	 * unique seed.  This keeps eth0/eth1 deterministic and distinct.
	 */
	hash = crc32(0, (unsigned char *)seed, sizeof(seed));
	if ((hash & 0xffffff) == 0)
		hash |= 0x800000;

	mac[0] = 0x02;
	mac[1] = ((seed[0] >> 0) ^ (seed[1] >> 8) ^ (hash >> 24)) & 0xff;
	mac[2] = (hash >> 24) & 0xff;
	mac[3] = (hash >> 16) & 0xff;
	mac[4] = (hash >> 8) & 0xff;
	mac[5] = hash & 0xff;

	while (index--)
		sunxi_mac_addr_inc(mac);

	return 0;
}

static void sunxi_print_mac(const char *tag, const uint8_t *mac)
{
	printf("%s %02x:%02x:%02x:%02x:%02x:%02x\n", tag,
	       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/*
 * Note this function gets called multiple times.
 * It must not make any changes to env variables which already exist.
 */
static void setup_environment(const void *fdt)
{
	uint8_t mac_addr[ARP_HLEN];
	uint8_t env_mac[ARP_HLEN];

	(void)fdt;

	if (!eth_env_get_enetaddr("ethaddr", env_mac)) {
		if (!sunxi_generate_eth_mac(0, mac_addr)) {
			eth_env_set_enetaddr("ethaddr", mac_addr);
			sunxi_print_mac("sunxi: generated ethaddr from SID", mac_addr);
		}
	}

	/* U-Boot standard name for the second Ethernet port is eth1addr. */
	if (!eth_env_get_enetaddr("eth1addr", env_mac)) {
		if (!sunxi_generate_eth_mac(1, mac_addr)) {
			eth_env_set_enetaddr("eth1addr", mac_addr);
			sunxi_print_mac("sunxi: generated eth1addr from SID", mac_addr);
		}
	}
}

static const char *sunxi_eth_alias_fallback_path(const char *alias)
{
	/* A527 dual Ethernet paths used by sun55i-a527-ahd-a527.dts. */
	if (!strcmp(alias, "ethernet0"))
		return "/soc@3000000/gmac0@4500000";
	if (!strcmp(alias, "ethernet1"))
		return "/soc@3000000/ethernet@4510000";

	return NULL;
}

static int sunxi_fdt_set_eth_mac(void *fdt, const char *alias,
				 const char *env_name, unsigned int index)
{
	const char *path;
	unsigned char mac[ARP_HLEN];
	int node;
	int ret;

	if (!fdt || fdt_check_header(fdt)) {
		printf("sunxi: invalid fdt for %s mac fixup\n", alias);
		return -1;
	}

	path = fdt_get_alias(fdt, alias);
	if (!path)
		path = sunxi_eth_alias_fallback_path(alias);

	if (!path) {
		printf("sunxi: no alias %s in kernel fdt\n", alias);
		return -1;
	}

	/*
	 * Prefer an explicitly configured environment MAC if present, but do not
	 * depend on it.  In this vendor boot flow the environment may be loaded
	 * without ethaddr/eth1addr, and relying on env caused Linux to receive no
	 * mac-address/local-mac-address at all.
	 */
	if (eth_env_get_enetaddr(env_name, mac)) {
		printf("sunxi: use %s from env\n", env_name);
	} else {
		ret = sunxi_generate_eth_mac(index, mac);
		if (ret) {
			printf("sunxi: cannot generate %s mac, ret=%d\n", alias, ret);
			return ret;
		}
		printf("sunxi: no valid %s in env, use SID derived mac\n", env_name);
	}

	node = fdt_path_offset(fdt, path);
	if (node < 0) {
		printf("sunxi: alias %s path %s not found, ret=%d\n",
		       alias, path, node);
		return node;
	}

	ret = fdt_setprop(fdt, node, "mac-address", mac, sizeof(mac));
	if (ret) {
		printf("sunxi: set %s mac-address failed, ret=%d\n",
		       alias, ret);
		return ret;
	}

	ret = fdt_setprop(fdt, node, "local-mac-address", mac, sizeof(mac));
	if (ret) {
		printf("sunxi: set %s local-mac-address failed, ret=%d\n",
		       alias, ret);
		return ret;
	}

	printf("sunxi: set %s %s mac %02x:%02x:%02x:%02x:%02x:%02x\n",
	       alias, path, mac[0], mac[1], mac[2],
	       mac[3], mac[4], mac[5]);

	return 0;
}

static void sunxi_fdt_fixup_eth_macs(void *fdt)
{
	int ret;

	if (!fdt || fdt_check_header(fdt)) {
		printf("sunxi: skip ethernet mac fixup, invalid fdt\n");
		return;
	}

	/* Add room for mac-address and local-mac-address properties. */
	ret = fdt_increase_size(fdt, 512);
	if (ret)
		printf("sunxi: fdt increase size failed, ret=%d\n", ret);

	sunxi_fdt_set_eth_mac(fdt, "ethernet0", "ethaddr", 0);
	sunxi_fdt_set_eth_mac(fdt, "ethernet1", "eth1addr", 1);
}

int misc_init_r(void)
{
	__maybe_unused int ret;

	setup_environment(gd->fdt_blob);

	/*
	 * This fixes U-Boot's own DTB copy. The final kernel DTB is still
	 * fixed in ft_board_setup(). Keeping both paths makes the fix robust
	 * against vendor boot flows that reuse gd->fdt_blob.
	 */
	sunxi_fdt_fixup_eth_macs((void *)gd->fdt_blob);

#if 0
	ret = sunxi_usb_phy_probe();
	if (ret)
		return ret;
#endif
#ifdef CONFIG_USB_ETHER
	usb_ether_init();
#endif
	return 0;
}

int ft_board_setup(void *blob, bd_t *bd)
{
	int __maybe_unused r;

	/*
	 * Call setup_environment again in case the boot fdt has
	 * ethernet aliases the u-boot copy does not have.
	 */
	setup_environment(blob);

	/* Force MAC addresses into the final DTB passed to Linux. */
	sunxi_fdt_fixup_eth_macs(blob);

#ifdef CONFIG_VIDEO_DT_SIMPLEFB
	r = sunxi_simplefb_setup(blob);
	if (r)
		return r;
#endif
	return 0;
}

static int reserve_bootlogo(void)
{
#ifndef FPGA_PLATFORM
	uint32_t *compressed_logo_size =
		(uint32_t *)(CONFIG_SYS_TEXT_BASE + (3 * 1024 * 1024));
	uint32_t *compressed_logo_buf =
		(uint32_t *)(CONFIG_SYS_TEXT_BASE + (3 * 1024 * 1024) + 16);

	gd->boot_logo_addr = 0;
	if (get_boot_work_mode() != WORK_MODE_BOOT) {
		debug("no boot mode, dont read bootlogo\n");
		return 0;
	}

	if (((uboot_spare_head.boot_data.func_mask & UBOOT_FUNC_MASK_BIT_BOOTLOGO) !=
	     UBOOT_FUNC_MASK_BIT_BOOTLOGO) || (*compressed_logo_size == 0)) {
		debug("no bootlogo from boot_package\n");
		return 0;
	}

	gd->start_addr_sp -= ALIGN(*compressed_logo_size, 16);
	gd->boot_logo_addr =
		(ulong)map_sysmem(gd->start_addr_sp, *compressed_logo_size);

	/* reserve logo_buf size addr */
	gd->start_addr_sp -= 16;
	*(uint *)(gd->boot_logo_addr - 16) = *compressed_logo_size;
	memcpy((void *)gd->boot_logo_addr, compressed_logo_buf,
	       *compressed_logo_size);
	debug("reserve: 0x%lx from 0x%x: for boot logo in boot package\n",
	      gd->boot_logo_addr, *compressed_logo_size);
#endif
	return 0;
}

int reserve_arch(void)
{
	reserve_bootlogo();
	return 0;
}
