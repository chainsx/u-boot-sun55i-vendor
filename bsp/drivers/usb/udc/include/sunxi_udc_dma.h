/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __UDC_DMA_H__
#define __UDC_DMA_H__

struct sunxi_udc_hw_ep;

#define	DMA_ADDR_INVALID	(~(dma_addr_t)0)

#define	is_dma_capable()	(0)

#define	is_cppi_enabled()	0

enum dma_channel_status {
	USB_DMA_STATUS_UNKNOWN,
	USB_DMA_STATUS_FREE,
	USB_DMA_STATUS_BUSY,
	USB_DMA_STATUS_BUS_ABORT,
	USB_DMA_STATUS_CORE_ABORT
};

struct dma_controller;

struct dma_channel {
	void			*private_data;
	size_t			max_len;
	size_t			actual_len;
	enum dma_channel_status	status;
	bool			desired_mode;
};

static inline enum dma_channel_status
dma_channel_status(struct dma_channel *c)
{
	return (is_dma_capable() && c) ? c->status : USB_DMA_STATUS_UNKNOWN;
}

struct dma_controller {
	int			(*start)(struct dma_controller *);
	int			(*stop)(struct dma_controller *);
	struct dma_channel	*(*channel_alloc)(struct dma_controller *,
					struct sunxi_udc_hw_ep *, u8 is_tx);
	void			(*channel_release)(struct dma_channel *);
	int			(*channel_program)(struct dma_channel *channel,
							u16 maxpacket, u8 mode,
							dma_addr_t dma_addr,
							u32 length);
	int			(*channel_abort)(struct dma_channel *);
	int			(*is_compatible)(struct dma_channel *channel,
							u16 maxpacket,
							void *buf, u32 length);
};

extern void sunxi_udc_dma_completion(struct sunxi_udc *sunxi_udc, u8 epnum, u8 transmit);


extern struct dma_controller *__init
dma_controller_create(struct sunxi_udc *, void __iomem *);

extern void dma_controller_destroy(struct dma_controller *);

#endif	/* __UDC_DMA_H__ */
