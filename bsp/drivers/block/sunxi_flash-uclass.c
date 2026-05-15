/* SPDX-License-Identifier: GPL-2.0+
 * Copyright (C) 2023 Allwinnertech
*/
#define DEBUG
#include <common.h>
#include <dm.h>
#include <log.h>
#include <malloc.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <dm/uclass-internal.h>
#include <linux/err.h>
#include <sunxi_flash.h>
#include <sunxi_board.h>
#include <spare_head.h>
#include <blk.h>
#include <part.h>

struct udevice *cur_sunxi_flash;
struct udevice *spr_sunxi_flash;
struct udevice *cur_sunxi_flash_blk;

static int set_sunxi_flash_curdev(struct udevice *dev)
{
	int ret;
	int workmode = get_boot_work_mode();
	struct sunxi_flash_desc *desc;

	desc = dev_get_uclass_plat(dev);

	if (workmode == WORK_MODE_CARD_PRODUCT) {
#if defined(CONFIG_AW_MTD_SPINAND) || defined(CONFIG_AW_MTD_RAWNAND)
		if (!strstr(dev->name, "mmc")) {
			spr_sunxi_flash = dev;
			pr_err("%s...%d:spr_sunxi_flash :%s\n", __func__, __LINE__,
					spr_sunxi_flash->name);
		} else {
			cur_sunxi_flash = dev;
			pr_err("%s...%d:cur_sunxi_flash :%s\n", __func__, __LINE__,
					cur_sunxi_flash->name);
		}
#else


		if (!strstr(dev->name, "mmc")) {
			spr_sunxi_flash = dev;
			pr_err("%s...%d:spr_sunxi_flash :%s\n", __func__, __LINE__,
					spr_sunxi_flash->name);
		} else {
			if (desc->devnum) {
				spr_sunxi_flash = dev;
				pr_err("%s...%d:spr_sunxi_flash :%s\n", __func__, __LINE__,
						spr_sunxi_flash->name);
			} else {
				cur_sunxi_flash = dev;
				pr_err("%s...%d:cur_sunxi_flash :%s\n", __func__, __LINE__,
						cur_sunxi_flash->name);
			}
		}
#endif
	} else {
		cur_sunxi_flash = dev;
		spr_sunxi_flash = dev;
		pr_err("%s...%d:cur_sunxi_flash :%s\n", __func__, __LINE__,
				cur_sunxi_flash->name);
	}

	ret = device_probe(dev);
	if (ret) {
		pr_err("probing %s failed\n", dev->name);
		device_unbind(dev);
	}

	return ret;
}

static int set_sunxi_flash_blk_curdev(struct udevice *dev)
{
	int ret = 0;
	const struct sunxi_flash_ops *ops = sunxi_flash_get_ops(dev->parent);
	int storage_type = ops->get_storage(dev->parent);

	cur_sunxi_flash_blk = dev;
	if (storage_type == STORAGE_EMMC) {
		//mmc blk device Use native probe
		return 0;
	} else {
		ret = device_probe(dev);
		if (ret) {
			pr_err("probing %s failed\n", dev->name);
			device_unbind(dev);
		}
	}

	return ret;
}

int sunxi_flash_create_device(struct udevice *parent, const char *drv_name,
			      const char *name, int uclass_id, int devnum,
			      struct udevice **devp)
{
	struct sunxi_flash_desc *desc;
	struct udevice *dev;
	int ret;

	ret = device_bind_driver(parent, drv_name, name, &dev);
	if (ret)
		return ret;
	desc = dev_get_uclass_plat(dev);
	desc->uclass_id = uclass_id;
	desc->bdev = dev;
	desc->devnum = devnum;
	*devp = dev;
	set_sunxi_flash_curdev(dev);
	return 0;
}

int sunxi_flash_create_devicef(struct udevice *parent, const char *drv_name,
			       const char *name, int uclass_id, int devnum,
			       struct udevice **devp)
{
	char dev_name[30], *str;
	int ret;

	snprintf(dev_name, sizeof(dev_name), "%s.%s", parent->name, name);
	str = strdup(dev_name);
	if (!str)
		return -ENOMEM;

	ret = sunxi_flash_create_device(parent, drv_name, str, uclass_id,
					devnum, devp);
	if (ret) {
		free(str);
		return ret;
	}
	device_set_name_alloced(*devp);

	const struct sunxi_flash_ops *ops = sunxi_flash_get_ops(*devp);
	struct udevice *bdev = NULL;
	struct blk_desc *desc;
	int blksz = 512;

	ret = blk_create_devicef(*devp, "sunxi_flash_blk", "blk", UCLASS_SUNXI_FLASH,
				dev_seq(*devp), blksz, ops->size(*devp), &bdev);
	if (ret) {
		debug("Cannot create block device\n");
		return ret;
	}

