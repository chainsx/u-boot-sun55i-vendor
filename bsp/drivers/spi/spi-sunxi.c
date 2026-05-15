/*
* SUNXI SPI NG driver for uboot.
 *
 * Copyright (C) 2023
 * 2023.10.24  JingyanLiang <jingyanliang@allwinnertech.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <errno.h>
#include <hexdump.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <dm/device-internal.h>
#include <cpu_func.h>
#include <linux/iopoll.h>
#include <clk.h>
#include <reset.h>
#include <linux/mtd/spi-nor.h>

#include "spi-sunxi.h"

#define SUNXI_SPI_DEV_NAME "sunxi-spi"
#define SUNXI_SPI_MODULE_VERSION "1.0.3"

static const u32 sunxi_spi_sample_mode[] = {
	0x100, /* SUNXI_SPI_SAMP_DELAY_CYCLE_0_0 */
	0x000, /* SUNXI_SPI_SAMP_DELAY_CYCLE_0_5 */
	0x010, /* SUNXI_SPI_SAMP_DELAY_CYCLE_1_0 */
	0x110, /* SUNXI_SPI_SAMP_DELAY_CYCLE_1_5 */
	0x101, /* SUNXI_SPI_SAMP_DELAY_CYCLE_2_0 */
	0x001, /* SUNXI_SPI_SAMP_DELAY_CYCLE_2_5 */
	0x011  /* SUNXI_SPI_SAMP_DELAY_CYCLE_3_0 */
};

static void sunxi_spi_dump_reg(struct sunxi_spi *sspi, int len)
{
	fdt_addr_t paddr = dev_read_addr(sspi->dev);
	char str[32] = { 0 };
	int i;

	for (i = 0; i < len; i += 0x10) {
		scnprintf(str, sizeof(str), "0x%08lx: ", (ulong)(paddr + i));
		print_hex_dump(str, DUMP_PREFIX_NONE, 0x10, 4, sspi->base_addr + i, 0x10, false);
	}
}

/* SPI Controller Hardware Register Operation Start */

static void sunxi_spi_bus_sample_mode(struct sunxi_spi *sspi, u32 mode)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_GC_REG);

	switch (mode) {
	case SUNXI_SPI_SAMP_MODE_OLD:
		reg_val &= ~SUNXI_SPI_GC_MODE_SEL;
		break;
	case SUNXI_SPI_SAMP_MODE_NEW:
		reg_val |= SUNXI_SPI_GC_MODE_SEL;
		break;
	}

	writel(reg_val, sspi->base_addr + SUNXI_SPI_GC_REG);
}

static void sunxi_spi_set_master(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_GC_REG);
	reg_val |= SUNXI_SPI_GC_MODE;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_GC_REG);
}

static void sunxi_spi_enable_bus(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_GC_REG);
	reg_val |= SUNXI_SPI_GC_EN;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_GC_REG);
}

static void sunxi_spi_disable_bus(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_GC_REG);
	reg_val &= ~SUNXI_SPI_GC_EN;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_GC_REG);
}

static void sunxi_spi_start_xfer(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_TC_REG);
	reg_val |= SUNXI_SPI_TC_XCH;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_TC_REG);
}

static void sunxi_spi_set_sample_mode(struct sunxi_spi *sspi, u32 mode)
{
	u32 reg_old, reg_new;
	u32 sdm, sdc, sdc1;

	reg_new = reg_old = readl(sspi->base_addr + SUNXI_SPI_TC_REG);

	sdm = (sunxi_spi_sample_mode[mode] >> 8) & 0xf;
	sdc = (sunxi_spi_sample_mode[mode] >> 4) & 0xf;
	sdc1 = (sunxi_spi_sample_mode[mode] >> 0) & 0xf;

	if (sdm)
		reg_new |= SUNXI_SPI_TC_SDM;
	else
		reg_new &= ~SUNXI_SPI_TC_SDM;

	if (sdc)
		reg_new |= SUNXI_SPI_TC_SDC;
	else
		reg_new &= ~SUNXI_SPI_TC_SDC;

	if (sdc1)
		reg_new |= SUNXI_SPI_TC_SDC1;
	else
		reg_new &= ~SUNXI_SPI_TC_SDC1;

	if (reg_new != reg_old)
		writel(reg_new, sspi->base_addr + SUNXI_SPI_TC_REG);
}

static void sunxi_spi_set_discard_burst(struct sunxi_spi *sspi, bool dhb)
{
	u32 reg_old, reg_new;

	reg_new = reg_old = readl(sspi->base_addr + SUNXI_SPI_TC_REG);

	if (dhb)
		reg_new |= SUNXI_SPI_TC_DHB;
	else
		reg_new &= ~SUNXI_SPI_TC_DHB;

	if (reg_new != reg_old)
		writel(reg_new, sspi->base_addr + SUNXI_SPI_TC_REG);
}

static void sunxi_spi_ss_level(struct sunxi_spi *sspi, bool status)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_TC_REG);

	if (status)
		reg_val |= SUNXI_SPI_TC_SS_LEVEL;
	else
		reg_val &= ~SUNXI_SPI_TC_SS_LEVEL;

	writel(reg_val, sspi->base_addr + SUNXI_SPI_TC_REG);
}

static void sunxi_spi_ss_owner(struct sunxi_spi *sspi, u32 owner)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_TC_REG);

	switch (owner) {
	case SUNXI_SPI_CS_AUTO:
		reg_val &= ~SUNXI_SPI_TC_SS_OWNER;
		break;
	case SUNXI_SPI_CS_SOFT:
		reg_val |= SUNXI_SPI_TC_SS_OWNER;
		break;
	}

	writel(reg_val, sspi->base_addr + SUNXI_SPI_TC_REG);
}

static int sunxi_spi_ss_select(struct sunxi_spi *sspi, u16 cs)
{
	u32 reg_old, reg_new;
	int ret = 0;

	if (cs < SUNXI_SPI_CS_MAX) {
		reg_new = reg_old = readl(sspi->base_addr + SUNXI_SPI_TC_REG);
		reg_new &= ~SUNXI_SPI_TC_SS_SEL;
		reg_new |= FIELD_PREP(SUNXI_SPI_TC_SS_SEL, cs);
		if (reg_new != reg_old)
			writel(reg_new, sspi->base_addr + SUNXI_SPI_TC_REG);
	} else {
		ret = -EINVAL;
	}

	return ret;
}

static void sunxi_spi_ss_polarity(struct sunxi_spi *sspi, bool pol)
{
	u32 reg_old, reg_new;

	reg_new = reg_old = readl(sspi->base_addr + SUNXI_SPI_TC_REG);

	if (pol)
		reg_new |= SUNXI_SPI_TC_SPOL;
	else
		reg_new &= ~SUNXI_SPI_TC_SPOL;

	if (reg_new != reg_old)
		writel(reg_new, sspi->base_addr + SUNXI_SPI_TC_REG);
}

static void sunxi_spi_config_tc(struct sunxi_spi *sspi, u32 config)
{
	u32 reg_old, reg_new;

	reg_new = reg_old = readl(sspi->base_addr + SUNXI_SPI_TC_REG);

	if (config & SPI_CPOL)
		reg_new |= SUNXI_SPI_TC_CPOL;
	else
		reg_new &= ~SUNXI_SPI_TC_CPOL;

	if (config & SPI_CPHA)
		reg_new |= SUNXI_SPI_TC_CPHA;
	else
		reg_new &= ~SUNXI_SPI_TC_CPHA;

	if (config & SPI_LSB_FIRST)
		reg_new |= SUNXI_SPI_TC_FBS;
	else
		reg_new &= ~SUNXI_SPI_TC_FBS;

	if (reg_new != reg_old)
		writel(reg_new, sspi->base_addr + SUNXI_SPI_TC_REG);
}

