/* SPDX-License-Identifier: GPL-2.0+
 * Copyright (C) 2023 Allwinnertech
*/

#include <asm/global_data.h>
#include <private_uboot.h>
#include <sunxi_flash.h>
#include <asm/arch/rtc.h>
#include <asm/io.h>
#include <sunxi_board.h>
#include <common.h>
#include <sysreset.h>
#include <android_image.h>
#include <image.h>
#include <malloc.h>
#include <efuse_map.h>
#include <dm/read.h>
#include <axp_pmic.h>

DECLARE_GLOBAL_DATA_PTR;
#include <asm/types.h>

int  __attribute__((weak)) sunxi_platform_power_off(int status)
{
	return 0;
}

int sunxi_set_uboot_shell(int flag)
{
//	gd->uboot_shell = flag;
	return 0;
}

void set_boot_work_mode(int work_mode)
{
       uboot_spare_head.boot_data.work_mode = work_mode;
}

int get_boot_work_mode(void)
{
	return uboot_spare_head.boot_data.work_mode;
}

int sunxi_probe_secure_monitor(void)
{
	return uboot_spare_head.boot_data.monitor_exist ==
			       SUNXI_SECURE_MODE_USE_SEC_MONITOR ?
		       1 :
		       0;
}

int sunxi_probe_secure_os(void)
{
	return uboot_spare_head.boot_data.secureos_exist;
}

/*
  * note:
  * call driver exit here.
  * this func be called before enter linux.
  */
void board_quiesce_devices(void)
{
	sunxi_flash_flush();
	/*modify 2 for nor to finally exit*/
	sunxi_flash_exit(2);

#ifdef CONFIG_AW_DMA
	/* no need  */
	//sunxi_dma_remove(NULL);
#endif
}

void sunxi_board_close_source(void)
{
	board_quiesce_devices();
	disable_interrupts();
	return;
}

int sunxi_board_run_fel(void)
{
	rtc_set_fel_flag();
	sunxi_board_close_source();
	reset_cpu();
	return 0;
}

int sunxi_get_secureboard(void)
{
#ifdef SID_SECURE_MODE
	return readl(IOMEM_ADDR(SID_SECURE_MODE)) & 1;
#elif defined(EFUSE_ANTI_BRUSH)
	return (readl(ANTI_BRUSH_MODE) >> ANTI_BRUSH_BIT_OFFSET) & 1;
#elif defined(SECURE_READ_TEST_REG)
	/*
	 * two way start up uboot:
	 * 1.fel:
	 *		board secure = ~(cpu secure)
	 * 2.from optee(boot0/sboot)
	 *		optee help read secure enable bit
	 */
	if (sunxi_probe_secure_os()) {
		return (((arm_svc_read_sec_reg(SUNXI_SID_SRAM_BASE + EFUSE_LCJS)) &
					(1 << 11)) != 0);
	} else {
		return (readl(SECURE_READ_TEST_REG) == 0);
	}
#else
	return 0;
#endif
}

int sunxi_probe_securemode(void)
{
	int secure_mode = 0;

	secure_mode = sunxi_get_secureboard();
	pr_notice("secure enable bit: %d\n", secure_mode);

	if (secure_mode) {
		// sbrom  set  secureos_exist flag,
		// 1: secure os exist 0: secure os not exist
		if (uboot_spare_head.boot_data.secureos_exist == 1) {
			gd->securemode = SUNXI_SECURE_MODE_WITH_SECUREOS;
			pr_debug("secure mode: with secureos\n");
		} else {
			gd->securemode = SUNXI_SECURE_MODE_NO_SECUREOS;
			pr_debug("secure mode: no secureos\n");
		}
		gd->bootfile_mode = SUNXI_BOOT_FILE_TOC;
#ifdef CONFIG_SUNXI_ANTI_BRUSH
		debug("init preserve toc1\n");
		if (sunxi_verify_preserve_toc1((void *)CONFIG_SUNXI_BOOTPKG_BASE)) {
			pr_err("%s: preserve toc1 error\n", __func__);
		}
#endif
	} else {
		//boot0  set  secureos_exist flag,
		//1: secure monitor exist 0: secure monitor  not exist
		gd->securemode = SUNXI_NORMAL_MODE;
		gd->bootfile_mode = SUNXI_BOOT_FILE_PKG;
	}
	return 0;
}

int sunxi_get_securemode(void)
{
	return gd->securemode;
}

int sunxi_boot_image_get_embbed_cert_len(const void *hdr)
{
	struct boot_img_hdr_ex *hdr_ex = (struct boot_img_hdr_ex *)hdr;
	if (strncmp((void *)(hdr_ex->cert_magic), AW_CERT_MAGIC,
		    strlen(AW_CERT_MAGIC)) != 0)
		return 0;
	else
		return hdr_ex->cert_size;
}

int sunxi_boot_fitimage_get_embbed_cert_len(const void *hdr)
{
	struct boot_img_fdt_ex *hdr_ex = (struct boot_img_fdt_ex *)hdr;
	if (strncmp((void *)(hdr_ex->cert_magic), AW_CERT_MAGIC,
		    strlen(AW_CERT_MAGIC)) != 0)
		return 0;
	else
		return hdr_ex->cert_size;
}