	desc = dev_get_uclass_plat(bdev);
	set_sunxi_flash_blk_curdev(bdev);
	return 0;
}

UCLASS_DRIVER(sunxiflash) = {
	.id = UCLASS_SUNXI_FLASH,
	.name = "sunxiflash",
	.per_device_plat_auto = sizeof(struct sunxi_flash_desc),
};

long sunxi_flash_dev_read(struct udevice *dev, lbaint_t start, lbaint_t blkcnt,
			  void *dst)
{
	const struct sunxi_flash_ops *ops = sunxi_flash_get_ops(dev);
	if (!ops->read)
		return -ENOSYS;

	return ops->read(dev, start, blkcnt, dst);
}

long sunxi_flash_dev_write(struct udevice *dev, lbaint_t start, lbaint_t blkcnt,
			   void *src)
{
	const struct sunxi_flash_ops *ops = sunxi_flash_get_ops(dev);
	if (!ops->write)
		return -ENOSYS;

	return ops->write(dev, start, blkcnt, src);
}

long sunxi_flash_dev_erase(struct udevice *dev, lbaint_t start, lbaint_t blkcnt)
{
	const struct sunxi_flash_ops *ops = sunxi_flash_get_ops(dev);
	if (!ops->erase)
		return -ENOSYS;

	return ops->erase(dev, start, blkcnt);
}

long sunxi_flash_dev_phyread(struct udevice *dev, lbaint_t start_block, lbaint_t nblock, void *buffer)
{
	const struct sunxi_flash_ops *ops = sunxi_flash_get_ops(dev);
	if (!ops->phyread)
		return -ENOSYS;

	return ops->phyread(dev, start_block, nblock, buffer);
}

long sunxi_flash_dev_phywrite(struct udevice *dev, lbaint_t start, lbaint_t blkcnt, void *dst)
{
	const struct sunxi_flash_ops *ops = sunxi_flash_get_ops(dev);
	if (!ops->phywrite)
		return -ENOSYS;

	return ops->phywrite(dev, start, blkcnt, dst);
}

long sunxi_flash_dev_phyerase(struct udevice *dev, lbaint_t start, lbaint_t blkcnt, void *skip)
{
	const struct sunxi_flash_ops *ops = sunxi_flash_get_ops(dev);
	if (!ops->phyerase)
		return -ENOSYS;

	return ops->phyerase(dev, start, blkcnt, skip);
}

int sunxi_flash_dev_init(int boot_mod, struct udevice *dev)
{
	const struct sunxi_flash_ops *ops = sunxi_flash_get_ops(dev);
	if (!ops->size)
		return -ENOSYS;

#if defined(CONFIG_AW_MTD_SPINAND) || defined(CONFIG_AW_MTD_RAWNAND)
	set_boot_storage_type(STORAGE_NAND);
#else
	set_boot_storage_type(get_sprite_storage_type());
#endif
	return 0;
}

uint sunxi_flash_dev_size(struct udevice *dev)
{
	const struct sunxi_flash_ops *ops = sunxi_flash_get_ops(dev);
	if (!ops->size)
		return -ENOSYS;

	return ops->size(dev);
}

uint sunxi_flash_dev_get_logical_offset(struct udevice *dev)
{
	const struct sunxi_flash_ops *ops = sunxi_flash_get_ops(dev);
	if (!ops->get_logical_offset)
		return -ENOSYS;

	return ops->get_logical_offset(dev);
}


int sunxi_flash_dev_download_spl(struct udevice *dev, unsigned char *buf, int len,
			    unsigned int ext)
{
	const struct sunxi_flash_ops *ops = sunxi_flash_get_ops(dev);
	if (!ops->download_spl)
		return -ENOSYS;

	return ops->download_spl(dev, buf, len, ext);
}

int sunxi_flash_dev_download_boot_param(struct udevice *dev)
{
	const struct sunxi_flash_ops *ops = sunxi_flash_get_ops(dev);
	if (!ops->download_boot_param)
		return -ENOSYS;

	return ops->download_boot_param(dev);
}

int sunxi_flash_dev_download_toc(struct udevice *dev, unsigned char *buf, int len,
			    unsigned int ext)
{
	const struct sunxi_flash_ops *ops = sunxi_flash_get_ops(dev);
	if (!ops->download_toc)
		return -ENOSYS;

	return ops->download_toc(dev, buf, len, ext);
}

int sunxi_flash_dev_write_end(struct udevice *dev)
{
	const struct sunxi_flash_ops *ops = sunxi_flash_get_ops(dev);
	if (!ops->write_end)
		return -ENOSYS;

	return ops->write_end(dev);
}

int sunxi_flash_dev_flush(struct udevice *dev)
{
	const struct sunxi_flash_ops *ops = sunxi_flash_get_ops(dev);
	if (!ops->flush)
		return -ENOSYS;

	return ops->flush(dev);
}

