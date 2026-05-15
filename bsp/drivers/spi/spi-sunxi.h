/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _SPI_SUNXI_H_
#define _SPI_SUNXI_H_

#include <linux/bitfield.h>
#include <spi.h>
#include <dt-bindings/spi/sunxi-spi.h>
#ifdef CONFIG_AW_DMA
#include <asm/arch/dma.h>
#endif

#if defined(CONFIG_MACH_SUN55IW6)
	#define SPI_CLK_BGR(num)		(0x0F00 + ((num) > 2 ? (num+1):(num)) * (0x08))
	#define SPI_BGR_REG(num)		(0x0F04 + ((num) > 2 ? (num+1):(num)) * (0x08))
#endif

#define SUNXI_SPI_XFER_TIMEOUT	(5000)
#define SUNXI_SPI_POLL_TIMEOUT	(0x1ffffff)

#define SUNXI_SPI_CS_MAX		(4)			/* SPI Controller support max chip select */
#define SUNXI_SPI_FIFO_DEFAULT	(64)		/* SPI Controller default fifo depth */
#define SUNXI_SPI_MAX_FREQUENCY	(100000000)	/* SPI Controller support max freq 100Mhz */
#define SUNXI_SPI_MIN_FREQUENCY	(187500)	/* SPI Controller support min freq 187.5Khz(24M/8/16) */
#define SUNXI_SPI_MAX_XFER_LEN	(0xfffff0)	/* Burst len maximum is 0xffffff and software limited to 0xfffff0 */

/* SPI Global Control Register */
#define SUNXI_SPI_GC_REG		(0x04)
	#define SUNXI_SPI_GC_SRST			BIT(31)	/* Soft Reset */
	#define SUNXI_SPI_GC_TP_EN			BIT(7)	/* Transmit Pause Enable */
	#define SUNXI_SPI_GC_MODE_SEL		BIT(2)	/* Sample Timing Mode Select */
	#define SUNXI_SPI_GC_MODE			BIT(1)	/* SPI Function Mode Select */
	#define SUNXI_SPI_GC_EN				BIT(0)	/* SPI Module Enable Control */
	/* non-Register */
	#define SUNXI_SPI_SAMP_MODE_OLD		(0)
	#define SUNXI_SPI_SAMP_MODE_NEW		(1)

/* SPI Transfer Control Register */
#define SUNXI_SPI_TC_REG		(0x08)
	#define SUNXI_SPI_TC_XCH		BIT(31)	/* Exchange Burst */
	#define SUNXI_SPI_TC_SDC1		BIT(15)	/* Master Sample Data Control Register 1 */
	#define SUNXI_SPI_TC_SDM		BIT(13)	/* Master Sample Data Mode */
	#define SUNXI_SPI_TC_FBS		BIT(12)	/* First Transmit Bit Select */
	#define SUNXI_SPI_TC_SDC		BIT(11)	/* Master Sample Data Control */
	#define SUNXI_SPI_TC_DHB		BIT(8)	/* Discard Hash Burst */
	#define SUNXI_SPI_TC_SS_LEVEL	BIT(7)	/* SS Signal Level Output */
	#define SUNXI_SPI_TC_SS_OWNER	BIT(6)	/* SS Output Owner Select */
	#define SUNXI_SPI_TC_SS_SEL		GENMASK(5, 4)	/* SPI Chip Select */
	#define SUNXI_SPI_TC_SPOL		BIT(2)	/* SPI Chip Select Signal Polarity Control */
	#define SUNXI_SPI_TC_CPOL		BIT(1)	/* SPI Clock Polarity Control */
	#define SUNXI_SPI_TC_CPHA		BIT(0)	/* SPI Clock/Data Phase Control */

