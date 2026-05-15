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
#include <asm/arch/clock.h>
#include <asm/arch-sunxi/cpu.h>
#include <asm/arch/gpio.h>
#include <malloc.h>
#include <linux/libfdt.h>
#include <fdt_support.h>
#include <private_uboot.h>
#include <dm/read.h>
#include <sunxi_mmc.h>
#include <mmc_def.h>

#include "sunxi_mmc_host_common.h"
#include "sunxi_mmc_host_tm0.h"
#include "sunxi_mmc_host_tm1.h"
#include "sunxi_mmc_host_tm4.h"
#include "sunxi_mmc_host_tm5.h"

extern char *spd_name[];

int sunxi_auto_pow_mode(unsigned int bit)
{
	printf("-------sunxi_auto_pow_mode\n");
	return 0;
}

static int mmc_clear_timing_para(struct sunxi_mmc_priv *priv)
{
	struct sunxi_mmc_priv *mmcpriv = priv;
	int i, j;

	for (i = 0; i < MAX_SPD_MD_NUM; i++) {
		for (j = 0; j < MAX_CLK_FREQ_NUM; j++) {
			mmcpriv->tm1.odly[i*MAX_CLK_FREQ_NUM+j] = 0xFF;
			mmcpriv->tm1.sdly[i*MAX_CLK_FREQ_NUM+j] = 0xFF;
			mmcpriv->tm1.def_odly[i*MAX_CLK_FREQ_NUM+j] = 0xFF;
			mmcpriv->tm1.def_sdly[i*MAX_CLK_FREQ_NUM+j] = 0xFF;
			mmcpriv->tm3.odly[i*MAX_CLK_FREQ_NUM+j] = 0xFF;
			mmcpriv->tm3.sdly[i*MAX_CLK_FREQ_NUM+j] = 0xFF;
			mmcpriv->tm3.def_odly[i*MAX_CLK_FREQ_NUM+j] = 0xFF;
			mmcpriv->tm3.def_sdly[i*MAX_CLK_FREQ_NUM+j] = 0xFF;
			mmcpriv->tm4.odly[i*MAX_CLK_FREQ_NUM+j] = 0xFF;
			mmcpriv->tm4.sdly[i*MAX_CLK_FREQ_NUM+j] = 0xFF;
			mmcpriv->tm4.def_odly[i*MAX_CLK_FREQ_NUM+j] = 0xFF;
			mmcpriv->tm4.def_sdly[i*MAX_CLK_FREQ_NUM+j] = 0xFF;
			mmcpriv->tm5.odly[i*MAX_CLK_FREQ_NUM+j] = 0xFF;
			mmcpriv->tm5.sdly[i*MAX_CLK_FREQ_NUM+j] = 0xFF;
			mmcpriv->tm5.def_odly[i*MAX_CLK_FREQ_NUM+j] = 0xFF;
			mmcpriv->tm5.def_sdly[i*MAX_CLK_FREQ_NUM+j] = 0xFF;
		}
	}

	for (j = 0; j < MAX_CLK_FREQ_NUM; j++) {
		mmcpriv->tm4.dsdly[j] = 0xFF;
		mmcpriv->tm4.def_dsdly[j] = 0xFF;
		mmcpriv->tm5.dsdly[j] = 0xFF;
		mmcpriv->tm5.def_dsdly[j] = 0xFF;
	}

	return 0;
}

static int mmc_init_default_timing_para(struct sunxi_mmc_priv *priv)
{
	struct sunxi_mmc_priv *mmcpriv = priv;
	int ret = 0;

	ret = mmcpriv->mmc_init_default_timing_para(priv);

	return ret;
}

static int mmc_get_sdly_auto_sample(struct sunxi_mmc_priv *priv)
{
#if 1
	struct sunxi_mmc_priv *mmcpriv = priv;
	u32 *p = (u32 *)&mmcpriv->cfg->sdly.tm4_smx_fx[0];
	int tm = mmcpriv->timing_mode;
	u8 *sdly = NULL;
	u8 *dsdly = NULL;
	int spd_md, f;
	u32 val;
	int work_mode = uboot_spare_head.boot_data.work_mode;
	struct boot_sdmmc_private_info_t *priv_info =
		(struct boot_sdmmc_private_info_t *)(uboot_spare_head.boot_data.sdcard_spare_data);

/*
	if (sdc_no != 2) {
		MMCINFO("don't support auto sample\n");
		return -1;
	}
*/
	/* sdly is invalid */
	if (work_mode != WORK_MODE_BOOT) {
		MMCINFO("Is not Boot mode!\n");
		return 0;
	}

	if (!(((priv_info->ext_para0 & 0xFF000000) == EXT_PARA0_ID)
		&& (priv_info->ext_para0 & EXT_PARA0_TUNING_SUCCESS_FLAG))) {
		MMCINFO("Is not EXT_PARA0_ID or EXT_PARA0_TUNING_SUCCESS_FLAG!\n");
		return 0;
	}

#if 0
	for (f = 0; f < 5; f++)
		MMCINFO("0x%x 0x%x\n", p[f*2 + 0], p[f*2 + 1]);
#endif
	if (tm == SUNXI_MMC_TIMING_MODE_4) {
		sdly = mmcpriv->tm4.sdly;
		dsdly = mmcpriv->tm4.dsdly;
	} else if (tm == SUNXI_MMC_TIMING_MODE_5) {
		sdly = mmcpriv->tm5.sdly;
		dsdly = mmcpriv->tm5.dsdly;
	}

	for (spd_md = 0; spd_md < MAX_SPD_MD_NUM; spd_md++) {
		if (spd_md == HS400) {
			val = p[spd_md*2 + 0];
			for (f = 0; f < 4; f++) {
				dsdly[f] = (val>>(f*8)) & 0xFF;
				/*MMCINFO("hs400 dsdly-0 0x%x\n", mmcpriv->tm4.dsdly[f]);*/
			}

			val = p[spd_md*2 + 1];
			for (f = 4; f < MAX_CLK_FREQ_NUM; f++) {
				dsdly[f] = (val>>((f-4)*8)) & 0xFF;
				/*MMCINFO("hs400 dsdly-1 0x%x\n", mmcpriv->tm4.dsdly[f]);*/
			}

			val = p[spd_md*2 + 2 + 0];
			for (f = 0; f < 4; f++) {
				sdly[spd_md*MAX_CLK_FREQ_NUM + f] = (val>>(f*8)) & 0xFF;
				/*MMCINFO("hs400 sdly-0 0x%x\n", mmcpriv->tm4.sdly[spd_md*MAX_CLK_FREQ_NUM + f]);*/
			}

			val = p[spd_md*2 + 2 + 1];
			for (f = 4; f < MAX_CLK_FREQ_NUM; f++) {
				sdly[spd_md*MAX_CLK_FREQ_NUM + f] = (val>>((f-4)*8)) & 0xFF;
				/*MMCINFO("hs400 sdly-1 0x%x\n", mmcpriv->tm4.sdly[spd_md*MAX_CLK_FREQ_NUM + f]);*/
			}
		} else {
			val = p[spd_md*2 + 0];
			for (f = 0; f < 4; f++) {
				sdly[spd_md*MAX_CLK_FREQ_NUM + f] = (val>>(f*8)) & 0xFF;
				/*MMCINFO("sdly-0 0x%x\n", mmcpriv->tm4.sdly[spd_md*MAX_CLK_FREQ_NUM + f]);*/
			}

			val = p[spd_md*2 + 1];
			for (f = 4; f < MAX_CLK_FREQ_NUM; f++) {
				sdly[spd_md*MAX_CLK_FREQ_NUM + f] = (val>>((f-4)*8)) & 0xFF;
				/*MMCINFO("sdly-1 0x%x\n", mmcpriv->tm4.sdly[spd_md*MAX_CLK_FREQ_NUM + f]);*/
			}
		}

	}
	return 0;
#else
	MMCINFO("don't support auto sample\n");
	return 0;
#endif

}