int sunxi_flash_dev_exit(struct udevice *dev, int force)
{
	const struct sunxi_flash_ops *ops = sunxi_flash_get_ops(dev);
	if (!ops->exit)
		return -ENOSYS;

	return ops->exit(dev, force);
}

int sunxi_sprite_init(int boot_mod)
{
	return sunxi_flash_dev_init(boot_mod, spr_sunxi_flash);
}

uint sunxi_sprite_size(void)
{
	return sunxi_flash_dev_size(spr_sunxi_flash);
}

long sunxi_sprite_read(lbaint_t start, lbaint_t blkcnt, void *dst)
{
	return sunxi_flash_dev_read(spr_sunxi_flash, start, blkcnt, dst);
}

long sunxi_sprite_write(lbaint_t start, lbaint_t blkcnt, void *src)
{
	return sunxi_flash_dev_write(spr_sunxi_flash, start, blkcnt, src);
}

long sunxi_sprite_phyread(lbaint_t start_block, lbaint_t nblock, void *buffer)
{
	return sunxi_flash_dev_phyread(spr_sunxi_flash, start_block, nblock, buffer);
}

long sunxi_sprite_phywrite(lbaint_t start_block, lbaint_t nblock, void *buffer)
{
	return sunxi_flash_dev_phywrite(spr_sunxi_flash, start_block, nblock, buffer);
}

long sunxi_sprite_phyerase(lbaint_t start_block, lbaint_t nblock, void *buffer)
{
	return sunxi_flash_dev_phyerase(spr_sunxi_flash, start_block, nblock, buffer);
}

int sunxi_sprite_write_end(void)
{
	return sunxi_flash_dev_write_end(spr_sunxi_flash);
}
int sunxi_sprite_flush(void)
{
	return sunxi_flash_dev_flush(spr_sunxi_flash);
}

long sunxi_flash_read(lbaint_t start, lbaint_t blkcnt, void *dst)
{
	return sunxi_flash_dev_read(cur_sunxi_flash, start, blkcnt, dst);
}

long sunxi_flash_write(lbaint_t start, lbaint_t blkcnt, void *src)
{
	return sunxi_flash_dev_write(cur_sunxi_flash, start, blkcnt, src);
}

long sunxi_flash_erase(lbaint_t start, lbaint_t blkcnt)
{
	return sunxi_flash_dev_erase(cur_sunxi_flash, start, blkcnt);
}

long sunxi_flash_phyread(lbaint_t start_block, lbaint_t nblock, void *buffer)
{
	return sunxi_flash_dev_phyread(cur_sunxi_flash, start_block, nblock, buffer);
}

long sunxi_flash_phywrite(lbaint_t start_block, lbaint_t nblock, void *buffer)
{
	return sunxi_flash_dev_phywrite(cur_sunxi_flash, start_block, nblock, buffer);
}

long sunxi_flash_phyerase(lbaint_t start_block, lbaint_t nblock, void *buffer)
{
	return sunxi_flash_dev_phyerase(cur_sunxi_flash, start_block, nblock, buffer);
}

uint sunxi_flash_size(void)
{
	return sunxi_flash_dev_size(cur_sunxi_flash);
}

uint sunxi_flash_get_logical_offset(void)
{
	return sunxi_flash_dev_get_logical_offset(cur_sunxi_flash);
}

int sunxi_flash_download_spl(unsigned char *buf, int len, unsigned int ext)
{
	return sunxi_flash_dev_download_spl(spr_sunxi_flash, buf, len, ext);
}

int sunxi_flash_download_boot_param(void)
{
	return sunxi_flash_dev_download_boot_param(cur_sunxi_flash);
}

int sunxi_flash_download_toc(unsigned char *buf, int len, unsigned int ext)
{
	return sunxi_flash_dev_download_toc(spr_sunxi_flash, buf, len, ext);
}

int sunxi_flash_write_end(void)
{
	return sunxi_flash_dev_write_end(cur_sunxi_flash);
}

int sunxi_flash_flush(void)
{
	return sunxi_flash_dev_flush(cur_sunxi_flash);
}

int sunxi_flash_exit(int force)
{
	return sunxi_flash_dev_exit(cur_sunxi_flash, force);
}

#if CONFIG_IS_ENABLED(SUNXI_PRIVATE_STORAGE)
long sunxi_flash_dev_pristorage_read(struct udevice *dev, int item,
				     void *buffer, unsigned int len)
{
	const struct sunxi_flash_ops *ops = sunxi_flash_get_ops(dev);
	ulong ret;
	if (!ops->pristorage_read)
		return -ENOSYS;

	ret = ops->pristorage_read(dev, item, buffer, len);
	return ret;
}