int sunxi_board_restart(int next_mode)
{
    rtc_set_bootmode_flag(next_mode);
    sunxi_board_close_source();
    reset_cpu();

    return 0;
}

int sunxi_board_shutdown(void)
{
	sunxi_board_close_source();
#ifdef CONFIG_SUNXI_UBOOT_POWER_OFF
	sunxi_platform_power_off(0);
#endif
#ifdef CONFIG_PMIC_AXP
	axp_set_power_off();
#endif
	while (1) {
		asm volatile ("wfi");
	}
	return 0;
}

int sunxi_board_shutdown_charge(void)
{
	sunxi_board_close_source();
#ifdef CONFIG_SUNXI_UBOOT_POWER_OFF
	sunxi_platform_power_off(1);
#endif
#ifdef CONFIG_PMIC_AXP
	axp_set_power_off();
#endif
	while (1) {
		asm volatile ("wfi");
	}
	return 0;
}

u32 sunxi_generate_checksum(void *buffer, u32 length, u32 div, u32 src_sum)
{
	u32 *buf;
	int count;
	u32 sum;

	count = length >> 2;
	sum   = 0;
	buf   = (__u32 *)buffer;
	do {
		sum += *buf++;
		sum += *buf++;
		sum += *buf++;
		sum += *buf++;
	} while ((count -= (4*div)) > (4 - 1));

	while (count-- > 0)
		sum += *buf++;

	sum = sum - src_sum + STAMP_VALUE;

	return sum;
}


u32 sunxi_verify_checksum(void *buffer, u32 length, u32 src_sum)
{
	u32 sum;
	sum = sunxi_generate_checksum(buffer, length, 1, src_sum);

	debug("src sum=%x, check sum=%x\n", src_sum, sum);
	if (sum == src_sum)
		return 0;
	else
		return -1;

}

/*return 0:android image, -1:other image*/
int sunxi_probe_android_kernel(void)
{
	int ret = 0;
	uint start_block = env_get_hex("kernel_start_blk", 0x0) - sunxi_flash_get_logical_offset();
	uint head_size = ALIGN(sizeof(struct andr_img_hdr), 512); //bytes

	struct andr_img_hdr *buf = (struct andr_img_hdr *)malloc(head_size);
	if (buf == NULL)
		return ret;

	memset(buf, '0', head_size);
	sunxi_flash_read(start_block, head_size / 512, (void *)buf);

	ret = android_image_check_header(buf);

	free(buf);
	return ret;
}

void board_prep_linux(struct bootm_headers *images)
{
	if (sunxi_probe_android_kernel()) {
		pr_debug("not android kernel, update dts now\n");
		int sunxi_update_fdt_para_for_kernel(void);
		sunxi_update_fdt_para_for_kernel();
	} else {
		//android image
#ifdef CONFIG_SUNXI_ANDROID_OVERLAY
		void set_andriod_dtbo_idx(void);
		int check_dtbo_idx(void);
		void *sunxi_support_ufdt(void *dtb_base, u32 dtb_len);

		set_andriod_dtbo_idx();

		if (check_dtbo_idx() == 0) {
			if (sunxi_support_ufdt((void *)images->ft_addr, fdt_totalsize(images->ft_addr)) == NULL) {
				pr_err("sunxi android dto merge fail\n");
			}
		}
#endif
	}
}

int sunxi_set_force_32bit_os(int forced)
{
	gd->force_32bit_os = forced;
	return 0;
}

int sunxi_get_force_32bit_os(void)
{
	return gd->force_32bit_os;
}

int sunxi_get_irq(struct udevice *dev)
{
       int irq_num;

       dev_read_u32(dev, "sunxi-interrupt", &irq_num);

       return irq_num;
}

void sunxi_update_subsequent_processing(int next_work)
{
	printf("next work %d\n", next_work);
	switch (next_work) {
	case SUNXI_UPDATE_NEXT_ACTION_REBOOT:
	case SUNXI_UPDATA_NEXT_ACTION_SPRITE_TEST:
		printf("SUNXI_UPDATE_NEXT_ACTION_REBOOT\n");
		sunxi_board_restart(0);
		break;

	case SUNXI_UPDATE_NEXT_ACTION_SHUTDOWN:
		printf("SUNXI_UPDATE_NEXT_ACTION_SHUTDOWN\n");
#ifdef CONFIG_SUNXI_UPDATE_REMIND
		sunxi_update_remind();
#endif
		sunxi_board_shutdown();
		break;

	case SUNXI_UPDATE_NEXT_ACTION_REUPDATE:
		printf("SUNXI_UPDATE_NEXT_ACTION_REUPDATE\n");
		sunxi_board_run_fel();
		break;

	case SUNXI_UPDATE_NEXT_ACTION_BOOT:
	case SUNXI_UPDATE_NEXT_ACTION_NORMAL:
	default:
		printf("SUNXI_UPDATE_NEXT_ACTION_NULL\n");
#ifdef CONFIG_SUNXI_UPDATE_REMIND
		sunxi_update_remind();
#endif
		break;
	}

	return;
}
