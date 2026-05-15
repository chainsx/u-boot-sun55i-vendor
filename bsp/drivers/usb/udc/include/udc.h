/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __UDC_H
#define __UDC_H

#ifndef __deprecated
#define __deprecated
#endif

#include <linux/compat.h>

enum sunxi_udc_mode {
	USB_UNDEFINED = 0,
	USB_HOST,
	USB_PERIPHERAL,
	USB_OTG
};

struct clk;

enum sunxi_udc_fifo_style {
	FIFO_RXTX,
	FIFO_TX,
	FIFO_RX
} __attribute__ ((packed));

enum sunxi_udc_buf_mode {
	BUF_SINGLE,
	BUF_DOUBLE
} __attribute__ ((packed));

struct sunxi_udc_fifo_cfg {
	u8			hw_ep_num;
	enum sunxi_udc_fifo_style	style;
	enum sunxi_udc_buf_mode	mode;
	u16			maxpacket;
};

#define USB_EP_FIFO(ep, st, m, pkt)		\
{						\
	.hw_ep_num	= ep,			\
	.style		= st,			\
	.mode		= m,			\
	.maxpacket	= pkt,			\
}

#define USB_EP_FIFO_SINGLE(ep, st, pkt)	\
	USB_EP_FIFO(ep, st, BUF_SINGLE, pkt)

#define USB_EP_FIFO_DOUBLE(ep, st, pkt)	\
	USB_EP_FIFO(ep, st, BUF_DOUBLE, pkt)

struct sunxi_udc_hdrc_eps_bits {
	const char	name[16];
	u8		bits;
};

struct sunxi_udc_hdrc_config {
	struct sunxi_udc_fifo_cfg	*fifo_cfg;
	unsigned			fifo_cfg_size;

	/* USB configuration-specific details */
	unsigned			multipoint:1;	/* multipoint device */
	unsigned			dyn_fifo:1 __deprecated; /* supports dynamic fifo sizing */
	unsigned			soft_con:1 __deprecated; /* soft connect required */
	unsigned			utm_16:1 __deprecated; /* utm data witdh is 16 bits */
	unsigned			big_endian:1;	/* true if CPU uses big-endian */
	unsigned			mult_bulk_tx:1;	/* Tx ep required for multbulk pkts */
	unsigned			mult_bulk_rx:1;	/* Rx ep required for multbulk pkts */
	unsigned			high_iso_tx:1;	/* Tx ep required for HB iso */
	unsigned			high_iso_rx:1;	/* Rx ep required for HD iso */
	unsigned			dma:1 __deprecated; /* supports DMA */
	unsigned			vendor_req:1 __deprecated; /* vendor registers required */

	u8				num_eps;	/* number of endpoints _with_ ep0 */
	u8				dma_channels __deprecated; /* number of dma channels */
	u8				dyn_fifo_size;	/* dynamic size in bytes */
	u8				vendor_ctrl __deprecated; /* vendor control reg width */
	u8				vendor_stat __deprecated; /* vendor status reg witdh */
	u8				dma_req_chan __deprecated; /* bitmask for required dma channels */
	u8				ram_bits;	/* ram address size */

	struct sunxi_udc_hdrc_eps_bits *eps_bits __deprecated;
};

struct sunxi_udc_hdrc_platform_data {
	u8		mode;
	const char	*clock;
	int		(*set_vbus)(struct device *dev, int is_on);
	u8		power;
	u8		min_power;
	u8		potpgt;
	unsigned	extvbus:1;
	int		(*set_power)(int state);
	struct sunxi_udc_hdrc_config	*config;
	void		*board_data;
	const void	*platform_ops;
};

/*
 * U-Boot specfic stuff
 */
struct sunxi_udc *sunxi_udc_register(struct sunxi_udc_hdrc_platform_data *plat, void *bdata,
			   void *ctl_regs);

#endif /* __UDC_H */
