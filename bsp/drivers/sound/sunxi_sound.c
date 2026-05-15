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

#define LOG_CATEGORY UCLASS_SOUND

#include <common.h>
#include <command.h>
#include <audio_codec.h>
#include <clk.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <dm/device-internal.h>
#include <log.h>
#include <sound.h>
#include <dma.h>
#include <sunxi_codec.h>
#include <sunxi_sound.h>
#include <sunxi_board.h>
#include <asm/arch/dma.h>
#include <dma-uclass.h>

//#define sunxi_sound_debug

static void sunxi_sound_dma_cb_tx(void *p_arg)
{
	struct sunxi_sound_priv *uc_priv = (struct sunxi_sound_priv *)p_arg;

	dev_dbg(uc_priv->plat, "dma sound callback\n");
}

static int sunxi_sound_dma_init(struct sunxi_sound_priv *uc_priv)
{
	sunxi_dma_set dma_set;
	int ret;

	dma_set.channal_cfg.src_drq_type	= DMAC_CFG_TYPE_DRAM;
	dma_set.channal_cfg.src_addr_mode	= DMAC_CFG_SRC_ADDR_TYPE_LINEAR_MODE;
	dma_set.channal_cfg.src_burst_length	= DMAC_CFG_SRC_1_BURST;
	dma_set.channal_cfg.src_data_width	= DMAC_CFG_SRC_DATA_WIDTH_16BIT;
	dma_set.channal_cfg.reserved0		= 0;

#if (defined CONFIG_MACH_SUN50IW10) || (defined CONFIG_MACH_SUN55IW3) || (defined CONFIG_MACH_SUN55IW5) | (defined CONFIG_MACH_SUN55IW6)
	dma_set.channal_cfg.dst_drq_type	= 0x07;
#else
	dma_set.channal_cfg.dst_drq_type	= 0x06;
#endif
	dma_set.channal_cfg.dst_addr_mode	= DMAC_CFG_DEST_ADDR_TYPE_IO_MODE;
	dma_set.channal_cfg.dst_burst_length	= DMAC_CFG_DEST_4_BURST;
	dma_set.channal_cfg.dst_data_width	= DMAC_CFG_DEST_DATA_WIDTH_16BIT;
	dma_set.channal_cfg.reserved1		= 0;

	debug("dma config width(%#x) burst(%#x)\n", dma_set.channal_cfg.dst_data_width, dma_set.channal_cfg.dst_burst_length);

	dma_set.wait_cyc		= 8;
	dma_set.loop_mode		= 0;
	dma_set.iospeed			= true;

//	flush_cache((ulong)uc_priv->buf_start, uc_priv->buf_size);

	sunxi_dma_install_int(uc_priv->sound_dma.id, sunxi_sound_dma_cb_tx, uc_priv);

	ret = 	sunxi_dma_enable(&uc_priv->sound_dma);
	if (ret < 0) {
		log_debug("sunxi_dma_enable_int failed, error=%d\n", ret);
		return ret;
	}

	ret = sunxi_dma_setting(uc_priv->sound_dma.id, &dma_set);
	if (ret < 0) {
		log_debug("sunxi_dma_setting failed, error=%d\n", ret);
		return ret;
	}

	return 0;
}

static int sunxi_sound_setup(struct udevice *dev)
{
	struct sunxi_sound_priv *uc_priv = dev_get_priv(dev);
	int ret;

	if (uc_priv->setup_done)
		return -EALREADY;

	ret = sunxi_sound_dma_init(uc_priv);
	if (ret < 0) {
		log_debug("sunxi_sound_dma_init failed, error=%d\n", ret);
		return ret;
	}

	uc_priv->setup_done = true;

	return 0;
}

static int sunxi_sound_stop_play(struct udevice *dev)
{
	struct sunxi_sound_priv *uc_priv = dev_get_priv(dev);
	int ret = 0;

	ret = sunxi_codec_playback_stop(uc_priv->codec, uc_priv->sound_dma.id);
	if (ret < 0)
		log_debug("sunxi_codec_playback_stop failed, error=%d\n", ret);
	return 0;
}

static int sunxi_sound_play(struct udevice *dev, void *data, uint data_size)
{
	struct sunxi_sound_priv *uc_priv = dev_get_priv(dev);
	int ret = 0;

	sunxi_codec_playback_prepare(uc_priv->codec);

	ret = sunxi_codec_playback_start(uc_priv->codec, uc_priv->sound_dma.id, data, ALIGN(data_size, 64));
	if (ret < 0)
		log_debug("sunxi_codec_playback_start failed, error=%d\n", ret);

#ifdef sunxi_sound_debug
	sunxi_codec_playback_debug(uc_priv->codec);
#endif
	return 0;
}

static int sunxi_sound_probe(struct udevice *dev)
{
	struct sunxi_sound_priv *uc_priv = dev_get_priv(dev);
	struct ofnode_phandle_args args;
	ofnode node;
	int ret;
	uint tmp;


	ret = uclass_get_device_by_phandle(UCLASS_DMA, dev,
					   "sunxi-sound,cpu",
					   &uc_priv->plat);
	if (ret) {
		printf("Cannot find plat: %d\n", ret);
		return ret;
	}

	node = ofnode_find_subnode(dev_ofnode(dev), "sunxi-sound,codec");
	if (!ofnode_valid(node)) {
		printf("Failed to find /codec subnode\n");
		return -EINVAL;
	}
	ret = ofnode_parse_phandle_with_args(node, "sound-dai",
					     "#sound-dai-cells", 0, 0, &args);
	if (ret) {
		printf("Cannot find codec phandle: %d\n", ret);
		return ret;
	}
	ret = uclass_get_device_by_ofnode(UCLASS_AUDIO_CODEC, args.node,
					  &uc_priv->codec);
	if (ret) {
		printf("Cannot find audio codec: %d\n", ret);
		return ret;
	}

	ret = sunxi_dma_request(&uc_priv->sound_dma);
	if (ret) {
		printf("failed to request sound dma channel %d\n", ret);
		return ret;
	}

	ret = dev_read_u32u(dev, "boottone", &tmp);
	if (ret) {
		debug("pa_level get failed\n");
		uc_priv->boot_tone = 0;
	} else {
		if (tmp > 0)
			uc_priv->boot_tone = 1;
	}
	debug("boot_tone = %d\n", uc_priv->boot_tone);

	ret = dev_read_u32u(dev, "lenlimit", &tmp);
	if (ret) {
		debug("pa_level get failed\n");
		uc_priv->len_limit = 0;
	} else {
		uc_priv->len_limit = tmp;
	}
	debug("len_limit = %d\n", uc_priv->len_limit);


	printf("Probed sound '%s' with codec '%s' and plat '%s'\n", dev->name,
		  uc_priv->codec->name, uc_priv->plat->name);

	return 0;
}

static const struct sound_ops sunxi_sound_ops = {
	.setup	= sunxi_sound_setup,
	.play	= sunxi_sound_play,
	.stop_play = sunxi_sound_stop_play,
};

static const struct udevice_id sunxi_sound_ids[] = {
	{ .compatible = "allwinner,sunxi-sound" },
	{ }
};

U_BOOT_DRIVER(sunxi_sound) = {
	.name		= "sunxi_sound",
	.id		= UCLASS_SOUND,
	.of_match	= sunxi_sound_ids,
	.probe		= sunxi_sound_probe,
	.ops		= &sunxi_sound_ops,
	.priv_auto	= sizeof(struct sunxi_sound_priv),
};
