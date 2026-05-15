// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2023-2027
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Tom Cubie <huangrongcun@allwinnertech.com>
 *
 * SRAM init for older sunxi SoCs.
 */

#ifndef __SUNXI_FLASH_H__
#define __SUNXI_FLASH_H__
#include <dm/uclass-id.h>
#include <efi.h>
#include <blk.h>

/*
 * With driver model (CONFIG_sunxi_flash) this is uclass platform data, accessible
 * with dev_get_uclass_plat(dev)
 */
struct sunxi_flash_desc {
	/*
	 * TODO: With driver model we should be able to use the parent
	 * device's uclass instead.
	 */
	enum uclass_id uclass_id; /* type of the interface */
	int devnum; /* device number */
	unsigned char type; /* device type */
	unsigned char removable; /* removable device */
	struct udevice *bdev;
	lbaint_t	lba;		/* number of blocks */
	unsigned long	blksz;		/* block size */
};

struct sunxi_flash_ops {
	// int (*probe)(struct udevice *dev);
	// int (*init)(struct udevice *dev, int stage, int);
	int (*exit)(struct udevice *dev, int force);
	unsigned long (*read)(struct udevice *dev, lbaint_t start,
			      lbaint_t blkcnt, void *buffer);
	unsigned long (*write)(struct udevice *dev, lbaint_t start,
			       lbaint_t blkcnt, void *buffer);
	unsigned long (*erase)(struct udevice *dev, lbaint_t start,
			       lbaint_t blkcnt);
	int (*force_erase)(struct udevice *dev);
	int (*flush)(struct udevice *dev);
	uint (*size)(struct udevice *dev);
	uint (*get_logical_offset)(struct udevice *dev);
	uint (*get_storage)(struct udevice *dev);
	int (*phyread)(struct udevice *dev, lbaint_t start, lbaint_t blkcnt,
		       void *buffer);
	int (*phywrite)(struct udevice *dev, lbaint_t start, lbaint_t blkcnt,
			void *buffer);
	int (*phyerase)(struct udevice *dev, lbaint_t start, lbaint_t blkcnt, void *skip);
	int (*download_spl)(struct udevice *dev, unsigned char *buf, int len,
			    unsigned int ext);
	int (*download_boot_param)(struct udevice *dev);
	int (*download_toc)(struct udevice *dev, unsigned char *buf, int len,
			    unsigned int ext);
	int (*upload_toc)(struct udevice *dev, void *buf, unsigned int len);
	int (*write_end)(struct udevice *dev);
	int (*erase_area)(struct udevice *dev, lbaint_t start_bloca,
			  lbaint_t nblock);
	int (*update_backup_boot0)(struct udevice *dev);
#if CONFIG_IS_ENABLED(SUNXI_PRIVATE_STORAGE)
	unsigned long (*pristorage_read)(struct udevice *dev, int item,
					 void *buffer, unsigned int len);
	unsigned long (*pristorage_write)(struct udevice *dev, int item,
					  void *buffer, unsigned int len);
	int (*secstorage_flush)(struct udevice *dev);
	int (*secstorage_fast_write)(struct udevice *dev, int item,
				     unsigned char *buf, unsigned int len);
#endif
};

#define sunxi_flash_get_ops(dev) ((struct sunxi_flash_ops *)(dev)->driver->ops)

int sunxi_flash_create_device(struct udevice *parent, const char *drv_name,
			      const char *name, int uclass_id, int devnum,
			      struct udevice **devp);
int sunxi_flash_create_devicef(struct udevice *parent, const char *drv_name,
			       const char *name, int uclass_id, int devnum,
			       struct udevice **devp);

uint sunxi_sprite_size(void);
long sunxi_sprite_read(lbaint_t start, lbaint_t blkcnt, void *dst);
int sunxi_sprite_init(int boot_mode);
long sunxi_sprite_write(lbaint_t start, lbaint_t blkcnt, void *src);
long sunxi_sprite_phywrite(lbaint_t start_block, lbaint_t nblock, void *buffer);
long sunxi_sprite_phyread(lbaint_t start_block, lbaint_t nblock, void *buffer);
long sunxi_sprite_phyerase(lbaint_t start_block, lbaint_t nblock, void *buffer);
int sunxi_sprite_write_end(void);
int sunxi_sprite_flush(void);

long sunxi_flash_read(lbaint_t start, lbaint_t blkcnt, void *dst);
long sunxi_flash_write(lbaint_t start, lbaint_t blkcnt, void *src);
long sunxi_flash_erase(lbaint_t start, lbaint_t blkcnt);

long sunxi_flash_phyread(lbaint_t start_block, lbaint_t nblock, void *buffer);
long sunxi_flash_phywrite(lbaint_t start_block, lbaint_t nblock, void *buffer);
long sunxi_flash_phyerase(lbaint_t start_block, lbaint_t nblock, void *buffer);
uint sunxi_flash_size(void);
uint sunxi_flash_get_logical_offset(void);
int sunxi_flash_download_spl(unsigned char *buf, int len, unsigned int ext);
int sunxi_flash_download_boot_param(void);
int sunxi_flash_download_toc(unsigned char *buf, int len, unsigned int ext);
int sunxi_flash_write_end(void);
int sunxi_flash_flush(void);
int sunxi_flash_exit(int force);
int sunxi_flash_dev_get_blk(struct blk_desc **descp);
const char *sunxi_flash_to_storage_string(int storage_type);
int sunxi_flash_get_storage(void);

#if CONFIG_IS_ENABLED(SUNXI_PRIVATE_STORAGE)
long sunxi_flash_pristorage_read(int item, void *buffer, unsigned int len);
long sunxi_flash_pristorage_write(int item, void *buffer, unsigned int len);
long sunxi_flash_pristorage_flush(void);
long sunxi_flash_pristorage_fast_write(int item, unsigned char *buf,
				  unsigned int len);
#endif

extern unsigned long sunxi_spinor_init(void);
extern int sunxi_ubi_nand_init(void);
extern int ubi_nand_update_ubi_env(void);
#endif //__SUNXI_FLASH_H__
