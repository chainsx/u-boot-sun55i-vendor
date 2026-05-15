// SPDX-License-Identifier: GPL-2.0+
/*
 * SPINOR driver for allwinner sunxi platform.
 *
 */

#include <common.h>
#include <command.h>
#include <errno.h>
#include <dm.h>
#include <log.h>
#include <sunxi_flash.h>
#include <blk.h>
#include <spare_head.h>
#include <asm/global_data.h>
#include <linux/mtd/aw-ubi.h>
#include "spinand/sunxi-spinand.h"
#ifdef CONFIG_AW_MTD_SPINAND
#include <linux/mtd/aw-spinand.h>
struct udevice *spinand_dev;
#endif
#ifdef CONFIG_AW_MTD_RAWNAND
#include <linux/mtd/aw-rawnand.h>
struct udevice *rawnand_dev;
#endif

static int init_end;

DECLARE_GLOBAL_DATA_PTR;

static unsigned long sunxi_flash_nand_read(struct udevice *dev, lbaint_t start,
			      lbaint_t blkcnt, void *buffer)
{
	int ret = -EINVAL;

	if (!init_end)
		return -EBUSY;

#ifdef CONFIG_AW_MTD_SPINAND
	if (spinand_dev && dev->parent == spinand_dev)
		return spinand_mtd_read_ubi(start, blkcnt, buffer);
#endif

#ifdef CONFIG_AW_MTD_RAWNAND
	if (rawnand_dev && dev->parent == rawnand_dev)
		return rawnand_mtd_read_ubi(start, blkcnt, buffer);
#endif
	return ret;
}

static unsigned long sunxi_flash_nand_write(struct udevice *dev, lbaint_t start,
			       lbaint_t blkcnt, void *buffer)
{
	int ret = -EINVAL;

	if (!init_end)
		return -EBUSY;

#ifdef CONFIG_AW_MTD_SPINAND
	if (spinand_dev && dev->parent == spinand_dev)
		return spinand_mtd_write_ubi(start, blkcnt, buffer);
#endif

#ifdef CONFIG_AW_MTD_RAWNAND
	if (rawnand_dev && dev->parent == rawnand_dev)
		return rawnand_mtd_write_ubi(start, blkcnt, buffer);
#endif
	return ret;
}

static unsigned long sunxi_flash_nand_erase(struct udevice *dev, lbaint_t start,
			       lbaint_t blkcnt)
{
	int ret = -EINVAL;

	if (!init_end)
		return -EBUSY;

#ifdef CONFIG_AW_MTD_SPINAND
	if (spinand_dev && dev->parent == spinand_dev)
		return spinand_mtd_erase(1);
#endif

#ifdef CONFIG_AW_MTD_RAWNAND
	if (rawnand_dev && dev->parent == rawnand_dev)
		return rawnand_mtd_erase(1);
#endif
	return ret;
}

static int sunxi_flash_nand_flush(struct udevice *dev)
{
	int ret = -EINVAL;

	if (!init_end)
		return -EBUSY;

#ifdef CONFIG_AW_MTD_SPINAND
	if (spinand_dev && dev->parent == spinand_dev)
		return spinand_mtd_flush();
#endif

#ifdef CONFIG_AW_MTD_RAWNAND
	if (rawnand_dev && dev->parent == rawnand_dev)
		return rawnand_mtd_flush();

#endif
	return ret;
}

static uint sunxi_flash_nand_get_size(struct udevice *dev)
{
	int ret = -EINVAL;

	if (!init_end)
		return -EINVAL;

#ifdef CONFIG_AW_MTD_SPINAND
	if (spinand_dev && dev->parent == spinand_dev)
		return spinand_mtd_size() >> 9;
#endif

#ifdef CONFIG_AW_MTD_RAWNAND
	if (rawnand_dev && dev->parent == rawnand_dev)
		return rawnand_mtd_size() >> 9;

#endif
	return ret;
}

static uint sunxi_flash_nand_get_logical_offset(struct udevice *dev)
{
	int ret = -EINVAL;

#ifdef CONFIG_AW_MTD_SPINAND
	if (spinand_dev && dev->parent == spinand_dev)
		return spinand_sys_part_offset() / 512;
#endif

#ifdef CONFIG_AW_MTD_RAWNAND
	if (rawnand_dev && dev->parent == rawnand_dev)
		return rawnand_sys_part_offset() / 512;
#endif

	return ret;
}

static uint sunxi_flash_nand_get_storage(struct udevice *dev)
{
	int ret = -EINVAL;

#ifdef CONFIG_AW_MTD_SPINAND
	if (spinand_dev && dev->parent == spinand_dev)
		return STORAGE_SPI_NAND;
#endif

#ifdef CONFIG_AW_MTD_RAWNAND
	if (rawnand_dev && dev->parent == rawnand_dev)
		return STORAGE_NAND;
#endif

	return ret;
}

