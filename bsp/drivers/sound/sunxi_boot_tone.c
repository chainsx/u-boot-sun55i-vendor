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

#include <common.h>
#include <command.h>
#include <audio_codec.h>
#include <asm/global_data.h>
#include <fdt_support.h>
#include <fdtdec.h>
#include <console.h>
#include <malloc.h>
#include <dm.h>
#include <sound.h>
#include <log.h>

#include <sunxi_board.h>
#include <sunxi_codec.h>
#include <sunxi_sound.h>


DECLARE_GLOBAL_DATA_PTR;

static void dump_wavheader(wav_header_t *wav)
{
	log_debug("wav header:\n");
	log_debug("channel: %u\n", wav->numChannels);
	log_debug("sample rate: %u\n", wav->sampleRate);
	log_debug("bytes per sec: %u\n", wav->bytesPerSecond);
	log_debug("sample resolution: %u\n", wav->bitsPerSample);
	log_debug("data size: %u\n", wav->dataSize);
}

static wav_header_t *wav_file_parser(wav_header_t *wav)
{
	if (strncmp("WAVE", wav->waveType, 4) == 0) {
		dump_wavheader(wav);
		return wav;
	}
	return NULL;
}

static int sunxi_sound_playback(struct udevice *dev, void *data, uint data_size)
{
	struct sound_ops *ops = sound_get_ops(dev);

	if (!ops->play)
		return -ENOSYS;

	return ops->play(dev, data, data_size);
}

int sunxi_boot_tone_play(void)
{
	struct udevice *dev;
	struct sunxi_sound_priv *uc_priv;
	int ret;
	int workmode = 0;
	void *tone_buffer;
	u32 buf_size;
	u32 *buf_start;
	wav_header_t *wav_header;
	char read[64];
	char *boottone = NULL;

	ret = uclass_first_device_err(UCLASS_SOUND, &dev);
	if (ret)
		goto err;

	ret = sound_setup(dev);
	if (ret && ret != -EALREADY) {
		printf("Initialise Audio driver failed (ret=%d)\n", ret);
		return CMD_RET_FAILURE;
	}

	uc_priv = dev_get_priv(dev);
	if (uc_priv == NULL) {
		printf("uc_priv is NULL\n");
		return -EINVAL;
	}

	workmode = get_boot_work_mode();
	if (workmode != WORK_MODE_BOOT) {
		printf("boot tone is %d\n", uc_priv->boot_tone);
		return 0;
	}

	if (!uc_priv->boot_tone) {
		printf("boot tone is %d\n", uc_priv->boot_tone);
		return 0;
	}

	tone_buffer = (void *)simple_strtoul(env_get("uboot_tone_addr"), NULL, 16);
	if (!tone_buffer)
		return 0;

	boottone = env_get("boottone_partition");
	if (boottone == NULL)
		boottone = "boottone";

	snprintf(read, sizeof(read), "sunxi_flash read 0x%p %s", tone_buffer, boottone);
	printf("run command:%s\n", read);
	ret = run_command(read, 0);
	if (ret == 0)
		printf("load boottone into %p success.\n", tone_buffer);
	else
		return 0;

	/* parser wav file */
	wav_header = wav_file_parser((wav_header_t *)tone_buffer);
	if (!wav_header) {
		printf("unknown wav header.\n");
		return 0;
	}

	ret = audio_codec_set_params(uc_priv->codec, -1,
				     wav_header->sampleRate, -1,
				     wav_header->bitsPerSample,
				     wav_header->numChannels);
	if (ret)
		return ret;

	buf_size = wav_header->dataSize;

	buf_start = (u32 *)(tone_buffer + sizeof(wav_header_t));

	if (uc_priv->len_limit > 0 && uc_priv->len_limit < buf_size)
		buf_size = uc_priv->len_limit;

	buf_size = ALIGN(buf_size, 64);
	printf("buffer start:%p, buffer size:%u\n", buf_start, buf_size);

	ret = sunxi_sound_playback(dev, buf_start, buf_size);
	if (ret)
		goto err;

	/*printf("[%s] line:%d end\n", __func__, __LINE__);*/
	return 0;

err:
	printf("Sunxi Sound device failed to play (err=%d)\n", ret);
	return ret;

}
