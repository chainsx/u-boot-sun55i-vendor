/*
 * (C) Copyright 2007-2013
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

#ifndef __SUNXI_SPRITE_DOWNLOAD_H__
#define __SUNXI_SPRITE_DOWNLOAD_H__

#include <config.h>
#include <common.h>

int sunxi_sprite_download_mbr(void *buffer, uint buffer_size);

int sunxi_sprite_download_uboot(void *buffer, int production_media, int mode);

int sunxi_sprite_download_private_ubidev(void *buffer, uint length, int production_media);

int sunxi_sprite_upload_uboot(void *buffer, uint len);

int sunxi_sprite_download_boot0(void *buffer, int production_media);

int sunxi_sprite_erase_flash(void *);

int sunxi_mbr_convert_to_gpt(void *sunxi_mbr_buf, char *gpt_buf, int storage_type);

int gpt_convert_to_sunxi_mbr(void *sunxi_mbr_buf, char *gpt_buf, int storage_type);

void sunxi_get_logical_offset_param(int storage_type, u32 *logic_offset, int *total_sectors);

void dump_dram_para(void *dram, uint size);

int sunxi_sprite_get_boot0_cheksum(void *buffer, int *boot0_checksum);

#endif
