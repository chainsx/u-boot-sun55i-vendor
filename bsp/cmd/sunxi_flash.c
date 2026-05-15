/*
 * (C) Copyright 2013-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 *
 * wangwei <wangwei@allwinnertech.com>
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <config.h>
#include <command.h>
#include <sunxi_board.h>
#include <malloc.h>
#include <memalign.h>
#include <sunxi_flash.h>
#include <part.h>
#include <image.h>
#include <android_image.h>
#include <sys_partition.h>
#include <sprite.h>

#define SUNXI_FLASH_READ_FIRST_SIZE (32 * 1024)

#ifdef CONFIG_SUNXI_MELIS_AUTO_UPDATE
int sunxi_flash_read_part(struct blk_desc *desc, struct disk_partition *info,
				 ulong buffer, ulong load_size)
#else
static int sunxi_flash_read_part(struct blk_desc *desc, struct disk_partition *info,
				 ulong buffer, ulong load_size)
#endif
{
	int ret;
	u32 rbytes = 0, rblock, testblock;
	u32 start_block;
	u8 *addr;

	addr	= (void *)buffer;
	start_block = (uint)info->start;

#ifdef CONFIG_SUNXI_RTOS
	struct rtos_img_hdr *rtos_hdr;
	rtos_hdr = (struct rtos_img_hdr *)addr;
#endif

#ifdef CONFIG_ANDROID_BOOT_IMAGE
	struct andr_img_hdr *fb_hdr;
	fb_hdr = (struct andr_img_hdr *)addr;
#endif

#if CONFIG_IS_ENABLED(FIT)
	struct fdt_header *fdt_hdr;
	fdt_hdr = (struct fdt_header *)addr;
#endif

	testblock = SUNXI_FLASH_READ_FIRST_SIZE / 512;
	ret       = blk_dread(desc, start_block, testblock, (u_char *)buffer);
	if (ret != testblock) {
		return 1;
	}

	if (load_size)
		rbytes = load_size;
#ifdef CONFIG_ANDROID_BOOT_IMAGE
	else if (!memcmp(fb_hdr->magic, ANDR_BOOT_MAGIC, 8)) {
		//rbytes = android_image_get_end_by_avbfooter();
		//image size from avbfooter have higher priority
		if (!rbytes) {
			rbytes = android_image_get_end(fb_hdr) - (ulong)fb_hdr;

			/*secure boot img may attached with an embbed cert*/
			rbytes += sunxi_boot_image_get_embbed_cert_len(fb_hdr);
		}
	}
#endif

#ifdef CONFIG_SUNXI_RTOS
	else if (!memcmp(rtos_hdr->rtos_magic, RTOS_BOOT_MAGIC, 8)) {
		rbytes = sizeof(struct rtos_img_hdr) + rtos_hdr->rtos_size;
	}
#endif
#if CONFIG_IS_ENABLED(FIT)
	else if (fdt_magic(fdt_hdr) == FDT_MAGIC) {
		rbytes = ALIGN(be32_to_cpu(fdt_hdr->totalsize), AW_PAGESIZE) + AW_PAGESIZE;
		/*secure boot img may attached with an embbed cert*/
		rbytes += sunxi_boot_fitimage_get_embbed_cert_len(fdt_hdr);
	}
#endif
	else {
		debug("bad boot image magic, maybe not a boot.img?\n");
		rbytes = info->size * 512;
	}

	rblock = (rbytes + 511) / 512 - testblock;
	start_block += testblock;
	addr += SUNXI_FLASH_READ_FIRST_SIZE;

	ret = blk_dread(desc, start_block, rblock, (u_char *)addr);
	ret = (ret == rblock) ? 0 : 1;
	debug("sunxi flash read :offset %x, %d bytes %s\n", (u32)info->start,
	      rbytes, ret == 0 ? "OK" : "ERROR");

	return ret;
}