static void sunxi_spi_enable_irq(struct sunxi_spi *sspi, u32 bitmap)
{
	u32 reg_old, reg_new;
	reg_new = reg_old = readl(sspi->base_addr + SUNXI_SPI_INT_CTL_REG);
	bitmap &= SUNXI_SPI_INT_CTL_MASK;
	reg_new |= bitmap;
	if (reg_new != reg_old)
		writel(reg_new, sspi->base_addr + SUNXI_SPI_INT_CTL_REG);
}

static void sunxi_spi_disable_irq(struct sunxi_spi *sspi, u32 bitmap)
{
	u32 reg_old, reg_new;
	reg_new = reg_old = readl(sspi->base_addr + SUNXI_SPI_INT_CTL_REG);
	bitmap &= SUNXI_SPI_INT_CTL_MASK;
	reg_new &= ~bitmap;
	if (reg_new != reg_old)
		writel(reg_new, sspi->base_addr + SUNXI_SPI_INT_CTL_REG);
}

static inline u32 sunxi_spi_qry_irq_enable(struct sunxi_spi *sspi)
{
	return (SUNXI_SPI_INT_CTL_MASK & readl(sspi->base_addr + SUNXI_SPI_INT_CTL_REG));
}

static inline u32 sunxi_spi_qry_irq_pending(struct sunxi_spi *sspi)
{
	return (SUNXI_SPI_INT_STA_MASK & readl(sspi->base_addr + SUNXI_SPI_INT_STA_REG));
}

static void sunxi_spi_clr_irq_pending(struct sunxi_spi *sspi, u32 bitmap)
{
	bitmap &= SUNXI_SPI_INT_STA_MASK;
	writel(bitmap, sspi->base_addr + SUNXI_SPI_INT_STA_REG);
}

static void sunxi_spi_reset_txfifo(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG);
	int ret;

	reg_val |= SUNXI_SPI_FIFO_CTL_TX_RST;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG);

	/* Hardware will auto clear this bit when fifo reset
	 * Before return, driver must wait reset opertion complete
	 */
	ret = readl_poll_timeout(sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG, reg_val, !(reg_val & SUNXI_SPI_FIFO_CTL_TX_RST), 1000);
	if (ret) {
		dev_err(sspi->dev, "timeout for waiting txfifo reset (%#x)\n", reg_val);
		return ;
	}
}

static void sunxi_spi_reset_rxfifo(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG);
	int ret;

	reg_val |= SUNXI_SPI_FIFO_CTL_RX_RST;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG);

	/* Hardware will auto clear this bit when fifo reset
	 * Before return, driver must wait reset opertion complete
	 */
	ret = readl_poll_timeout(sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG, reg_val, !(reg_val & SUNXI_SPI_FIFO_CTL_RX_RST), 1000);
	if (ret) {
		dev_err(sspi->dev, "timeout for waiting rxfifo reset (%#x)\n", reg_val);
		return ;
	}
}

static inline void sunxi_spi_reset_fifo(struct sunxi_spi *sspi)
{
	sunxi_spi_reset_txfifo(sspi);
	sunxi_spi_reset_rxfifo(sspi);
}

#ifdef CONFIG_AW_DMA
static void sunxi_spi_enable_dma_irq(struct sunxi_spi *sspi, u32 bitmap)
{
	u32 reg_old, reg_new;
	reg_new = reg_old = readl(sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG);
	bitmap &= SUNXI_SPI_FIFO_CTL_DRQ_EN;
	reg_new |= bitmap;
	if (reg_new != reg_old)
		writel(reg_new, sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG);
}

static void sunxi_spi_disable_dma_irq(struct sunxi_spi *sspi, u32 bitmap)
{
	u32 reg_old, reg_new;
	reg_new = reg_old = readl(sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG);
	bitmap &= SUNXI_SPI_FIFO_CTL_DRQ_EN;
	reg_new &= ~bitmap;
	if (reg_new != reg_old)
		writel(reg_new, sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG);
}
#endif

static void sunxi_spi_set_fifo_trig_level_rx(struct sunxi_spi *sspi, u32 level)
{
	u32 reg_old, reg_new;
	reg_new = reg_old = readl(sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG);
	reg_new &= ~SUNXI_SPI_FIFO_CTL_RX_TRIG_LEVEL;
	reg_new |= FIELD_PREP(SUNXI_SPI_FIFO_CTL_RX_TRIG_LEVEL, level);
	if (reg_new != reg_old)
		writel(reg_new, sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG);
}

static void sunxi_spi_set_fifo_trig_level_tx(struct sunxi_spi *sspi, u32 level)
{
	u32 reg_old, reg_new;
	reg_new = reg_old = readl(sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG);
	reg_new &= ~SUNXI_SPI_FIFO_CTL_TX_TRIG_LEVEL;
	reg_new |= FIELD_PREP(SUNXI_SPI_FIFO_CTL_TX_TRIG_LEVEL, level);
	if (reg_new != reg_old)
		writel(reg_new, sspi->base_addr + SUNXI_SPI_FIFO_CTL_REG);
}

static inline u32 sunxi_spi_get_txfifo_cnt(struct sunxi_spi *sspi)
{
	return FIELD_GET(SUNXI_SPI_FIFO_STA_TX_CNT, readl(sspi->base_addr + SUNXI_SPI_FIFO_STA_REG));
}

static inline u32 sunxi_spi_get_rxfifo_cnt(struct sunxi_spi *sspi)
{
	return FIELD_GET(SUNXI_SPI_FIFO_STA_RX_CNT, readl(sspi->base_addr + SUNXI_SPI_FIFO_STA_REG));
}

static void sunxi_spi_set_sample_delay_sw(struct sunxi_spi *sspi, u32 status)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_SAMP_DL_REG);

	if (status)
		reg_val |= SUNXI_SPI_SAMP_DL_SW_EN;
	else
		reg_val &= ~SUNXI_SPI_SAMP_DL_SW_EN;

	writel(reg_val, sspi->base_addr + SUNXI_SPI_SAMP_DL_REG);
}

static void sunxi_spi_set_sample_delay(struct sunxi_spi *sspi, u32 sample_delay)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_SAMP_DL_REG);
	reg_val &= ~SUNXI_SPI_SAMP_DL_SW;
	reg_val |= FIELD_PREP(SUNXI_SPI_SAMP_DL_SW, sample_delay);
	writel(reg_val, sspi->base_addr + SUNXI_SPI_SAMP_DL_REG);
}

static void sunxi_spi_set_bc_tc_stc(struct sunxi_spi *sspi, u32 tx_len, u32 rx_len, u32 stc_len, u32 dummy_cnt)
{
	u32 reg_val;

	/* set total burst number into MBC register */
	reg_val = readl(sspi->base_addr + SUNXI_SPI_MBC_REG);
	reg_val &= ~SUNXI_SPI_MBC;
	reg_val |= FIELD_PREP(SUNXI_SPI_MBC, tx_len + rx_len + dummy_cnt);
	writel(reg_val, sspi->base_addr + SUNXI_SPI_MBC_REG);

	/* set write transmit counter into MTC register */
	reg_val = readl(sspi->base_addr + SUNXI_SPI_MTC_REG);
	reg_val &= ~SUNXI_SPI_MWTC;
	reg_val |= FIELD_PREP(SUNXI_SPI_MWTC, tx_len);
	writel(reg_val, sspi->base_addr + SUNXI_SPI_MTC_REG);

	/* set dummy burst counter and single mode transmit counter into BCC register */
	reg_val = readl(sspi->base_addr + SUNXI_SPI_BCC_REG);
	reg_val &= ~SUNXI_SPI_BCC_STC;
	reg_val |= FIELD_PREP(SUNXI_SPI_BCC_STC, stc_len);
	reg_val &= ~SUNXI_SPI_BCC_DBC;
	reg_val |= FIELD_PREP(SUNXI_SPI_BCC_DBC, dummy_cnt);
	writel(reg_val, sspi->base_addr + SUNXI_SPI_BCC_REG);
}