static int mmc_get_dly_manual_sample(struct sunxi_mmc_priv *priv)
{
#if 0
	struct sunxi_mmc_priv *mmcpriv = priv;
	struct mmc_config *cfg = &mmcpriv->cfg;
	int imd, ifreq;

	if (mmcpriv->mmc_no == 2) {
		if (mmcpriv->timing_mode == SUNXI_MMC_TIMING_MODE_1) {
			struct sunxi_mmc_timing_mode1 *p = &mmcpriv->tm1;

			for (imd = 0; imd < MAX_SPD_MD_NUM; imd++) {
				for (ifreq = 0; ifreq < MAX_CLK_FREQ_NUM; ifreq++) {
					//MMCINFO("%d %d\n", cfg->odly_spd_freq[imd*MAX_CLK_FREQ_NUM + ifreq], cfg->sdly_spd_freq[imd*MAX_CLK_FREQ_NUM + ifreq]);
					p->odly[imd*MAX_CLK_FREQ_NUM + ifreq] = cfg->odly_spd_freq[imd*MAX_CLK_FREQ_NUM + ifreq];
					p->sdly[imd*MAX_CLK_FREQ_NUM + ifreq] = cfg->sdly_spd_freq[imd*MAX_CLK_FREQ_NUM + ifreq];
				}
				//MMCINFO("\n");
			}
		} else {
			MMCINFO("sdc %d timing mode %d error --1\n", sdc_no, mmcpriv->timing_mode);
			return -1;
		}
	} else if (mmcpriv->mmc_no == 0) {
		if (mmcpriv->timing_mode == SUNXI_MMC_TIMING_MODE_0) {
			struct sunxi_mmc_timing_mode0 *p = &mmcpriv->tm0;

			for (imd = 0; imd < MAX_SPD_MD_NUM; imd++) {
				for (ifreq = 0; ifreq < MAX_CLK_FREQ_NUM; ifreq++) {
					//MMCINFO("%d %d\n", cfg->odly_spd_freq[imd*MAX_CLK_FREQ_NUM + ifreq], cfg->sdly_spd_freq[imd*MAX_CLK_FREQ_NUM + ifreq]);
					p->odly[imd*MAX_CLK_FREQ_NUM + ifreq] = cfg->odly_spd_freq[imd*MAX_CLK_FREQ_NUM + ifreq];
					p->sdly[imd*MAX_CLK_FREQ_NUM + ifreq] = cfg->sdly_spd_freq[imd*MAX_CLK_FREQ_NUM + ifreq];
				}
				//MMCINFO("\n");
			}
		} else {
			MMCINFO("sdc %d timing mode %d error --2\n", sdc_no, mmcpriv->timing_mode);
			return -1;
		}
	} else {
		MMCINFO("mmc_get_dly_manual_sample: error sdc_no %d\n", sdc_no);
		return -1;
	}
#endif
	return 0;
}

static int mmc_update_timing_para(struct sunxi_mmc_priv *priv)
{
	int ret = 0;
	struct sunxi_mmc_priv *mmcpriv = priv;
	struct mmc_config *p = mmcpriv->cfg;

	if (p->sample_mode == AUTO_SAMPLE_MODE) {
		ret = mmc_get_sdly_auto_sample(mmcpriv);
		if (ret) {
			MMCINFO("update auto sample timing para fail!\n");
		}
	} else if (p->sample_mode == MAUNAL_SAMPLE_MODE) {
		ret = mmc_get_dly_manual_sample(mmcpriv);
		if (ret) {
			MMCINFO("update manual sample timing para fail!\n");
		}
	} else {
		MMCINFO("Using default timing para\n");
		mmcpriv->tm1.odly[HSDDR52_DDR50 * MAX_CLK_FREQ_NUM + CLK_50M] =
			mmcpriv->tm1.def_odly[HSDDR52_DDR50 * MAX_CLK_FREQ_NUM + CLK_50M];
		mmcpriv->tm1.sdly[HSDDR52_DDR50 * MAX_CLK_FREQ_NUM + CLK_50M] =
			mmcpriv->tm1.def_sdly[HSDDR52_DDR50* MAX_CLK_FREQ_NUM + CLK_50M];
		mmcpriv->tm4.odly[HSSDR52_SDR25 * MAX_CLK_FREQ_NUM + CLK_50M] =
			mmcpriv->tm4.def_odly[HSSDR52_SDR25 * MAX_CLK_FREQ_NUM + CLK_50M];
		mmcpriv->tm4.sdly[HSSDR52_SDR25 * MAX_CLK_FREQ_NUM + CLK_50M] =
			mmcpriv->tm4.def_sdly[HSSDR52_SDR25* MAX_CLK_FREQ_NUM + CLK_50M];
		mmcpriv->tm5.odly[HSSDR52_SDR25 * MAX_CLK_FREQ_NUM + CLK_50M] =
			mmcpriv->tm5.def_odly[HSSDR52_SDR25 * MAX_CLK_FREQ_NUM + CLK_50M];
		mmcpriv->tm5.sdly[HSSDR52_SDR25 * MAX_CLK_FREQ_NUM + CLK_50M] =
			mmcpriv->tm5.def_sdly[HSSDR52_SDR25* MAX_CLK_FREQ_NUM + CLK_50M];

	}

	return ret;
}

static int sunxi_mmc_host_init(struct udevice *dev, struct sunxi_mmc_priv *priv)
{
	struct sunxi_mmc_host_table host_str[] = {
		{"tm0", sunxi_mmc_host_tm0_init},
		{"tm1", sunxi_mmc_host_tm1_init},
		{"tm4", sunxi_mmc_host_tm4_init},
		{"tm5", sunxi_mmc_host_tm5_init}
	};
	int i;
	int sdc_no = priv->mmc_no;
	const char *tm_str;

	tm_str = dev_read_string(dev, "sdc_type");
	if (tm_str) {
		MMCINFO("current host is %s\n", tm_str);
	} else {
#if (defined (CONFIG_MACH_SUN8IW7))
		if ((sdc_no == 0) || (sdc_no == 1))
			tm_str = "tm0";
		else if (sdc_no == 2)
			tm_str = "tm1";
#elif (defined (CONFIG_MACH_SUN50IW11))
		if ((sdc_no == 0) || (sdc_no == 1))
			tm_str = "tm4";
#else
		if ((sdc_no == 0) || (sdc_no == 1))
#if defined(CONFIG_MACH_SUN65IW1)
//TODO:temporary plan,there is a hardware bug
			tm_str = "tm4";
#else
			tm_str = "tm1";
#endif
		else if (sdc_no == 2)
			tm_str = "tm4";
#endif
		else {
			MMCINFO("unknown device!\n");
			return -1;
		}
		MMCINFO("get sdc_type fail and use default host:%s.\n", tm_str);
	}

	for (i = 0; i < ARRAY_SIZE(host_str); i++) {
		if (strcmp(host_str[i].tm_str, tm_str) == 0) {
			MMCDBG("using the host of %s\n", host_str[i].tm_str);
			host_str[i].sunxi_mmc_host_init_priv(priv);
			return 0;
		}
	}

	MMCINFO("Can not find the proper host!\n");
	return -1;
}