/* SPI Interrupt Control Register */
#define SUNXI_SPI_INT_CTL_REG	(0x10)
	#define SUNXI_SPI_INT_CTL_SS_EN		BIT(13)	/* SSI Interrupt Enable */
	#define SUNXI_SPI_INT_CTL_TC_EN		BIT(12)	/* Transfer Completed Interrupt Enable */
	#define SUNXI_SPI_INT_CTL_TX_UDR_EN	BIT(11)	/* TX FIFO Underrun Interrupt Enable */
	#define SUNXI_SPI_INT_CTL_TX_OVF_EN	BIT(10)	/* TX FIFO Overflow Interrupt Enable */
	#define SUNXI_SPI_INT_CTL_RX_UDR_EN	BIT(9)	/* RX FIFO Underrun Interrupt Enable */
	#define SUNXI_SPI_INT_CTL_RX_OVF_EN	BIT(8)	/* RX FIFO Overflow Interrupt Enable */
	#define SUNXI_SPI_INT_CTL_TX_FUL_EN	BIT(6)	/* TX FIFO Full Interrupt Enable */
	#define SUNXI_SPI_INT_CTL_TX_EMP_EN	BIT(5)	/* TX FIFO Empty Interrupt Enable */
	#define SUNXI_SPI_INT_CTL_TX_ERQ_EN	BIT(4)	/* TX FIFO Empty Request Interrupt Enable */
	#define SUNXI_SPI_INT_CTL_RX_FUL_EN	BIT(2)	/* RX FIFO Full Interrupt Enable */
	#define SUNXI_SPI_INT_CTL_RX_EMP_EN	BIT(1)	/* RX FIFO Empty Interrupt Enable */
	#define SUNXI_SPI_INT_CTL_RX_RDY_EN	BIT(0)	/* RX FIFO Ready Request Interrupt Enable */
	/* non-Register */
	#define SUNXI_SPI_INT_CTL_ERR	(SUNXI_SPI_INT_CTL_TX_OVF_EN|SUNXI_SPI_INT_CTL_RX_UDR_EN|SUNXI_SPI_INT_CTL_RX_OVF_EN)	/* No TX FIFO Underrun */
	#define SUNXI_SPI_INT_CTL_MASK	(GENMASK(13, 8) | GENMASK(6, 4) | GENMASK(2, 0))

/* SPI Interrupt Status Register */
#define SUNXI_SPI_INT_STA_REG	(0x14)
	#define SUNXI_SPI_INT_STA_SSI		BIT(13)	/* SS Invalid Interrupt */
	#define SUNXI_SPI_INT_STA_TC		BIT(12)	/* Transfer Completed */
	#define SUNXI_SPI_INT_STA_TX_UDR	BIT(11)	/* TX FIFO Underrun */
	#define SUNXI_SPI_INT_STA_TX_OVF	BIT(10)	/* TX FIFO Overflow */
	#define SUNXI_SPI_INT_STA_RX_UDR	BIT(9)	/* RX FIFO Underrun */
	#define SUNXI_SPI_INT_STA_RX_OVF	BIT(8)	/* RX FIFO Overflow */
	#define SUNXI_SPI_INT_STA_TX_FULL	BIT(6)	/* TX FIFO Full */
	#define SUNXI_SPI_INT_STA_TX_EMP	BIT(5)	/* TX FIFO Empty */
	#define SUNXI_SPI_INT_STA_TX_RDY	BIT(4)	/* TX FIFO Ready */
	#define SUNXI_SPI_INT_STA_RX_FULL	BIT(2)	/* RX FIFO Full */
	#define SUNXI_SPI_INT_STA_RX_EMP	BIT(1)	/* RX FIFO Empty */
	#define SUNXI_SPI_INT_STA_RX_RDY	BIT(0)	/* RX FIFO Ready */
	/* non-Register */
	#define SUNXI_SPI_INT_STA_ERR	(SUNXI_SPI_INT_STA_TX_OVF|SUNXI_SPI_INT_STA_RX_UDR|SUNXI_SPI_INT_STA_RX_OVF)	/* No TX FIFO Underrun */
	#define SUNXI_SPI_INT_STA_MASK	(GENMASK(13, 8) | GENMASK(6, 4) | GENMASK(2, 0))