static void sunxi_spi_enable_dual(struct sunxi_spi *sspi)
{
	u32 reg_new, reg_old;
	reg_new = reg_old = readl(sspi->base_addr + SUNXI_SPI_BCC_REG);
	reg_new |= SUNXI_SPI_BCC_DRM;
	if (reg_new != reg_old)
		writel(reg_new, sspi->base_addr + SUNXI_SPI_BCC_REG);
}

static void sunxi_spi_disable_dual(struct sunxi_spi *sspi)
{
	u32 reg_new, reg_old;
	reg_new = reg_old = readl(sspi->base_addr + SUNXI_SPI_BCC_REG);
	reg_new &= ~SUNXI_SPI_BCC_DRM;
	if (reg_new != reg_old)
		writel(reg_new, sspi->base_addr + SUNXI_SPI_BCC_REG);
}

static void sunxi_spi_enable_quad(struct sunxi_spi *sspi)
{
	u32 reg_new, reg_old;
	reg_new = reg_old = readl(sspi->base_addr + SUNXI_SPI_BCC_REG);
	reg_new |= SUNXI_SPI_BCC_QUAD_EN;
	if (reg_new != reg_old)
		writel(reg_new, sspi->base_addr + SUNXI_SPI_BCC_REG);
}

static void sunxi_spi_disable_quad(struct sunxi_spi *sspi)
{
	u32 reg_new, reg_old;
	reg_new = reg_old = readl(sspi->base_addr + SUNXI_SPI_BCC_REG);
	reg_new &= ~SUNXI_SPI_BCC_QUAD_EN;
	if (reg_new != reg_old)
		writel(reg_new, sspi->base_addr + SUNXI_SPI_BCC_REG);
}

static void sunxi_spi_soft_reset(struct sunxi_spi *sspi)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_GC_REG);
	int ret;

	reg_val |= SUNXI_SPI_GC_SRST;
	writel(reg_val, sspi->base_addr + SUNXI_SPI_GC_REG);

	/* Hardware will auto clear this bit when soft reset
	 * Before return, driver must wait reset opertion complete */
	ret = readl_poll_timeout(sspi->base_addr + SUNXI_SPI_GC_REG, reg_val, !(reg_val & SUNXI_SPI_GC_SRST), 1000);
	if (ret) {
		dev_err(sspi->dev, "timeout for waiting soft reset (%#x)\n", reg_val);
		return ;
	}

	if ((sspi->bus_sample_mode == SUNXI_SPI_SAMP_MODE_NEW) && (sspi->data->quirk_flag & NEW_SAMPLE_MODE_RST)) {
		u32 wait_cycle = sspi->speed_hz ? sspi->max_speed_hz / sspi->speed_hz : 1;
		/* Wait at least 32 clk cycle after srst for invisible fifo synchronized into rxfifo
		 * Max 100MHz is 10ns per cycle
		 */
		ndelay(10 * (wait_cycle + 1) * 32);
		if (sunxi_spi_get_rxfifo_cnt(sspi) > 0) {
			sunxi_spi_reset_fifo(sspi);
			dev_dbg(sspi->dev, "get quirk %#x and fixed\n", NEW_SAMPLE_MODE_RST);
		}
	}
}

static int sunxi_spi_cpu_rx(struct sunxi_spi *sspi, struct sunxi_spi_transfer *t)
{
	int rx_len = t->len;
	u8 *rx_buf = (u8 *)t->rx_buf;
	int poll_time = SUNXI_SPI_POLL_TIMEOUT;

	while (rx_len && poll_time) {
		if (sunxi_spi_get_rxfifo_cnt(sspi)) {
			*rx_buf++ =  readb(sspi->base_addr + SUNXI_SPI_RXDATA_REG);
			--rx_len;
			poll_time = SUNXI_SPI_POLL_TIMEOUT;
		} else {
			--poll_time;
		}
	}

	if (poll_time <= 0) {
		dev_err(sspi->dev, "cpu receive data time out\n");
		return -ETIME;
	}

	return 0;
}

static int sunxi_spi_cpu_tx(struct sunxi_spi *sspi, struct sunxi_spi_transfer *t)
{
	int tx_len = t->len;
	u8 *tx_buf = (u8 *)t->tx_buf;
	int poll_time = SUNXI_SPI_POLL_TIMEOUT;

	while (tx_len && poll_time) {
		if (sunxi_spi_get_txfifo_cnt(sspi) >= sspi->data->tx_fifosize) {
			--poll_time;
		} else {
			writeb(*tx_buf++, sspi->base_addr + SUNXI_SPI_TXDATA_REG);
			--tx_len;
			poll_time = SUNXI_SPI_POLL_TIMEOUT;
		}
	}

	if (poll_time <= 0) {
		dev_err(sspi->dev, "cpu transfer data time out\n");
		return -ETIME;
	}

	return 0;
}

static int sunxi_spi_cpu_tx_rx(struct sunxi_spi *sspi, struct sunxi_spi_transfer *t)
{
	int len = t->len;
	u8 *tx_buf = (u8 *)t->tx_buf;
	u8 *rx_buf = (u8 *)t->rx_buf;
	u8 fifosize = min(sspi->data->rx_fifosize, sspi->data->tx_fifosize);
	int poll_time = SUNXI_SPI_POLL_TIMEOUT;

	while (len && poll_time) {
		if (sunxi_spi_get_txfifo_cnt(sspi) >= fifosize) {
			--poll_time;
		} else {
			writeb(*tx_buf++, sspi->base_addr + SUNXI_SPI_TXDATA_REG);
			while (--poll_time && !sunxi_spi_get_rxfifo_cnt(sspi))
				;
			if (poll_time < 0)
				break;
			*rx_buf++ =  readb(sspi->base_addr + SUNXI_SPI_RXDATA_REG);
			--len;
			poll_time = SUNXI_SPI_POLL_TIMEOUT;
		}
	}

	if (poll_time <= 0) {
		dev_err(sspi->dev, "cpu duplex transfer data time out\n");
		return -ETIME;
	}

	return 0;
}

/* SPI Controller Hardware Register Operation End */

static void sunxi_spi_delay_chain_init(struct sunxi_spi *sspi)
{
	if (sspi->data->quirk_flag & NEW_SAMPLE_MODE) {
		sunxi_spi_bus_sample_mode(sspi, SUNXI_SPI_SAMP_MODE_NEW);
		if (sspi->bus_sample_mode == SUNXI_SPI_SAMP_MODE_MANUAL) {
			sunxi_spi_set_sample_mode(sspi, sspi->spi_sample_mode);
			if (sspi->spi_sample_delay) {
				sunxi_spi_set_sample_delay_sw(sspi, true);
				sunxi_spi_set_sample_delay(sspi, sspi->spi_sample_delay);
			}
		}
	} else {
		sunxi_spi_bus_sample_mode(sspi, SUNXI_SPI_SAMP_MODE_OLD);
		if (sspi->bus_sample_mode == SUNXI_SPI_SAMP_MODE_MANUAL)
			sunxi_spi_set_sample_mode(sspi, sspi->spi_sample_mode);
	}
}