static void mmc_get_para_from_fex(struct udevice *dev, struct sunxi_mmc_priv *priv)
{
	int ret = 0;
	uint32_t rval;
	//int rval_ker, ret1 = 0;
	struct sunxi_mmc_priv *mmcpriv = priv;
	struct mmc_config *cfg = mmcpriv->cfg;
#if 0
	struct sunxi_mmc_pininfo *pin_default = &mmcpriv->pin_default;
	struct sunxi_mmc_pininfo *pin_disable = &mmcpriv->pin_disable;
	user_gpio_set_t card_pwr_gpio;
#endif
	int tm = mmcpriv->timing_mode;
	int i, imd, ifreq;
	char ctmp[30];
	const char *tm_str;
#if 0
	int nodeoffset = 0;
	u32 handle[10] = {0};
	u32 rval_64bit_H32, rval_64bit_L32;
#endif

	if (priv->mmc_no == 0) {
#if 0
		nodeoffset =  fdt_path_offset(working_fdt, FDT_PATH_CARD0_BOOT_PARA);
		if (nodeoffset < 0) {
			MMCINFO("get card0 para fail\n");
			return ;
		}

		ret = fdt_getprop_u32(working_fdt, nodeoffset, "card_line",
				(uint32_t *)(&rval));
		if (ret < 0) {
			MMCDBG("get card0_boot_para:card_line fail\n");
		} else {
			if (rval == 8) {
				cfg->host_caps |= MMC_MODE_4BIT;
				cfg->host_caps |= MMC_MODE_8BIT;
				MMCDBG("card0 use 8bit!\n");
			} else if (rval == 4) {
				cfg->host_caps |= MMC_MODE_4BIT;
				cfg->host_caps &= ~MMC_MODE_8BIT;
				MMCDBG("card0 use 4bit!\n");
			} else if (rval == 1) {
				cfg->host_caps &= ~MMC_MODE_4BIT;
				cfg->host_caps &= ~MMC_MODE_8BIT;
				MMCDBG("card0 use 1bit!\n");
			} else {
				cfg->host_caps |= MMC_MODE_4BIT;
				cfg->host_caps &= ~MMC_MODE_8BIT;
				MMCINFO("input card_line error, use default 4bit!\n");
			}
		}

		ret = dev_read_u32(dev, "sdc_io_1v8", &rval);
		if (ret != 0) {
			MMCDBG("get card0_boot_para:sdc_io_1v8 fail\n");
		} else {
			if (rval == 1) {
				cfg->io_is_1v8 = 1;
			} else {
				MMCDBG("card0 try use io 3V.\n");
			}
		}
#endif

#if 0
		pin_default->pin_count = fdt_get_all_pin(nodeoffset, "pinctrl-0", pin_default->pin_set);
			if (pin_default->pin_count <= 0) {
				MMCINFO("get card0 default pin fail\n");
				return ;
		}

		ret = sunxi_auto_pow_mode(pin_default->pin_set[0].port-1);
		if (!ret)
			cfg->io_is_1v8 = 0;

		/*avoid the error print from fdt_get_all_pin*/
		pin_disable->pin_count = fdt_getprop_u32(working_fdt, nodeoffset, "pinctrl-1", handle);
		if (pin_disable->pin_count >= 0) {
			pin_disable->pin_count = fdt_get_all_pin(nodeoffset, "pinctrl-1", pin_disable->pin_set);
			if (pin_disable->pin_count <= 0)
				MMCINFO("get card0 disable pin fail\n");
		}
#endif
		/* set gpio */
		//gpio_request_simple("card0_boot_para", NULL);

		//ret = script_parser_fetch("card0_boot_para", "sdc_wipe", &rval, 1);
		ret = dev_read_u32(dev, "sdc_wipe", &rval);
		if (ret != 0)
			MMCDBG("get sdc_wipe fail.\n");
		else {
			if (rval & DRV_PARA_DISABLE_SECURE_WIPE) {
				MMCDBG("disable driver secure wipe operation.\n");
				cfg->drv_wipe_feature |= DRV_PARA_DISABLE_SECURE_WIPE;
			}
			if (rval & DRV_PARA_DISABLE_EMMC_SANITIZE) {
				MMCDBG("disable emmc sanitize feature.\n");
				cfg->drv_wipe_feature |= DRV_PARA_DISABLE_EMMC_SANITIZE;
			}
			if (rval & DRV_PARA_DISABLE_EMMC_SECURE_PURGE) {
				MMCDBG("disable emmc secure purge feature.\n");
				cfg->drv_wipe_feature |= DRV_PARA_DISABLE_EMMC_SECURE_PURGE;
			}
			if (rval & DRV_PARA_DISABLE_EMMC_TRIM) {
				MMCDBG("disable emmc trim feature.\n");
				cfg->drv_wipe_feature |= DRV_PARA_DISABLE_EMMC_TRIM;
			}
		}

		//ret = script_parser_fetch("card0_boot_para", "sdc_erase", &rval, 1);
		ret = dev_read_u32(dev, "sdc_erase", &rval);
		if (ret != 0)
			MMCDBG("get sdc0 sdc_erase fail.\n");
		else {
			if (rval & DRV_PARA_DISABLE_EMMC_ERASE) {
				MMCDBG("disable emmc erase.\n");
				cfg->drv_erase_feature |= DRV_PARA_DISABLE_EMMC_ERASE;
			}
			if (rval & DRV_PARA_ENABLE_EMMC_SANITIZE_WHEN_ERASE) {
				MMCDBG("enable emmc sanitize when erase.\n");
				cfg->drv_erase_feature |= DRV_PARA_ENABLE_EMMC_SANITIZE_WHEN_ERASE;
			}
		}

		//ret = script_parser_fetch("card0_boot_para", "sdc_boot", &rval, 1);
		ret = dev_read_u32(dev, "sdc_boot", &rval);
		if (ret != 0)
			MMCDBG("get sdc0 sdc_boot fail.\n");
		else {
			if (rval & DRV_PARA_NOT_BURN_USER_PART) {
				MMCDBG("don't burn boot0 to user part.\n");
				cfg->drv_burn_boot_pos |= DRV_PARA_NOT_BURN_USER_PART;
			}
			if (rval & DRV_PARA_BURN_EMMC_BOOT_PART) {
				MMCDBG("burn boot0 to emmc boot part.\n");
				cfg->drv_burn_boot_pos |= DRV_PARA_BURN_EMMC_BOOT_PART;
			}
		}

		ret = dev_read_u32(dev, "sdc_odly_50M", &rval);
		if (ret != 0) {
			MMCDBG("get sdc0 sdc_odly_50M fail.\n");
			cfg->boot_odly_50M = 0xff;
		} else {
			MMCDBG("get sdc0 sdc_odly_50M %d.\n", rval);
			cfg->boot_odly_50M = rval;
		}

		ret = dev_read_u32(dev, "sdc_sdly_50M", &rval);
		if (ret != 0) {
			MMCDBG("get sdc0 sdc_sdly_50M fail.\n");
			cfg->boot_sdly_50M = 0xff;
		} else {
			MMCDBG("get sdc0 sdc_sdly_50M %d.\n", rval);
			cfg->boot_sdly_50M = rval;

		}

		ret = dev_read_u32(dev, "sdc_odly_50M_ddr", &rval);
		if (ret != 0) {
			MMCDBG("get sdc0 sdc_odly_50M_ddr fail.\n");
			cfg->boot_odly_50M_ddr = 0xff;
		} else {
			MMCDBG("get sdc0 sdc_odly_50M_ddr %d.\n", rval);
			cfg->boot_odly_50M_ddr = rval;
		}

		ret = dev_read_u32(dev, "sdc_sdly_50M_ddr", &rval);
		if (ret != 0) {
			MMCDBG("get sdc0 sdc_sdly_50M_ddr fail.\n");
			cfg->boot_sdly_50M_ddr = 0xff;
		} else {
			MMCDBG("get sdc0 sdc_sdly_50M_ddr %d.\n", rval);
			cfg->boot_sdly_50M_ddr = rval;
		}

		ret = dev_read_u32(dev, "sdc_freq", &rval);
		if (ret != 0) {
			MMCDBG("get sdc0 sdc_freq fail.\n");
			cfg->boot_hs_f_max = 0x0;
		} else {
			if (rval >= 50)
				cfg->boot_hs_f_max = 50;
			else
				cfg->boot_hs_f_max = rval;
			MMCDBG("get sdc0 sdc_freq %d.%d\n", rval, cfg->boot_hs_f_max);
		}

		ret = dev_read_u32(dev, "sdc_b0p", &rval);
		if (ret != 0) {
			MMCDBG("get sdc0 sdc_b0p fail.\n");
			cfg->boot0_para = 0x0;
		} else {
			MMCDBG("get sdc0 sdc_b0p %d.\n", rval);
			cfg->boot0_para = rval;
		}

		ret = dev_read_u32(dev, "sdc_ex_dly_used", &rval);
		if (ret != 0) {
			MMCDBG("get card0_boot_para:sdc_ex_dly_used fail\n");

			/* fill invalid sample config */
			memset(mmcpriv->cfg->sdly.tm4_smx_fx,
					0xff, sizeof(mmcpriv->cfg->sdly.tm4_smx_fx));
		}
		MMCDBG("get sdc_ex_dly_used %d, use auto tuning sdly\n", rval);

		if (ret == 0) {
			int work_mode = uboot_spare_head.boot_data.work_mode;
			struct boot_sdmmc_private_info_t *priv_info =
				(struct boot_sdmmc_private_info_t *)(uboot_spare_head.boot_data.sdcard_spare_data);
			struct tune_sdly *sdly = &(priv_info->tune_sdly);
			u32 *p = (u32 *)&mmcpriv->cfg->sdly.tm4_smx_fx[0];

			if (rval == 2) { //only when kernal use auto sample,uboot will use auto sample.
				cfg->sample_mode = AUTO_SAMPLE_MODE;
				MMCDBG("get sdc_ex_dly_used %d, use auto tuning sdly\n", rval);
				if (work_mode != WORK_MODE_BOOT) {
					/*usb product will auto get sample point, no need to get auto sdly, so first used default value*/
					MMCDBG("current is product mode, it will tune sdly later\n");
				} else {
					/* copy sample point cfg from uboot header to internal variable */
					if (((priv_info->ext_para0 & 0xFF000000) == EXT_PARA0_ID)
						&& (priv_info->ext_para0 & EXT_PARA0_TUNING_SUCCESS_FLAG)) {
						if (sdly != NULL)
							memcpy(p, sdly, sizeof(struct tune_sdly));
						else
							memset(p, 0xFF, sizeof(struct tune_sdly));
					} else {
						memset(p, 0xFF, sizeof(struct tune_sdly));
						MMCINFO("get sdly from uboot header fail\n");
					}
				}
			} else if (rval == 1) {  /* maual sample point from fex */
				cfg->sample_mode = MAUNAL_SAMPLE_MODE;
				MMCDBG("get sdc_ex_dly_used %d, use manual sdly in fex\n", rval);
			} else {
				cfg->sample_mode = 0x0;
				MMCINFO("undefined value %d, use default dly\n", rval);
			}
		}
#if 0
		ret = fdt_get_one_gpio(FDT_PATH_CARD0_BOOT_PARA, "card-pwr-gpios",
							&card_pwr_gpio);
		if (ret < 0) {
			MMCDBG("get card-pwr-gpios faild\n");
			mmcpriv->pwr_handler = 0;
		} else {
			mmcpriv->pwr_handler = sunxi_gpio_request(&card_pwr_gpio, 1);
			MMCDBG("get card-pwr-gpios handler:%d\n", mmcpriv->pwr_handler);
		}
#endif
		ret = dev_read_u32(dev, "time_pwroff_ms", &rval);
		if (ret != 0) {
			mmcpriv->time_pwroff = 200;
			MMCDBG("get card0_boot_para:time_pwroff_ms:use default time_pwroff:%d\n",
				mmcpriv->time_pwroff);
		} else {
			mmcpriv->time_pwroff = rval;
			MMCDBG("get card0_boot_para:time_pwroff:%dms\n", mmcpriv->time_pwroff);
		}

		if (cfg->sample_mode == MAUNAL_SAMPLE_MODE) {
			for (imd = 0; imd < MAX_SPD_MD_NUM; imd++) {
				for (ifreq = 0; ifreq < MAX_CLK_FREQ_NUM; ifreq++) {
					sprintf(ctmp, "sdc_odly_%d_%d", imd, ifreq);
					ret = dev_read_u32(dev, ctmp, &rval);
					if (ret != 0) {
						MMCDBG("get sdc0 %s fail.\n", ctmp);
						cfg->odly_spd_freq[imd*MAX_CLK_FREQ_NUM + ifreq] = 0x0;
					} else {
						cfg->odly_spd_freq[imd*MAX_CLK_FREQ_NUM + ifreq] = rval&0x1;
						MMCDBG("get sdc0 %s 0x%x for %d-%s %d.\n", ctmp,
								cfg->odly_spd_freq[imd*MAX_CLK_FREQ_NUM + ifreq], imd, spd_name[imd], ifreq);
					}

					sprintf(ctmp, "sdc_sdly_%d_%d", imd, ifreq);
					ret = dev_read_u32(dev, ctmp, &rval);
					if (ret != 0) {
						MMCDBG("get sdc0 %s fail.\n", ctmp);
						cfg->sdly_spd_freq[imd*MAX_CLK_FREQ_NUM + ifreq] = 0xff;
					} else {
						cfg->sdly_spd_freq[imd*MAX_CLK_FREQ_NUM + ifreq] = rval&0xff;
						MMCDBG("get sdc0 %s 0x%x for %d-%s %d.\n", ctmp,
								cfg->sdly_spd_freq[imd*MAX_CLK_FREQ_NUM + ifreq], imd, spd_name[imd], ifreq);
					}
				}
			}
		}

		ret = dev_read_u32(dev, "sdc_force_boot_tuning", &rval);
		if (ret != 0)
			MMCDBG("get card0_boot_para:sdc_force_boot_tuning fail\n");
		else {
			if (rval == 1) {
				MMCDBG("card0 force to execute tuning during boot.\n");
				cfg->force_boot_tuning = 1;
			} else {
				MMCDBG("card0 don't force to execute tuning during boot.\n");
				cfg->force_boot_tuning = 0;
			}
		}

		ret = dev_read_u32(dev, "sdc_tm4_win_th", &rval);
		if (ret != 0) {
			MMCDBG("get sdc0 sdc_tm4_win_th fail.\n");
			if (tm == SUNXI_MMC_TIMING_MODE_5)
				cfg->tm4_timing_window_th = 5;
			else
				cfg->tm4_timing_window_th = 12;
		} else {
			if ((rval < 4) || (rval > 64))
				cfg->tm4_timing_window_th = 12;
			else
				cfg->tm4_timing_window_th = rval;
			MMCDBG("get sdc0 sdc_tm4_win_th %d.\n", cfg->tm4_timing_window_th);
		}

		ret = dev_read_u32(dev, "sdc_tm4_hs200_max_freq", &rval);
		if (ret != 0) {
			MMCDBG("get sdc0 sdc_tm4_hs200_max_freq fail.\n");
			cfg->tm4_tune_hs200_max_freq = 150;
		} else {
			if ((rval < 50) || (rval > 200))
				cfg->tm4_tune_hs200_max_freq = 0x0;
			else
				cfg->tm4_tune_hs200_max_freq = rval;
			MMCDBG("get sdc0 sdc_tm4_hs200_max_freq %d.\n", cfg->tm4_tune_hs200_max_freq);
		}

		ret = dev_read_u32(dev, "sdc_tm4_hs400_max_freq", &rval);
		if (ret != 0) {
			MMCDBG("get sdc0 sdc_tm4_hs400_max_freq fail.\n");
			cfg->tm4_tune_hs400_max_freq = 100;
		} else {
			if ((rval < 50) || (rval > 200))
				cfg->tm4_tune_hs400_max_freq = 0x0;
			else
				cfg->tm4_tune_hs400_max_freq = rval;
			MMCDBG("get sdc0 sdc_tm4_hs400_max_freq %d.\n", cfg->tm4_tune_hs400_max_freq);
		}

		/* speed mode caps */
		ret = dev_read_u32(dev, "sdc_dis_host_caps", &rval);
		if (ret != 0) {
			MMCDBG("get sdc0 sdc_dis_host_caps fail.\n");
			cfg->host_caps_mask = 0x0;
		} else {
			cfg->host_caps_mask = rval;
			MMCINFO("get sdc0 sdc_dis_host_caps 0x%x.\n", cfg->host_caps_mask);
		}

		ret = dev_read_u32(dev, "sdc_kernel_no_limit", &rval);
		if (ret != 0) {
			MMCDBG("get sdc0 sdc_kernel_no_limit fail.\n");
			cfg->tune_limit_kernel_timing = 0x1;
		} else {
			if (rval == 1)
				cfg->tune_limit_kernel_timing = 0x0;
			else
				cfg->tune_limit_kernel_timing = 0x1;
			MMCDBG("get sdc0 sdc_kernel_no_limit 0x%x, limit 0x%x.\n", rval, cfg->tune_limit_kernel_timing);
		}

		tm_str = dev_read_string(dev, "clk_mmc");
		if (tm_str == NULL || strlen(tm_str) > MMC_MAX_PLL_STRING_LEN) {
			MMCDBG("get clk_mmc_name string failed\n");
		} else {
			memcpy(cfg->clk_mmc_name, tm_str, strlen(tm_str));
			MMCDBG("get clk_mmc_name = %s\n", cfg->clk_mmc_name);
		}

		tm_str = dev_read_string(dev, "pll-0");
		if (tm_str == NULL || strlen(tm_str) > MMC_MAX_PLL_STRING_LEN) {
			MMCDBG("get pll0 string failed\n");
		} else {
			memcpy(cfg->pll0, tm_str, strlen(tm_str));
			MMCDBG("get pll0 = %s\n", cfg->pll0);
		}

		tm_str = dev_read_string(dev, "pll-1");
		if (tm_str == NULL || strlen(tm_str) > MMC_MAX_PLL_STRING_LEN) {
			MMCDBG("get pll1 string failed\n");
		} else {
			memcpy(cfg->pll1, tm_str, strlen(tm_str));
			MMCDBG("get pll1 = %s\n", cfg->pll1);
		}

		tm_str = dev_read_string(dev, "pll-2");
		if (tm_str == NULL || strlen(tm_str) > MMC_MAX_PLL_STRING_LEN) {
			MMCDBG("get pll2 string failed\n");
		} else {
			memcpy(cfg->pll2, tm_str, strlen(tm_str));
			MMCDBG("get pll2 = %s\n", cfg->pll2);
		}

	} else if (priv->mmc_no == 2) {
#if 0
		nodeoffset =  fdt_path_offset(working_fdt, FDT_PATH_CARD2_BOOT_PARA);
		if (nodeoffset < 0) {
			MMCINFO("get card2 para fail\n");
			return ;
		}

		ret = fdt_getprop_u32(working_fdt, nodeoffset, "card_line",
				(uint32_t *)(&rval));
		if (ret < 0) {
			MMCDBG("get card2_boot_para:card_line fail\n");
		} else {
			if (rval == 8) {
				cfg->host_caps |= MMC_MODE_4BIT;
				cfg->host_caps |= MMC_MODE_8BIT;
				MMCDBG("card2 use 8bit!\n");
			} else if (rval == 4) {
				cfg->host_caps |= MMC_MODE_4BIT;
				cfg->host_caps &= ~MMC_MODE_8BIT;
				MMCDBG("card2 use 4bit!\n");
			} else if (rval == 1) {
				cfg->host_caps &= ~MMC_MODE_4BIT;
				cfg->host_caps &= ~MMC_MODE_8BIT;
				MMCDBG("card2 use 1bit!\n");
			} else {
				cfg->host_caps |= MMC_MODE_4BIT;
				cfg->host_caps |= MMC_MODE_8BIT;
				MMCINFO("input card_line error, use default 8bit!\n");
			}
		}

		ret = dev_read_u32(dev, "sdc_io_1v8", &rval);
		if (ret != 0) {
			MMCDBG("get card2_boot_para:sdc_io_1v8 fail\n");
		} else {
			if (rval == 1) {
				cfg->io_is_1v8 = 1;
			} else {
				MMCDBG("card2 try set io t 3V.\n");
			}
		}
#endif

#if 0
		pin_default->pin_count = fdt_get_all_pin(nodeoffset, "pinctrl-0", pin_default->pin_set);
		if (pin_default->pin_count <= 0) {
			MMCDBG("get card2 default pin fail\n");
			return ;
		}

		ret = sunxi_auto_pow_mode(pin_default->pin_set[0].port-1);
		if (!ret)
			cfg->io_is_1v8 = 0;

		/*avoid the error print from fdt_get_all_pin*/
		pin_disable->pin_count = fdt_getprop_u32(working_fdt, nodeoffset, "pinctrl-1", handle);
		if (pin_disable->pin_count >= 0) {
			pin_disable->pin_count = fdt_get_all_pin(nodeoffset, "pinctrl-1", pin_disable->pin_set);
			if (pin_disable->pin_count <= 0)
				MMCINFO("get card0 disable pin fail\n");
		}
#endif
		ret = dev_read_u32(dev, "sdc_wipe", &rval);
		if (ret != 0)
			MMCDBG("get sdc_wipe fail.\n");
		else {
			if (rval & DRV_PARA_DISABLE_SECURE_WIPE) {
				MMCDBG("disable driver secure wipe operation.\n");
				cfg->drv_wipe_feature |= DRV_PARA_DISABLE_SECURE_WIPE;
			}
			if (rval & DRV_PARA_DISABLE_EMMC_SANITIZE) {
				MMCDBG("disable emmc sanitize feature.\n");
				cfg->drv_wipe_feature |= DRV_PARA_DISABLE_EMMC_SANITIZE;
			}
			if (rval & DRV_PARA_DISABLE_EMMC_SECURE_PURGE) {
				MMCDBG("disable emmc secure purge feature.\n");
				cfg->drv_wipe_feature |= DRV_PARA_DISABLE_EMMC_SECURE_PURGE;
			}
			if (rval & DRV_PARA_DISABLE_EMMC_TRIM) {
				MMCDBG("disable emmc trim feature.\n");
				cfg->drv_wipe_feature |= DRV_PARA_DISABLE_EMMC_TRIM;
			}
		}

		//ret = script_parser_fetch("card2_boot_para", "sdc_erase", &rval, 1);
		ret = dev_read_u32(dev, "sdc_erase", &rval);
		if (ret != 0)
			MMCDBG("get sdc2 sdc_erase fail.\n");
		else {
			if (rval & DRV_PARA_DISABLE_EMMC_ERASE) {
				MMCDBG("disable emmc erase.\n");
				cfg->drv_erase_feature |= DRV_PARA_DISABLE_EMMC_ERASE;
			}
			if (rval & DRV_PARA_ENABLE_EMMC_SANITIZE_WHEN_ERASE) {
				MMCDBG("enable emmc sanitize when erase.\n");
				cfg->drv_erase_feature |= DRV_PARA_ENABLE_EMMC_SANITIZE_WHEN_ERASE;
			}
		}

		//ret = script_parser_fetch("card2_boot_para", "sdc_boot", &rval, 1);
		ret = dev_read_u32(dev, "sdc_boot", &rval);
		if (ret != 0)
			MMCDBG("get sdc2 sdc_boot fail.\n");
		else {
			if (rval & DRV_PARA_NOT_BURN_USER_PART) {
				MMCDBG("don't burn boot0 to user part.\n");
				cfg->drv_burn_boot_pos |= DRV_PARA_NOT_BURN_USER_PART;
			}
			if (rval & DRV_PARA_BURN_EMMC_BOOT_PART) {
				MMCDBG("burn boot0 to emmc boot part.\n");
				cfg->drv_burn_boot_pos |= DRV_PARA_BURN_EMMC_BOOT_PART;
			}
		}

		ret = dev_read_u32(dev, "sdc_wp", &rval);
		if (ret != 0)
			MMCDBG("get sdc2 sdc_wp fail.\n");
		else {
			if (rval & DRV_PARA_ENABLE_EMMC_USER_PART_WP) {
				MMCDBG("enable emmc write protect.\n");
				cfg->drv_wp_feature |= DRV_PARA_ENABLE_EMMC_USER_PART_WP;
			}
		}

		ret = dev_read_u32(dev, "sdc_hc_cap_unit", &rval);
		if (ret != 0)
			MMCDBG("get sdc2 sdc_hc_cap_unit fail.\n");
		else {
			if (rval & DRV_PARA_ENABLE_EMMC_USER_PART_WP) {
				MMCDBG("enable emmc high capacity-erase/high-capacity write protect unit size.\n");
				cfg->drv_hc_cap_unit_feature |= DRV_PARA_ENABLE_EMMC_HC_CAP_UNIT;
			}
		}

		ret = dev_read_u32(dev, "sdc_cal_delay_unit", &rval);
		if (ret != 0)
			MMCDBG("get card2_boot_para:sdc_cal_delay_unit fail\n");
		else {
			MMCDBG("card2 calibration delay unit\n");
			cfg->cal_delay_unit = 1;
		}

		ret = dev_read_u32(dev, "sdc_ex_dly_used", &rval);
		if (ret != 0) {
			MMCDBG("get card2_boot_para:sdc_ex_dly_used fail\n");

			/* fill invalid sample config */
			memset(mmcpriv->cfg->sdly.tm4_smx_fx,
					0xff, sizeof(mmcpriv->cfg->sdly.tm4_smx_fx));
		}
		MMCDBG("get sdc_ex_dly_used %d, use auto tuning sdly\n", rval);

		if (ret  == 0) {
			int work_mode = uboot_spare_head.boot_data.work_mode;
			struct boot_sdmmc_private_info_t *priv_info =
				(struct boot_sdmmc_private_info_t *)(uboot_spare_head.boot_data.sdcard_spare_data);
			struct tune_sdly *sdly = &(priv_info->tune_sdly);
			u32 *p = (u32 *)&mmcpriv->cfg->sdly.tm4_smx_fx[0];

			if (/*(rval_ker == 2) &&*/ (rval == 2)) { //only when kernal use auto sample,uboot will use auto sample.
				cfg->sample_mode = AUTO_SAMPLE_MODE;
				MMCDBG("get sdc_ex_dly_used %d, use auto tuning sdly\n", rval);
				if (work_mode != WORK_MODE_BOOT) {
					/*usb product will auto get sample point, no need to get auto sdly, so first used default value*/
					MMCDBG("current is product mode, it will tune sdly later\n");
				} else {
					/* copy sample point cfg from uboot header to internal variable */
					if (((priv_info->ext_para0 & 0xFF000000) == EXT_PARA0_ID)
						&& (priv_info->ext_para0 & EXT_PARA0_TUNING_SUCCESS_FLAG)) {
						if (sdly != NULL)
							memcpy(p, sdly, sizeof(struct tune_sdly));
						else
							memset(p, 0xFF, sizeof(struct tune_sdly));
					} else {
						memset(p, 0xFF, sizeof(struct tune_sdly));
						MMCINFO("get sdly from uboot header fail\n");
					}
				}
			} else if (rval == 1) {  /* maual sample point from fex */
				cfg->sample_mode = MAUNAL_SAMPLE_MODE;
				MMCDBG("get sdc_ex_dly_used %d, use manual sdly in fex\n", rval);
			} else {
				cfg->sample_mode = 0x0;
				MMCINFO("undefined value %d, use default dly\n", rval);
			}
		}

		ret = dev_read_u32(dev, "sdc_force_boot_tuning", &rval);
		if (ret != 0)
			MMCDBG("get card2_boot_para:sdc_force_boot_tuning fail\n");
		else {
			if (rval == 1) {
				MMCDBG("card2 force to execute tuning during boot.\n");
				cfg->force_boot_tuning = 1;
			} else {
				MMCDBG("card2 don't force to execute tuning during boot.\n");
				cfg->force_boot_tuning = 0;
			}
		}


		ret = dev_read_u32(dev, "sdc_odly_50M", &rval);
		if (ret != 0) {
			MMCDBG("get sdc2 sdc_odly_50M fail.\n");
			cfg->boot_odly_50M = 0xff;
		} else {
			MMCDBG("get sdc2 sdc_odly_50M %d.\n", rval);
			cfg->boot_odly_50M = rval;
		}

		ret = dev_read_u32(dev, "sdc_sdly_50M", &rval);
		if (ret != 0) {
			MMCDBG("get sdc2 sdc_sdly_50M fail.\n");
			cfg->boot_sdly_50M = 0xff;
		} else {
			MMCDBG("get sdc2 sdc_sdly_50M %d.\n", rval);
			cfg->boot_sdly_50M = rval;
		}

		ret = dev_read_u32(dev, "sdc_odly_50M_ddr", &rval);
		if (ret != 0) {
			MMCDBG("get sdc2 sdc_odly_50M_ddr fail.\n");
			cfg->boot_odly_50M_ddr = 0xff;
		} else {
			MMCDBG("get sdc2 sdc_odly_50M_ddr %d.\n", rval);
			cfg->boot_odly_50M_ddr = rval;
		}

		ret = dev_read_u32(dev, "sdc_sdly_50M_ddr", &rval);
		if (ret != 0) {
			MMCDBG("get sdc2 sdc_sdly_50M_ddr fail.\n");
			cfg->boot_sdly_50M_ddr = 0xff;
		} else {
			MMCDBG("get sdc2 sdc_sdly_50M_ddr %d.\n", rval);
			cfg->boot_sdly_50M_ddr = rval;
		}

		ret = dev_read_u32(dev, "sdc_freq", &rval);
		if (ret != 0) {
			MMCDBG("get sdc2 sdc_freq fail.\n");
			cfg->boot_hs_f_max = 0x0;
		} else {
			if (rval >= 50)
				cfg->boot_hs_f_max = 50;
			else
				cfg->boot_hs_f_max = rval;
			MMCDBG("get sdc2 sdc_freq %d.%d\n", rval, cfg->boot_hs_f_max);
		}
#if 0
		ret = dev_read_u32(dev, "sdc_boot0_sup_1v8", &rval);
		if (ret != 0) {
			MMCDBG("get sdc2 sdc_boot0_sup_1v8 fail.\n");
			cfg->boot0_sup_1v8 = 0x0;
		} else {
			if (rval == 1)
				cfg->boot0_sup_1v8 = rval;
			else
				cfg->boot0_sup_1v8 = 0x0;
			MMCDBG("get sdc2 sdc_boot0_sup_1v8 %d\n", cfg->boot0_sup_1v8);
		}
#endif
		ret = dev_read_u32(dev, "sdc_b0p", &rval);
		if (ret != 0) {
			MMCDBG("get sdc2 sdc_b0p fail.\n");
			cfg->boot0_para = 0x0;
		} else {
			cfg->boot0_para = rval;
			MMCDBG("get sdc2 sdc_b0p %d.\n", cfg->boot0_para);
		}

		ret = dev_read_u32(dev, "sdc_tm4_win_th", &rval);
		if (ret != 0) {
			MMCDBG("get sdc2 sdc_tm4_win_th fail.\n");
			if (tm == SUNXI_MMC_TIMING_MODE_5)
				cfg->tm4_timing_window_th = 5;
			else
				cfg->tm4_timing_window_th = 12;
		} else {
			if ((rval < 4) || (rval > 64))
				cfg->tm4_timing_window_th = 12;
			else
				cfg->tm4_timing_window_th = rval;
			MMCDBG("get sdc2 sdc_tm4_win_th %d.\n", cfg->tm4_timing_window_th);
		}

		ret = dev_read_u32(dev, "sdc_tm4_r_cycle", &rval);
		if (ret != 0) {
			MMCDBG("get sdc2 sdc_tm4_r_cycle fail.\n");
			cfg->tm4_tune_r_cycle = 0x0;
		} else {
			if ((rval < 1) || (rval > 255))
				cfg->tm4_tune_r_cycle = 15;
			else
				cfg->tm4_tune_r_cycle = rval;
			MMCINFO("get sdc2 sdc_tm4_r_cycle %d.\n", cfg->tm4_tune_r_cycle);
		}

		ret = dev_read_u32(dev, "sdc_tm4_hs200_max_freq", &rval);
		if (ret != 0) {
			MMCDBG("get sdc2 sdc_tm4_hs200_max_freq fail.\n");
			cfg->tm4_tune_hs200_max_freq = 150;
		} else {
			if ((rval < 50) || (rval > 200))
				cfg->tm4_tune_hs200_max_freq = 0x0;
			else
				cfg->tm4_tune_hs200_max_freq = rval;
			MMCDBG("get sdc2 sdc_tm4_hs200_max_freq %d.\n", cfg->tm4_tune_hs200_max_freq);
		}

		ret = dev_read_u32(dev, "sdc_tm4_hs400_max_freq", &rval);
		if (ret != 0) {
			MMCDBG("get sdc2 sdc_tm4_hs400_max_freq fail.\n");
			cfg->tm4_tune_hs400_max_freq = 100;
		} else {
			if ((rval < 50) || (rval > 200))
				cfg->tm4_tune_hs400_max_freq = 0x0;
			else
				cfg->tm4_tune_hs400_max_freq = rval;
			MMCDBG("get sdc2 sdc_tm4_hs400_max_freq %d.\n", cfg->tm4_tune_hs400_max_freq);
		}

		for (i = 0; i < MAX_EXT_FREQ_POINT_NUM; i++) {
			sprintf(ctmp, "sdc_tm4_ext_freq_%d", i);
			ret = dev_read_u32(dev, ctmp, &rval);
			if (ret != 0) {
				MMCDBG("get sdc2 %s fail.\n", ctmp);
				cfg->tm4_tune_ext_freq[i] = 0x0;
			} else {
				MMCDBG("get sdc2 %s 0x%x.\n", ctmp, rval);
				if ((rval & 0xff) >= 25)
					cfg->tm4_tune_ext_freq[i] = rval;
				else {
					cfg->tm4_tune_ext_freq[i] = 0x0;
					MMCDBG("invalid freq %d MHz, discard this ext freq point\n", (rval & 0xff));
				}
			}
		}
#if 0
		ret = fdt_get_one_gpio(FDT_PATH_CARD2_BOOT_PARA, "card-pwr-gpios",
							&card_pwr_gpio);
		if (ret < 0) {
			MMCDBG("get card-pwr-gpios faild\n");
			mmcpriv->pwr_handler = 0;
		} else {
			mmcpriv->pwr_handler = sunxi_gpio_request(&card_pwr_gpio, 1);
			MMCDBG("get card-pwr-gpios handler:%d\n", mmcpriv->pwr_handler);
		}
#endif
		ret = dev_read_u32(dev, "time_pwroff_ms", &rval);
		if (ret != 0) {
			mmcpriv->time_pwroff = 200;
			MMCDBG("get card2_boot_para:time_pwroff_ms:use default time_pwroff:%d\n",
				mmcpriv->time_pwroff);
		} else {
			mmcpriv->time_pwroff = rval;
			MMCDBG("get card2_boot_para:time_pwroff:%dms\n", mmcpriv->time_pwroff);
		}

		if (cfg->sample_mode == MAUNAL_SAMPLE_MODE) {
			for (imd = 0; imd < MAX_SPD_MD_NUM; imd++) {
				for (ifreq = 0; ifreq < MAX_CLK_FREQ_NUM; ifreq++) {
					sprintf(ctmp, "sdc_odly_%d_%d", imd, ifreq);
					ret = dev_read_u32(dev, ctmp, &rval);
					if (ret != 0) {
						MMCDBG("get sdc2 %s fail.\n", ctmp);
						cfg->odly_spd_freq[imd * MAX_CLK_FREQ_NUM + ifreq] = 0x0;
					} else {
						cfg->odly_spd_freq[imd * MAX_CLK_FREQ_NUM + ifreq] = rval & 0x1;
						MMCDBG("get sdc2 %s 0x%x for %d-%s %d.\n", ctmp,
							cfg->odly_spd_freq[imd*MAX_CLK_FREQ_NUM + ifreq], imd, spd_name[imd], ifreq);
					}

					sprintf(ctmp, "sdc_sdly_%d_%d", imd, ifreq);
					ret = dev_read_u32(dev, ctmp, &rval);
					if (ret != 0) {
						MMCDBG("get sdc2 %s fail.\n", ctmp);
						cfg->sdly_spd_freq[imd * MAX_CLK_FREQ_NUM + ifreq] = 0xff;
					} else {
						cfg->sdly_spd_freq[imd * MAX_CLK_FREQ_NUM + ifreq] = rval&0xff;
						MMCDBG("get sdc2 %s 0x%x for %d-%s %d.\n", ctmp,
							cfg->sdly_spd_freq[imd * MAX_CLK_FREQ_NUM + ifreq], imd, spd_name[imd], ifreq);
					}
				}
			}
		}

		/* speed mode caps */
		ret = dev_read_u32(dev, "sdc_dis_host_caps", &rval);
		if (ret != 0) {
			MMCDBG("get sdc2 sdc_dis_host_caps fail.\n");
			cfg->host_caps_mask = 0x0;
		} else {
			cfg->host_caps_mask = rval;
			MMCINFO("get sdc2 sdc_dis_host_caps 0x%x.\n", cfg->host_caps_mask);
		}

		ret = dev_read_u32(dev, "sdc_kernel_no_limit", &rval);
		if (ret != 0) {
			MMCDBG("get sdc2 sdc_kernel_no_limit fail.\n");
			cfg->tune_limit_kernel_timing = 0x1;
		} else {
			if (rval == 1)
				cfg->tune_limit_kernel_timing = 0x0;
			else
				cfg->tune_limit_kernel_timing = 0x1;
			MMCDBG("get sdc2 sdc_kernel_no_limit 0x%x, limit 0x%x.\n", rval, cfg->tune_limit_kernel_timing);
		}
#if 0
		ret = fdt_getprop_u32(working_fdt, nodeoffset, "ffu_src_fw_version_H32", (uint32_t *)(&rval_64bit_H32));
		if (ret < 0) {
			MMCDBG("get sdc2 ffu_src_fw_version_H32 fail.\n");
			cfg->ffu_src_fw_version = 0x0;
		} else {
			ret = fdt_getprop_u32(working_fdt, nodeoffset, "ffu_src_fw_version_L32", (uint32_t *)(&rval_64bit_L32));
			if (ret < 0) {
				MMCDBG("get sdc2 ffu_src_fw_version_L32 fail.\n");
				cfg->ffu_src_fw_version = 0x0;
			} else {
				cfg->ffu_src_fw_version = ((u64)rval_64bit_H32) << 32 | rval_64bit_L32 ;
				MMCDBG("get sdc2 ffu_src_fw_version 0x%llx.\n", cfg->ffu_src_fw_version);
			}
		}

		ret = fdt_getprop_u32(working_fdt, nodeoffset, "ffu_dest_fw_version_H32", (uint32_t *)(&rval_64bit_H32));
		if (ret < 0) {
			MMCDBG("get sdc2 ffu_dest_fw_version_H32 fail.\n");
			cfg->ffu_dest_fw_version = 0x0;
		} else {
			ret = fdt_getprop_u32(working_fdt, nodeoffset, "ffu_dest_fw_version_L32", (uint32_t *)(&rval_64bit_L32));
			if (ret < 0) {
				MMCDBG("get sdc2 ffu_dest_fw_version_L32 fail.\n");
				cfg->ffu_dest_fw_version = 0x0;
			} else {
				cfg->ffu_dest_fw_version = ((u64)rval_64bit_H32) << 32 | rval_64bit_L32 ;
				MMCDBG("get sdc2 ffu_dest_fw_version 0x%llx.\n", cfg->ffu_dest_fw_version);
			}
		}
#endif
		tm_str = dev_read_string(dev, "clk_mmc");
		if (tm_str == NULL || strlen(tm_str) > MMC_MAX_PLL_STRING_LEN) {
			MMCDBG("get clk_mmc_name string failed\n");
		} else {
			memcpy(cfg->clk_mmc_name, tm_str, strlen(tm_str));
			MMCDBG("get clk_mmc_name = %s\n", cfg->clk_mmc_name);
		}

		tm_str = dev_read_string(dev, "pll-0");
		if (tm_str == NULL || strlen(tm_str) > MMC_MAX_PLL_STRING_LEN) {
			MMCDBG("get pll0 string failed\n");
		} else {
			memcpy(cfg->pll0, tm_str, strlen(tm_str));
			MMCDBG("get pll0 = %s\n", cfg->pll0);
		}

		tm_str = dev_read_string(dev, "pll-1");
		if (tm_str == NULL || strlen(tm_str) > MMC_MAX_PLL_STRING_LEN) {
			MMCDBG("get pll1 string failed\n");
		} else {
			memcpy(cfg->pll1, tm_str, strlen(tm_str));
			MMCDBG("get pll1 = %s\n", cfg->pll1);
		}

		tm_str = dev_read_string(dev, "pll-2");
		if (tm_str == NULL || strlen(tm_str) > MMC_MAX_PLL_STRING_LEN) {
			MMCDBG("get pll2 string failed\n");
		} else {
			memcpy(cfg->pll2, tm_str, strlen(tm_str));
			MMCDBG("get pll2 = %s\n", cfg->pll2);
		}

		//dumphex32("gpio_config", (char *)SUNXI_PIO_BASE + 0x48, 0x10);
		//dumphex32("gpio_pull", (char *)SUNXI_PIO_BASE + 0x64, 0x8);
		/* fmax, fmax_ddr */
	} else {
		MMCINFO("%s: input sdc_no error: %d\n", __FUNCTION__, priv->mmc_no);
	}

	return ;
}
#if 0
static void mmc_get_para_from_dtb(int sdc_no)
{
	int ret = 0;
	//struct sunxi_mmc_priv* mmcpriv = &mmc_priv[sdc_no];
	//struct mmc_config *cfg = &mmcpriv->cfg;
	int nodeoffset;
	char prop_path[128] = {0};
	//u32 prop_val = 0;

	if (sdc_no == 0) {
		strcpy(prop_path, "mmc0");
		nodeoffset = fdt_path_offset(working_fdt, prop_path);
		if (nodeoffset < 0) {
			MMCINFO("can't find node \"%s\",will add new node\n", prop_path);
			goto __ERRRO_END;
		}
	}

	return ;

__ERRRO_END:
	MMCINFO("fdt err returned %s\n", fdt_strerror(ret));
	return ;
}
#endif
int sunxi_host_mmc_config(struct udevice *dev, struct sunxi_mmc_priv *priv)
{
	int ret = 0;

	ret = sunxi_mmc_host_init(dev, priv);
	if (ret) {
		MMCINFO("sunxi mmc host init failed!\n");
		goto ERR;
	}

	mmc_clear_timing_para(priv);
	mmc_get_para_from_fex(dev, priv);
#if 0
	mmc_get_para_from_dtb(priv->mmc_no);
#endif
	ret = mmc_init_default_timing_para(priv);
	if (ret) {
		MMCINFO("mmc init default timing para failed\n");
		goto ERR;
	}

	ret = mmc_update_timing_para(priv);
	if (ret) {
		MMCINFO("mmc update timing para failed\n");
		goto ERR;
	}
ERR:
	return ret;
}