/* SPI FIFO Control Register */
#define SUNXI_SPI_FIFO_CTL_REG	(0x18)
	#define SUNXI_SPI_FIFO_CTL_TX_RST			BIT(31)			/* TX FIFO Reset */
	#define SUNXI_SPI_FIFO_CTL_TX_DRQ_EN		BIT(24)			/* TX FIFO DMA Request Enable */
	#define SUNXI_SPI_FIFO_CTL_TX_TRIG_LEVEL	GENMASK(23, 16)	/* TX FIFO Empty Request Trigger Level */
	#define SUNXI_SPI_FIFO_CTL_RX_RST			BIT(15)			/* RX FIFO Reset */
	#define SUNXI_SPI_FIFO_CTL_RX_DRQ_EN		BIT(8)			/* RX FIFO DMA Request Enable */
	#define SUNXI_SPI_FIFO_CTL_RX_TRIG_LEVEL	GENMASK(7, 0)	/* RX FIFO Ready Request Trigger Level */
	/* non-Register */
	#define SUNXI_SPI_FIFO_CTL_RST		(SUNXI_SPI_FIFO_CTL_TX_RST|SUNXI_SPI_FIFO_CTL_RX_RST)
	#define SUNXI_SPI_FIFO_CTL_DRQ_EN	(SUNXI_SPI_FIFO_CTL_TX_DRQ_EN|SUNXI_SPI_FIFO_CTL_RX_DRQ_EN)

/* SPI FIFO Status Register */
#define SUNXI_SPI_FIFO_STA_REG	(0x1C)
	#define SUNXI_SPI_FIFO_STA_TB_WR	BIT(31)			/* TX FIFO Write Buffer Write Enable */
	#define SUNXI_SPI_FIFO_STA_TX_CNT	GENMASK(23, 16)	/* TX FIFO Counter */
	#define SUNXI_SPI_FIFO_STA_RB_WR	BIT(15)			/* RX FIFO Read Buffer Write Enable */
	#define SUNXI_SPI_FIFO_STA_RX_CNT	GENMASK(7, 0)	/* RX FIFO Counter */

/* SPI Sample Delay Register */
#define SUNXI_SPI_SAMP_DL_REG	(0x28)
	#define SUNXI_SPI_SAMP_DL_SW_EN		BIT(7)			/* Sample Delay Software Enable */
	#define SUNXI_SPI_SAMP_DL_SW		GENMASK(5, 0)	/* Sample Delay Software */

/* SPI Master Burst Counter Register */
#define SUNXI_SPI_MBC_REG	(0x30)
	#define SUNXI_SPI_MBC		GENMASK(23, 0)	/* Master Burst Counter */

/* SPI Master Transmit Counter Register */
#define SUNXI_SPI_MTC_REG	(0x34)
	#define SUNXI_SPI_MWTC		GENMASK(23, 0)	/* Master Write Transmit Counter */

/* SPI Master Burst Control Counter Register */
#define SUNXI_SPI_BCC_REG		(0x38)
	#define SUNXI_SPI_BCC_QUAD_EN	BIT(29)	/* Master Quad Mode Enable */
	#define SUNXI_SPI_BCC_DRM		BIT(28)	/* Master Dual Mode Enable */
	#define SUNXI_SPI_BCC_DBC		GENMASK(27, 24)	/* Master Dummy Burst Counter */
	#define SUNXI_SPI_BCC_STC		GENMASK(23, 0)	/* Master Single Mode Transmit Counter */

/* SPI TX Data Register */
#define SUNXI_SPI_TXDATA_REG	(0x200)

/* SPI RX Data Register */
#define SUNXI_SPI_RXDATA_REG	(0x300)

#define SUNXI_SPI_SAMP_HIGH_FREQ	(60000000)	/* sample mode threshold frequency */
#define SUNXI_SPI_SAMP_LOW_FREQ		(24000000)	/* sample mode threshold frequency */
#define SUNXI_SPI_SAMP_MODE_DL_DEFAULT	(0xaaaaffff)