static void sunxi_spi_set_delay_chain(struct sunxi_spi *sspi, u32 clk)
{
	switch (sspi->bus_sample_mode) {
	case SUNXI_SPI_SAMP_MODE_AUTO:
		if (sspi->data->quirk_flag & NEW_SAMPLE_MODE) {
			if (clk >= SUNXI_SPI_SAMP_HIGH_FREQ)
				sspi->spi_sample_mode = SUNXI_SPI_SAMP_DELAY_CYCLE_0_5;
			else
				sspi->spi_sample_mode = SUNXI_SPI_SAMP_DELAY_CYCLE_0_0;
		} else {
			if (clk >= SUNXI_SPI_SAMP_HIGH_FREQ)
				sspi->spi_sample_mode = SUNXI_SPI_SAMP_DELAY_CYCLE_1_0;
			else if (clk <= SUNXI_SPI_SAMP_LOW_FREQ)
				sspi->spi_sample_mode = SUNXI_SPI_SAMP_DELAY_CYCLE_0_0;
			else
				sspi->spi_sample_mode = SUNXI_SPI_SAMP_DELAY_CYCLE_0_5;
		}
		sunxi_spi_set_sample_mode(sspi, sspi->spi_sample_mode);
		break;
	case SUNXI_SPI_SAMP_MODE_MANUAL:
		break;
	default:
		dev_err(sspi->dev, "unsupport sample mode %d\n", sspi->bus_sample_mode);
	}
}

static void sunxi_spi_set_cs(struct sunxi_spi *sspi, struct dm_spi_slave_plat *slave_plat, bool enable)
{
	int ret = 0;

	ret = sunxi_spi_ss_select(sspi, slave_plat->cs);
	if (ret < 0) {
		dev_warn(sspi->dev, "cs %d over range, need control by gpio\n", slave_plat->cs);
		return ;
	}

	sunxi_spi_ss_polarity(sspi, !(slave_plat->mode & SPI_CS_HIGH));

	if (sspi->cs_mode == SUNXI_SPI_CS_SOFT)
		sunxi_spi_ss_level(sspi, (slave_plat->mode & SPI_CS_HIGH) ? enable : !enable);
}

static int sunxi_spi_mode_check(struct sunxi_spi *sspi, struct sunxi_spi_transfer *t, unsigned long flags)
{
	int ret = 0;

	switch (flags) {
	case SPI_XFER_BEGIN:
		if (t->tx_buf && t->rx_buf) {
			dev_err(sspi->dev, "begin mode duplex type unsupport\n");
			ret = -EINVAL;
		} else if (t->tx_buf) {
			sspi->opcode = t->tx_buf[0];
			sunxi_spi_set_discard_burst(sspi, true);
			sunxi_spi_disable_quad(sspi);
			sunxi_spi_disable_dual(sspi);
			sunxi_spi_set_bc_tc_stc(sspi, t->len, 0, t->len, 0);
			sspi->mode_type = SINGLE_HALF_DUPLEX_TX;
		} else {
			dev_err(sspi->dev, "begin mode rx type unsupport\n");
			ret = -EINVAL;
		}
		break;
	case SPI_XFER_END:
		if (t->tx_buf && t->rx_buf) {
			dev_err(sspi->dev, "end mode duplex type unsupport\n");
			ret = -EINVAL;
		} else {
			sunxi_spi_set_discard_burst(sspi, true);
			if (t->tx_buf) {
				switch (sspi->opcode) {
				case SPINOR_OP_PP_1_1_4:
				case SPINOR_OP_PP_1_1_4_4B:
					sunxi_spi_disable_dual(sspi);
					sunxi_spi_enable_quad(sspi);
					sunxi_spi_set_bc_tc_stc(sspi, t->len, 0, 0, 0);
					sspi->mode_type = QUAD_HALF_DUPLEX_TX;
					break;
				default:
					sunxi_spi_disable_quad(sspi);
					sunxi_spi_disable_dual(sspi);
					sunxi_spi_set_bc_tc_stc(sspi, t->len, 0, t->len, 0);
					sspi->mode_type = SINGLE_HALF_DUPLEX_TX;
				}
			} else if (t->rx_buf) {
				switch (sspi->opcode) {
				case SPINOR_OP_READ_1_1_4:
				case SPINOR_OP_READ_1_1_4_4B:
					sunxi_spi_disable_dual(sspi);
					sunxi_spi_enable_quad(sspi);
					sunxi_spi_set_bc_tc_stc(sspi, 0, t->len, 0, 0);
					sspi->mode_type = QUAD_HALF_DUPLEX_RX;
					break;
				case SPINOR_OP_READ_1_1_2:
				case SPINOR_OP_READ_1_1_2_4B:
					sunxi_spi_disable_quad(sspi);
					sunxi_spi_enable_dual(sspi);
					sunxi_spi_set_bc_tc_stc(sspi, 0, t->len, 0, 0);
					sspi->mode_type = DUAL_HALF_DUPLEX_RX;
					break;
				default:
					sunxi_spi_disable_quad(sspi);
					sunxi_spi_disable_dual(sspi);
					sunxi_spi_set_bc_tc_stc(sspi, 0, t->len, 0, 0);
					sspi->mode_type = SINGLE_HALF_DUPLEX_RX;
				}
			}
		}
		dev_dbg(sspi->dev, "end mode type %d\n", sspi->mode_type);
		break;
	case SPI_XFER_ONCE:
		if (t->tx_buf && t->rx_buf) {
			/* full duplex */
			sunxi_spi_set_discard_burst(sspi, false);
			sunxi_spi_disable_quad(sspi);
			sunxi_spi_disable_dual(sspi);
			sunxi_spi_set_bc_tc_stc(sspi, t->len, 0, t->len, 0);
			sspi->mode_type = SINGLE_FULL_DUPLEX_TX_RX;
		} else {
			/* half duplex transmit */
			sunxi_spi_set_discard_burst(sspi, true);
			if (t->tx_buf) {
				if (t->slave->mode & sspi->bus_mode & SPI_TX_QUAD) {
					sunxi_spi_disable_dual(sspi);
					sunxi_spi_enable_quad(sspi);
					sunxi_spi_set_bc_tc_stc(sspi, t->len, 0, 0, 0);
					sspi->mode_type = QUAD_HALF_DUPLEX_TX;
				} else if (t->slave->mode & sspi->bus_mode & SPI_TX_QUAD) {
					sunxi_spi_disable_quad(sspi);
					sunxi_spi_enable_dual(sspi);
					sunxi_spi_set_bc_tc_stc(sspi, t->len, 0, 0, 0);
					sspi->mode_type = DUAL_HALF_DUPLEX_TX;
				} else {
					sunxi_spi_disable_quad(sspi);
					sunxi_spi_disable_dual(sspi);
					sunxi_spi_set_bc_tc_stc(sspi, t->len, 0, t->len, 0);
					sspi->mode_type = SINGLE_HALF_DUPLEX_TX;
				}
			} else if (t->rx_buf) {
				if (t->slave->mode & sspi->bus_mode & SPI_RX_QUAD) {
					sunxi_spi_disable_dual(sspi);
					sunxi_spi_enable_quad(sspi);
					sunxi_spi_set_bc_tc_stc(sspi, 0, t->len, 0, 0);
					sspi->mode_type = QUAD_HALF_DUPLEX_RX;
				} else if (t->slave->mode & sspi->bus_mode & SPI_RX_DUAL) {
					sunxi_spi_disable_quad(sspi);
					sunxi_spi_enable_dual(sspi);
					sunxi_spi_set_bc_tc_stc(sspi, 0, t->len, 0, 0);
					sspi->mode_type = DUAL_HALF_DUPLEX_RX;
				} else {
					sunxi_spi_disable_quad(sspi);
					sunxi_spi_disable_dual(sspi);
					sunxi_spi_set_bc_tc_stc(sspi, 0, t->len, 0, 0);
					sspi->mode_type = SINGLE_HALF_DUPLEX_RX;
				}
			}
		}
		dev_dbg(sspi->dev, "once mode type %d\n", sspi->mode_type);
		break;
	default:
		dev_err(sspi->dev, "unsupport xfer flags %lu\n", flags);
		ret = -EINVAL;
	}

	return ret;
}

