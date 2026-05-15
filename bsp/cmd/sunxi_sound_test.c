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
#include <linux/delay.h>

#include <fdtdec.h>
#include <console.h>
#include <malloc.h>
#include <dm.h>
#include <sound.h>
#include <log.h>
#include <dma.h>
#include <asm/arch/dma.h>

#include <sunxi_board.h>
#include <sunxi_codec.h>
#include <sunxi_sound.h>

DECLARE_GLOBAL_DATA_PTR;

//#define CPU_MODE

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

#ifndef CPU_MODE

static int sunxi_sound_playback(struct udevice *dev, void *data, uint data_size)
{
	struct sound_ops *ops = sound_get_ops(dev);

	if (!ops->play)
		return -ENOSYS;

	return ops->play(dev, data, data_size);
}


static int sound_stop_playback(struct udevice *dev)
{
	struct sound_ops *ops = sound_get_ops(dev);

	if (!ops->play)
		return -ENOSYS;

	return ops->stop_play(dev);
}
#endif

/* Initilaise sound subsystem */
static int do_sunxi_sound_init(struct cmd_tbl *cmdtp, int flag, int argc,
		   char *const argv[])
{
	struct udevice *dev;
	int ret;

	ret = uclass_first_device_err(UCLASS_SOUND, &dev);
	if (!ret)
		ret = sound_setup(dev);

	if (ret && ret != -EALREADY) {
		printf("Initialise Audio driver failed (ret=%d)\n", ret);
		return CMD_RET_FAILURE;
	}

	return 0;
}

/*#define CPU_MODE 1*/
static int sunxi_boot_tone_test(void)
{
	struct udevice *dev;
	struct sunxi_sound_priv *uc_priv;
	int ret;
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

#ifdef CPU_MODE
	/* Only for test */
	u16 *ptr = tone_buffer + sizeof(wav_header_t);
	u32 len = 0;

	sunxi_codec_playback_prepare(uc_priv->codec);
	while (len < buf_size) {
		u32 data = (u32)(*ptr);
		sunxi_codec_fill_txfifo(uc_priv->codec, &data);
		len += 2;
		ptr++;
		udelay(62);
	}

#else

	ret = sunxi_sound_playback(dev, buf_start, buf_size);
	if (ret)
		goto err;

#if 1
	u32 st;
	ulong timeout;

	timeout = get_timer(0);
	st = sunxi_dma_querystatus(uc_priv->sound_dma.id);
	printf("dma st=0x%x\n", st);
#define BOOTTONE_TIMEOUT_LIMIT (5000)
	/* limit 5000ms, if music larger than 5s, will stop it */
	while ((get_timer(timeout) < BOOTTONE_TIMEOUT_LIMIT) && st)
		st = sunxi_dma_querystatus(uc_priv->sound_dma.id);
	if (st)
		printf("wait dma timeout(%dms)!, status:0x%x\n", BOOTTONE_TIMEOUT_LIMIT, st);

	printf("dma st=0x%x\n", st);

	sound_stop_playback(dev);

#endif

#endif //def CPU_MODE
	return 0;
err:
	log_err("Sunxi Sound device failed test play (err=%d)\n", ret);
	return CMD_RET_FAILURE;

}

static int do_sunxi_play_boot_tone(struct cmd_tbl *cmdtp, int flag, int argc,
		   char *const argv[])
{
	return  sunxi_boot_tone_test();
}

static struct cmd_tbl cmd_sunxi_sound_sub[] = {
	U_BOOT_CMD_MKENT(init, 1, 1, do_sunxi_sound_init, "", ""),
	U_BOOT_CMD_MKENT(playtone, 1, 1, do_sunxi_play_boot_tone, "", ""),
};

/* process sound command */
static int do_sunxi_sound(struct cmd_tbl *cmdtp, int flag, int argc,
		    char *const argv[])
{
	struct cmd_tbl *c;

	if (argc < 1)
		return CMD_RET_USAGE;

	/* Strip off leading 'sound' command argument */
	argc--;
	argv++;

	c = find_cmd_tbl(argv[0], &cmd_sunxi_sound_sub[0], ARRAY_SIZE(cmd_sunxi_sound_sub));

	if (c)
		return c->cmd(cmdtp, flag, argc, argv);
	else
		return CMD_RET_USAGE;
}

U_BOOT_CMD(
	boottone, 2, 1, do_sunxi_sound,
	"sunxi sound sub-system",
	"init - initialise the sound driver\n"
	"playtone - play boot tone\n"
);
