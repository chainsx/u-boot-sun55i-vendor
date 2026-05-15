
/*
 * Copyright (C) 2023 Allwinner.
 * huangrongcun <huangrongcun@allwinnertech.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef _SUNXI_BOARD_H_
#define _SUNXI_BOARD_H_

#include <part.h>
#include <spare_head.h>
#include <sunxi_log.h>

extern int sunxi_flash_try_partition(struct blk_desc *desc, const char *str,
				     struct disk_partition *info);
#include <asm/types.h>

extern int sunxi_probe_secure_monitor(void);
extern int sunxi_probe_secure_os(void);
extern int sunxi_probe_securemode(void);
extern int sunxi_get_securemode(void);
extern int get_boot_work_mode(void);
extern int sunxi_get_force_32bit_os(void);

extern int smc_init(void);

void sunxi_dump(void *addr, unsigned int size);
int sunxi_board_run_fel(void);
int disable_interrupts(void);
int sunxi_board_restart(int next_mode);
int sunxi_board_shutdown(void);
int sunxi_board_shutdown_charge(void);

u32 sunxi_generate_checksum(void *buffer, u32 length, u32 div, u32 src_sum);
u32 sunxi_verify_checksum(void *buffer, u32 length, u32 src_sum);
int aw_bsp_reserve_board_f(void);
int aw_bsp_board_f_init(void);
int aw_bsp_board_f_init_ago_dm(void);
int sunxi_get_secureboard(void);

int sunxi_auto_fel_by_usb(void);
int sunxi_keydata_burn_by_usb(void);
int sunxi_usb_dev_register(uint dev_name);
extern void sunxi_usb_main_loop(int delaytime);

int sunxi_probe_android_kernel(void);

extern int get_boot_storage_type(void);
extern int get_boot_storage_type_ext(void);
void set_boot_storage_type(int storage_type);

extern int get_sprite_storage_type(void);
void set_sprite_storage_type(int storage_type);

extern void set_boot_work_mode(int work_mode);
extern int sunxi_set_uboot_shell(int flag);
void sunxi_dump(void *addr, unsigned int size);
int sunxi_board_run_fel(void);
int disable_interrupts(void);

int sunxi_set_serial_num(void);
int sunxi_get_sid(unsigned int *sid);

int sunxi_get_irq(struct udevice *dev);

extern int sunxi_boot_image_get_embbed_cert_len(const void *hdr);
extern int sunxi_boot_fitimage_get_embbed_cert_len(const void *hdr);

int sunxi_set_sramc_mode(void);
int sunxi_boot_tone_play(void);
extern void sunxi_update_subsequent_processing(int next_work);
extern int mmc_read_info(int dev_num, void *buffer, u32 buffer_size, void *priv_info);
extern int mmc_write_info(int dev_num, void *buffer, u32 buffer_size);

#endif /*_SUNXI_BOARD_H_ */