#ifdef CONFIG_AW_DMA
static bool sunxi_spi_can_dma(struct sunxi_spi *sspi, struct sunxi_spi_transfer *t)
{
	return !!(sspi->use_dma && t->len >= min(sspi->data->rx_fifosize, sspi->data->tx_fifosize));
}

static void sunxi_spi_config_dma_src(sunxi_dma_set *dma_set, int len, u32 triglevel, bool dma_force_fixed)
{
	int width, burst;

	if (dma_force_fixed) {
		/* if dma is force fixed, use old configuration to make sure the stability and compatibility */
		if (len % 4 == 0)
			width = DMAC_CFG_SRC_DATA_WIDTH_32BIT;
		else
			width = DMAC_CFG_SRC_DATA_WIDTH_8BIT;
		burst = DMAC_CFG_SRC_4_BURST;
	} else {
		if (len % 4 == 0) {
			width = DMAC_CFG_SRC_DATA_WIDTH_32BIT;
			if (triglevel < SUNXI_SPI_FIFO_DEFAULT)
				burst = DMAC_CFG_SRC_8_BURST;
			else
				burst = DMAC_CFG_SRC_16_BURST;
		} else if (len % 2 == 0) {
			width = DMAC_CFG_SRC_DATA_WIDTH_16BIT;
			burst = DMAC_CFG_SRC_16_BURST;
		} else {
			width = DMAC_CFG_SRC_DATA_WIDTH_8BIT;
			burst = DMAC_CFG_SRC_16_BURST;
		}
	}

	dma_set->channal_cfg.src_data_width = width;
	dma_set->channal_cfg.src_burst_length = burst;
}

static void sunxi_spi_config_dma_dst(sunxi_dma_set *dma_set, int len, u32 triglevel, bool dma_force_fixed)
{
	int width, burst;

	if (dma_force_fixed) {
		/* if dma is force fixed, use old configuration to make sure the stability and compatibility */
		if (len % 4 == 0)
			width = DMAC_CFG_DEST_DATA_WIDTH_32BIT;
		else
			width = DMAC_CFG_DEST_DATA_WIDTH_8BIT;
		burst = DMAC_CFG_DEST_4_BURST;
	} else {
		if (len % 4 == 0) {
			width = DMAC_CFG_DEST_DATA_WIDTH_32BIT;
			if (triglevel < SUNXI_SPI_FIFO_DEFAULT)
				burst = DMAC_CFG_DEST_8_BURST;
			else
				burst = DMAC_CFG_DEST_16_BURST;
		} else if (len % 2 == 0) {
			width = DMAC_CFG_DEST_DATA_WIDTH_16BIT;
			burst = DMAC_CFG_DEST_16_BURST;
		} else {
			width = DMAC_CFG_DEST_DATA_WIDTH_8BIT;
			burst = DMAC_CFG_DEST_16_BURST;
		}
	}

	dma_set->channal_cfg.dst_data_width = width;
	dma_set->channal_cfg.dst_burst_length = burst;
}

static void sunxi_spi_dma_cb_rx(void *p_arg)
{
	struct sunxi_spi *sspi = (struct sunxi_spi *)p_arg;
	u32 cnt = sunxi_spi_get_rxfifo_cnt(sspi);

	dev_dbg(sspi->dev, "dma rx callback\n");

	if (cnt)
		dev_err(sspi->dev, "dma done but rxfifo not empty %d\n", cnt);
}

static void sunxi_spi_dma_cb_tx(void *p_arg)
{
	struct sunxi_spi *sspi = (struct sunxi_spi *)p_arg;

	dev_dbg(sspi->dev, "dma tx callback\n");
}

static void sunxi_spi_dma_rx(struct sunxi_spi *sspi, struct sunxi_spi_transfer *t)
{
	sunxi_dma_set dma_set;

	sunxi_spi_enable_dma_irq(sspi, SUNXI_SPI_FIFO_CTL_RX_DRQ_EN);

	/* dma config for rx */
	dma_set.loop_mode = 0;
	dma_set.wait_cyc  = 8;
	dma_set.iospeed = true;
	dma_set.channal_cfg.src_drq_type     = DMAC_CFG_TYPE_SPI0 + sspi->bus_num;
	dma_set.channal_cfg.src_addr_mode    = DMAC_CFG_SRC_ADDR_TYPE_IO_MODE;
	dma_set.channal_cfg.reserved0        = 0;
	sunxi_spi_config_dma_src(&dma_set, t->len, sspi->rx_triglevel, sspi->data->quirk_flag & DMA_FORCE_FIXED);
	dma_set.channal_cfg.dst_drq_type     = DMAC_CFG_TYPE_DRAM;
	dma_set.channal_cfg.dst_addr_mode    = DMAC_CFG_DEST_ADDR_TYPE_LINEAR_MODE;
	dma_set.channal_cfg.reserved1        = 0;
	sunxi_spi_config_dma_dst(&dma_set, t->len, sspi->rx_triglevel, sspi->data->quirk_flag & DMA_FORCE_FIXED);
	dev_dbg(sspi->dev, "dma rx config width(%#x) burst(%#x)\n", dma_set.channal_cfg.src_data_width, dma_set.channal_cfg.src_burst_length);

	flush_cache((ulong)t->rx_buf, t->len);

	sunxi_dma_install_int(sspi->dma_rx.id, sunxi_spi_dma_cb_rx, sspi);
	sunxi_dma_enable(&sspi->dma_rx);
	sunxi_dma_setting(sspi->dma_rx.id, &dma_set);
	sunxi_dma_start(sspi->dma_rx.id, (ulong)sspi->base_addr + SUNXI_SPI_RXDATA_REG, (ulong)t->rx_buf, t->len);
}

static void sunxi_spi_dma_tx(struct sunxi_spi *sspi, struct sunxi_spi_transfer *t)
{
	sunxi_dma_set dma_set;

	sunxi_spi_enable_dma_irq(sspi, SUNXI_SPI_FIFO_CTL_TX_DRQ_EN);

	/* dma config for tx */
	dma_set.loop_mode = 0;
	dma_set.wait_cyc  = 8;
	dma_set.iospeed = true;
	dma_set.channal_cfg.src_drq_type     = DMAC_CFG_TYPE_DRAM;
	dma_set.channal_cfg.src_addr_mode    = DMAC_CFG_SRC_ADDR_TYPE_LINEAR_MODE;
	dma_set.channal_cfg.reserved0        = 0;
	sunxi_spi_config_dma_src(&dma_set, t->len, sspi->tx_triglevel, sspi->data->quirk_flag & DMA_FORCE_FIXED);
	dma_set.channal_cfg.dst_drq_type     = DMAC_CFG_TYPE_SPI0 + sspi->bus_num;
	dma_set.channal_cfg.dst_addr_mode    = DMAC_CFG_DEST_ADDR_TYPE_IO_MODE;
	dma_set.channal_cfg.reserved1        = 0;
	sunxi_spi_config_dma_dst(&dma_set, t->len, sspi->tx_triglevel, sspi->data->quirk_flag & DMA_FORCE_FIXED);
	dev_dbg(sspi->dev, "dma tx config width(%#x) burst(%#x)\n", dma_set.channal_cfg.dst_data_width, dma_set.channal_cfg.dst_burst_length);

	flush_cache((ulong)t->tx_buf, t->len);

	sunxi_dma_install_int(sspi->dma_tx.id, sunxi_spi_dma_cb_tx, sspi);
	sunxi_dma_enable(&sspi->dma_tx);
	sunxi_dma_setting(sspi->dma_tx.id, &dma_set);
	sunxi_dma_start(sspi->dma_tx.id, (ulong)t->tx_buf, (ulong)sspi->base_addr + SUNXI_SPI_TXDATA_REG, t->len);
}
#else
static bool sunxi_spi_can_dma(struct sunxi_spi *sspi, struct sunxi_spi_transfer *t) { return false; }
static void sunxi_spi_dma_rx(struct sunxi_spi *sspi, struct sunxi_spi_transfer *t) { }
static void sunxi_spi_dma_tx(struct sunxi_spi *sspi, struct sunxi_spi_transfer *t) { }
#endif

