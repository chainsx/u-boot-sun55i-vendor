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

#ifndef __SPRITE_PRIVATEDATA_H__

#define __SPRITE_PRIVATEDATA_H__


extern int sunxi_sprite_store_part_data(void  *mbr);

extern int sunxi_sprite_restore_part_data(void  *mbr);

extern int sunxi_sprite_probe_prvt(void  *mbr);

extern int sunxi_sprite_erase_private_key(void *buffer);

#endif