long sunxi_flash_dev_pristorage_write(struct udevice *dev, int item,
				      void *buffer, unsigned int len)
{
	const struct sunxi_flash_ops *ops = sunxi_flash_get_ops(dev);
	ulong ret;
	if (!ops->pristorage_write)
		return -ENOSYS;

	ret = ops->pristorage_write(dev, item, buffer, len);
	return ret;
}

long sunxi_flash_dev_pristorage_flush(struct udevice *dev)
{
	const struct sunxi_flash_ops *ops = sunxi_flash_get_ops(dev);
	if (!ops->secstorage_flush)
		return 0;

	return ops->secstorage_flush(dev);
}

int sunxi_flash_dev_pristorage_fast_write(struct udevice *dev, int item,
				     unsigned char *buf, unsigned int len)
{
	const struct sunxi_flash_ops *ops = sunxi_flash_get_ops(dev);
	if (!ops->secstorage_fast_write)
		return -ENOSYS;

	return ops->secstorage_fast_write(dev, item, buf, len);
}

long sunxi_flash_pristorage_read(int item, void *buffer, unsigned int len)
{
	return sunxi_flash_dev_pristorage_read(cur_sunxi_flash, item, buffer,
					       len);
}

long sunxi_flash_pristorage_write(int item, void *buffer, unsigned int len)
{
	return sunxi_flash_dev_pristorage_write(cur_sunxi_flash, item, buffer,
						len);
}

long sunxi_flash_pristorage_flush(void)
{
	return sunxi_flash_dev_pristorage_flush(cur_sunxi_flash);
}

long sunxi_flash_pristorage_fast_write(int item, unsigned char *buf,
				  unsigned int len)
{
	return sunxi_flash_dev_pristorage_fast_write(cur_sunxi_flash, item, buf,
						len);
}

#endif

const char *sunxi_bootstorage[STORAGE_MAX] = {
	"nand", "sd", "emmc", "nor", "emmc3", "spinand", "sd1", "emmc0", "ufs",
};
#define SUNXI_FLASH_ERR_STORAGE "err storage"

const char *sunxi_flash_to_storage_string(int storage_type)
{
	if (storage_type >= 0 && storage_type < STORAGE_MAX)
		return sunxi_bootstorage[storage_type];
	else
		return SUNXI_FLASH_ERR_STORAGE;
}

int sunxi_flash_dev_get_blk(struct blk_desc **descp)
{
	struct blk_desc *desc;

	if (sunxi_flash_get_storage() == STORAGE_EMMC) {
		//Use native processes
		desc = blk_get_devnum_by_uclass_id(UCLASS_MMC, 0);
	} else {
		desc = dev_get_uclass_plat(cur_sunxi_flash_blk);
	}

	pr_info(" DM_DRIVER_GET(sunxi_flash):%s...%d,desc uclass id = 0x%x desc->devnum = 0x%x\n",
		__func__, __LINE__, desc->uclass_id, desc->devnum);

	*descp = desc;
	return 0;
}

int sunxi_flash_get_dev_storage(struct udevice *dev)
{
	const struct sunxi_flash_ops *ops = sunxi_flash_get_ops(dev);
	if (!ops->get_storage)
		return -ENOSYS;

	return ops->get_storage(dev);
}

int sunxi_flash_get_storage(void)
{
	return sunxi_flash_get_dev_storage(cur_sunxi_flash);
}

static ulong sunxi_flash_blk_dev_read(struct udevice *dev, lbaint_t start,
				      lbaint_t blkcnt, void *buffer)

{
	const struct sunxi_flash_ops *ops = sunxi_flash_get_ops(dev->parent);
	int storage_type = ops->get_storage(dev->parent);

	if (storage_type == STORAGE_EMMC)
		return ops->phyread(dev->parent, start, blkcnt, (void *)buffer);
	else
		return ops->read(dev->parent, start, blkcnt, (void *)buffer);
}

static ulong sunxi_flash_blk_dev_write(struct udevice *dev, lbaint_t start,
				       lbaint_t blkcnt, const void *buffer)

{
	const struct sunxi_flash_ops *ops = sunxi_flash_get_ops(dev->parent);
	int storage_type = ops->get_storage(dev->parent);
	if (storage_type == STORAGE_EMMC)
		return ops->phywrite(dev->parent, start, blkcnt, (void *)buffer);
	else
		return ops->write(dev->parent, start, blkcnt, (void *)buffer);
}

static const struct blk_ops sunxi_flash_storage_ops = {
	.read = sunxi_flash_blk_dev_read,
	.write = sunxi_flash_blk_dev_write,
};

U_BOOT_DRIVER(sunxi_flash_blk) = {
	.name = "sunxi_flash_blk",
	.id = UCLASS_BLK,
	.ops = &sunxi_flash_storage_ops,
	.flags = DM_FLAG_ACTIVATED,
};