static int sunxi_spi_claim_bus(struct udevice *dev)
{
	struct sunxi_spi *sspi = dev_get_priv(dev->parent);

	sunxi_spi_soft_reset(sspi);
	sunxi_spi_enable_bus(sspi);
	sunxi_spi_set_master(sspi);
	sunxi_spi_ss_owner(sspi, sspi->cs_mode);
	sunxi_spi_delay_chain_init(sspi);
	sunxi_spi_reset_fifo(sspi);

	return 0;
}

static int sunxi_spi_release_bus(struct udevice *dev)
{
	struct sunxi_spi *sspi = dev_get_priv(dev->parent);

	sunxi_spi_disable_bus(sspi);

	return 0;
}

static int sunxi_spi_xfer_one(struct sunxi_spi *sspi, struct sunxi_spi_transfer *t, unsigned long flags)
{
	u32 status = 0, enable = 0;
	bool can_dma = sunxi_spi_can_dma(sspi, t);
	ulong timeout;
	int ret = 0;

	if ((!t->tx_buf && !t->rx_buf) || !t->len)
		return -EINVAL;

	dev_dbg(sspi->dev, "xfer txbuf %p, rxbuf %p, len %d\n", t->tx_buf, t->rx_buf, t->len);

	if (sunxi_spi_get_rxfifo_cnt(sspi) || sunxi_spi_get_txfifo_cnt(sspi))
		sunxi_spi_reset_fifo(sspi);
	sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_MASK);
	sunxi_spi_clr_irq_pending(sspi, SUNXI_SPI_INT_STA_MASK);
	sunxi_spi_enable_irq(sspi, SUNXI_SPI_INT_CTL_ERR);
#ifdef CONFIG_AW_DMA
	sunxi_spi_disable_dma_irq(sspi, SUNXI_SPI_FIFO_CTL_DRQ_EN);
#endif

	ret = sunxi_spi_mode_check(sspi, t, flags);
	if (ret < 0) {
		dev_err(sspi->dev, "failed to mode check spi xfer %d\n", ret);
		goto out;
	}

	sunxi_spi_enable_irq(sspi, SUNXI_SPI_INT_CTL_TC_EN);

	switch (sspi->mode_type) {
	case SINGLE_HALF_DUPLEX_RX:
	case DUAL_HALF_DUPLEX_RX:
	case QUAD_HALF_DUPLEX_RX:
		if (can_dma) {
			dev_dbg(sspi->dev, "xfer rx by dma %d\n", t->len);
			sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_TC_EN);
			sunxi_spi_dma_rx(sspi, t);
			sunxi_spi_start_xfer(sspi);
		} else {
			dev_dbg(sspi->dev, "xfer rx by cpu %d\n", t->len);
			sunxi_spi_start_xfer(sspi);
			sunxi_spi_cpu_rx(sspi, t);
		}
		break;
	case SINGLE_HALF_DUPLEX_TX:
	case DUAL_HALF_DUPLEX_TX:
	case QUAD_HALF_DUPLEX_TX:
		if (can_dma) {
			dev_dbg(sspi->dev, "xfer tx by dma %d\n", t->len);
			sunxi_spi_dma_tx(sspi, t);
			sunxi_spi_start_xfer(sspi);
		} else {
			dev_dbg(sspi->dev, "xfer tx by cpu %d\n", t->len);
			sunxi_spi_start_xfer(sspi);
			sunxi_spi_cpu_tx(sspi, t);
		}
		break;
	case SINGLE_FULL_DUPLEX_TX_RX:
		if (can_dma) {
			dev_dbg(sspi->dev, "xfer tx & rx by dma %d\n", t->len);
			sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_TC_EN);
			sunxi_spi_dma_tx(sspi, t);
			sunxi_spi_dma_rx(sspi, t);
			sunxi_spi_start_xfer(sspi);
		} else {
			dev_dbg(sspi->dev, "xfer tx & rx by cpu %d\n", t->len);
			sunxi_spi_start_xfer(sspi);
			sunxi_spi_cpu_tx_rx(sspi, t);
		}
		break;
	default:
		dev_err(sspi->dev, "unknown xfer mode type %d\n", sspi->mode_type);
		ret = -EINVAL;
		goto out;
	}

	timeout = get_timer(0);
	while (get_timer(timeout) < SUNXI_SPI_XFER_TIMEOUT) {
#ifdef CONFIG_AW_DMA
		if (can_dma && ((sspi->mode_type != SINGLE_HALF_DUPLEX_TX) &&
			(sspi->mode_type != DUAL_HALF_DUPLEX_TX) && (sspi->mode_type != QUAD_HALF_DUPLEX_TX))) {
			if (sunxi_dma_querystatus(sspi->dma_rx.id) == 0) {
				dev_dbg(sspi->dev, "xfer dma done\n");
				break;
			}
		} else {
#endif
			enable = sunxi_spi_qry_irq_enable(sspi);
			status = sunxi_spi_qry_irq_pending(sspi);
			sunxi_spi_clr_irq_pending(sspi, status);
			dev_dbg(sspi->dev, "interrupt enable(%#x) status(%#x)\n", enable, status);
			if ((enable & SUNXI_SPI_INT_CTL_TC_EN) && (status & SUNXI_SPI_INT_STA_TC)) {
				dev_dbg(sspi->dev, "xfer irq tc comes\n");
				sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_TC_EN);
				break;
			} else if ((enable & SUNXI_SPI_INT_CTL_ERR) && (status & SUNXI_SPI_INT_STA_ERR)) {
				dev_err(sspi->dev, "xfer irq status error %#x\n", status);
				sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_ERR);
				sunxi_spi_soft_reset(sspi);
				ret = -EIO;
				goto out;
			}
#ifdef CONFIG_AW_DMA
		}
#endif
	}

	if (get_timer(timeout) >= SUNXI_SPI_XFER_TIMEOUT) {
		dev_err(sspi->dev, "wait for xfer timeout\n");
		ret = -ETIME;
		goto out;
	}

out:
	sunxi_spi_disable_irq(sspi, SUNXI_SPI_INT_CTL_MASK);
#ifdef CONFIG_AW_DMA
	if (ret < 0 && can_dma) {
		sunxi_dma_stop(sspi->dma_tx.id);
		sunxi_dma_stop(sspi->dma_rx.id);
	}
#endif
	return ret;
}

