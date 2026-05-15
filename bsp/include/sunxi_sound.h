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

#ifndef _SUNXI_SOUND_H
#define _SUNXI_SOUND_H

#include <dma.h>

struct udevice;

typedef struct wav_header {
	char            riffType[4];            //4byte,资源交换文件标志:RIFF
	unsigned int    riffSize;               //4byte,从下个地址到文件结尾的总字节数
	char            waveType[4];            //4byte,wav文件标志:WAVE
	char            formatType[4];          //4byte,波形文件标志:FMT(最后一位空格符)最后一位空格符
	unsigned int    formatSize;             //4byte,音频属性(compressionCode,numChannels,sampleRate,bytesPerSecond,blockAlign,bitsPerSample)所占字节数
	unsigned short  compressionCode;        //2byte,格式种类(1-线性pcm-WAVE_FORMAT_PCM,WAVEFORMAT_ADPCM)
	unsigned short  numChannels;            //2byte,通道数
	unsigned int    sampleRate;             //4byte,采样率
	unsigned int    bytesPerSecond;         //4byte,传输速率
	unsigned short  blockAlign;             //2byte,数据块的对齐，即DATA数据块长度
	unsigned short  bitsPerSample;          //2byte,采样精度-PCM位宽
	char            dataType[4];            //4byte,数据标志:data
	unsigned int    dataSize;               //4byte,从下个地址到文件结尾的总字节数，即除了wav header以外的pcm data length
} wav_header_t;

/* sound private data */
struct sunxi_sound_priv {
	struct dma sound_dma;
	u32 len_limit;
	int setup_done;
	bool boot_tone;
	struct udevice *codec;
	struct udevice *plat;
};


#endif /* _SUNXI_SOUND_H */
