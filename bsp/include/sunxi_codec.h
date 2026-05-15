/*
 * (C) Copyright 2019-2025
 * allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * xudongpdc <xudongpdc@allwinnertech.com>
 *
 * some simple description for this code
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#ifndef _SUNXI_CODEC_H
#define _SUNXI_CODEC_H

struct udevice;


int sunxi_codec_playback_prepare(struct udevice *dev);
int sunxi_codec_playback_start(struct udevice *dev, ulong handle, u32 *srcBuf, u32 cnt);
int sunxi_codec_playback_stop(struct udevice *dev, ulong handle);
int sunxi_codec_playback_debug(struct udevice *dev);

void sunxi_codec_fill_txfifo(struct udevice *dev, u32 *data);
void sunxi_codec_dump_reg(void);

#endif /* __SUNXI_CODEC_H */
