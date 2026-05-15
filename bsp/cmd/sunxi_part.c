/* SPDX-License-Identifier: GPL-2.0+
 * Copyright (C) 2023 Allwinnertech
*/

#include <command.h>
#include <common.h>
#include <part.h>
#include <android_image.h>
#include <image.h>
#include <sunxi_board.h>
#include <private_uboot.h>
#include <spare_head.h>
#include <sunxi_flash.h>
#include <sys_partition.h>
#include <sunxi_flash.h>
#include <memalign.h>
#include <sunxi_mbr.h>
#include <android_misc.h>
#include <android_ab.h>
#include <linux/crc32.h>
#define LINUX_MMC_LOGIC_OFFSET 40960

#undef crc32
#include <u-boot/crc.h>

#ifndef CONFIG_ENABLE_MTD_CMDLINE_PARTS_BY_ENV
static sunxi_mbr_t *mbr ;
#endif

static int sprite_storage_type = STORAGE_EMMC;

int get_boot_storage_type_ext(void)
{
	/* get real storage type that from BROM at boot mode*/
	return uboot_spare_head.boot_data.storage_type;
}

int get_boot_storage_type(void)
{
	/* we think that nand and spi-nand are the same storage medium */
	/* so we can use the same process to deal with them */
	if (uboot_spare_head.boot_data.storage_type == STORAGE_NAND ||
	    uboot_spare_head.boot_data.storage_type == STORAGE_SPI_NAND) {
		return STORAGE_NAND;
	}
	return uboot_spare_head.boot_data.storage_type;
}

void set_boot_storage_type(int storage_type)
{
	uboot_spare_head.boot_data.storage_type = storage_type;
}

int get_sprite_storage_type(void)
{
	return sprite_storage_type;
}

void set_sprite_storage_type(int storage_type)
{
	sprite_storage_type = storage_type;
}

int get_boot_storage_str(char *buff)
{
	switch (uboot_spare_head.boot_data.storage_type) {
	case STORAGE_SD:
	case STORAGE_EMMC:
	case STORAGE_EMMC0:
	case STORAGE_EMMC3:
		memcpy(buff, "mmc", 3);
		break;
	default:
		pr_err("need to do\n");
		return -1;
	}

	return 0;
}

int sunxi_flash_try_partition(struct blk_desc *desc, const char *str,
			      struct disk_partition *info)
{
	int i, ret;

	for (i = 1;; i++) {
	/* Get partitiones by GPT */
		ret = part_get_info(desc, i, info);
		pr_debug("%s: try part %d, ret = %d\n", __func__, i, ret);

		if (ret < 0)
			return ret;

		if (!strncmp((const char *)info->name, (char *)str, sizeof(info->name)))
			break;
	}
	return 0;
}

int sunxi_partition_get_info(const char *part_name, struct disk_partition *info)
{
	struct blk_desc *desc;
	int ret;
	int logic_offset;

	logic_offset = sunxi_flash_get_logical_offset();
	sunxi_flash_dev_get_blk(&desc);
	ret = sunxi_flash_try_partition(desc, part_name, info);
	if (ret < 0) {
		pr_debug("%s: get partition info fail\n", __func__);
		ret = -ENODEV;
		goto __err;
	}
	pr_debug("name:%s start:0x%x, size: 0x%x\n", info->name, (u32)info->start,
		(u32)info->size);
	/* conver gpt info to sunxi_part */
	/* gpt part use phy address */
	/* sunxi part use logic address */
	if (sunxi_flash_get_storage() == STORAGE_EMMC)
		info->start -= logic_offset;

	return 0;
__err:
	return ret;

}

int sunxi_partition_get_partno_byname(const char *part_name)
{
	int i;
	struct blk_desc *desc;
	int ret;
	struct disk_partition info;
	char temp_part_name[16] = {0};

	sunxi_flash_dev_get_blk(&desc);
	for (i = 1;; i++) {
		ret = part_get_info(desc, i, &info);
		pr_debug("%s: try part %d, ret = %d\n", __func__, i, ret);
		if (ret < 0) {
			pr_err("partno erro : can't find partition %s\n", part_name);
			return ret;
		}
		if (!strncmp((const char *)info.name, temp_part_name, sizeof(info.name))) {
			return i;
		} else if (!strncmp((const char *)info.name, part_name, sizeof(info.name))) {
			return i;
		}
	}

	return -1;
}

int sunxi_partition_get_info_byname(const char *part_name, uint *part_offset,
				    uint *part_size)
{
	struct disk_partition info = { 0 };
	if (sunxi_partition_get_info(part_name, &info) == 0) {
		*part_offset = info.start;
		*part_size  = info.size;
		return 0;
	}
	return -1;
}

