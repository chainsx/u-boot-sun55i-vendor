// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2007-2011
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Aaron <leafy.myeh@allwinnertech.com>
 *
 * MMC driver for allwinner sunxi platform.
 *
 * This driver is used by the (ARM) SPL with the legacy MMC interface, and
 * by U-Boot proper using the full DM interface. The actual hardware access
 * code is common, and comes first in this file.
 * The legacy MMC interface implementation comes next, followed by the
 * proper DM_MMC implementation at the end.
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <log.h>
#include <malloc.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/arch/cpu.h>
#include <cpu_func.h>
// #include <asm/arch/mmc.h>
#include <linux/delay.h>
//#include <fdt_support.h>
#include <sunxi_mmc.h>

#include "host/sunxi_mmc_host_common.h"

#ifndef CCM_MMC_CTRL_MODE_SEL_NEW
#define CCM_MMC_CTRL_MODE_SEL_NEW	0
#endif

#define CONFIG_MMC_SUNXI_USE_DMA

static u8 ext_odly_spd_freq[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
static u8 ext_sdly_spd_freq[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
static u8 ext_odly_spd_freq_sdc0[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
static u8 ext_sdly_spd_freq_sdc0[MAX_SPD_MD_NUM*MAX_CLK_FREQ_NUM];
static struct sunxi_mmc mmc_reg_bak[3];

/* Struct for Intrrrupt Information */
#define SDXC_RespErr		BIT(1) //0x2
#define SDXC_CmdDone		BIT(2) //0x4
#define SDXC_DataOver		BIT(3) //0x8
#define SDXC_TxDataReq		BIT(4) //0x10
#define SDXC_RxDataReq		BIT(5) //0x20
#define SDXC_RespCRCErr		BIT(6) //0x40
#define SDXC_DataCRCErr		BIT(7) //0x80
#define SDXC_RespTimeout	BIT(8) //0x100
#define SDXC_ACKRcv			BIT(8)  //0x100
#define SDXC_DataTimeout	BIT(9)  //0x200
#define SDXC_BootStart		BIT(9)  //0x200
#define SDXC_DataStarve		BIT(10) //0x400
#define SDXC_VolChgDone		BIT(10) //0x400
#define SDXC_FIFORunErr		BIT(11) //0x800
#define SDXC_HardWLocked	BIT(12) //0x1000
#define SDXC_StartBitErr	BIT(13) //0x2000
#define SDXC_AutoCMDDone	BIT(14) //0x4000
#define SDXC_EndBitErr		BIT(15) //0x8000
#define SDXC_SDIOInt		BIT(16) //0x10000
#define SDXC_CardInsert		BIT(30) //0x40000000
#define SDXC_CardRemove		BIT(31) //0x80000000
#define SDXC_IntErrBit		(SDXC_RespErr | SDXC_RespCRCErr | SDXC_DataCRCErr \
								| SDXC_RespTimeout | SDXC_DataTimeout | SDXC_FIFORunErr \
								| SDXC_HardWLocked | SDXC_StartBitErr | SDXC_EndBitErr)  //0xbfc2

void dumphex32(char *name, char *base, int len)
{
	__u32 i;

	printf("dump %s registers:", name);
	for (i = 0; i < len; i += 4) {
		if (!(i & 0xf))
			printf("\n0x%p : ", base + i);
		printf("0x%08x ", readl((ulong)base + i));
	}
	printf("\n");
}

static void mmc_dump_errinfo(struct sunxi_mmc_priv *smc_priv, struct mmc_cmd *cmd)
{
	MMCMSG(smc_priv->mmc, "smc %d err, cmd %d, %s%s%s%s%s%s%s%s%s%s%s\n",
		smc_priv->mmc_no, cmd ? cmd->cmdidx : -1,
		smc_priv->raw_int_bak & SDXC_RespErr     ? " RE"     : "",
		smc_priv->raw_int_bak & SDXC_RespCRCErr  ? " RCE"    : "",
		smc_priv->raw_int_bak & SDXC_DataCRCErr  ? " DCE"    : "",
		smc_priv->raw_int_bak & SDXC_RespTimeout ? " RTO"    : "",
		smc_priv->raw_int_bak & SDXC_DataTimeout ? " DTO"    : "",
		smc_priv->raw_int_bak & SDXC_DataStarve  ? " DS"     : "",
		smc_priv->raw_int_bak & SDXC_FIFORunErr  ? " FE"     : "",
		smc_priv->raw_int_bak & SDXC_HardWLocked ? " HL"     : "",
		smc_priv->raw_int_bak & SDXC_StartBitErr ? " SBE"    : "",
		smc_priv->raw_int_bak & SDXC_EndBitErr   ? " EBE"    : "",
		smc_priv->raw_int_bak == 0 ? " STO"    : ""
	);
}

#if 0
/*
 * All A64 and later MMC controllers feature auto-calibration. This would
 * normally be detected via the compatible string, but we need something
 * which works in the SPL as well.
 */
static bool sunxi_mmc_can_calibrate(void)
{
	return IS_ENABLED(CONFIG_MACH_SUN50I) ||
	       IS_ENABLED(CONFIG_MACH_SUN50I_H5) ||
	       IS_ENABLED(CONFIG_SUN50I_GEN_H6) ||
	       IS_ENABLED(CONFIG_MACH_SUN8I_R40);
}
#endif
int mmc_speedmode_convert(enum bus_mode speed_mode)
{
	if (speed_mode == MMC_LEGACY)
		return DS26_SDR12;
	else if ((speed_mode == MMC_HS_52) || (speed_mode == SD_HS) || (speed_mode == MMC_HS))
		return HSSDR52_SDR25;
	else if ((speed_mode == MMC_DDR_52) || (speed_mode == UHS_DDR50))
		return HSDDR52_DDR50;
	else if ((speed_mode == MMC_HS_200) || (speed_mode == UHS_SDR104))
		return HS200_SDR104;
	else if ((speed_mode == MMC_HS_400) || (speed_mode == MMC_HS_400_ES))
		return HS400;
	else {
		MMCINFO("error : no such speed! speed_mode = %d\n", speed_mode);
		return -1;
	}
}

static int mmc_set_mod_clk(struct sunxi_mmc_priv *priv, unsigned int hz)
{
	int rval;

	rval = priv->mmc_set_mod_clk(priv, hz);
	return rval;
}

static int mmc_clk_io_onoff(struct sunxi_mmc_priv *priv, int onoff, int reset_clk)
{
	priv->sunxi_mmc_clk_io_onoff(priv, onoff, reset_clk);

	return 0;
}

int mmc_update_clk(struct sunxi_mmc_priv *priv)
{
	unsigned int cmd;
	unsigned timeout_msecs = 2000;
	unsigned long start = get_timer(0);
	writel(readl(&priv->reg->clkcr)|(0x1 << 31), &priv->reg->clkcr);
	cmd = SUNXI_MMC_CMD_START |
	      SUNXI_MMC_CMD_UPCLK_ONLY |
	      SUNXI_MMC_CMD_WAIT_PRE_OVER;

	writel(cmd, &priv->reg->cmd);
	while (readl(&priv->reg->cmd) & SUNXI_MMC_CMD_START) {
		if (get_timer(start) > timeout_msecs) {
			dumphex32("mmc", (char *)priv->reg, 0x200);
			return -1;
		}
	}
	writel(readl(&priv->reg->clkcr) & (~(0x1 << 31)), &priv->reg->clkcr);

	/* clock update sets various irq status bits, clear these */
	writel(readl(&priv->reg->rint), &priv->reg->rint);

	return 0;
}

static int mmc_config_clock(struct sunxi_mmc_priv *priv, struct mmc *mmc)
{
	unsigned rval = readl(&priv->reg->clkcr);

	/* Disable Clock */
	rval &= ~SUNXI_MMC_CLK_ENABLE;
	writel(rval, &priv->reg->clkcr);
	if (mmc_update_clk(priv))
		return -1;

	/* Set mod_clk to new rate */
	if (mmc_set_mod_clk(priv, mmc->clock))
		return -1;

#if defined(CONFIG_SUNXI_GEN_SUN6I) || defined(CONFIG_SUN50I_GEN_H6)
	/* A64 supports calibration of delays on MMC controller and we
	 * have to set delay of zero before starting calibration.
	 * Allwinner BSP driver sets a delay only in the case of
	 * using HS400 which is not supported by mainline U-Boot or
	 * Linux at the moment
	 */
	if (sunxi_mmc_can_calibrate())
		writel(SUNXI_MMC_CAL_DL_SW_EN, &priv->reg->samp_dl);
#endif

	/* Re-enable Clock */
	rval = readl(&priv->reg->clkcr);
	rval |= SUNXI_MMC_CLK_ENABLE;
	writel(rval, &priv->reg->clkcr);
	if (mmc_update_clk(priv))
		return -1;

	return 0;
}

static int sunxi_mmc_set_ios_common(struct sunxi_mmc_priv *priv,
				    struct mmc *mmc)
{
	debug("set ios: bus_width: %x, clock: %d\n",
	      mmc->bus_width, mmc->clock);

	/* Change clock first */
	if (mmc->clock && mmc_config_clock(priv, mmc) != 0) {
		priv->fatal_err = 1;
		return -EINVAL;
	}

	/* Change bus width */
	if (mmc->bus_width == 8)
		writel(0x2, &priv->reg->width);
	else if (mmc->bus_width == 4)
		writel(0x1, &priv->reg->width);
	else
		writel(0x0, &priv->reg->width);

	/* set speed mode */
	priv->sunxi_mmc_set_speed_mode(priv, mmc);

	return 0;
}

static int mmc_save_regs(struct sunxi_mmc_priv *mmchost)
{
	struct sunxi_mmc *reg = (struct sunxi_mmc *)mmchost->reg;
	struct sunxi_mmc *reg_bak = (struct sunxi_mmc *)mmchost->reg_bak;

	reg_bak->gctrl     = readl(&reg->gctrl);
	reg_bak->clkcr     = readl(&reg->clkcr);
	reg_bak->timeout   = readl(&reg->timeout);
	reg_bak->width     = readl(&reg->width);
	reg_bak->imask     = readl(&reg->imask);
	reg_bak->ftrglevel = readl(&reg->ftrglevel);
	reg_bak->dbgc      = readl(&reg->dbgc);
	reg_bak->ntsr      = readl(&reg->ntsr);
	reg_bak->hwrst     = readl(&reg->hwrst);
	reg_bak->dmac      = readl(&reg->dmac);
	reg_bak->idie      = readl(&reg->idie);
	reg_bak->thldc     = readl(&reg->thldc);
	reg_bak->dsbd      = readl(&reg->dsbd);
#if (!defined(CONFIG_MACH_SUN8IW7))
	reg_bak->csdc      = readl(&reg->csdc);
	reg_bak->drv_dl    = readl(&reg->drv_dl);
	reg_bak->samp_dl   = readl(&reg->samp_dl);
	reg_bak->ds_dl     = readl(&reg->ds_dl);
#endif
#if defined(CONFIG_MACH_SUN55IW6)
	if (mmchost->mmc_no == 2) {
		reg_bak->skew_dat0_dl	= readl(&reg->skew_dat0_dl);
		reg_bak->skew_dat1_dl	= readl(&reg->skew_dat1_dl);
		reg_bak->skew_dat2_dl	= readl(&reg->skew_dat2_dl);
		reg_bak->skew_dat3_dl	= readl(&reg->skew_dat3_dl);
		reg_bak->skew_dat4_dl	= readl(&reg->skew_dat4_dl);
		reg_bak->skew_dat5_dl	= readl(&reg->skew_dat5_dl);
		reg_bak->skew_dat6_dl	= readl(&reg->skew_dat6_dl);
		reg_bak->skew_dat7_dl	= readl(&reg->skew_dat7_dl);
		reg_bak->skew_ctrl	= readl(&reg->skew_ctrl);
	}
#endif
	return 0;
}

static int mmc_restore_regs(struct sunxi_mmc_priv *mmchost)
{	struct sunxi_mmc *reg = (struct sunxi_mmc *)mmchost->reg;
	struct sunxi_mmc *reg_bak = (struct sunxi_mmc *)mmchost->reg_bak;

	writel(reg_bak->gctrl, &reg->gctrl);
	writel(reg_bak->clkcr, &reg->clkcr);
	writel(reg_bak->timeout, &reg->timeout);
	writel(reg_bak->width, &reg->width);
	writel(reg_bak->imask, &reg->imask);
	writel(reg_bak->ftrglevel, &reg->ftrglevel);
	if (reg_bak->dbgc)
		writel(0xdeb, &reg->dbgc);
	writel(reg_bak->ntsr, &reg->ntsr);
	writel(reg_bak->hwrst, &reg->hwrst);
	writel(reg_bak->dmac, &reg->dmac);
	writel(reg_bak->idie, &reg->idie);
	writel(reg_bak->thldc, &reg->thldc);
	writel(reg_bak->dsbd, &reg->dsbd);
#if (!defined(CONFIG_MACH_SUN8IW7))
	writel(reg_bak->csdc, &reg->csdc);
	sunxi_r_op(mmchost, writel(reg_bak->drv_dl, &reg->drv_dl));
	writel(reg_bak->samp_dl, &reg->samp_dl);
	writel(reg_bak->ds_dl, &reg->ds_dl);
#endif
#if defined(CONFIG_MACH_SUN55IW6)
	if (mmchost->mmc_no == 2) {
		writel(reg_bak->skew_dat0_dl, &reg->skew_dat0_dl);
		writel(reg_bak->skew_dat1_dl, &reg->skew_dat1_dl);
		writel(reg_bak->skew_dat2_dl, &reg->skew_dat2_dl);
		writel(reg_bak->skew_dat3_dl, &reg->skew_dat3_dl);
		writel(reg_bak->skew_dat4_dl, &reg->skew_dat4_dl);
		writel(reg_bak->skew_dat5_dl, &reg->skew_dat5_dl);
		writel(reg_bak->skew_dat6_dl, &reg->skew_dat6_dl);
		writel(reg_bak->skew_dat7_dl, &reg->skew_dat7_dl);
		writel(reg_bak->skew_ctrl, &reg->skew_ctrl);
	}
#endif
	return 0;
}

#if defined(CONFIG_MACH_SUN55IW6)
static int read_smhc_ecc_status(u32 status_reg)
{
	u32 val = 0;
	int ret = 0;

	val = readl(status_reg);

	if ((val >> 23) & 0x1) {
		//double bit error
		if ((val >> 22) & 0x1) {
			MMCINFO("check ecc error: double bit error\n");
			return -1;
		}

		//sigle bit error or more double two
		if ((val >> 21) & 0x1) {
			MMCINFO("check ecc: sigle bit error or more than double bit\n");
			return 0;
		}

		//check_code err
		MMCINFO("check ecc error: unkown error\n");
		return -1;
	}

	return ret;
}

static int smhc_ecc_check_and_disablbe(struct sunxi_mmc_priv *priv)
{
	int val = 0;
	int err = 0;

	//MMCINFO("%s: smhc_ecc_check\n", __func__);

	//read ecc sta
	err = read_smhc_ecc_status((u32)&(priv->reg->b2c_ecc_int_status));
	if (err < 0) {
		MMCINFO("check smhc ecc error\n");
	}

	//clean irq. set and clear for next transfer (write in spec)
	val = readl(&priv->reg->b2c_ecc_int_clear);
	val |= (1<<0);
	writel(val, &priv->reg->b2c_ecc_int_clear);
	val = readl(&priv->reg->b2c_ecc_int_clear);
	val &= ~(1<<0);
	writel(val, &priv->reg->b2c_ecc_int_clear);

	//disabled irq
	val = readl(&priv->reg->b2c_ecc_ctrl);
	val &= ~(0x1 << 16);
	writel(val, &priv->reg->b2c_ecc_ctrl);

	//disable ecc check
	val = readl(&priv->reg->b2c_ecc_ctrl);
	val &= ~(0x1 << 8);
	writel(val, &priv->reg->b2c_ecc_ctrl);

	return err;
}

void smhc_ecc_enable(struct sunxi_mmc_priv *priv)
{
	u32 val = 0;

	//MMCINFO("%s: smhc_ecc_enable\n", __func__);

	//enabled irq
	val = readl(&priv->reg->b2c_ecc_ctrl);
	val |= (0x1 << 16);
	writel(val, &priv->reg->b2c_ecc_ctrl);

	//enable ecc check
	val = readl(&priv->reg->b2c_ecc_ctrl);
	val |= (0x1 << 8);
	writel(val, &priv->reg->b2c_ecc_ctrl);
}
#endif

static int mmc_trans_data_by_cpu(struct sunxi_mmc_priv *priv, struct mmc *mmc,
				 struct mmc_data *data)
{
	const int reading = !!(data->flags & MMC_DATA_READ);
	const uint32_t status_bit = reading ? SUNXI_MMC_STATUS_FIFO_EMPTY :
					      SUNXI_MMC_STATUS_FIFO_FULL;
	unsigned i;
	unsigned *buff = (unsigned int *)(reading ? data->dest : data->src);
	unsigned byte_cnt = data->blocksize * data->blocks;
	unsigned timeout_msecs = byte_cnt;
	unsigned long  start;

	if (timeout_msecs < 2000)
		timeout_msecs = 2000;

	/* Always read / write data through the CPU */
	setbits_le32(&priv->reg->gctrl, SUNXI_MMC_GCTRL_ACCESS_BY_AHB);

	start = get_timer(0);

	for (i = 0; i < (byte_cnt >> 2); i++) {
		while (readl(&priv->reg->status) & status_bit) {
			if (get_timer(start) > timeout_msecs)
				return -1;
		}

		if (reading)
			buff[i] = readl(&priv->reg->fifo);
		else
			writel(buff[i], &priv->reg->fifo);
	}

	return 0;
}


static int mmc_trans_data_by_dma(struct sunxi_mmc_priv *priv, struct mmc *mmc, struct mmc_data *data)
{
	struct mmc_des_v4p1 *pdes = priv->pdes;
	unsigned byte_cnt = data->blocksize * data->blocks;
	unsigned char *buff;
	unsigned des_idx = 0;
	unsigned buff_frag_num = 0;
	unsigned remain;
	unsigned i, rval;

	buff = data->flags & MMC_DATA_READ ?
			(unsigned char *)data->dest : (unsigned char *)data->src;
	buff_frag_num = byte_cnt >> SDXC_DES_NUM_SHIFT;
	remain = byte_cnt & (SDXC_DES_BUFFER_MAX_LEN - 1);
	if (remain)
		buff_frag_num++;
	else
		remain = SDXC_DES_BUFFER_MAX_LEN;

	flush_cache((unsigned long)buff, ALIGN((unsigned long)byte_cnt, CONFIG_SYS_CACHELINE_SIZE));

	for (i = 0; i < buff_frag_num; i++, des_idx++) {
		memset((void *)&pdes[des_idx], 0, sizeof(struct mmc_des_v4p1));
		pdes[des_idx].des_chain = 1;
		pdes[des_idx].own = 1;
		pdes[des_idx].dic = 1;
		if (buff_frag_num > 1 && i != buff_frag_num - 1)
			pdes[des_idx].data_buf1_sz = SDXC_DES_BUFFER_MAX_LEN;
		else
			pdes[des_idx].data_buf1_sz = remain;
		if (priv->version == 0x40200 || priv->version == 0x40502 || priv->version >= 0x50300 || priv->version == 0x40104)
			pdes[des_idx].buf_addr_ptr1 = ((ulong)buff + i * SDXC_DES_BUFFER_MAX_LEN)
							>> 2;
		else
			pdes[des_idx].buf_addr_ptr1 = ((ulong)buff + i * SDXC_DES_BUFFER_MAX_LEN);
		if (i == 0)
			pdes[des_idx].first_des = 1;

		if (i == buff_frag_num - 1) {
			pdes[des_idx].dic = 0;
			pdes[des_idx].last_des = 1;
			pdes[des_idx].end_of_ring = 1;
			pdes[des_idx].buf_addr_ptr2 = 0;
		} else {
			if (priv->version == 0x40200 || priv->version == 0x40502 || priv->version >= 0x50300 || priv->version == 0x40104)
				pdes[des_idx].buf_addr_ptr2 = ((ulong)&pdes[des_idx + 1]) >> 2;
			else
				pdes[des_idx].buf_addr_ptr2 = ((ulong)&pdes[des_idx + 1]);
		}
		MMCDBG("frag %d, remain %d, des[%d](%08x): "
			"[0] = %08x, [1] = %08x, [2] = %08x, [3] = %08x\n",
			i, remain, des_idx, PT_TO_PHU(&pdes[des_idx]),
			(u32)((u32 *)&pdes[des_idx])[0], (u32)((u32 *)&pdes[des_idx])[1],
			(u32)((u32 *)&pdes[des_idx])[2], (u32)((u32 *)&pdes[des_idx])[3]);
	}
	flush_cache((unsigned long)pdes, ALIGN(sizeof(struct mmc_des_v4p1) * (des_idx + 1), CONFIG_SYS_CACHELINE_SIZE));

	WR_MB();

	/*
	 * GCTRLREG
	 * GCTRL[2]	: DMA reset
	 * GCTRL[5]	: DMA enable
	 *
	 * IDMACREG
	 * IDMAC[0]	: IDMA soft reset
	 * IDMAC[1]	: IDMA fix burst flag
	 * IDMAC[7]	: IDMA on
	 *
	 * IDIECREG
	 * IDIE[0]	: IDMA transmit interrupt flag
	 * IDIE[1]	: IDMA receive interrupt flag
	 */
	rval = readl(&priv->reg->gctrl);
	writel(rval | (1 << 5) | (1 << 2), &priv->reg->gctrl);	/* dma enable */
	writel((1 << 0), &priv->reg->dmac); /* idma reset */
	while (readl(&priv->reg->dmac) & 0x1) {
	} /* wait idma reset done */

	writel((1 << 1) | (1 << 7), &priv->reg->dmac); /* idma on */
	rval = readl(&priv->reg->idie) & (~3);
	if (data->flags & MMC_DATA_WRITE)
		rval |= (1 << 0);
	else
		rval |= (1 << 1);
	writel(rval, &priv->reg->idie);

	if (priv->version == 0x40200 || priv->version == 0x40502 || priv->version >= 0x50300 || priv->version == 0x40104)
		writel(((unsigned long)pdes) >> 2, &priv->reg->dlba);
	else
		writel(((unsigned long)pdes), &priv->reg->dlba);
	writel(priv->dma_tl, &priv->reg->ftrglevel);
	return 0;
}

static int mmc_rint_wait(struct sunxi_mmc_priv *priv, struct mmc *mmc,
			 uint timeout_msecs, uint done_bit, const char *what, uint usedma)
{
	unsigned int status;
	unsigned int done = 0;
	unsigned long start = get_timer(0);
	do {
		status = readl(&priv->reg->rint);
		if ((get_timer(start) > timeout_msecs) || (status & SUNXI_MMC_RINT_INTERRUPT_ERROR_BIT)) {
			MMCMSG(mmc, "mmc %d %s timeout %x status %x\n", priv->mmc_no, what,
					status & SUNXI_MMC_RINT_INTERRUPT_ERROR_BIT, status);
			return -ETIMEDOUT;
		}
		if (usedma && !strncmp(what, "data", sizeof("data")))
			done = ((status & done_bit) && (readl(&priv->reg->idst) & 0x3)) ? 1 : 0;
		else
			done = (status & done_bit);
	} while (!done);

	return 0;
}

#define DTO_MAX 200
static void sunxi_mmc_set_rdtmout_reg(struct sunxi_mmc_priv *priv, struct mmc *mmc,
					unsigned int rdtmout)
{
	unsigned int rval = 0;
	unsigned int rdto_clk = 0;
	unsigned int mode_2x = 0;
	unsigned int hs400_ntm_en = 0;
	int mmc_speed_mode = mmc_speedmode_convert(mmc->selected_mode);

	rdto_clk = mmc->clock / 1000 * rdtmout;
	rval = readl(&priv->reg->ntsr);
	mode_2x = rval & (0x1 << 31);
	hs400_ntm_en = rval & 0x1;

	if ((mmc_speed_mode == HS400 && hs400_ntm_en)
	     || (mmc_speed_mode == HSDDR52_DDR50 && mmc->bus_width == 8)
	     || (mmc_speed_mode == HSDDR52_DDR50 && mmc->bus_width == 4 && mode_2x)) {
		rdto_clk = rdto_clk << 1;
	}

	rval = readl(&priv->reg->gctrl);
	/*ddr50 mode don't use 256x timeout unit*/
	if (rdto_clk > 0xffffff && mmc_speed_mode != HSDDR52_DDR50) {
		rdto_clk = (rdto_clk + 255)/256;
		rval |= (0x1 << 11);
	} else {
		rdto_clk = 0xffffff;
		rval &= ~(0x1 << 11);
	}
	writel(rval, &priv->reg->gctrl);

	rval = readl(&priv->reg->timeout);
	rval &= ~(0xffffff << 8);
	rval |= (rdto_clk << 8);
	writel(rval, &priv->reg->timeout);

	MMCDBG("rdtoclk:%d, reg-tmout:%d, gctl:%x, speed_mode:%d, clock:%d, nstr:%x\n",
		rdto_clk, readl(&priv->reg->timeout), readl(&priv->reg->gctrl),
		mmc_speed_mode, mmc->clock, readl(&priv->reg->ntsr));
}

#define EXT_CSD_SANITIZE_START          165     /* W */
static int sunxi_mmc_send_cmd_common(struct sunxi_mmc_priv *priv,
				     struct mmc *mmc, struct mmc_cmd *cmd,
				     struct mmc_data *data)
{
	unsigned int cmdval = SUNXI_MMC_CMD_START;
	unsigned int timeout_msecs;
	int error = 0;
	unsigned int status = 0;
	unsigned int usedma = 0;
	unsigned int bytecnt = 0;

	if (priv->fatal_err) {
		MMCINFO("mmc %d Found fatal err,so no send cmd\n", priv->mmc_no);
		return -1;
	}
	if (cmd->resp_type & MMC_RSP_BUSY)
		MMCDBG("mmc cmd %d check rsp busy\n", cmd->cmdidx);
	if (cmd->cmdidx == 12 && mmc->manual_stop_flag == 0) {
		MMCDBG("usually, cmd12 is sent after cmd18/cmd25 automantically.\n");
		/* don't wait write busy here, because no cmd12 will be sent for cmd24.
		 * write busy status will be check after sent cmd25. */
		return 0;
	}

	if (!cmd->cmdidx)
		cmdval |= SUNXI_MMC_CMD_SEND_INIT_SEQ;
	if (cmd->resp_type & MMC_RSP_PRESENT)
		cmdval |= SUNXI_MMC_CMD_RESP_EXPIRE;
	if (cmd->resp_type & MMC_RSP_136)
		cmdval |= SUNXI_MMC_CMD_LONG_RESPONSE;
	if (cmd->resp_type & MMC_RSP_CRC)
		cmdval |= SUNXI_MMC_CMD_CHK_RESPONSE_CRC;

	if (data) {
		if ((u32)(long)data->dest & 0x3) {
			error = -1;
			MMCINFO("%s,%d,dest is not 4 aligned\n", __FUNCTION__, __LINE__);
			goto out;
		}

		cmdval |= SUNXI_MMC_CMD_DATA_EXPIRE|SUNXI_MMC_CMD_WAIT_PRE_OVER;
		if (data->flags & MMC_DATA_WRITE)
			cmdval |= SUNXI_MMC_CMD_WRITE;
		if (data->blocks > 1)
			cmdval |= SUNXI_MMC_CMD_AUTO_STOP;
		writel(data->blocksize, &priv->reg->blksz);
		writel(data->blocks * data->blocksize, &priv->reg->bytecnt);
#if defined(CONFIG_MACH_SUN55IW6)
		smhc_ecc_enable(priv);
#endif
	} else {
		if (cmd->cmdidx == 12 && mmc->manual_stop_flag == 1) {
			/*stop current data transferin progress.*/
			cmdval |= SUNXI_MMC_CMD_STOP_ABORT;
			/*Send command at once, even if previous data transfer has not completed*/
			cmdval &= ~SUNXI_MMC_CMD_WAIT_PRE_OVER;
		}
	}

	writel(cmd->cmdarg, &priv->reg->arg);

	if (!data)
		writel(cmdval | cmd->cmdidx, &priv->reg->cmd);
	/*
	 * transfer data and check status
	 * STATREG[2] : FIFO empty
	 * STATREG[3] : FIFO full
	 */
	if (data) {
		int ret = 0;

		/*dto set to 200ms*/
		sunxi_mmc_set_rdtmout_reg(priv, mmc, DTO_MAX);

		bytecnt = data->blocksize * data->blocks;
		MMCDBG("trans data %d bytes\n", bytecnt);
#ifdef CONFIG_MMC_SUNXI_USE_DMA
		if (bytecnt > 64) {
#else
		if (0) {
#endif
			usedma = 1;
			writel(readl(&priv->reg->gctrl) & (~SUNXI_MMC_GCTRL_ACCESS_BY_AHB), &priv->reg->gctrl);
			ret = mmc_trans_data_by_dma(priv, mmc, data);
			writel(cmdval | cmd->cmdidx, &priv->reg->cmd);
		} else {
			writel(readl(&priv->reg->gctrl) | SUNXI_MMC_GCTRL_ACCESS_BY_AHB, &priv->reg->gctrl);
			writel(cmdval | cmd->cmdidx, &priv->reg->cmd);
			ret = mmc_trans_data_by_cpu(priv, mmc, data);
		}
		if (ret) {
			error = readl(&priv->reg->rint) &
				SUNXI_MMC_RINT_INTERRUPT_ERROR_BIT;
			error = -ETIMEDOUT;
			goto out;
		}
	}

	error = mmc_rint_wait(priv, mmc, 1000, SUNXI_MMC_RINT_COMMAND_DONE,
				  "cmd", usedma);
	if (error) {
		goto out;
	}

	if (data) {
		timeout_msecs = 6000;
		MMCDBG("cacl timeout %x msec\n", timeout_msecs);
		error = mmc_rint_wait(priv, mmc, timeout_msecs,
					  data->blocks > 1 ?
					  SUNXI_MMC_RINT_AUTO_COMMAND_DONE :
					  SUNXI_MMC_RINT_DATA_OVER,
					  "data", usedma);
		if (error) {
			goto out;
		}
	}

	if ((cmd->resp_type & MMC_RSP_BUSY) ||
			((data) && (data->flags & MMC_DATA_WRITE))) {
		unsigned long start = get_timer(0);
		if ((cmd->cmdidx == MMC_CMD_ERASE) ||
				((cmd->cmdidx == MMC_CMD_SWITCH) &&
				(((cmd->cmdarg >> 16) & 0xFF) == EXT_CSD_SANITIZE_START)))
			timeout_msecs = 0x1fffffff;
		else
			timeout_msecs = 2000;

		do {
			status = readl(&priv->reg->status);
			if (get_timer(start) > timeout_msecs) {
				MMCDBG("busy timeout\n");
				error = -ETIMEDOUT;
				goto out;
			}
		} while (status & SUNXI_MMC_STATUS_CARD_DATA_BUSY);
		if ((cmd->cmdidx == MMC_CMD_ERASE) ||
				((cmd->cmdidx == MMC_CMD_SWITCH) &&
				 (((cmd->cmdarg >> 16) & 0xFF) == EXT_CSD_SANITIZE_START)))
			MMCINFO("%s: cmd %d wait rsp busy 0x%lx ms \n", __FUNCTION__,
					cmd->cmdidx, get_timer(start));
	}

	if (cmd->resp_type & MMC_RSP_136) {
		cmd->response[0] = readl(&priv->reg->resp3);
		cmd->response[1] = readl(&priv->reg->resp2);
		cmd->response[2] = readl(&priv->reg->resp1);
		cmd->response[3] = readl(&priv->reg->resp0);
		MMCDBG("mmc resp 0x%08x 0x%08x 0x%08x 0x%08x\n",
			  cmd->response[3], cmd->response[2],
			  cmd->response[1], cmd->response[0]);
	} else {
		cmd->response[0] = readl(&priv->reg->resp0);
		MMCDBG("mmc resp 0x%08x\n", cmd->response[0]);
	}
out:
#if defined(CONFIG_MACH_SUN55IW6)
	if (data) {
		if (smhc_ecc_check_and_disablbe(priv))
			error = -ETIMEDOUT;
	}
#endif
	if (error) {
		priv->raw_int_bak = readl(&priv->reg->rint) & SUNXI_MMC_RINT_INTERRUPT_ERROR_BIT;
		mmc_dump_errinfo(priv, cmd);
#if 0
	if (cmd->cmdidx == 8) {
		dumphex32("mmc", (char *)priv->reg, 0x200);
		dumphex32("ccmu_mmc0", (char *)SUNXI_CCM_BASE + 0x830, 0x4);
		dumphex32("ccmu_pll", (char *)SUNXI_PRCM_BASE + 0x1010, 0x10);
		dumphex32("ccmu_bgr", (char *)SUNXI_CCM_BASE + 0x84C, 0x4);
		dumphex32("ccmu_srr", (char *)SUNXI_CCM_BASE + 0x84C, 0x4);
		dumphex32("gpio_config", (char *)SUNXI_PIO_BASE + 0x48, 0x10);
		dumphex32("gpio_pull", (char *)SUNXI_PIO_BASE + 0x64, 0x8);
	}
#endif
	}
	if (data && usedma) {
		status = readl(&priv->reg->idst);
		writel(status, &priv->reg->idst);
		writel(0, &priv->reg->idie);
		writel(0, &priv->reg->dmac);
		writel(readl(&priv->reg->gctrl) & (~(1 << 5)), &priv->reg->gctrl);
	}
	if (error < 0) {
		/* during tuning sample point, some sample point may cause timing problem.
		for example, if a RTO error occurs, host may stop clock and device may still output data.
		we need to read all data(512bytes) from device to avoid to update clock fail.
		*/
		signed int timeout = 0;
		if (mmc->do_tuning && data && (data->flags&MMC_DATA_READ) && (bytecnt == 512)) {
			writel(readl(&priv->reg->gctrl)|0x80000000, &priv->reg->gctrl);
			writel(0xdeb, &priv->reg->dbgc);
			timeout = 1000;
			MMCMSG(mmc, "Read remain data\n");
			while (readl(&priv->reg->bbcr) < 512) {
				unsigned int tmp = readl((ulong)priv->reg->fifo);
				tmp = tmp + 1;
				MMCDBG("Read data 0x%x, bbcr 0x%x\n", tmp, readl(&priv->reg->bbcr));
				udelay(1);
				if (!(timeout--)) {
					MMCMSG(mmc, "Read remain data timeout\n");
					break;
				}
			}
		}


		writel(0x7, &priv->reg->gctrl);
		while (readl(&priv->reg->gctrl)&0x7) {
			MMCDBG("mmc reset dma fifo and fifo\n");
		};

		{
			mmc_save_regs(priv);
			mmc_clk_io_onoff(priv, 0, 0);
			MMCMSG(mmc, "mmc %d close bus gating and reset\n", priv->mmc_no);
			mmc_clk_io_onoff(priv, 1, 0);
			mmc_restore_regs(priv);

			writel(0x7, &priv->reg->gctrl);
			while (readl(&priv->reg->gctrl)&0x7) {
				MMCDBG("mmc reset dma fifo and fifo\n");
			};
		}

		writel(SUNXI_MMC_GCTRL_RESET, &priv->reg->gctrl);
		mmc_update_clk(priv);
	}
	writel(0xffffffff, &priv->reg->rint);
	writel(readl(&priv->reg->gctrl) | SUNXI_MMC_GCTRL_FIFO_RESET,
		   &priv->reg->gctrl);
	if (data && (data->flags&MMC_DATA_READ) && usedma) {
		unsigned char *buff = (unsigned char *)data->dest;
		unsigned byte_cnt = data->blocksize * data->blocks;
		invalidate_dcache_range((unsigned long)buff,
					((unsigned long)buff + ALIGN((unsigned long)byte_cnt,
					CONFIG_SYS_CACHELINE_SIZE)));

		MMCDBG("invald cache after read complete\n");
	}

	return error;

}

static ulong sunxi_flash_fdt_getprop_u32(const void *fdt, int node, const char *prop)
{
	const u32 *cell;
	int len;

	cell = fdt_getprop(fdt, node, prop, &len);
	if (!cell || len != sizeof(*cell))
		return -1;

	return fdt32_to_cpu(*cell);
}

void mmc_update_config_for_sdly(struct mmc *mmc)
{
	int ret = 0;
	int nodeoffset;
	char prop_path[128] = {0};
	u32 f3210, f7654;

	struct sunxi_mmc_priv *priv = (struct sunxi_mmc_priv *)mmc->priv;
	struct tune_sdly *sdly = &priv->cfg->sdly;
	int imd, ifreq;
	int dly, dsdly;
	int null_hs200, null_hs400, null_hsddr;
	int clear_hs200, clear_hs400 = 0, clear_hsddr;
	u32 max_hs200 = 0, max_hs400 = 0, max_hsddr = 0, min_val, defval;
	int tm = priv->timing_mode;
	u8 *sdly_cfg = NULL;
	u8 *dsdly_cfg = NULL;

	if (priv->mmc_no == 2)
		strcpy(prop_path, "sunxi-mmc2");
	else if (priv->mmc_no == 0)
		strcpy(prop_path, "sunxi-mmc0");
	else
		strcpy(prop_path, "sunxi-mmc3");

	nodeoffset = fdt_path_offset(working_fdt, prop_path);
	if (nodeoffset < 0) {
		MMCINFO("can't find node \"%s\" try \"mmc\"\n", prop_path);
		if (priv->mmc_no == 2)
			strcpy(prop_path, "mmc2");
		else if (priv->mmc_no == 0)
			strcpy(prop_path, "mmc0");
		else
			strcpy(prop_path, "mmc3");
		nodeoffset = fdt_path_offset(working_fdt, prop_path);
		if (nodeoffset < 0) {
			MMCINFO("can't find node \"%s\" \n",
					prop_path);
			goto __ERROR_END;
		}
	}

	f3210 = sdly->tm4_smx_fx[0 * 2 + 0]; //sdly->tm4_sm0_f3210;
	ret = fdt_setprop_u32(working_fdt, nodeoffset, "sdc_tm4_sm0_freq0",
				f3210);
	if (ret < 0) {
		MMCINFO("update dtb fail, sdc_tm4_sm0_freq0, %d\n", ret);
		goto __ERROR_END;
	}
	f7654 = sdly->tm4_smx_fx[0 * 2 + 1]; // sdly->tm4_sm0_f7654;
	ret = fdt_setprop_u32(working_fdt, nodeoffset, "sdc_tm4_sm0_freq1",
				f7654);
	if (ret < 0) {
		MMCINFO("update dtb fail, sdc_tm4_sm0_freq1, %d\n", ret);
		goto __ERROR_END;
	}

	f3210 = sdly->tm4_smx_fx[1 * 2 + 0]; //sdly->tm4_sm0_f3210;
	ret = fdt_setprop_u32(working_fdt, nodeoffset, "sdc_tm4_sm1_freq0",
				f3210);
	if (ret < 0) {
		MMCINFO("update dtb fail, sdc_tm4_sm1_freq0, %d\n", ret);
		goto __ERROR_END;
	}
	f7654 = sdly->tm4_smx_fx[1 * 2 + 1]; // sdly->tm4_sm0_f7654;
	ret = fdt_setprop_u32(working_fdt, nodeoffset, "sdc_tm4_sm1_freq1",
				f7654);
	if (ret < 0) {
		MMCINFO("update dtb fail, sdc_tm4_sm1_freq1, %d\n", ret);
		goto __ERROR_END;
	}

	f3210 = sdly->tm4_smx_fx[2 * 2 + 0]; //sdly->tm4_sm0_f3210;
	ret = fdt_setprop_u32(working_fdt, nodeoffset, "sdc_tm4_sm2_freq0",
				f3210);
	if (ret < 0) {
		MMCINFO("update dtb fail, sdc_tm4_sm2_freq0, %d\n", ret);
		goto __ERROR_END;
	}
	f7654 = sdly->tm4_smx_fx[2 * 2 + 1]; // sdly->tm4_sm0_f7654;
	ret = fdt_setprop_u32(working_fdt, nodeoffset, "sdc_tm4_sm2_freq1",
				f7654);
	if (ret < 0) {
		MMCINFO("update dtb fail, sdc_tm4_sm2_freq1, %d\n", ret);
		goto __ERROR_END;
	}

	f3210 = sdly->tm4_smx_fx[3 * 2 + 0]; //sdly->tm4_sm0_f3210;
	ret = fdt_setprop_u32(working_fdt, nodeoffset, "sdc_tm4_sm3_freq0",
				f3210);
	if (ret < 0) {
		MMCINFO("update dtb fail, sdc_tm4_sm3_freq0, %d\n", ret);
		goto __ERROR_END;
	}
	f7654 = sdly->tm4_smx_fx[3 * 2 + 1]; // sdly->tm4_sm0_f7654;
	ret = fdt_setprop_u32(working_fdt, nodeoffset, "sdc_tm4_sm3_freq1",
				f7654);
	if (ret < 0) {
		MMCINFO("update dtb fail, sdc_tm4_sm3_freq1, %d\n", ret);
		goto __ERROR_END;
	}

	f3210 = sdly->tm4_smx_fx[4 * 2 + 0]; //sdly->tm4_sm0_f3210;
	ret = fdt_setprop_u32(working_fdt, nodeoffset, "sdc_tm4_sm4_freq0",
				f3210);
	if (ret < 0) {
		MMCINFO("update dtb fail, sdc_tm4_sm4_freq0, %d\n", ret);
		goto __ERROR_END;
	}
	f7654 = sdly->tm4_smx_fx[4 * 2 + 1]; // sdly->tm4_sm0_f7654;
	ret = fdt_setprop_u32(working_fdt, nodeoffset, "sdc_tm4_sm4_freq1",
			f7654);
	if (ret < 0) {
		MMCINFO("update dtb fail, sdc_tm4_sm4_freq1, %d\n", ret);
		goto __ERROR_END;
	}

	defval = sunxi_flash_fdt_getprop_u32(working_fdt, nodeoffset, "sdc_tm4_sm4_freq0_cmd");
	if (defval  == -1) {
		MMCDBG("get sdc_tm4_sm4_freq0_cmd fail %d\n", ret);
		goto KERNEL_NO_USE_HS400_CMD;
	} else {
		MMCDBG("get sdc_tm4_sm4_freq0_cmd ok\n");
	}

	defval = sunxi_flash_fdt_getprop_u32(working_fdt, nodeoffset, "sdc_tm4_sm4_freq1_cmd");
	if (defval  == -1) {
		MMCDBG("get sdc_tm4_sm4_freq1_cmd fail %d\n", ret);
		goto KERNEL_NO_USE_HS400_CMD;
	} else {
		MMCDBG("get sdc_tm4_sm4_freq1_cmd ok\n");
	}

	f3210 = sdly->tm4_smx_fx[5*2 + 0];
	ret = fdt_setprop_u32(working_fdt, nodeoffset, "sdc_tm4_sm4_freq0_cmd", f3210);
	if (ret < 0) {
		MMCINFO("update dtb fail, sdc_tm4_sm4_freq0_cmd, %d\n", ret);
	}

	f7654 = sdly->tm4_smx_fx[5*2 + 1];
	ret = fdt_setprop_u32(working_fdt, nodeoffset, "sdc_tm4_sm4_freq1_cmd", f7654);
	if (ret < 0) {
		MMCINFO("update dtb fail, sdc_tm4_sm4_freq1i_cmd, %d\n", ret);
	}

KERNEL_NO_USE_HS400_CMD:
	if (priv->cfg->tune_limit_kernel_timing == 0) {
		goto __NORMAL_RET;
	}

	if (tm == SUNXI_MMC_TIMING_MODE_4) {
		sdly_cfg = priv->tm4.sdly;
		dsdly_cfg = priv->tm4.dsdly;
	} else if (tm == SUNXI_MMC_TIMING_MODE_5) {
		sdly_cfg = priv->tm5.sdly;
		dsdly_cfg = priv->tm5.dsdly;
	}
	/*
	* 1. check sample point cfg for each hsddr/hs200/hs400.
	* 2. don't support speed mode which has no valid sample point cfg.
	* 3. decrease max frequency accroding sample point cfg.
	*/
	null_hsddr = 1;
	if (mmc->card_caps & MMC_MODE_DDR_52MHz) {
		imd = HSDDR52_DDR50;
		/*1-25MHz; 2-50MHz; 3-100MHz;4-150MHz; 5-200MHz*/
		for (ifreq = 2; ifreq >= 2; ifreq--) {
			dly = sdly_cfg[imd*MAX_CLK_FREQ_NUM + ifreq];
			if (dly != 0xFF) {
				max_hsddr = sunxi_select_freq(mmc, imd, ifreq);
				MMCDBG("hsddr %d-%d\n", ifreq, max_hsddr);
				null_hsddr = 0;
				break;
			}
		}
	}
	null_hs200 = 1;
	if (mmc->card_caps & MMC_MODE_HS200) {
		imd = HS200_SDR104;
		/*1-25MHz; 2-50MHz; 3-100MHz;4-150MHz; 5-200MHz*/
		for (ifreq = 5; ifreq >= 2; ifreq--) {
			dly = sdly_cfg[imd*MAX_CLK_FREQ_NUM + ifreq];
			if (dly != 0xFF) {
				max_hs200 = sunxi_select_freq(mmc, imd, ifreq);
				MMCDBG("hs200 %d-%d\n", ifreq, max_hs200);
				null_hs200 = 0;
				break;
			}
		}
	}
	null_hs400 = 1;
	if ((mmc->card_caps & (MMC_MODE_HS400|MMC_MODE_8BIT))
		== (MMC_MODE_HS400|MMC_MODE_8BIT)) {
		imd = HS400;
		/*1-25MHz; 2-50MHz; 3-100MHz;4-150MHz; 5-200MHz*/
		for (ifreq = 5; ifreq >= 2; ifreq--) {
			imd = HS200_SDR104;
			dly = sdly_cfg[imd*MAX_CLK_FREQ_NUM + ifreq];
			imd = HS400;
			dsdly = dsdly_cfg[ifreq];
			if ((dly != 0xff) && (dsdly != 0xff)) {
				max_hs400 = sunxi_select_freq(mmc, imd, ifreq);
				MMCDBG("hs400 %d-%d\n", ifreq, max_hs400);
				null_hs400 = 0;
				break;
			}
		}
	}

	defval = sunxi_flash_fdt_getprop_u32(working_fdt, nodeoffset, "max-frequency");
	if (defval == -1) {
		MMCINFO("get max-frequency fail %d\n", ret);
		goto __ERROR_END;
	} else {
		MMCDBG("get max-frequency ok %d Hz\n", defval);
	}

	if (null_hsddr || null_hs200 || null_hs400)
		clear_hs400 = 1;
	else if (!null_hs400)
		clear_hs400 = 0;

	if (null_hs200)
		clear_hs200 = 1;
	else
		clear_hs200 = 0;

	if (null_hsddr)
		clear_hsddr = 1;
	else
		clear_hsddr = 0;

	MMCDBG("%d %d %d: %d %d %d\n", null_hs200, null_hs400, null_hsddr,
			clear_hs200, clear_hs400, clear_hsddr);

	if (clear_hs400) {
		ret = fdt_delprop(working_fdt, nodeoffset, "mmc-hs400-1_8v");
		if (ret == 0)
			MMCINFO("delete mmc-hs400-1_8v from dtb\n");
		else if (ret == -FDT_ERR_NOTFOUND)
			MMCINFO("no mmc-hs400-1_8v!\n");
		else
			MMCINFO("update dtb fail, delete mmc-hs400-1_8v fail\n");
	}

	if (clear_hs200) {
		ret = fdt_delprop(working_fdt, nodeoffset, "mmc-hs200-1_8v");
		if (ret == 0)
			MMCINFO("delete mmc-hs200-1_8v from dtb\n");
		else if (ret == -FDT_ERR_NOTFOUND)
			MMCINFO("no mmc-hs200-1_8v!\n");
		else
			MMCINFO("update dtb fail, delete mmc-hs200-1_8v fail\n");
	}

	if (clear_hsddr) {
		ret = fdt_delprop(working_fdt, nodeoffset, "mmc-ddr-1_8v");
		if (ret == 0)
			MMCINFO("delete mmc-ddr-1_8v from dtb\n");
		else if (ret == -FDT_ERR_NOTFOUND)
			MMCINFO("no mmc-ddr-1_8v!\n");
		else
			MMCINFO("update dtb fail, delete mmc-ddr-1_8v fail\n");
	}

	if (!clear_hs400) {
		if (max_hs200 > max_hs400)
			min_val = max_hs400;
		else
			min_val = max_hs200;
	} else if (!clear_hs200)
		min_val = max_hs200;
	else if (!clear_hsddr)
		min_val = max_hsddr;
	else
		min_val = 50000000; //25MHz

	if (min_val < defval) {
		ret = fdt_setprop_u32(working_fdt, nodeoffset,
				"max-frequency", min_val);
		if (ret < 0) {
			MMCINFO("update dtb fail, max-frequency, %d\n", ret);
			goto __ERROR_END;
		} else {
			defval = sunxi_flash_fdt_getprop_u32(working_fdt, nodeoffset, "max-frequency");
			if (defval == -1) {
				MMCINFO("get max-frequency fail %d\n", ret);
				goto __ERROR_END;
			} else {
				MMCINFO("get max-frequency ok %d Hz\n", defval);
			}
			if (defval != min_val)
				MMCINFO("update max-frequency compare err!\n");
		}
	}

	if ((mmc->cid[0] >> 24) == 0xec) {
			defval = sunxi_flash_fdt_getprop_u32(working_fdt, nodeoffset, "ctl-cmdq-md");
			if (defval == -1) {
				MMCINFO("get ctl-cmdq-md fail %d\n", ret);
				goto __ERROR_END;
			} else {
				MMCINFO("get ctl-cmdq-md ok %d\n", defval);
			}

			if (defval == 2) {
				ret = fdt_setprop_u32(working_fdt, nodeoffset,
						"ctl-cmdq-md", 1);
				if (ret < 0) {
					MMCINFO("update dtb fail, ctl-cmdq-md\n");
					goto __ERROR_END;
				} else {
					MMCINFO("update dtb ok, ctl-cmdq-md\n");
				}
			}

	}

__NORMAL_RET:
	return;

__ERROR_END:
	MMCINFO("fdt err returned %s\n", fdt_strerror(ret));
	return;
}

/* Maximum dts string for MMC */
#define MMC_MAX_DTS_STRING_LEN	50
/*
*if SD card boot,change sdc0 to mmc0; if emmc boot,change sdc2 to mmc0;
*add mmc0 to aliases(in dtsi), kernel(after linux 5.10 ver) will detect it and change the corresponding sdc to mmcblck0
*/
void mmc_set_mmcblckx(const char *node_name)
{
	int aliases_nodeoffset, len, err;
	char *fdt_get_str = NULL;
	char fdt_set_str[MMC_MAX_DTS_STRING_LEN];
	aliases_nodeoffset = fdt_path_offset(working_fdt, "/aliases");
	fdt_get_str = (void *)fdt_getprop(working_fdt, aliases_nodeoffset,
					node_name, &len);

	if (fdt_get_str == NULL || strlen(fdt_get_str)+1 > MMC_MAX_DTS_STRING_LEN) {
		MMCINFO("get %s string failed\n", node_name);
		return;
	}

	memcpy(fdt_set_str, fdt_get_str, strlen(fdt_get_str)+1);
	MMCDBG("fdt_str_p len = %d | fdt_str len --%d | fdt_get_str = %s | fdt_set_str = %s \n",
			strlen(fdt_get_str), strlen(fdt_set_str), fdt_get_str, fdt_set_str);

	err = fdt_setprop_string(working_fdt, aliases_nodeoffset, "mmc0", fdt_set_str);
	if (err < 0) {
		MMCINFO("error, fdt_setprop_string(): %s\n", fdt_strerror(err));
	}
}

/* non-DM code here is used by the (ARM) SPL only */

#if !CONFIG_IS_ENABLED(DM_MMC)
/* support 4 mmc hosts */
struct sunxi_mmc_priv mmc_host[4];

static int mmc_resource_init(int sdc_no)
{
	struct sunxi_mmc_priv *priv = &mmc_host[sdc_no];
	struct sunxi_ccm_reg *ccm = (struct sunxi_ccm_reg *)SUNXI_CCM_BASE;

	debug("init mmc %d resource\n", sdc_no);

	switch (sdc_no) {
	case 0:
		priv->reg = (struct sunxi_mmc *)SUNXI_MMC0_BASE;
		priv->mclkreg = &ccm->sd0_clk_cfg;
		break;
	case 1:
		priv->reg = (struct sunxi_mmc *)SUNXI_MMC1_BASE;
		priv->mclkreg = &ccm->sd1_clk_cfg;
		break;
#ifdef SUNXI_MMC2_BASE
	case 2:
		priv->reg = (struct sunxi_mmc *)SUNXI_MMC2_BASE;
		priv->mclkreg = &ccm->sd2_clk_cfg;
		break;
#endif
#ifdef SUNXI_MMC3_BASE
	case 3:
		priv->reg = (struct sunxi_mmc *)SUNXI_MMC3_BASE;
		priv->mclkreg = &ccm->sd3_clk_cfg;
		break;
#endif
	default:
		printf("Wrong mmc number %d\n", sdc_no);
		return -1;
	}
	priv->mmc_no = sdc_no;

	return 0;
}

static int sunxi_mmc_core_init(struct mmc *mmc)
{
	struct sunxi_mmc_priv *priv = mmc->priv;

	/* Reset controller */
	writel(SUNXI_MMC_GCTRL_RESET, &priv->reg->gctrl);
	udelay(1000);

	return 0;
}

static int sunxi_mmc_set_ios_legacy(struct mmc *mmc)
{
	struct sunxi_mmc_priv *priv = mmc->priv;

	return sunxi_mmc_set_ios_common(priv, mmc);
}

static int sunxi_mmc_send_cmd_legacy(struct mmc *mmc, struct mmc_cmd *cmd,
				     struct mmc_data *data)
{
	struct sunxi_mmc_priv *priv = mmc->priv;
	int ret = 0;
	void *buff_align = NULL;
	void *read_buff_old = NULL;
	if (data) {
		if ((data->flags & MMC_DATA_READ) && (PT_TO_PHU(data->dest) % CONFIG_SYS_CACHELINE_SIZE)) {
			buff_align = memalign(CONFIG_SYS_CACHELINE_SIZE, data->blocks * data->blocksize);
			if (buff_align == NULL) {
				MMCINFO("memalign buff_align is NULL!\n");
				return -1;
			}
			/* memset(buff_align, 0, data->blocks * data->blocksize); */
			read_buff_old = data->dest;
			data->dest = buff_align;
		}

		if ((data->flags & MMC_DATA_WRITE) && (PT_TO_PHU(data->src) % CONFIG_SYS_CACHELINE_SIZE)) {
			buff_align = memalign(CONFIG_SYS_CACHELINE_SIZE, data->blocks * data->blocksize);
			if (buff_align == NULL) {
				MMCINFO("memalign buff_align is NULL!\n");
				return -1;
			}
			/* memset(buff_align, 0, data->blocks * data->blocksize); */
			memcpy(buff_align, data->src, data->blocks * data->blocksize);
			data->src = buff_align;
		}
	}

	ret = sunxi_mmc_send_cmd_common(priv, mmc, cmd, data);
	if (buff_align) {
		if (data->flags & MMC_DATA_READ)
			memcpy(read_buff_old, buff_align, data->blocks * data->blocksize);
		free(buff_align);
		buff_align = NULL;
	}

	return ret;
}

/* .getcd is not needed by the SPL */
static const struct mmc_ops sunxi_mmc_ops = {
	.send_cmd	= sunxi_mmc_send_cmd_legacy,
	.set_ios	= sunxi_mmc_set_ios_legacy,
	.init		= sunxi_mmc_core_init,
};

struct mmc *sunxi_mmc_init(int sdc_no)
{
	struct sunxi_ccm_reg *ccm = (struct sunxi_ccm_reg *)SUNXI_CCM_BASE;
	struct sunxi_mmc_priv *priv = &mmc_host[sdc_no];
	struct mmc_config *cfg = &priv->cfg;
	int ret;

	memset(priv, '\0', sizeof(struct sunxi_mmc_priv));

	cfg->name = "SUNXI SD/MMC";
	cfg->ops  = &sunxi_mmc_ops;

	cfg->voltages = MMC_VDD_32_33 | MMC_VDD_33_34;
	cfg->host_caps = MMC_MODE_4BIT;

	if ((IS_ENABLED(CONFIG_MACH_SUN50I) || IS_ENABLED(CONFIG_MACH_SUN8I) ||
	    IS_ENABLED(CONFIG_SUN50I_GEN_H6)) && (sdc_no == 2))
		cfg->host_caps = MMC_MODE_8BIT;

	cfg->host_caps |= MMC_MODE_HS_52MHz | MMC_MODE_HS;
	cfg->b_max = CONFIG_SYS_MMC_MAX_BLK_COUNT;

	cfg->f_min = 400000;
	cfg->f_max = 52000000;

	if (mmc_resource_init(sdc_no) != 0)
		return NULL;

	/* config ahb clock */
	debug("init mmc %d clock and io\n", sdc_no);
#if !defined(CONFIG_SUN50I_GEN_H6)
	setbits_le32(&ccm->ahb_gate0, 1 << AHB_GATE_OFFSET_MMC(sdc_no));

#ifdef CONFIG_SUNXI_GEN_SUN6I
	/* unassert reset */
	setbits_le32(&ccm->ahb_reset0_cfg, 1 << AHB_RESET_OFFSET_MMC(sdc_no));
#endif
#if defined(CONFIG_MACH_SUN9I)
	/* sun9i has a mmc-common module, also set the gate and reset there */
	writel(SUNXI_MMC_COMMON_CLK_GATE | SUNXI_MMC_COMMON_RESET,
	       SUNXI_MMC_COMMON_BASE + 4 * sdc_no);
#endif
#else /* CONFIG_SUN50I_GEN_H6 */
	setbits_le32(&ccm->sd_gate_reset, 1 << sdc_no);
	/* unassert reset */
	setbits_le32(&ccm->sd_gate_reset, 1 << (RESET_SHIFT + sdc_no));
#endif
	ret = mmc_set_mod_clk(priv, 24000000);
	if (ret)
		return NULL;

	return mmc_create(cfg, priv);
}

#else /* CONFIG_DM_MMC code below, as used by U-Boot proper */

#if 0
static int sunxi_mmc_execute_tuning(struct udevice *dev, uint opcode)
{
	printf("********Function: %s, Line: %d - \n", __func__, __LINE__);
#if 0
	int err;
	struct sunxi_mmc_priv *priv = dev_get_priv(dev);
	struct mmc *mmc = mmc_get_mmc_dev(dev);


	priv->msglevel = 0x0;
	priv->do_tuning = 0x1;
	priv->tuning_end = 0x0;

	err = sunxi_mmc_tuning_init();
	if (err) {
		MMCINFO("init tuning failed\n");
		goto ERR_RET;
	}

	err = sunxi_write_tuning(mmc);
	if (err) {
		MMCINFO("Write pattern failed\n");
		goto ERR_RET;
	}

	err = sunxi_bus_tuning(mmc);
	if (err) {
		MMCINFO("bus tuning fail, err %d\n", err);
		goto ERR_RET;
	}

	priv->msglevel = 0x1;
	priv->do_tuning = 0x0;
	priv->tuning_end = 0x1; //comment this line for debug, test tuning during boot.

	err = sunxi_mmc_tuning_exit();
	if (err) {
		MMCINFO("exit tuning failed\n");
		goto ERR_RET;
	}

	err = sunxi_switch_to_best_bus(mmc);
	if (err) {
		MMCINFO("switch to best speed mode fail\n");
		goto ERR_RET;
	}

	if (need_tuning) {
		err = mmc_write_info(priv->mmc_no, NULL,
				SUNXI_SDMMC_PARAMETER_REGION_SIZE_BYTE - sizeof(struct sunxi_sdmmc_parameter_region_header));
		if (err) {
			MMCINFO("%s:Write timing info fail, err %d\n", __func__, err);
			goto ERR_RET;
		}
	}
#endif
	return 0;
}
#endif

static int sunxi_mmc_set_ios(struct udevice *dev)
{
	struct sunxi_mmc_plat *plat = dev_get_plat(dev);
	struct sunxi_mmc_priv *priv = dev_get_priv(dev);

	return sunxi_mmc_set_ios_common(priv, &plat->mmc);
}

static int sunxi_mmc_send_cmd(struct udevice *dev, struct mmc_cmd *cmd,
			      struct mmc_data *data)
{
	struct sunxi_mmc_plat *plat = dev_get_plat(dev);
	struct sunxi_mmc_priv *priv = dev_get_priv(dev);

	return sunxi_mmc_send_cmd_common(priv, &plat->mmc, cmd, data);
}

static int sunxi_mmc_getcd(struct udevice *dev)
{
	struct mmc *mmc = mmc_get_mmc_dev(dev);
	struct sunxi_mmc_priv *priv = dev_get_priv(dev);

	/* If polling, assume that the card is always present. */
	if ((mmc->cfg->host_caps & MMC_CAP_NONREMOVABLE) ||
	    (mmc->cfg->host_caps & MMC_CAP_NEEDS_POLL))
		return 1;

	if (dm_gpio_is_valid(&priv->cd_gpio)) {
		int cd_state = dm_gpio_get_value(&priv->cd_gpio);

		if (mmc->cfg->host_caps & MMC_CAP_CD_ACTIVE_HIGH)
			return !cd_state;
		else
			return cd_state;
	}
	return 1;
}

#ifdef MMC_SUPPORTS_TUNING
int sunxi_mmc_execute_tuning(struct udevice *dev, uint opcode)
{
	/* use sunxi tuing from uboot2018 and don't tuning here */
	return 0;
}
#endif

static const struct dm_mmc_ops sunxi_mmc_ops = {
	.send_cmd	= sunxi_mmc_send_cmd,
	.set_ios	= sunxi_mmc_set_ios,
	.get_cd		= sunxi_mmc_getcd,
#ifdef MMC_SUPPORTS_TUNING
	.execute_tuning = sunxi_mmc_execute_tuning,
#endif
};

static int sunxi_tm4_retry(struct mmc *mmc, u8 tm4_retry_gap, u8 type)
{
	struct sunxi_mmc_priv *priv = (struct sunxi_mmc_priv *)mmc->priv;
	u32 spd_md, freq;
	u8 *sdly;

	spd_md = priv->tm4.cur_spd_md;
	freq = priv->tm4.cur_freq;

	if (type == 1) {
		sdly = &priv->tm4.sdly[spd_md*MAX_CLK_FREQ_NUM+freq];
		MMCINFO("Current spd_md %d freq_id %d sdly %d\n", spd_md, freq, *sdly);
		priv->sd_retry_cnt++;
		if (priv->sd_retry_cnt * tm4_retry_gap <  MMC_CLK_SAMPLE_POINIT_MODE_4) {
			if ((*sdly + tm4_retry_gap) < MMC_CLK_SAMPLE_POINIT_MODE_4) {
				*sdly = *sdly + tm4_retry_gap;
			} else {
				*sdly = *sdly + tm4_retry_gap - MMC_CLK_SAMPLE_POINIT_MODE_4;
			}
			MMCINFO("Get next samply point %d at spd_md %d freq_id %d\n", *sdly, spd_md, freq);
		} else {
			MMCINFO("Beyond the sdly retry times\n");
			return -1;
		}
	} else {
		sdly = &priv->tm4.dsdly[freq];
		MMCINFO("Current spd_md %d freq_id %d dsdly %d\n", spd_md, freq, *sdly);
		priv->dsd_retry_cnt++;
		if (priv->dsd_retry_cnt * tm4_retry_gap <  MMC_CLK_SAMPLE_POINIT_MODE_4) {
			if ((*sdly + tm4_retry_gap) < MMC_CLK_SAMPLE_POINIT_MODE_4) {
				*sdly = *sdly + tm4_retry_gap;
			} else {
				*sdly = *sdly + tm4_retry_gap - MMC_CLK_SAMPLE_POINIT_MODE_4;
			}
			MMCINFO("Get next ds point %d at spd_md %d freq_id %d\n", *sdly, spd_md, freq);
		} else {
			MMCINFO("Beyond the dsdly retry times\n");
			return -1;
		}
	}
	return 0;
}

int sunxi_decide_retry(struct mmc *mmc, int err_no, uint rst_cnt)
{
	struct sunxi_mmc_priv *priv = (struct sunxi_mmc_priv *)mmc->priv;
	unsigned tmode = priv->timing_mode;
	u32 spd_md, freq;
	u8 *sdly;
	u8 tm1_retry_gap = 1;
	u8 tm3_retry_gap = 8;
#ifdef SUNXI_MMC_RETRY_TEST
	u8 tm4_retry_gap = 32;
#else
	u8 tm4_retry_gap = 2;
#endif

	if (rst_cnt) {
		priv->sd_retry_cnt = 0;
		priv->dsd_retry_cnt = 0;
	}

	if (err_no && (!(err_no & SDXC_RespTimeout) || (err_no == 0xffffffff))) {
		if (tmode == SUNXI_MMC_TIMING_MODE_1) {
			spd_md = priv->tm1.cur_spd_md;
			freq = priv->tm1.cur_freq;
			sdly = &priv->tm1.sdly[spd_md*MAX_CLK_FREQ_NUM+freq];

			priv->sd_retry_cnt++;
			if (priv->sd_retry_cnt * tm1_retry_gap <  MMC_CLK_SAMPLE_POINIT_MODE_1) {
				if ((*sdly + tm1_retry_gap) < MMC_CLK_SAMPLE_POINIT_MODE_1) {
					*sdly = *sdly + tm1_retry_gap;
				} else {
					*sdly = *sdly + tm1_retry_gap - MMC_CLK_SAMPLE_POINIT_MODE_1;
				}
				MMCINFO("Get next samply point %d at spd_md %d freq_id %d\n", *sdly, spd_md, freq);
			} else {
				MMCINFO("Beyond the retry times\n");
				return -1;
			}
		} else if (tmode == SUNXI_MMC_TIMING_MODE_3) {
			spd_md = priv->tm3.cur_spd_md;
			freq = priv->tm3.cur_freq;
			sdly = &priv->tm3.sdly[spd_md*MAX_CLK_FREQ_NUM+freq];

			priv->sd_retry_cnt++;
			if (priv->sd_retry_cnt * tm3_retry_gap <  MMC_CLK_SAMPLE_POINIT_MODE_3) {
				if ((*sdly + tm3_retry_gap) < MMC_CLK_SAMPLE_POINIT_MODE_3) {
					*sdly = *sdly + tm3_retry_gap;
				} else {
					*sdly = *sdly + tm3_retry_gap - MMC_CLK_SAMPLE_POINIT_MODE_3;
				}
				MMCINFO("Get next samply point %d at spd_md %d freq_id %d\n", *sdly, spd_md, freq);
			} else {
				MMCINFO("Beyond the retry times\n");
				return -1;
			}
		} else if (tmode == SUNXI_MMC_TIMING_MODE_4) {
			spd_md = priv->tm4.cur_spd_md;
			freq = priv->tm4.cur_freq;
			if (spd_md == HS400) {
				if ((err_no == 0xffffffff)) {
					if (sunxi_tm4_retry(mmc, tm4_retry_gap, 1)) {
						if (sunxi_tm4_retry(mmc, tm4_retry_gap, 0))
							return -1;
					}
				} else if ((err_no & (SDXC_RespErr | SDXC_RespCRCErr))) {
					if (sunxi_tm4_retry(mmc, tm4_retry_gap, 1))
						return -1;
				} else {
					if (sunxi_tm4_retry(mmc, tm4_retry_gap, 0))
						return -1;
				}
			} else {
				if (sunxi_tm4_retry(mmc, tm4_retry_gap, 1))
					return -1;
			}
		}
		priv->raw_int_bak = 0;
		return 0;
	}
	MMCDBG("rto or no error or software timeout,no need retry\n");

	return -1;
}

int sunxi_get_detail_errno(struct mmc *mmc)
{
	struct sunxi_mmc_priv *priv = (struct sunxi_mmc_priv *)mmc->priv;
	u32 err_no = priv->raw_int_bak;

	priv->raw_int_bak = 0;
	return err_no;
}

static unsigned get_mclk_offset(void)
{
	if (IS_ENABLED(CONFIG_MACH_SUN9I_A80))
		return 0x410;

	if (IS_ENABLED(CONFIG_MACH_SUN55IW6))
		return 0xd00;

	return 0x830;
};

static int sunxi_mmc_probe(struct udevice *dev)
{
	struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
	struct sunxi_mmc_plat *plat = dev_get_plat(dev);
	struct sunxi_mmc_priv *priv = dev_get_priv(dev);
	struct mmc_config *cfg = &plat->cfg;
	struct ofnode_phandle_args args;
	u32 *ccu_reg;
	int ret;
	int version;

	cfg->name = dev->name;
	priv->cfg = cfg;

	MMCINFO("mmc driver ver %s\n", DRIVER_VER);

	if ((priv->mmc_no == 2)) {
		priv->cfg->odly_spd_freq = &ext_odly_spd_freq[0];
		priv->cfg->sdly_spd_freq = &ext_sdly_spd_freq[0];
	} else if (priv->mmc_no == 0) {
		priv->cfg->odly_spd_freq = &ext_odly_spd_freq_sdc0[0];
		priv->cfg->sdly_spd_freq = &ext_sdly_spd_freq_sdc0[0];
	}

	cfg->name = "SUNXI SD/MMC";

	cfg->voltages = MMC_VDD_32_33 | MMC_VDD_33_34;
	cfg->host_caps = MMC_MODE_HS_52MHz | MMC_MODE_HS;
	cfg->b_max = CONFIG_SYS_MMC_MAX_BLK_COUNT;

	cfg->f_min = 400000;
	cfg->f_max = 52000000;

	/* default timing mode */
	priv->timing_mode = SUNXI_MMC_TIMING_MODE_1;

	priv->pdes = memalign(CONFIG_SYS_CACHELINE_SIZE, 256 * 1024);

	if (priv->pdes == NULL) {
		MMCINFO("get mem for descripter failed !\n");
		return -1;
	} else {
		MMCDBG("get mem for descripter OK !\n");
	}

	ret = mmc_of_parse(dev, cfg);
	if (ret)
		return ret;

	priv->reg = dev_read_addr_ptr(dev);

	/* We don't have a sunxi clock driver so find the clock address here */
	ret = dev_read_phandle_with_args(dev, "clocks", "#clock-cells", 0,
					  1, &args);
	if (ret)
		return ret;
	ccu_reg = (u32 *)(uintptr_t)ofnode_get_addr(args.node);

	priv->mmc_no = ((uintptr_t)priv->reg - SUNXI_MMC0_BASE) / 0x1000;

#if defined(CONFIG_MACH_SUN55IW6)
	priv->mclkreg = (void *)ccu_reg + get_mclk_offset() + priv->mmc_no * 0x10;
#else
	priv->mclkreg = (void *)ccu_reg + get_mclk_offset() + priv->mmc_no * 4;
#endif

	priv->reg_bak =  &mmc_reg_bak[priv->mmc_no];
	priv->cfg->host_no = dev_seq(dev);

	ret = reset_get_bulk(dev, &(priv->reset_bulk));
	if (ret) {
		MMCINFO("clk get ahb error!\n");
		return -1;
	}

	ret = clk_get_by_name(dev, "ahb", &(priv->gate_clk_ahb));
	if (ret) {
		MMCINFO("clk get ahb error!\n");
		return -1;
	}

	ret = clk_get_by_name(dev, "mmc", &(priv->gate_clk_mmc));
	if (ret) {
		MMCINFO("clk get mmc error!\n");
		return -1;
	}

	if (priv->cfg->io_is_1v8) {
		MMCDBG("io is 1.8V\n");
		cfg->host_caps |= MMC_MODE_HS200;
		if (cfg->host_caps & MMC_MODE_8BIT)
			cfg->host_caps |= MMC_MODE_HS400;
	}

	if (priv->cfg->host_caps_mask) {
		u32 mask = priv->cfg->host_caps_mask;
		if (mask & DRV_PARA_DISABLE_MMC_MODE_HS400)
			cfg->host_caps &= (~MMC_MODE_HS400);
		if (mask & DRV_PARA_DISABLE_MMC_MODE_HS200)
			cfg->host_caps &= (~(MMC_MODE_HS200
						| MMC_MODE_HS400));
		if (mask & DRV_PARA_DISABLE_MMC_MODE_DDR_52MHz)
			cfg->host_caps &= (~(MMC_MODE_DDR_52MHz
						| MMC_MODE_HS400
						| MMC_MODE_HS200));
		if (mask & DRV_PARA_DISABLE_MMC_MODE_HS_52MHz)
			cfg->host_caps &= (~(MMC_MODE_HS_52MHz
						| MMC_MODE_DDR_52MHz
						| MMC_MODE_HS400
						| MMC_MODE_HS200));
		if (mask & DRV_PARA_DISABLE_MMC_MODE_8BIT)
			cfg->host_caps &= (~(MMC_MODE_8BIT
						| MMC_MODE_HS400));
		if (mask & DRV_PARA_ENABLE_EMMC_HW_RST)
			cfg->host_caps |= DRV_PARA_ENABLE_EMMC_HW_RST;
	}
	MMCDBG("host_caps:0x%x\n", cfg->host_caps);

	if (sunxi_host_mmc_config(dev, priv) != 0) {
		MMCINFO("sunxi host mmc config failed!\n");
		return -1;
	}

	mmc_clk_io_onoff(priv, 1, 1);
	version = readl(&priv->reg->vers) & SMHC_VERSION_MASK;
	MMCINFO("SUNXI SDMMC Controller Version:0x%x\n", version);
	priv->version = version;

	upriv->mmc = &plat->mmc;
	priv->mmc = &plat->mmc;
	priv->mmc->priv = priv;

	priv->mmc->dev = dev;

	/* Reset controller */
	writel(0x7, &priv->reg->gctrl);
	while (readl(&priv->reg->gctrl)&0x7) {
		MMCDBG("mmc reset dma fifo and fifo\n");
	};
	mmc_clk_io_onoff(priv, 0, 0);
	mmc_clk_io_onoff(priv, 1, 1);

	ret = mmc_set_mod_clk(priv, 24000000);
	if (ret)
		return ret;

	/* This GPIO is optional */
	gpio_request_by_name(dev, "cd-gpios", 0, &priv->cd_gpio,
			     GPIOD_IS_IN | GPIOD_PULL_UP);

	/* need hwrst and  threshold init in probe */
	priv->sunxi_mmc_core_init(priv->mmc);

	return 0;
}

extern int mmc_init_sunxi_flash_ops(struct udevice *dev);
static int sunxi_mmc_bind(struct udevice *dev)
{
	int ret = 0;
	struct sunxi_mmc_plat *plat = dev_get_plat(dev);
	ret = mmc_bind(dev, &plat->mmc, &plat->cfg);
	if (ret != 0)
		return ret;
#if CONFIG_IS_ENABLED(AW_FLASH)
	ret = mmc_init_sunxi_flash_ops(dev);
#endif
	return ret;
}

static const struct udevice_id sunxi_mmc_ids[] = {
	{ .compatible = "allwinner,sun4i-a10-mmc" },
	{ .compatible = "allwinner,sun5i-a13-mmc" },
	{ .compatible = "allwinner,sun7i-a20-mmc" },
	{ .compatible = "allwinner,sun8i-a83t-emmc" },
	{ .compatible = "allwinner,sun9i-a80-mmc" },
	{ .compatible = "allwinner,sun50i-a64-mmc" },
	{ .compatible = "allwinner,sun50i-a64-emmc" },
	{ .compatible = "allwinner,sun50i-h6-mmc" },
	{ .compatible = "allwinner,sun50i-h6-emmc" },
	{ .compatible = "allwinner,sun50i-a100-mmc" },
	{ .compatible = "allwinner,sun50i-a100-emmc" },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(sunxi_mmc_drv) = {
	.name		= "sunxi_mmc",
	.id		= UCLASS_MMC,
	.of_match	= sunxi_mmc_ids,
	.bind		= sunxi_mmc_bind,
	.probe		= sunxi_mmc_probe,
	.ops		= &sunxi_mmc_ops,
	.plat_auto	= sizeof(struct sunxi_mmc_plat),
	.priv_auto	= sizeof(struct sunxi_mmc_priv),
};
#endif
