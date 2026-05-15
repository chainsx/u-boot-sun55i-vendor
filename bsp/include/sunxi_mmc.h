/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2007-2011
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Aaron <leafy.myeh@allwinnertech.com>
 *
 * MMC register definition for allwinner sunxi platform.
 */

#ifndef _SUNXI_MMC_H
#define _SUNXI_MMC_H

#include <clk.h>
#include <reset.h>
#include "mmc_def.h"
#include <linux/types.h>
#include <linux/delay.h>
#include <asm/gpio.h>
#include <mmc.h>
#include <private_uboot.h>
#include <sunxi_flashmap.h>

/* speed mode */
#define DS26_SDR12            (0)
#define HSSDR52_SDR25         (1)
#define HSDDR52_DDR50         (2)
#define HS200_SDR104          (3)
#define HS400                 (4)
#define MAX_SPD_MD_NUM        (5)

/*the speed mode of tuning*/
#define TUNING_END		     (0)
#define TUNING_DS26_SDR12            (1)
#define TUNING_HSSDR52_SDR25         (2)
#define TUNING_HSDDR52_DDR50         (3)
#define TUNING_HS200_SDR104          (4)
#define TUNING_HS400_CMD             (5)
#define TUNING_HS400		     (6)

/* frequency point */
#define CLK_400K         (0)
#define CLK_25M          (1)
#define CLK_50M          (2)
#define CLK_100M         (3)
#define CLK_150M         (4)
#define CLK_200M         (5)
#define MAX_CLK_FREQ_NUM (8)

/*
timing mode
0: output and input are both based on [0,1,...,7] pll delay.
1: output and input are both based on phase.
2: output is based on phase, input is based on delay chain except hs400.
	input of hs400 is based on delay chain.
3: output is based on phase, input is based on delay chain.
4: output is based on phase, input is based on delay chain.
    it also support to use delay chain on data strobe signal.
*/
#define SUNXI_MMC_TIMING_MODE_0 0U
#define SUNXI_MMC_TIMING_MODE_1 1U
#define SUNXI_MMC_TIMING_MODE_2 2U
#define SUNXI_MMC_TIMING_MODE_3 3U
#define SUNXI_MMC_TIMING_MODE_4 4U
#define SUNXI_MMC_TIMING_MODE_5 5U

#define MMC_CLK_SAMPLE_POINIT_MODE_0 8U
#define MMC_CLK_SAMPLE_POINIT_MODE_1 3U
#define MMC_CLK_SAMPLE_POINIT_MODE_2 2U
#define MMC_CLK_SAMPLE_POINIT_MODE_2_HS400 64U
#define MMC_CLK_SAMPLE_POINIT_MODE_3 64U
#define MMC_CLK_SAMPLE_POINIT_MODE_4 64U
#define MMC_CLK_SAMPLE_POINIT_MODE_5 64U

#define TM1_OUT_PH90   (0)
#define TM1_OUT_PH180  (1)
#define TM1_IN_PH90    (0)
#define TM1_IN_PH180   (1)
#define TM1_IN_PH270   (2)

#define TM3_OUT_PH90   (0)
#define TM3_OUT_PH180  (1)

#define TM4_OUT_PH90   (0)
#define TM4_OUT_PH180  (1)

#define TM5_OUT_PH90   (0)
#define TM5_OUT_PH180  (1)
#define TM5_IN_PH90    (0)
#define TM5_IN_PH180   (1)
#define TM5_IN_PH270   (2)
#define TM5_IN_PH0   (3)

/* error number defination */
#define ERR_NO_BEST_DLY (2)

/* need malloc low len when flush unaligned addr cache */
#if  defined (CONFIG_MACH_SUN8IW18) || defined (CONFIG_MACH_SUN8IW19) || \
	(defined (CONFIG_MACH_SUN8IW20) && (CONFIG_SUNXI_MALLOC_LEN < 0x3700000))
#define SUNXI_MMC_MALLOC_LOW_LEN	(4 << 20)
#else
#define SUNXI_MMC_MALLOC_LOW_LEN        (16 << 20)
#endif