int do_sunxi_flash(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	struct blk_desc *desc;
	struct disk_partition info = { 0 };
	ulong load_addr;
	ulong load_size = 0;
	char *cmd;
	char *part_name;
	int ret;

#ifdef CONFIG_SUNXI_SPRITE
	static int partdata_format;
#endif

	/* at least four arguments please */
	if (argc < 4)
		goto usage;

#ifdef CONFIG_SUNXI_SPRITE
	else if (argc < 5)
		partdata_format = 0;
#endif

	cmd       = argv[1];
	part_name = argv[3];

	sunxi_flash_dev_get_blk(&desc);
	if (desc == NULL)
		return -ENODEV;

	if (strncmp(cmd, "read", strlen("read")) == 0) {
		load_addr = (ulong)simple_strtoul(argv[2], NULL, 16);
		if (argc == 5)
			load_size = (ulong)simple_strtoul(argv[4], NULL, 16);
		env_set("boot_from_partion", part_name);
	}
#ifdef CONFIG_SUNXI_SPRITE
	 else if (!strncmp(cmd, "write", strlen("write"))) {
		load_addr = (ulong)simple_strtoul(argv[2], NULL, 16);
		if (!strncmp(part_name, "boot_package", strlen("boot_package")) ||
			!strncmp(part_name, "uboot", strlen("uboot")) ||
			!strncmp(part_name, "toc1", strlen("toc1"))) {
			return sunxi_sprite_download_uboot((void *)load_addr,
					get_boot_storage_type(), 0);
		} else if (!strncmp(part_name, "boot0", strlen("boot0")) ||
			!strncmp(part_name, "toc0", strlen("toc0"))) {
			return sunxi_sprite_download_boot0((void *)load_addr,
					get_boot_storage_type());
		}
#if defined(CONFIG_AW_MTD_SPINAND) || defined(CONFIG_AW_MTD_RAWNAND)
		if (strncmp(cmd, "write_mtd", strlen("write_mtd")) == 0) {
			ret = sunxi_flash_try_partition(desc, part_name, &info);
			if (ret < 0)
				return -1;

			info.size = (info.size + 511) / 512;
			ret = sunxi_flash_phywrite(info.start, info.size, (void *)load_addr);
			sunxi_flash_flush();
			return ret;
		}
#endif
		/* write size: indecated on partemeter 1 */
		if (sunxi_partition_get_info_byname(part_name,
					(uint *)&info.start, (uint *)&info.size))
			goto usage;
		if (argc == 5) {
			/* write size: partemeter 2 */
			info.size = ALIGN((u32)simple_strtoul(argv[4], NULL, 16), 512)/512;
		} /*else if (argc == 6) {
			info.start += ALIGN((u32)simple_strtoul(argv[4], NULL, 16), 512)/512;
			info.size = ALIGN((u32)simple_strtoul(argv[5], NULL, 16), 512)/512;
			if (simple_strtoul(argv[4], NULL, 16) == 0) {
				partdata_format = unsparse_probe((char *)load_addr, info.size*512, (u32)info.start);
				pr_debug("partdata_format:%d\n", partdata_format);
			}
		}

		if (partdata_format != ANDROID_FORMAT_DETECT) {*/
			ret = sunxi_flash_write(info.start, info.size, (void *)load_addr);
		/*} else {
			ret = unsparse_direct_write((void *)load_addr, (u32)simple_strtoul(argv[5], NULL, 16)) ? 0 : 1;
		}*/

		sunxi_flash_flush();
		pr_debug("sunxi flash write :offset %lx, %ld bytes %s\n",
				info.start, info.size*512, ret ? "OK" : "ERROR");

		return ret;

	}
#endif
	else {
		goto usage;
	}

#if defined(CONFIG_AW_MTD_SPINAND) || defined(CONFIG_AW_MTD_RAWNAND)
	if (strncmp(cmd, "read_mtd", strlen("read_mtd")) == 0) {
		ret = sunxi_flash_try_partition(desc, part_name, &info);
		if (ret < 0)
			return -1;
		if (load_size)
			info.size = load_size;
		info.size = (info.size + 511) / 512;
		return sunxi_flash_phyread(info.start, info.size, (void *)load_addr);
	}
#endif

	ret = sunxi_flash_try_partition(desc, part_name, &info);
	if (ret < 0)
		return -ENODEV;
	pr_debug("partinfo: name %s, start 0x%lx, size 0x%lx\n",
			info.name, info.start, info.size);
	return sunxi_flash_read_part(desc, &info, load_addr, load_size);

usage:
	return cmd_usage(cmdtp);
}

U_BOOT_CMD(sunxi_flash, 6, 1, do_sunxi_flash, "sunxi_flash sub-system",
	   "sunxi_flash read mem_addr part_name [size]\n"
	   "sunxi_flash read_mtd mem_addr part_name [size]\n"
	   "sunxi_flash write <mem_addr> <part_name> [size]\n"
	   "sunxi_flash write <mem_addr> <part_name> [offset] [size]\n"
	   "sunxi_flash write_mtd <mem_addr> <part_name>\n");