uint sunxi_partition_get_offset_byname(const char *part_name)
{
	struct disk_partition info = { 0 };
	if (sunxi_partition_get_info(part_name, &info) == 0) {
		return info.start;
	}
	return 0;
}

int sunxi_partition_init(void)
{
#ifndef CONFIG_ENABLE_MTD_CMDLINE_PARTS_BY_ENV

	if (mbr == NULL) {
		mbr = memalign(ARCH_DMA_MINALIGN, SUNXI_MBR_SIZE);
		if (mbr == NULL) {
			printf("unable to allocate TDs\n");
			return -1;
		}

		if (sunxi_flash_read(0, SUNXI_MBR_SIZE / 512, (void *)mbr) <=
		    0) {
			printf("line:%d:sunxi_flash_read fail\n", __LINE__);
			return -1;
		}
		if (!strncmp((const char *)mbr->magic, SUNXI_MBR_MAGIC, 8)) {
			int crc = 0;
			crc     = crc32(0, (const unsigned char *)&mbr->version,
				    SUNXI_MBR_SIZE - 4);
			if (crc != mbr->crc32) {
				return -1;
			}
			gd->lockflag = mbr->lockflag;
		}
	}
#endif
	return 0;
}

lbaint_t sunxi_partition_get_offset(int part_index)
{

	if (get_boot_work_mode() != WORK_MODE_CARD_PRODUCT) {
		printf("****not support*****\n");
		return (lbaint_t)(-1);
	}

#ifdef CONFIG_ENABLE_MTD_CMDLINE_PARTS_BY_ENV
		disk_partition_t info;
		int ret;
		ret = sunxi_partition_parse_get_info(part_index, &info);
		if (ret == 0) {
			debug(" mbr->array[%d].name=%s\n", part_index,
			       info.name);
			debug(" mbr->array[%d].lenlo=0x%x\n", part_index,
			       (u32)info.size);
			debug("mbr->array[%d].addrlo=0x%x\n", part_index,
			       (u32)info.start);
			return (lbaint_t)info.start;
		}
#else
	sunxi_partition_init();
	if (mbr->PartCount && part_index <= mbr->PartCount) {
		debug(" mbr->array[%d].name=%s\n", part_index,
		       mbr->array[part_index].name);
		debug(" mbr->array[%d].lenlo=%d\n", part_index,
		       mbr->array[part_index].lenlo);
		debug("mbr->array[%d].addrlo=%d\n", part_index,
		       mbr->array[part_index].addrlo);
		return (lbaint_t)mbr->array[part_index].addrlo;
	}

#endif
	return (lbaint_t)(-1);
}

static int sunxi_probe_kernel_ramdisk(uint start_block)
{
	if (sunxi_probe_android_kernel()) {
		pr_err("not android kernel, set sunxi_ramd_addr, sunxi_fdt_addr to NULL\n");
		env_set("sunxi_ramd_addr", "");
		env_set("sunxi_fdt_addr", "");
		return 0;
	}

	ulong buf_addr = env_get_hex("sunxi_ramd_addr", 0x42000000);

	sunxi_flash_read(start_block, ALIGN(sizeof(struct andr_img_hdr), 512)/512, (void *)buf_addr);

	if (!((struct andr_img_hdr *)buf_addr)->ramdisk_size) {
		pr_debug("no ramdisk in kernel\n");
		env_set("sunxi_ramd_addr", "-");
	}

	return 0;
}

int do_sunxi_get_kern_env(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	int ret;
	struct disk_partition info = { 0 };
	char start_blk_str[12] = {0}, nblk_str[12] = {12};

	char *boot_name = env_get("boot_partition");
	if (boot_name == NULL) {
		ret = sunxi_partition_get_info("boot", &info);
	} else {
		ret = sunxi_partition_get_info(boot_name, &info);
	}

	pr_debug("kernel start blk:%08lx, nblk:%08lx\n", info.start, info.size);

	sprintf(start_blk_str, "%08lx", info.start + sunxi_flash_get_logical_offset());
	sprintf(nblk_str, "%08lx", info.size);

	env_set("kernel_start_blk", start_blk_str);
	env_set("kernel_blk_count", nblk_str);

	sunxi_probe_kernel_ramdisk(info.start);

	return 0;
}

U_BOOT_CMD(
	sunxi_get_kern_env, 1, 0, do_sunxi_get_kern_env,
	"update kernel start block and block num to env",
	"no parameters\n"
)