static int sunxi_spi_xfer(struct udevice *dev, unsigned int bitlen,
			  const void *dout, void *din, unsigned long flags)
{
	struct sunxi_spi *sspi = dev_get_priv(dev->parent);
	struct dm_spi_slave_plat *slave_plat = dev_get_parent_plat(dev);
	struct sunxi_spi_transfer t;
	int i, num, len;
	int ret = 0;

	if ((!dout && !din) || !bitlen)
		return -EINVAL;

	memset(&t, 0, sizeof(t));
	len = bitlen / 8;
	num = DIV_ROUND_UP(len, SUNXI_SPI_MAX_XFER_LEN);
	dev_dbg(sspi->dev, "size %d splite to %d num xfer\n", len, num);

	t.tx_buf = (u8 *)dout;
	t.rx_buf = (u8 *)din;
	t.slave = slave_plat;

	if (flags & SPI_XFER_BEGIN)
		sunxi_spi_set_cs(sspi, slave_plat, true);

	for (i = 0; i < num; i++) {
		if (len < SUNXI_SPI_MAX_XFER_LEN)
			t.len = len;
		else
			t.len = SUNXI_SPI_MAX_XFER_LEN;

		if (IS_ENABLED(DEBUG) || IS_ENABLED(CONFIG_DEBUG))
			sunxi_spi_dump_reg(sspi, 0x50);
		ret = sunxi_spi_xfer_one(sspi, &t, flags);
		if (ret < 0)
			dev_err(sspi->dev, "xfer index %d failed %d\n", i, ret);
		if (ret < 0 || IS_ENABLED(DEBUG) || IS_ENABLED(CONFIG_DEBUG))
			sunxi_spi_dump_reg(sspi, 0x50);

		len -= SUNXI_SPI_MAX_XFER_LEN;
		if (t.tx_buf)
			t.tx_buf += SUNXI_SPI_MAX_XFER_LEN;
		if (t.rx_buf)
			t.rx_buf += SUNXI_SPI_MAX_XFER_LEN;
	}

	if (flags & SPI_XFER_END)
		sunxi_spi_set_cs(sspi, slave_plat, false);

	return ret;
}

static int sunxi_spi_set_speed(struct udevice *dev, uint speed)
{
	struct sunxi_spi *sspi = dev_get_priv(dev);

	if (speed < sspi->min_speed_hz || speed > sspi->max_speed_hz) {
		dev_err(sspi->dev, "speed %d not in range\n", speed);
		return -EINVAL;
	}

	sunxi_spi_set_delay_chain(sspi, speed);
	sspi->speed_hz = speed;

	return 0;
}

static int sunxi_spi_set_mode(struct udevice *dev, uint mode)
{
	struct sunxi_spi *sspi = dev_get_priv(dev);
	u32 bad_bits;

	bad_bits = mode & ~sspi->mode_bits;
	if (bad_bits) {
		dev_err(sspi->dev, "unsupport mode bits %x\n", bad_bits);
		return -EINVAL;
	}

	sunxi_spi_config_tc(sspi, mode);

	return 0;
}

static const struct dm_spi_ops sunxi_spi_ops = {
	.claim_bus		= sunxi_spi_claim_bus,
	.release_bus	= sunxi_spi_release_bus,
	.xfer			= sunxi_spi_xfer,
	.set_speed		= sunxi_spi_set_speed,
	.set_mode		= sunxi_spi_set_mode,
};

static int sunxi_spi_resource_get(struct sunxi_spi *sspi)
{
	int ret = 0;

	sspi->bus_num = dev_seq(sspi->dev);
	sspi->base_addr = dev_read_addr_ptr(sspi->dev);

	sspi->bus_freq = dev_read_u32_default(sspi->dev, "clock-frequency", SUNXI_SPI_MAX_FREQUENCY);
	if (sspi->bus_freq < SUNXI_SPI_MIN_FREQUENCY || sspi->bus_freq > SUNXI_SPI_MAX_FREQUENCY) {
		dev_warn(sspi->dev, "frequency no in range, use default value %d\n", SUNXI_SPI_MAX_FREQUENCY);
		sspi->bus_freq = SUNXI_SPI_MAX_FREQUENCY;
	}

	sspi->cs_num = dev_read_u32_default(sspi->dev, "sunxi,spi-num-cs", SUNXI_SPI_CS_MAX);
	if (sspi->cs_num < 0 || sspi->cs_num > SUNXI_SPI_CS_MAX) {
		dev_warn(sspi->dev, "cs number no in range, use default value %d\n", SUNXI_SPI_CS_MAX);
		sspi->bus_freq = SUNXI_SPI_CS_MAX;
	}

	sspi->bus_mode = dev_read_u32_default(sspi->dev, "sunxi,spi-bus-mode", SUNXI_SPI_BUS_MASTER);
	sspi->cs_mode = dev_read_u32_default(sspi->dev, "sunxi,spi-cs-mode", SUNXI_SPI_CS_AUTO);
	if (sspi->bus_mode & sspi->data->hw_bus_mode) {
		if (sspi->bus_mode == SUNXI_SPI_BUS_FLASH && sspi->cs_mode != SUNXI_SPI_CS_SOFT) {
			sspi->cs_mode = SUNXI_SPI_CS_SOFT;
			dev_warn(sspi->dev, "bus in flash mode, cs force use software control\n");
		}
	} else {
		dev_err(sspi->dev, "unsupport hw bus mode %#x\n", sspi->bus_mode);
		ret = -EINVAL;
		goto out;
	}

	sspi->spi_sample_mode = dev_read_u32_default(sspi->dev, "sample_mode", SUNXI_SPI_SAMP_MODE_DL_DEFAULT);
	sspi->spi_sample_delay = dev_read_u32_default(sspi->dev, "sample_delay", SUNXI_SPI_SAMP_MODE_DL_DEFAULT);
	if (sspi->spi_sample_mode == SUNXI_SPI_SAMP_MODE_DL_DEFAULT && sspi->spi_sample_delay == SUNXI_SPI_SAMP_MODE_DL_DEFAULT) {
		sspi->bus_sample_mode = SUNXI_SPI_SAMP_MODE_AUTO;
		sspi->spi_sample_mode = sspi->spi_sample_delay = 0;
	} else {
		sspi->bus_sample_mode = SUNXI_SPI_SAMP_MODE_MANUAL;
		if (sspi->data->quirk_flag & NEW_SAMPLE_MODE) {
			if (sspi->spi_sample_mode > SUNXI_SPI_SAMP_DELAY_CYCLE_3_0) {
				dev_warn(sspi->dev, "sample mode %d over new delay cycle\n", sspi->spi_sample_mode);
				sspi->spi_sample_mode = SUNXI_SPI_SAMP_DELAY_CYCLE_0_0;
			}
			if (sspi->spi_sample_delay > SUNXI_SPI_SAMPLE_DELAY_CHAIN_MAX) {
				dev_warn(sspi->dev, "sample delay %d over new delay chain\n", sspi->spi_sample_delay);
				sspi->spi_sample_delay = SUNXI_SPI_SAMPLE_DELAY_CHAIN_MIN;
			}
		} else {
			if (sspi->spi_sample_mode > SUNXI_SPI_SAMP_DELAY_CYCLE_1_0) {
				dev_warn(sspi->dev, "sample mode %d over old delay cycle\n", sspi->spi_sample_mode);
				sspi->spi_sample_mode = SUNXI_SPI_SAMP_DELAY_CYCLE_0_0;
			}
			sspi->spi_sample_delay = 0;
		}
	}

#if !defined(CONFIG_MACH_SUN55IW6)
	ret = clk_get_by_name(sspi->dev, "mod", &sspi->clk_mod);
	if (ret < 0) {
		dev_err(sspi->dev, "failed to get mod clk %d\n", ret);
		goto out;
	}
	ret = clk_get_by_name(sspi->dev, "bus", &sspi->clk_bus);
	if (ret < 0) {
		dev_err(sspi->dev, "failed to get bus clk %d\n", ret);
		goto out;
	}

	ret = reset_get_by_index(sspi->dev, 0, &sspi->reset);
	if (ret < 0) {
		dev_err(sspi->dev, "failed to get reset %d\n", ret);
		goto out;
	}
#endif
	dev_info(sspi->dev, "bus num_%d mode_%d freq_%d\n", sspi->bus_num, sspi->bus_mode, sspi->bus_freq);
	dev_info(sspi->dev, "cs num_%d mode_%d\n", sspi->cs_num, sspi->cs_mode);
	if (sspi->bus_sample_mode == SUNXI_SPI_SAMP_MODE_MANUAL)
		dev_info(sspi->dev, "spi manual set sample mode_%d, delay_%d\n", sspi->spi_sample_mode, sspi->spi_sample_delay);

out:
	return ret;
}