#if 0
int __attribute__((weak)) sunxi_mmc_get_src_clk_no(int sdc_no, int mod_hz, int tm) {
	unsigned int pll = CCM_MMC_CTRL_PLL6X2;

	if (tm == 4 || tm == 5) {
#ifdef CCM_MMC2_CTRL_PLL6X2
		pll = CCM_MMC2_CTRL_PLL6X2;
#elif CCM_MMC_CTRL_PLL6X2
		pll = CCM_MMC_CTRL_PLL6X2;
#else
		MMCINFO("There is no 2X pll!\n");
		return -1;
#endif
	} else if (tm == 1) {
#if (!(defined(CONFIG_MACH_SUN8IW7) || defined(CONFIG_MACH_SUN8IW11)))
		pll = CCM_MMC_CTRL_PLL6X2;
#else
		pll = CCM_MMC_CTRL_PLL6;
#endif
	} else if (tm == 0) {
#ifdef CCM_MMC_CTRL_PLL6
		pll = CCM_MMC_CTRL_PLL6;
#else
		MMCINFO("There is no 1X pll!\n");
		return -1;
#endif

	}

	return pll;
}

int __attribute__((weak)) sunxi_host_src_clk(int sdc_no, int src_clk, int tm) {
	unsigned int pll_hz = clock_get_pll6() * 2 *1000000;
	if (tm == 1) {
#if (!(defined(CONFIG_MACH_SUN8IW7) || defined(CONFIG_MACH_SUN8IW11)))
		pll_hz = clock_get_pll6() * 2 *1000000;
#else
		pll_hz = clock_get_pll6() * 1000000;
#endif
	} else if (tm == 5 || tm == 4) {
#ifdef CCM_MMC2_CTRL_PLL6X2
		pll_hz = clock_get_pll6() * 2 *1000000;
#elif CCM_MMC_CTRL_PLL6X2
		pll_hz = clock_get_pll6() * 2 *1000000;
#else
		MMCINFO("There is no 2X pll!\n");
		return -1;
#endif
	} else if (tm == 0) {
#ifdef CCM_MMC_CTRL_PLL6
		pll_hz = clock_get_pll6() * 1000000;
#else
		MMCINFO("There is no 1X pll!\n");
		return -1;
#endif
	}
	return pll_hz;
}
#endif

#if defined(CONFIG_MACH_SUN55IW6)
void sunxi_mmc_get_src_clk_calc(int sdc_no, int mod_hz, unsigned int *pll, unsigned int *pll_hz)
{
	unsigned int pll1, pll_hz1, pll2, pll_hz2;

	if (sdc_no == 2) {
		pll1 = CCM_MMC_CTRL_PERI0_800M;
		pll_hz1 = CCM_MMC_CTRL_PERI0_800M_freq;
		pll2 = CCM_MMC_CTRL_PERI0_600M;
		pll_hz2 = CCM_MMC_CTRL_PERI0_600M_freq;
	} else {
		pll1 = CCM_MMC_CTRL_PERI0_400M;
		pll_hz1 = CCM_MMC_CTRL_PERI0_400M_freq;
		pll2 = CCM_MMC_CTRL_PERI0_300M;
		pll_hz2 = CCM_MMC_CTRL_PERI0_300M_freq;
	}

	if ((pll_hz1 % mod_hz) <= (pll_hz2 % mod_hz)) {
		*pll = pll1;
		*pll_hz = pll_hz1;
	} else {
		*pll = pll2;
		*pll_hz = pll_hz2;
	}
}
#endif