struct sunxi_mmc_timing_mode0 {
	u32 cur_spd_md;
	u32 cur_freq;
	u8 odly[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
	u8 sdly[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
	u8 def_odly[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
	u8 def_sdly[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
	u32 sample_point_cnt;
	u8 cur_odly;
	u8 cur_sdly;
};

/* for smhc v4.1x*/
struct sunxi_mmc_timing_mode1 {
	u32 cur_spd_md;
	u32 cur_freq;
	u8 odly[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
	u8 sdly[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
	u8 def_odly[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
	u8 def_sdly[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
	u32 sample_point_cnt;
	u8 cur_odly;
	u8 cur_sdly;
};

/* for smhc v4.1x*/
struct sunxi_mmc_timing_mode3 {
	u32 cur_spd_md;
	u32 cur_freq;
	u8 odly[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
	u8 sdly[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
	u8 def_odly[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
	u8 def_sdly[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
	u32 sample_point_cnt;
	u32 sdly_unit_ps;
	u8 dly_calibrate_done;
	u8 cur_odly;
	u8 cur_sdly;
};

/* for smhc v4.5x*/
struct sunxi_mmc_timing_mode4 {
	u32 cur_spd_md;
	u32 cur_freq;
	u8 odly[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
	u8 sdly[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
	u8 dsdly[MAX_CLK_FREQ_NUM];
	u8 def_odly[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
	u8 def_sdly[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
	u8 def_dsdly[MAX_CLK_FREQ_NUM];
	u32 sample_point_cnt;
	u32 sdly_unit_ps;
	u32 dsdly_unit_ps;
	u8 dly_calibrate_done;
	u8 cur_odly;
	u8 cur_sdly;
	u8 cur_dsdly;
};

/* for smhc v5.1x*/
struct sunxi_mmc_timing_mode2 {
	u32 cur_spd_md;
	u32 cur_freq;
	u8 odly[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
	u8 sdly[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
	u8 dsdly[MAX_CLK_FREQ_NUM];
	u8 def_odly[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
	u8 def_sdly[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
	u8 def_dsdly[MAX_CLK_FREQ_NUM];
	u32 sample_point_cnt;
	//u32 sdly_unit_ps;
	u32 sample_point_cnt_hs400;
	u32 dsdly_unit_ps;
	u8 dly_calibrate_done;
	u8 cur_odly;
	u8 cur_sdly;
	u8 cur_dsdly;
};

/* for smhc v5.3x*/
struct sunxi_mmc_timing_mode5 {
	u32 cur_spd_md;
	u32 cur_freq;
	u8 odly[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
	u8 sdly[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
	u8 dsdly[MAX_CLK_FREQ_NUM];
	u8 def_odly[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
	u8 def_sdly[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
	u8 def_dsdly[MAX_CLK_FREQ_NUM];
	u32 sample_point_cnt;
	u32 sdly_unit_ps;
	u32 dsdly_unit_ps;
	u8 dly_calibrate_done;
	u8 cur_odly;
	u8 cur_sdly;
	u8 cur_dsdly;
};

struct sunxi_mmc {
	volatile u32 gctrl; 			 /* (0x00) SMC Global Control Register */
	volatile u32 clkcr; 			 /* (0x04) SMC Clock Control Register */
	volatile u32 timeout;		 /* (0x08) SMC Time Out Register */
	volatile u32 width; 		  /* (0x0C) SMC Bus Width Register */
	volatile u32 blksz; 		   /* (0x10) SMC Block Size Register */
	volatile u32 bytecnt;		/* (0x14) SMC Byte Count Register */
	volatile u32 cmd;			  /* (0x18) SMC Command Register */
	volatile u32 arg;			   /* (0x1C) SMC Argument Register */
	volatile u32 resp0; 		 /* (0x20) SMC Response Register 0 */
	volatile u32 resp1; 		 /* (0x24) SMC Response Register 1 */
	volatile u32 resp2; 		 /* (0x28) SMC Response Register 2 */
	volatile u32 resp3; 		 /* (0x2C) SMC Response Register 3 */
	volatile u32 imask; 		 /* (0x30) SMC Interrupt Mask Register */
	volatile u32 mint;			  /* (0x34) SMC Masked Interrupt Status Register */
	volatile u32 rint;				/* (0x38) SMC Raw Interrupt Status Register */
	volatile u32 status;		 /* (0x3C) SMC Status Register */
	volatile u32 ftrglevel; 	/* (0x40) SMC FIFO Threshold Watermark Register */
	volatile u32 funcsel;		/* (0x44) SMC Function Select Register */
	volatile u32 cbcr;			  /* (0x48) SMC CIU Byte Count Register */
	volatile u32 bbcr;			  /* (0x4C) SMC BIU Byte Count Register */
	volatile u32 dbgc;			 /* (0x50) SMC Debug Enable Register */
	volatile u32 csdc;			 /* (0x54) CRC status detect control register*/
	volatile u32 a12a;			/* (0x58)Auto command 12 argument*/
	volatile u32 ntsr;			  /* (0x5c)SMC2 Newtiming Set Register */
	volatile u32 res1[6];	  /* (0x60~0x74) */
	volatile u32 hwrst; 	   /* (0x78) SMC eMMC Hardware Reset Register */
	volatile u32 res2;			/*	(0x7c) */
	volatile u32 dmac;		  /*  (0x80) SMC IDMAC Control Register */
	volatile u32 dlba;			/*	(0x84) SMC IDMAC Descriptor List Base Address Register */
	volatile u32 idst;			 /*  (0x88) SMC IDMAC Status Register */
	volatile u32 idie;			 /*  (0x8C) SMC IDMAC Interrupt Enable Register */
	volatile u32 chda;		   /*  (0x90) */
	volatile u32 cbda;		   /*  (0x94) */
	volatile u32 res3[26];	/*	(0x98~0xff) */
	/* for some very old platform */
	/*volatile u32 fifo;*/			 /* (0x100) SMC FIFO Access Address */
	volatile u32 thldc; 	/*	(0x100) Card Threshold Control Register */
	volatile u32 sfc;		/* (0x104) Sample Fifo Control Register */
	volatile u32 res4[1];	 /*  (0x10b) */
	volatile u32 dsbd;		/* (0x10c) eMMC4.5 DDR Start Bit Detection Control */
	volatile u32 res5[12];	/* (0x110~0x13c) */
//#if (!defined(CONFIG_MACH_SUN8IW7))
	volatile u32 drv_dl;	/* (0x140) Drive Delay Control register*/
	volatile u32 samp_dl;	/* (0x144) Sample Delay Control register*/
	volatile u32 ds_dl; 	/* (0x148) Data Strobe Delay Control Register */
	volatile u32 ntdc;	   /* (0x14C) HS400 New Timing Delay Control Register */
//#else
//	volatile u32 res7[3];
//#endif
	volatile u32 res6[4];  /* (0x150~0x15f) */
	volatile u32 skew_dat0_dl; /*(0x160) deskew data0 delay control register*/
	volatile u32 skew_dat1_dl; /*(0x164) deskew data1 delay control register*/
	volatile u32 skew_dat2_dl; /*(0x168) deskew data2 delay control register*/
	volatile u32 skew_dat3_dl; /*(0x16c) deskew data3 delay control register*/
	volatile u32 skew_dat4_dl; /*(0x170) deskew data4 delay control register*/
	volatile u32 skew_dat5_dl; /*(0x174) deskew data5 delay control register*/
	volatile u32 skew_dat6_dl; /*(0x178) deskew data6 delay control register*/
	volatile u32 skew_dat7_dl; /*(0x17c) deskew data7 delay control register*/
	volatile u32 skew_ds_dl;	  /*(0x180) deskew ds delay control register*/
	volatile u32 skew_ctrl;    /*(0x184) deskew control control register*/
	volatile u32 res8[10];      /* (0x185~0x1af) */
	volatile u32 b2c_ecc_ctrl;       /* (0x1b0) B2C Memory ECC Control Register */
	volatile u32 b2c_ecc_int_clear;       /* (0x1b4) B2C Memory ECC Interrupt Clear Register */
	volatile u32 b2c_ecc_int_status;       /* (0x1b8) B2C Memory ECC Interrupt Status Register */
	volatile u32 b2c_ecc_err_inject;       /* (0x1bc) B2C Memory ECC Error injection data Register */
	volatile u32 b2c_ecc_ori_err_data;       /* (0x1c0) B2c Memory ECC Original Error data Register */
	volatile u32 cqe_ecc_ctrl;       /* (0x1c4) CQE Memory ECC Control Register */
	volatile u32 cqe_ecc_int_clear;       /* (0x1c8) CQE Memory ECC Interrupt Clear Register */
	volatile u32 cqe_ecc_int_status;       /* (0x1cc) CQE Memory ECC Interrupt Status Register */
	volatile u32 cqe_ecc_err_inject_0;       /* (0x1d0) CQE Memory ECC Error injection data Register */
	volatile u32 cqe_ecc_err_inject_1;       /* (0x1d4) CQE Memory ECC Error injection data Register 1*/
	volatile u32 cqe_ecc_ori_err_data_0;       /* (0x1d8) CQE Memory ECC Original Error data Register */
	volatile u32 cqe_ecc_ori_err_data_1;       /* (0x1dc) CQE Memory ECC Original Error data Register 1*/
	volatile u32 res9[8];      /* (0x1e0~0x1ff) */
	volatile u32 fifo;           /* (0x200) SMC FIFO Access Address */
	volatile u32 res10[63];	/* (0x201~0x2FF)*/
	volatile u32 vers;	/* (0x300) SMHC Version Register */
};

struct boot_mmc_cfg {
	u8 boot0_para;
	u8 boot_odly_50M;
	u8 boot_sdly_50M;
	u8 boot_odly_50M_ddr;
	u8 boot_sdly_50M_ddr;
	u8 boot_hs_f_max;
	u8 res[2];
};

#define SDMMC_PRIV_INFO_ADDR_OFFSET (128)
struct boot_sdmmc_private_info_t {
	struct tune_sdly tune_sdly;
	struct boot_mmc_cfg boot_mmc_cfg;

#define CARD_TYPE_SD  0x8000001
#define CARD_TYPE_MMC 0x8000000
#define CARD_TYPE_NULL 0xffffffff
	u32 card_type;  /*0xffffffff: invalid; 0x8000000: mmc card; 0x8000001: sd card*/

#define EXT_PARA0_ID                  (0x55000000)
#define EXT_PARA0_TUNING_SUCCESS_FLAG (1U<<0)
	u32 ext_para0;

/**GPIO 1.8V bias setting***/
#define  EXT_PARA1_1V8_GPIO_BIAS        0x1
#define BOOT0_SUP_HS			0x2
	u32 ext_para1;
	/* ext_para/2/3 reseved for future */
	u32 ext_para2;
	u32 ext_para3;
};

struct mmc_des_v4p1 {
		u32:1,
		dic:1, /* disable interrupt on completion */
		last_des:1, /* 1-this data buffer is the last buffer */
		first_des:1, /* 1-data buffer is the first buffer,
						   0-data buffer contained in the next descriptor is 1st buffer */
		des_chain:1, /* 1-the 2nd address in the descriptor is the next descriptor address */
		end_of_ring:1, /* 1-last descriptor flag when using dual data buffer in descriptor */
					: 24,
		card_err_sum:1, /* transfer error flag */
		own:1; /* des owner:1-idma owns it, 0-host owns it */

#define SDXC_DES_NUM_SHIFT 12  /* smhc2!! */
#define SDXC_DES_BUFFER_MAX_LEN	(1 << SDXC_DES_NUM_SHIFT)
	u32	data_buf1_sz:16,
		data_buf2_sz:16;

	u32	buf_addr_ptr1;
	u32	buf_addr_ptr2;

};

struct sunxi_mmc_priv {
	unsigned mmc_no;
	uint32_t *mclkreg;
	unsigned fatal_err;
	struct gpio_desc cd_gpio;	 /* Change Detect GPIO */
	struct sunxi_mmc *reg;
	struct mmc_config *cfg;
	struct reset_ctl_bulk reset_bulk;
	struct clk gate_clk_ahb;
	struct clk gate_clk_mmc;

	u32 version;
	iom hclkbase;	 /*hclkbase or hclkrst Avoid 64bit being truncated to 32bit*/
	iom hclkrst;
	u32 mclkbase;

	u32 clock; /* @clock, bankup current clock at host,  is updated when configure clock over */
	u32 mod_clk;
	u32 time_pwroff;
	u32 pwr_handler;
	int cd_inverted;		 /* Inverted Card Detect */
	void *reg_bak;//struct sunxi_mmc *reg_bak;
	struct mmc_des_v4p1 *pdes;//struct sunxi_mmc_des* pdes;

	/*sample delay and output deley setting*/
	u32 timing_mode;
	struct sunxi_mmc_timing_mode0 tm0;
	struct sunxi_mmc_timing_mode1 tm1;
	struct sunxi_mmc_timing_mode2 tm2;
	struct sunxi_mmc_timing_mode3 tm3;
	struct sunxi_mmc_timing_mode4 tm4;
	struct sunxi_mmc_timing_mode5 tm5;

	/* @retry_cnt used to count the retry times at a spcific speed mode and frequency during initial process or
	tuning process. it is always equal or less than the number of sample point.
	*/
	u32 retry_cnt;
	/* sample delay retry count*/
	u32 sd_retry_cnt;
	/* ds delay retry count */
	u32 dsd_retry_cnt;

	struct mmc *mmc;

	/*sample delay and output deley setting*/
	u32 raw_int_bak;
	u32 acmd_err_bak;
	u32 sample_mode;
	u32 tuning_smode;

	u32 dma_tl;
	int (*mmc_init_default_timing_para)(struct sunxi_mmc_priv *priv);
	int (*mmc_set_mod_clk)(struct sunxi_mmc_priv *priv, unsigned int hz);
	void (*sunxi_mmc_set_speed_mode)(struct sunxi_mmc_priv *priv,
	 struct mmc *mmc);
	void (*sunxi_mmc_core_init)(struct mmc *mmc);
	void (*sunxi_mmc_clk_io_onoff)(struct sunxi_mmc_priv *priv, int onoff, int reset_clk);
};

struct sunxi_mmc_plat {
	struct mmc_config cfg;
	struct mmc mmc;
};

struct sunxi_sdmmc_parameter_region_header {
	u8 name[16]; //sdmmc_arg
#define REGION_VERSION 0x0001
	u32 version; // describe the region version
#define SDMMC_PARAMETER_MAGIC 0x6D6D6361 // mmca
	u32 magic;
	u32 add_sum;
	u32 length;
	u8 reserved[16];
};

struct sunxi_sdmmc_parameter_region {
	struct sunxi_sdmmc_parameter_region_header header;
	struct boot_sdmmc_private_info_t info;
};

#define SUNXI_MMC_CLK_POWERSAVE		(0x1 << 17)
#define SUNXI_MMC_CLK_ENABLE		(0x1 << 16)
#define SUNXI_MMC_CLK_DIVIDER_MASK	(0xff)

#define SUNXI_MMC_GCTRL_SOFT_RESET	(0x1 << 0)
#define SUNXI_MMC_GCTRL_FIFO_RESET	(0x1 << 1)
#define SUNXI_MMC_GCTRL_DMA_RESET	(0x1 << 2)
#define SUNXI_MMC_GCTRL_RESET		(SUNXI_MMC_GCTRL_SOFT_RESET|\
					 SUNXI_MMC_GCTRL_FIFO_RESET|\
					 SUNXI_MMC_GCTRL_DMA_RESET)
#define SUNXI_MMC_GCTRL_DMA_ENABLE	(0x1 << 5)
#define SUNXI_MMC_GCTRL_ACCESS_BY_AHB   (0x1 << 31)

#define SUNXI_MMC_CMD_RESP_EXPIRE	(0x1 << 6)
#define SUNXI_MMC_CMD_LONG_RESPONSE	(0x1 << 7)
#define SUNXI_MMC_CMD_CHK_RESPONSE_CRC	(0x1 << 8)
#define SUNXI_MMC_CMD_DATA_EXPIRE	(0x1 << 9)
#define SUNXI_MMC_CMD_WRITE		(0x1 << 10)
#define SUNXI_MMC_CMD_AUTO_STOP		(0x1 << 12)
#define SUNXI_MMC_CMD_WAIT_PRE_OVER	(0x1 << 13)
#define SUNXI_MMC_CMD_STOP_ABORT	(0x1 << 14)
#define SUNXI_MMC_CMD_SEND_INIT_SEQ	(0x1 << 15)
#define SUNXI_MMC_CMD_UPCLK_ONLY	(0x1 << 21)
#define SUNXI_MMC_CMD_START		(0x1 << 31)

#define SUNXI_MMC_RINT_RESP_ERROR		(0x1 << 1)
#define SUNXI_MMC_RINT_COMMAND_DONE		(0x1 << 2)
#define SUNXI_MMC_RINT_DATA_OVER		(0x1 << 3)
#define SUNXI_MMC_RINT_TX_DATA_REQUEST		(0x1 << 4)
#define SUNXI_MMC_RINT_RX_DATA_REQUEST		(0x1 << 5)
#define SUNXI_MMC_RINT_RESP_CRC_ERROR		(0x1 << 6)
#define SUNXI_MMC_RINT_DATA_CRC_ERROR		(0x1 << 7)
#define SUNXI_MMC_RINT_RESP_TIMEOUT		(0x1 << 8)
#define SUNXI_MMC_RINT_DATA_TIMEOUT		(0x1 << 9)
#define SUNXI_MMC_RINT_VOLTAGE_CHANGE_DONE	(0x1 << 10)
#define SUNXI_MMC_RINT_FIFO_RUN_ERROR		(0x1 << 11)
#define SUNXI_MMC_RINT_HARD_WARE_LOCKED		(0x1 << 12)
#define SUNXI_MMC_RINT_START_BIT_ERROR		(0x1 << 13)
#define SUNXI_MMC_RINT_AUTO_COMMAND_DONE	(0x1 << 14)
#define SUNXI_MMC_RINT_END_BIT_ERROR		(0x1 << 15)
#define SUNXI_MMC_RINT_SDIO_INTERRUPT		(0x1 << 16)
#define SUNXI_MMC_RINT_CARD_INSERT		(0x1 << 30)
#define SUNXI_MMC_RINT_CARD_REMOVE		(0x1 << 31)
#define SUNXI_MMC_RINT_INTERRUPT_ERROR_BIT      \
	(SUNXI_MMC_RINT_RESP_ERROR |		\
	 SUNXI_MMC_RINT_RESP_CRC_ERROR |	\
	 SUNXI_MMC_RINT_DATA_CRC_ERROR |	\
	 SUNXI_MMC_RINT_RESP_TIMEOUT |		\
	 SUNXI_MMC_RINT_DATA_TIMEOUT |		\
	 SUNXI_MMC_RINT_VOLTAGE_CHANGE_DONE |	\
	 SUNXI_MMC_RINT_FIFO_RUN_ERROR |	\
	 SUNXI_MMC_RINT_HARD_WARE_LOCKED |	\
	 SUNXI_MMC_RINT_START_BIT_ERROR |	\
	 SUNXI_MMC_RINT_END_BIT_ERROR) /* 0xbfc2 */
#define SUNXI_MMC_RINT_INTERRUPT_DONE_BIT	\
	(SUNXI_MMC_RINT_AUTO_COMMAND_DONE |	\
	 SUNXI_MMC_RINT_DATA_OVER |		\
	 SUNXI_MMC_RINT_COMMAND_DONE |		\
	 SUNXI_MMC_RINT_VOLTAGE_CHANGE_DONE)

#define SUNXI_MMC_STATUS_RXWL_FLAG		(0x1 << 0)
#define SUNXI_MMC_STATUS_TXWL_FLAG		(0x1 << 1)
#define SUNXI_MMC_STATUS_FIFO_EMPTY		(0x1 << 2)
#define SUNXI_MMC_STATUS_FIFO_FULL		(0x1 << 3)
#define SUNXI_MMC_STATUS_CARD_PRESENT		(0x1 << 8)
#define SUNXI_MMC_STATUS_CARD_DATA_BUSY		(0x1 << 9)
#define SUNXI_MMC_STATUS_DATA_FSM_BUSY		(0x1 << 10)
#define SUNXI_MMC_STATUS_FIFO_LEVEL(reg)	(((reg) >> 17) & 0x3fff)

#define SUNXI_MMC_NTSR_MODE_SEL_NEW		(0x1 << 31)

#define SUNXI_MMC_IDMAC_RESET		(0x1 << 0)
#define SUNXI_MMC_IDMAC_FIXBURST	(0x1 << 1)
#define SUNXI_MMC_IDMAC_ENABLE		(0x1 << 7)

#define SUNXI_MMC_IDIE_TXIRQ		(0x1 << 0)
#define SUNXI_MMC_IDIE_RXIRQ		(0x1 << 1)

#define SUNXI_MMC_COMMON_CLK_GATE		(1 << 16)
#define SUNXI_MMC_COMMON_RESET			(1 << 18)

#define SUNXI_MMC_CAL_DL_SW_EN		(0x1 << 7)

/* delay control */
#define SDXC_StartCal        		(1<<15)
#define SDXC_CalDone         		(1<<14)
#define SDXC_CalDly          		(0x3F<<8)
#define SDXC_EnableDly       		(1<<7)
#define SDXC_CfgDly          		(0x3F<<0)
#define SDXC_CfgNewDly          		(0xF<<0)

/* host version mask */
#define SMHC_VERSION_MASK		(0xffffff)

struct mmc *sunxi_mmc_init(int sdc_no);
int mmc_speedmode_convert(enum bus_mode speed_mode);
void mmc_update_config_for_sdly(struct mmc *mmc);
void mmc_set_mmcblckx(const char *node_name);

extern void dumphex32(char *name, char *base, int len);
struct mmc *sunxi_mmc_init(int sdc_no);
void sunxi_mmc_pin_release(int sdc_no);

unsigned int sunxi_select_freq(struct mmc *mmc, int speed_md, int freq_index);
int mmc_send_manual_stop(struct mmc *mmc);
int mmc_send_ext_csd(struct mmc *mmc, u8 *ext_csd);
int sunxi_need_rty(struct mmc *mmc);
int sunxi_mmc_tuning_init(void);
int sunxi_write_tuning(struct mmc *mmc);
int sunxi_bus_tuning(struct mmc *mmc);
int sunxi_mmc_tuning_exit(void);


int sunxi_get_detail_errno(struct mmc *mmc);
int sunxi_decide_retry(struct mmc *mmc, int err_no, uint rst_cnt);

#define R_OP_ON

#ifdef R_OP_ON
#define sunxi_r_op(mmchost, op)\
{\
	MMCDBG("%s,%d\n", __FUNCTION__, __LINE__);\
	writel(readl(mmchost->mclkreg)&(~CCM_MMC_CTRL_ENABLE), mmchost->mclkreg);\
	MMCDBG("B mclk %08x\n", readl(mmchost->mclkreg));\
	op;\
	writel(readl(mmchost->mclkreg) | CCM_MMC_CTRL_ENABLE, mmchost->mclkreg);\
	MMCDBG("A mclk %08x\n", readl(mmchost->mclkreg));\
}
#else
#define sunxi_r_op(mmchost, op)\
{\
	op;\
}
#endif


#define CCM_MMC_CTRL_600M		(0x1 << 24)
#define CCM_MMC_CTRL_400M	(0x2 << 24)
#define CCM_MMC_FREQ_OSCM24		(24000000)
#define CCM_MMC_FREQ_600M		(600000000)
#define CCM_MMC_FREQ_400M	(400000000)


#endif /* _SUNXI_MMC_H */