static int sunxi_flash_nand_phyread(struct udevice *dev, lbaint_t start,
			      lbaint_t blkcnt, void *buffer)
{
	int ret = -EINVAL;

	if (!init_end)
		return -EBUSY;

#ifdef CONFIG_AW_MTD_SPINAND
	if (spinand_dev && dev->parent == spinand_dev)
		return aw_spinand_mtd_read(start, blkcnt, buffer);
#endif

#ifdef CONFIG_AW_MTD_RAWNAND
	if (rawnand_dev && dev->parent == rawnand_dev)
		return aw_rawnand_phy_read(start, blkcnt, buffer);
#endif
	return ret;
}

static int sunxi_flash_nand_phywrite(struct udevice *dev, lbaint_t start,
			       lbaint_t blkcnt, void *buffer)
{
	int ret = -EINVAL;

	if (!init_end)
		return -EBUSY;

#ifdef CONFIG_AW_MTD_SPINAND
	if (spinand_dev && dev->parent == spinand_dev)
		return aw_spinand_mtd_write(start, blkcnt, buffer);
#endif

#ifdef CONFIG_AW_MTD_RAWNAND
	if (rawnand_dev && dev->parent == rawnand_dev)
		return aw_rawnand_phy_write(start, blkcnt, buffer);
#endif
	return ret;
}

static int sunxi_flash_nand_phyerase(struct udevice *dev, lbaint_t start,
			       lbaint_t blkcnt, void *skip)
{
	return 0;
}

static int sunxi_flash_nand_download_spl(struct udevice *dev,
		unsigned char *buf, int len, unsigned int ext)
{
	int ret = -EINVAL;

	if (!init_end)
		return -EBUSY;

#ifdef CONFIG_AW_MTD_SPINAND
	if (spinand_dev && dev->parent == spinand_dev)
		return spinand_mtd_download_boot0(len, buf);
#endif

#ifdef CONFIG_AW_MTD_RAWNAND
	if (rawnand_dev && dev->parent == rawnand_dev)
		return rawnand_mtd_download_boot0(len, buf);
#endif

	return ret;
}

static int sunxi_flash_nand_download_toc(struct udevice *dev,
		unsigned char *buf, int len, unsigned int ext)
{
	int ret = -EINVAL;

	if (!init_end)
		return -EBUSY;

#ifdef CONFIG_AW_MTD_SPINAND
	if (spinand_dev && dev->parent == spinand_dev)
		return spinand_mtd_download_uboot(len, buf);
#endif
#ifdef CONFIG_AW_MTD_RAWNAND
	if (rawnand_dev && dev->parent == rawnand_dev)
		return rawnand_mtd_download_uboot(len, buf);
#endif
	return ret;
}

static int sunxi_flash_nand_download_bootparam(struct udevice *dev)
{
	int ret = -EINVAL;

	if (!init_end)
		return -EBUSY;

#ifdef CONFIG_SUNXI_BOOT_PARAM
#ifdef CONFIG_AW_MTD_SPINAND
	if (spinand_dev && dev->parent == spinand_dev)
		spinand_download_boot_param();
	return 0;
#endif

#ifdef CONFIG_AW_MTD_RAWNAND
	if (rawnand_dev && dev->parent == rawnand_dev)
		rawnand_download_boot_param();
	return 0;
#endif
#else
	printf("ubi nand:unsupport boot_param\n");
#endif /*CONFIG_SUNXI_BOOT_PARAM*/
	return ret;
}

static int sunxi_flash_nand_write_end(struct udevice *dev)
{
	int ret = -EINVAL;

	if (!init_end)
		return -EBUSY;

#ifdef CONFIG_AW_MTD_SPINAND
	if (spinand_dev && dev->parent == spinand_dev)
		return spinand_mtd_write_end();
#endif
#ifdef CONFIG_AW_MTD_RAWNAND
	if (rawnand_dev && dev->parent == rawnand_dev)
		return rawnand_mtd_write_end();
#endif
	return ret;
}

unsigned long sunxi_flash_nand_private_storage_read(struct udevice *dev, int item,
					   void *buffer, unsigned int len)
{
	int ret = -EINVAL;

	if (!init_end)
		return -EBUSY;

#ifdef CONFIG_AW_MTD_SPINAND
	if (spinand_dev && dev->parent == spinand_dev)
		return spinand_mtd_secure_storage_read(item, buffer, len);
#endif

#ifdef CONFIG_AW_MTD_RAWNAND
	if (rawnand_dev && dev->parent == rawnand_dev)
		return rawnand_mtd_secure_storage_read(item, buffer, len);
#endif
	return ret;
}