#ifdef CONFIG_AW_DMA
static int sunxi_spi_request_dma(struct sunxi_spi *sspi)
{
	struct uclass *uc;
	struct udevice *dev;
	int ret = 0;

	ret = uclass_get(UCLASS_DMA, &uc);
	if (ret) {
		dev_err(sspi->dev, "get dma uclass failed %d\n", ret);
		return -EINVAL;
	}

	uclass_foreach_dev(dev, uc) {
		ret = device_probe(dev);
		if (ret)
			dev_warn(sspi->dev, "dma device probe failed %d\n", ret);
	}

	ret = sunxi_dma_request(&sspi->dma_tx);
	if (ret) {
		dev_err(sspi->dev, "failed to request dma tx channel %d\n", ret);
		goto err0;
	}

	ret = sunxi_dma_request(&sspi->dma_rx);
	if (ret) {
		dev_err(sspi->dev, "failed to request dma rx channel %d\n", ret);
		goto err1;
	}

	dev_info(sspi->dev, "request dma channel %#lx(tx) and %#lx(rx) success\n", sspi->dma_tx.id, sspi->dma_rx.id);

	return 0;

err1:
	sunxi_dma_free(&sspi->dma_tx);
err0:
	return ret;
}
#else
static int sunxi_spi_request_dma(struct sunxi_spi *sspi) { return -EINVAL; }
#endif

static int sunxi_spi_clk_init(struct sunxi_spi *sspi, u32 clk)
{
#if defined(CONFIG_MACH_SUN55IW6)
	u32 reg_val, cfg;

	/*reset*/
	cfg = readl(SUNXI_CCMU_BASE + SPI_BGR_REG(sspi->bus_num));
	cfg &= (~(0x1<<16));
	writel(cfg, SUNXI_CCMU_BASE + SPI_BGR_REG(sspi->bus_num));

	cfg = readl(SUNXI_CCMU_BASE + SPI_BGR_REG(sspi->bus_num));
	cfg |= (0x1<<16);
	writel(cfg, SUNXI_CCMU_BASE + SPI_BGR_REG(sspi->bus_num));

	/*open ahb*/
	cfg = readl(SUNXI_CCMU_BASE + SPI_BGR_REG(sspi->bus_num));
	cfg |= (0x1<<0);
	writel(cfg, SUNXI_CCMU_BASE + SPI_BGR_REG(sspi->bus_num));

	/*open clock*/
	reg_val = readl(SUNXI_CCMU_BASE + SPI_CLK_BGR(sspi->bus_num));
	reg_val &= (~(0x1U << 31));
	writel(reg_val, SUNXI_CCMU_BASE + SPI_CLK_BGR(sspi->bus_num));

	reg_val = readl(SUNXI_CCMU_BASE + SPI_CLK_BGR(sspi->bus_num));
	reg_val &= ~(0xFFFFFFFF);
	reg_val |= 0x1U << 31;
	writel(reg_val, SUNXI_CCMU_BASE + SPI_CLK_BGR(sspi->bus_num));

	return 0;
#else
	int ret = 0;

	reset_assert(&sspi->reset);
	reset_deassert(&sspi->reset);

	ret = clk_enable(&sspi->clk_mod);
	if (ret) {
		dev_err(sspi->dev, "failed to enable clk_mod %d\n", ret);
		return ret;
	}

	ret = clk_enable(&sspi->clk_bus);
	if (ret) {
		dev_err(sspi->dev, "failed to enable clk_bus %d\n", ret);
		goto err_bus;
	}

	return 0;
err_bus:
	clk_disable(&sspi->clk_mod);
	return ret;
#endif
}

static int sunxi_spi_probe(struct udevice *dev)
{
	struct sunxi_spi *sspi = dev_get_priv(dev);
	int ret = 0;

	sspi->dev = dev;
	sspi->data = (struct sunxi_spi_hw_data *)dev_get_driver_data(dev);

	ret = sunxi_spi_resource_get(sspi);
	if (ret < 0) {
		dev_err(sspi->dev, "failed to get spi resource %d\n", ret);
		goto err;
	}

	if (sunxi_spi_request_dma(sspi))
		sspi->use_dma = false;
	else
		sspi->use_dma = true;

	sspi->rx_triglevel = sspi->data->rx_fifosize / 2;
	sspi->tx_triglevel = sspi->data->tx_fifosize / 2;
	sspi->min_speed_hz = SUNXI_SPI_MIN_FREQUENCY;
	sspi->max_speed_hz = sspi->bus_freq;
	sspi->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH | SPI_LSB_FIRST | sspi->data->bus_mode_extra;

	sunxi_spi_clk_init(sspi, sspi->bus_freq);

	sunxi_spi_soft_reset(sspi);
	sunxi_spi_reset_fifo(sspi);
	sunxi_spi_set_fifo_trig_level_rx(sspi, sspi->rx_triglevel);
	sunxi_spi_set_fifo_trig_level_tx(sspi, sspi->tx_triglevel);

	dev_info(sspi->dev, "probe success (v%s)\n", SUNXI_SPI_MODULE_VERSION);

	return 0;
err:
	return ret;
}

static const struct sunxi_spi_hw_data sunxi_spi_data_v1_1 = {
	.hw_bus_mode = SUNXI_SPI_BUS_MASTER | SUNXI_SPI_BUS_FLASH,
	.bus_mode_extra = SPI_TX_DUAL | SPI_TX_QUAD | SPI_RX_DUAL | SPI_RX_QUAD,
	.quirk_flag = NEW_SAMPLE_MODE | NEW_SAMPLE_MODE_RST,
	.rx_fifosize = 64,
	.tx_fifosize = 64,
};

static const struct sunxi_spi_hw_data sunxi_spi_data_v1_3 = {
	.hw_bus_mode = SUNXI_SPI_BUS_MASTER | SUNXI_SPI_BUS_FLASH,
	.bus_mode_extra = SPI_TX_DUAL | SPI_TX_QUAD | SPI_RX_DUAL | SPI_RX_QUAD,
	.quirk_flag = INDEPENDENT_BSR | NEW_SAMPLE_MODE | NEW_SAMPLE_MODE_RST,
	.rx_fifosize = 128,
	.tx_fifosize = 64,
};

static const struct sunxi_spi_hw_data sunxi_spi_data_v1_5 = {
	.hw_bus_mode = SUNXI_SPI_BUS_MASTER | SUNXI_SPI_BUS_FLASH,
	.bus_mode_extra = SPI_TX_DUAL | SPI_TX_QUAD | SPI_RX_DUAL | SPI_RX_QUAD,
	.quirk_flag = INDEPENDENT_BSR | NEW_SAMPLE_MODE,
	.rx_fifosize = 128,
	.tx_fifosize = 128,
};

static const struct udevice_id sunxi_spi_ids[] = {
	/* 1886 */
	{ .compatible = "allwinner,sunxi-spi-v1.1", .data = (ulong)&sunxi_spi_data_v1_1 },
	/* 1890/1885/1903 */
	{ .compatible = "allwinner,sunxi-spi-v1.3", .data = (ulong)&sunxi_spi_data_v1_3 },
	/* 1911/1912 */
	{ .compatible = "allwinner,sunxi-spi-v1.5", .data = (ulong)&sunxi_spi_data_v1_5 },
	{},
};

U_BOOT_DRIVER(sunxi_spi) = {
	.name	= SUNXI_SPI_DEV_NAME,
	.id	= UCLASS_SPI,
	.of_match	= sunxi_spi_ids,
	.ops	= &sunxi_spi_ops,
	.priv_auto	= sizeof(struct sunxi_spi),
	.probe	= sunxi_spi_probe,
};