enum sunxi_spi_sample_mode {
	SUNXI_SPI_SAMP_MODE_AUTO = 0,
	SUNXI_SPI_SAMP_MODE_MANUAL = 1,
};

enum sunxi_spi_sample_delay_cycle {
	SUNXI_SPI_SAMP_DELAY_CYCLE_0_0 = 0,
	SUNXI_SPI_SAMP_DELAY_CYCLE_0_5 = 1,
	SUNXI_SPI_SAMP_DELAY_CYCLE_1_0 = 2,
	SUNXI_SPI_SAMP_DELAY_CYCLE_1_5 = 3,
	SUNXI_SPI_SAMP_DELAY_CYCLE_2_0 = 4,
	SUNXI_SPI_SAMP_DELAY_CYCLE_2_5 = 5,
	SUNXI_SPI_SAMP_DELAY_CYCLE_3_0 = 6,
};

enum sunxi_spi_sample_delay_chain {
	SUNXI_SPI_SAMPLE_DELAY_CHAIN_MIN = 0,
	SUNXI_SPI_SAMPLE_DELAY_CHAIN_MAX = 63,
};

enum sunxi_spi_mode_type {
	MODE_TYPE_NULL,
	SINGLE_HALF_DUPLEX_RX,		/* single mode, half duplex read */
	SINGLE_HALF_DUPLEX_TX,		/* single mode, half duplex write */
	SINGLE_FULL_DUPLEX_TX_RX,	/* single mode, full duplex read and write */
	DUAL_HALF_DUPLEX_RX,		/* dual mode, half duplex read */
	DUAL_HALF_DUPLEX_TX,		/* dual mode, half duplex write */
	QUAD_HALF_DUPLEX_RX,		/* quad mode, half duplex read */
	QUAD_HALF_DUPLEX_TX,		/* quad mode, half duplex write */
};

enum sunxi_spi_quirk_flags {
	DMA_FORCE_FIXED = BIT(0),	/* workaround for spi RX_FIFO underrun issue. fixed on 1886 */
	INDEPENDENT_BSR = BIT(1),	/* identify SPI_BSR register exsit or not. addition on 1890 */
	NEW_SAMPLE_MODE = BIT(2),	/* new mode of sample timing including maximum 3 cycle delay and delay chain control. */
	NEW_SAMPLE_MODE_RST = BIT(3),	/* workaround for invisible fifo under new sample mode synchronize issue. */
};

struct sunxi_spi_hw_data {
	u32 hw_bus_mode;	/* flag to identify hardware controller bus mode support */
	u32 bus_mode_extra;	/* flag to identify hardware controller slave mode extra support */
	u32 quirk_flag;
	u8 rx_fifosize;
	u8 tx_fifosize;
};

struct sunxi_spi_transfer {
	u8 *tx_buf;
	u8 *rx_buf;
	int len;
	struct dm_spi_slave_plat *slave;
};

struct sunxi_spi {
	struct udevice *dev;
	void *base_addr;
	struct clk clk_pll, clk_mod, clk_bus;
	struct reset_ctl reset;

#ifdef CONFIG_AW_DMA
	struct dma dma_tx;
	struct dma dma_rx;
#endif

	int bus_num;
	u32 bus_mode;
	u32 bus_freq;
	u32 cs_num;
	u32 cs_mode;
	u8 rx_triglevel;
	u8 tx_triglevel;
	bool use_dma;
	u32 bus_sample_mode;
	u32 spi_sample_mode;
	u32 spi_sample_delay;

	u32 min_speed_hz;
	u32 max_speed_hz;
	u32 speed_hz;
	u32 mode_bits;
	enum sunxi_spi_mode_type mode_type;
	struct sunxi_spi_hw_data *data;

	u8 opcode; /* save flash opcode for single/dual/quad mode */
};

#endif /* _SPI_SUNXI_H_ */