unsigned long sunxi_flash_nand_private_storage_write(struct udevice *dev, int item,
					   void *buffer, unsigned int len)
{
	int ret = -EINVAL;

	if (!init_end)
		return -EBUSY;

#ifdef CONFIG_AW_MTD_SPINAND
	if (spinand_dev && dev->parent == spinand_dev)
		return spinand_mtd_secure_storage_write(item, buffer, len);
#endif

#ifdef CONFIG_AW_MTD_RAWNAND
	if (rawnand_dev && dev->parent == rawnand_dev)
		return rawnand_mtd_secure_storage_write(item, buffer, len);
#endif
	return ret;
}

static const struct sunxi_flash_ops sunxi_flash_nand_ops = {
	.exit = NULL,
	.read = sunxi_flash_nand_read,
	.write = sunxi_flash_nand_write,
	.erase = sunxi_flash_nand_erase,
	.force_erase = NULL,
	.flush = sunxi_flash_nand_flush,
	.size = sunxi_flash_nand_get_size,
	.get_logical_offset = sunxi_flash_nand_get_logical_offset,
	.get_storage = sunxi_flash_nand_get_storage,
	.phyread = sunxi_flash_nand_phyread,
	.phywrite = sunxi_flash_nand_phywrite,
	.phyerase = sunxi_flash_nand_phyerase,
	.download_spl = sunxi_flash_nand_download_spl,
	.download_boot_param = sunxi_flash_nand_download_bootparam,
	.download_toc = sunxi_flash_nand_download_toc,
	.upload_toc = NULL,
	.write_end = sunxi_flash_nand_write_end,
	.erase_area = NULL,
	.update_backup_boot0 = NULL,
#if CONFIG_IS_ENABLED(SUNXI_PRIVATE_STORAGE)
	.pristorage_read = sunxi_flash_nand_private_storage_read,
	.pristorage_write = sunxi_flash_nand_private_storage_write,
	.secstorage_flush = NULL,
	.secstorage_fast_write = NULL,
#endif
};

U_BOOT_DRIVER(sunxi_nand) = {
	.name		= "sunxi_nand",
	.id		= UCLASS_SUNXI_FLASH,
	.ops		= &sunxi_flash_nand_ops,
	.flags		= DM_FLAG_OS_PREPARE,
};

static int sunxi_flash_nand_ops_init(struct udevice *dev)
{
	int ret = 0;
	struct udevice *bdev;

	ret = sunxi_flash_create_devicef(dev, "sunxi_nand", "sunxi_flash",
					UCLASS_MTD, dev_seq(dev), &bdev);
	if (ret) {
		pr_err("Cannot create sunxi nand flash device\n");
		return ret;
	}

	return 0;
}

int sunxi_ubi_nand_init(void)
{
	int ret = -ENODEV;
	int ret_spinand = -ENODEV;
	int ret_rawnand = -ENODEV;

	if (init_end)
		return 0;

#ifdef CONFIG_AW_MTD_SPINAND
	unsigned int busnum = 0;
	unsigned int cs = 0;
	ret_spinand = spinand_mtd_init(busnum, cs, &spinand_dev);
	if (ret_spinand)
		pr_err("spinand flash probe error\n");
	else {
		ret_spinand = sunxi_flash_nand_ops_init(spinand_dev);
		/* ubi attch mtd */
		if (!ret_spinand)
			ret_spinand = spinand_ubi_attach_mtd();
	}
	//if (!ret_spinand)
	//	set_boot_storage_type(STORAGE_SPI_NAND);
#endif

#ifdef CONFIG_AW_MTD_RAWNAND
	ret_rawnand = rawnand_mtd_init(&rawnand_dev);
	if (ret_rawnand)
		pr_err("rawnand flash probe error\n");
	else {
		ret_rawnand = sunxi_flash_nand_ops_init(rawnand_dev);
		/* ubi attch mtd */
		if (!ret_rawnand)
			ret_rawnand = rawnand_ubi_attach_mtd();
	}
	//if (!ret_rawnand)
	//	set_boot_storage_type(STORAGE_NAND);
#endif
	ret = (ret_spinand && ret_rawnand) ? ret : 0;
	init_end = ret == 0 ? true : false;

	return ret;
}

int ubi_nand_update_ubi_env(void)
{
	int ret = -EINVAL;

	if (!init_end)
		return -EBUSY;

#ifdef CONFIG_AW_MTD_SPINAND
	return spinand_mtd_update_ubi_env();
#endif

#ifdef CONFIG_AW_MTD_RAWNAND
	return rawnand_mtd_update_ubi_env();
#endif
	return ret;
}
