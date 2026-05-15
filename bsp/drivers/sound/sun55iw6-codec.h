/* u-boot-bsp/drivers/sound/sun55iw6-codec.h
 * (C) Copyright 2021-2025
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * xudongpdc <xudongpdc@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef _SUN55IW6_CODEC_H
#define _SUN55IW6_CODEC_H

/* REG-Digital */
#define SUNXI_DAC_DPC		0x00
#define SUNXI_DAC_VOL_CTL	0x04
#define SUNXI_DAC_FIFO_CTL	0x10
#define SUNXI_DAC_FIFO_STA	0x14
#define SUNXI_DAC_TXDATA	0x20
#define SUNXI_DAC_CNT		0x24
#define SUNXI_DAC_DEBUG		0x28

#define SUNXI_VAR1SPEEDUP_DOWN_CTL	0x54

#define SUNXI_DAC_DAP_CTL	0xF0

#define SUNXI_DAC_DRC_CTL	0x108

#define SUNXI_VERSION		0x2C0

/* REG-Analog */
#define SUNXI_DAC_AN_REG	0x310
#define SUNXI_RAMP			0x31c
#define SUNXI_BIAS_AN_CTL	0x320
#define SUNXI_AUDIO_MAX_REG	SUNXI_BIAS_AN_CTL

/* BITS */
/* SUNXI_DAC_DPC:0x00 */
#define DAC_DIG_EN		31
#define MODQU			25
#define DWA_EN			24
#define HPF_EN			18
#define DVOL			12
#define DITHER_SGM		8
#define DITHER_SFT		4
#define DITHER_EN		1
#define HUB_EN			0
/* SUNXI_DAC_VOL_CTL:0x04 */
#define DAC_VOL_SEL		16
#define DAC_VOL_L		8
#define DAC_VOL_R		0
/* SUNXI_DAC_FIFO_CTL:0x10 */
#define DAC_FS			29
#define FIR_VER			28
#define SEND_LASAT		26
#define DAC_FIFO_MODE		24
#define DAC_DRQ_CLR_CNT		21
#define TX_TRIG_LEVEL		8
#define DAC_MONO_EN		6
#define TX_SAMPLE_BITS		5
#define DAC_DRQ_EN		4
#define DAC_IRQ_EN		3
#define DAC_FIFO_UNDERRUN_IRQ_EN	2
#define DAC_FIFO_OVERRUN_IRQ_EN		1
#define DAC_FIFO_FLUSH		0
/* SUNXI_DAC_FIFO_STA:0x14 */
#define	DAC_TX_EMPTY		23
#define	DAC_TXE_CNT		8
#define	DAC_TXE_INT		3
#define	DAC_TXU_INT		2
#define	DAC_TXO_INT		1
/* SUNXI_DAC_DEBUG:0x28 */
#define	DAC_MODU_SEL		11
#define	DAC_PATTERN_SEL		9
#define	CODEC_CLK_SEL		8
/* SUNXI_VAR1SPEEDUP_DOWN_CTL:0x54 */
#define VRA1SPEEDUP_DOWN_STATE		4
#define VRA1SPEEDUP_DOWN_CTL		1
#define VRA1SPEEDUP_DOWN_RST_CTL	0
/* SUNXI_DAC_DAP_CTL:0xF0 */
#define DDAP_EN			31
#define DDAP_DRC_EN		29
#define DDAP_HPF_EN		28
/* SUNXI_DAC_AN_REG:0x310 */
#define CURRENT_TEST_SEL	31
#define OPDRV_CUR			22
#define IOPVRS				20
#define ILINEOUTAMPS		18
#define IOPDACS				16
#define DACL_EN				15
#define LINEOUTL_EN			13
#define LMUTE				12
#define OPVR_OI_CTRL		7
#define LINEOUT_DIFFEN		6
#define LINEOUT_VOL_CTRL	0
/* SUNXI_RAMP:0x31C */
#define RAMP_RISE_INT_EN	31
#define RAMP_RISE_INT		30
#define RAMP_FALL_INT_EN	29
#define RAMP_FALL_INT		28
#define RAMP_SOFT_RESET		24
#define RAMP_CLK_DIV_M		16
#define HP_PULL_OUT_EN		15
#define RAMP_HOLD_STEP		12
#define GAP_STEP			8
#define RAMP_STEP			4
#define RMD_EN				3
#define RMU_EN				2
#define RMC_EN				1
#define RD_EN				0
/* SUNXI_BIAS_AN_CTL:0x320 */
#define BIASDATA		0

#define DACDRC_SHIFT		1
#define DACHPF_SHIFT		2


#endif /* _SUN55IW6_CODEC_H */
