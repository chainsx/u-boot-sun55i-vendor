// SPDX-License-Identifier: GPL-2.0

#include <common.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <linux/bug.h>
#include <linux/usb/ch9.h>
#include <linux-compat.h>

#include "sunxi_udc_core.h"

/* ----------------------------------------------------------------------- */

#define is_buffer_mapped(req) (is_dma_capable() && \
					(req->map_state != UN_MAPPED))

static inline void map_dma_buffer(struct sunxi_udc_request *request,
			struct sunxi_udc *sunxi_udc, struct sunxi_udc_ep *sunxi_udc_ep)
{
}

static inline void unmap_dma_buffer(struct sunxi_udc_request *request,
				struct sunxi_udc *sunxi_udc)
{
}

void sunxi_udc_g_giveback(
	struct sunxi_udc_ep		*ep,
	struct usb_request	*request,
	int			status)
__releases(ep->sunxi_udc->lock)
__acquires(ep->sunxi_udc->lock)
{
	struct sunxi_udc_request	*req;
	struct sunxi_udc		*sunxi_udc;
	int			busy = ep->busy;

	req = to_sunxi_udc_request(request);

	list_del(&req->list);
	if (req->request.status == -EINPROGRESS)
		req->request.status = status;
	sunxi_udc = req->sunxi_udc;

	ep->busy = 1;
	spin_unlock(&sunxi_udc->lock);
	unmap_dma_buffer(req, sunxi_udc);
	if (request->status == 0)
		dev_dbg(sunxi_udc->controller, "%s done request %p,  %d/%d\n",
				ep->end_point.name, request,
				req->request.actual, req->request.length);
	else
		dev_dbg(sunxi_udc->controller, "%s request %p, %d/%d fault %d\n",
				ep->end_point.name, request,
				req->request.actual, req->request.length,
				request->status);
	req->request.complete(&req->ep->end_point, &req->request);
	spin_lock(&sunxi_udc->lock);
	ep->busy = busy;
}

static void nuke(struct sunxi_udc_ep *ep, const int status)
{
	struct sunxi_udc		*sunxi_udc = ep->sunxi_udc;
	struct sunxi_udc_request	*req = NULL;
	void __iomem *epio = ep->sunxi_udc->endpoints[ep->current_epnum].regs;

	ep->busy = 1;

	if (is_dma_capable() && ep->dma) {
		struct dma_controller	*c = ep->sunxi_udc->dma_controller;
		int value;

		if (ep->is_in) {
			usb_writew(epio, USB_TXCSR,
				    USB_TXCSR_DMAMODE | USB_TXCSR_FLUSHFIFO);
			usb_writew(epio, USB_TXCSR,
					0 | USB_TXCSR_FLUSHFIFO);
		} else {
			usb_writew(epio, USB_RXCSR,
					0 | USB_RXCSR_FLUSHFIFO);
			usb_writew(epio, USB_RXCSR,
					0 | USB_RXCSR_FLUSHFIFO);
		}

		value = c->channel_abort(ep->dma);
		dev_dbg(sunxi_udc->controller, "%s: abort DMA --> %d\n",
				ep->name, value);
		c->channel_release(ep->dma);
		ep->dma = NULL;
	}

	while (!list_empty(&ep->req_list)) {
		req = list_first_entry(&ep->req_list, struct sunxi_udc_request, list);
		sunxi_udc_g_giveback(ep, &req->request, status);
	}
}

static inline int max_ep_writesize(struct sunxi_udc *sunxi_udc, struct sunxi_udc_ep *ep)
{
	if (is_bulk_split(sunxi_udc, ep->type))
		return ep->hw_ep->max_packet_sz_tx;
	else
		return ep->packet_sz;
}

static void txstate(struct sunxi_udc *sunxi_udc, struct sunxi_udc_request *req)
{
	u8			epnum = req->epnum;
	struct sunxi_udc_ep	*sunxi_udc_ep;
	void __iomem		*epio = sunxi_udc->endpoints[epnum].regs;
	struct usb_request	*request;
	u16			fifo_count = 0, csr;
	int			use_dma = 0;

	sunxi_udc_ep = req->ep;

	if (!sunxi_udc_ep->desc) {
		dev_dbg(sunxi_udc->controller, "ep:%s disabled - ignore request\n",
						sunxi_udc_ep->end_point.name);
		return;
	}

	if (dma_channel_status(sunxi_udc_ep->dma) == USB_DMA_STATUS_BUSY) {
		dev_dbg(sunxi_udc->controller, "dma pending...\n");
		return;
	}

	csr = usb_readw(epio, USB_TXCSR);

	request = &req->request;
	fifo_count = min(max_ep_writesize(sunxi_udc, sunxi_udc_ep),
			(int)(request->length - request->actual));

	if (csr & USB_TXCSR_TXPKTRDY) {
		dev_dbg(sunxi_udc->controller, "%s old packet still ready , txcsr %03x\n",
				sunxi_udc_ep->end_point.name, csr);
		return;
	}

	if (csr & USB_TXCSR_SENDSTALL) {
		dev_dbg(sunxi_udc->controller, "%s stalling, txcsr %03x\n",
				sunxi_udc_ep->end_point.name, csr);
		return;
	}

	dev_dbg(sunxi_udc->controller, "hw_ep%d, maxpacket %d, fifo count %d, txcsr %03x\n",
			epnum, sunxi_udc_ep->packet_sz, fifo_count,
			csr);

	if (!use_dma) {
		unmap_dma_buffer(req, sunxi_udc);

		usb_write_fifo(sunxi_udc_ep->hw_ep, fifo_count,
				(u8 *) (request->buf + request->actual));
		request->actual += fifo_count;
		csr |= USB_TXCSR_TXPKTRDY;
		csr &= ~USB_TXCSR_UNDERRUN;
		usb_writew(epio, USB_TXCSR, csr);
	}

	dev_dbg(sunxi_udc->controller, "%s TX/IN %s len %d/%d, txcsr %04x, fifo %d/%d\n",
			sunxi_udc_ep->end_point.name, use_dma ? "dma" : "pio",
			request->actual, request->length,
			usb_readw(epio, USB_TXCSR),
			fifo_count,
			usb_readw(epio, USB_TXMAXP));
}

void sunxi_udc_g_tx(struct sunxi_udc *sunxi_udc, u8 epnum)
{
	u16			csr;
	struct sunxi_udc_request	*req;
	struct usb_request	*request;
	u8 __iomem		*mbase = sunxi_udc->regs;
	struct sunxi_udc_ep	*sunxi_udc_ep = &sunxi_udc->endpoints[epnum].ep_in;
	void __iomem		*epio = sunxi_udc->endpoints[epnum].regs;
	struct dma_channel	*dma;

	sunxi_udc_ep_select(mbase, epnum);
	req = next_request(sunxi_udc_ep);
	request = &req->request;

	csr = usb_readw(epio, USB_TXCSR);
	dev_dbg(sunxi_udc->controller, "<== %s, txcsr %04x\n", sunxi_udc_ep->end_point.name, csr);

	dma = is_dma_capable() ? sunxi_udc_ep->dma : NULL;

	if (csr & USB_TXCSR_SENTSTALL) {
		csr |=	USB_TXCSR_P_WZC_BITS;
		csr &= ~USB_TXCSR_SENTSTALL;
		usb_writew(epio, USB_TXCSR, csr);
		return;
	}

	if (csr & USB_TXCSR_UNDERRUN) {
		csr |=	 USB_TXCSR_P_WZC_BITS;
		csr &= ~(USB_TXCSR_UNDERRUN | USB_TXCSR_TXPKTRDY);
		usb_writew(epio, USB_TXCSR, csr);
		dev_vdbg(sunxi_udc->controller, "underrun on ep%d, req %p\n",
				epnum, request);
	}

	if (dma_channel_status(dma) == USB_DMA_STATUS_BUSY) {
		dev_dbg(sunxi_udc->controller, "%s dma still busy?\n", sunxi_udc_ep->end_point.name);
		return;
	}

	if (request) {
		u8	is_dma = 0;

		if (dma && (csr & USB_TXCSR_DMAREQENAB)) {
			is_dma = 1;
			csr |= USB_TXCSR_P_WZC_BITS;
			csr &= ~(USB_TXCSR_DMAREQENAB | USB_TXCSR_UNDERRUN |
				 USB_TXCSR_TXPKTRDY | USB_TXCSR_AUTOSET);
			usb_writew(epio, USB_TXCSR, csr);
			/* Ensure writebuffer is empty. */
			csr = usb_readw(epio, USB_TXCSR);
			request->actual += sunxi_udc_ep->dma->actual_len;
			dev_dbg(sunxi_udc->controller, "TXCSR%d %04x, DMA off, len %zu, req %p\n",
				epnum, csr, sunxi_udc_ep->dma->actual_len, request);
		}

		if ((request->zero && request->length
			&& (request->length % sunxi_udc_ep->packet_sz == 0)
			&& (request->actual == request->length))

		) {
			if (csr & USB_TXCSR_TXPKTRDY)
				return;

			dev_dbg(sunxi_udc->controller, "sending zero pkt\n");
			usb_writew(epio, USB_TXCSR, USB_TXCSR_MODE
					| USB_TXCSR_TXPKTRDY);
			request->zero = 0;
		}

		if (request->actual == request->length) {
			sunxi_udc_g_giveback(sunxi_udc_ep, request, 0);
			sunxi_udc_ep_select(mbase, epnum);
			req = sunxi_udc_ep->desc ? next_request(sunxi_udc_ep) : NULL;
			if (!req) {
				dev_dbg(sunxi_udc->controller, "%s idle now\n",
					sunxi_udc_ep->end_point.name);
				return;
			}
		}

		txstate(sunxi_udc, req);
	}
}

static void rxstate(struct sunxi_udc *sunxi_udc, struct sunxi_udc_request *req)
{
	const u8		epnum = req->epnum;
	struct usb_request	*request = &req->request;
	struct sunxi_udc_ep		*sunxi_udc_ep;
	void __iomem		*epio = sunxi_udc->endpoints[epnum].regs;
	unsigned		fifo_count = 0;
	u16			len;
	u16			csr = usb_readw(epio, USB_RXCSR);
	struct sunxi_udc_hw_ep	*hw_ep = &sunxi_udc->endpoints[epnum];
	u8			use_mode_1;

	if (hw_ep->is_shared_fifo)
		sunxi_udc_ep = &hw_ep->ep_in;
	else
		sunxi_udc_ep = &hw_ep->ep_out;

	len = sunxi_udc_ep->packet_sz;

	if (!sunxi_udc_ep->desc) {
		dev_dbg(sunxi_udc->controller, "ep:%s disabled - ignore request\n",
						sunxi_udc_ep->end_point.name);
		return;
	}

	if (dma_channel_status(sunxi_udc_ep->dma) == USB_DMA_STATUS_BUSY) {
		dev_dbg(sunxi_udc->controller, "DMA pending...\n");
		return;
	}

	if (csr & USB_RXCSR_SENDSTALL) {
		dev_dbg(sunxi_udc->controller, "%s stalling, RXCSR %04x\n",
		    sunxi_udc_ep->end_point.name, csr);
		return;
	}

	if (is_cppi_enabled() && is_buffer_mapped(req)) {
		struct dma_controller	*c = sunxi_udc->dma_controller;
		struct dma_channel	*channel = sunxi_udc_ep->dma;

		if (c->channel_program(channel,
				sunxi_udc_ep->packet_sz,
				!request->short_not_ok,
				request->dma + request->actual,
				request->length - request->actual)) {

			csr &= ~(USB_RXCSR_AUTOCLEAR
					| USB_RXCSR_DMAMODE);
			csr |= USB_RXCSR_DMAREQENAB | USB_RXCSR_P_WZC_BITS;
			usb_writew(epio, USB_RXCSR, csr);
			return;
		}
	}

	if (csr & USB_RXCSR_RXPKTRDY) {
		len = usb_readw(epio, USB_RXCOUNT);

		if (request->short_not_ok && len == sunxi_udc_ep->packet_sz)
			use_mode_1 = 1;
		else
			use_mode_1 = 0;

		if (request->actual < request->length) {
			fifo_count = request->length - request->actual;
			dev_dbg(sunxi_udc->controller, "%s OUT/RX pio fifo %d/%d, maxpacket %d\n",
					sunxi_udc_ep->end_point.name,
					len, fifo_count,
					sunxi_udc_ep->packet_sz);

			fifo_count = min_t(unsigned, len, fifo_count);

			 if (is_buffer_mapped(req)) {
				unmap_dma_buffer(req, sunxi_udc);
				csr &= ~(USB_RXCSR_DMAREQENAB | USB_RXCSR_AUTOCLEAR);
				usb_writew(epio, USB_RXCSR, csr);
			}

			usb_read_fifo(sunxi_udc_ep->hw_ep, fifo_count, (u8 *)
					(request->buf + request->actual));
			request->actual += fifo_count;

			csr |= USB_RXCSR_P_WZC_BITS;
			csr &= ~USB_RXCSR_RXPKTRDY;
			usb_writew(epio, USB_RXCSR, csr);
		}
	}

	/* reach the end or short packet detected */
	if (request->actual == request->length || len < sunxi_udc_ep->packet_sz)
		sunxi_udc_g_giveback(sunxi_udc_ep, request, 0);
}

void sunxi_udc_g_rx(struct sunxi_udc *sunxi_udc, u8 epnum)
{
	u16			csr;
	struct sunxi_udc_request	*req;
	struct usb_request	*request;
	void __iomem		*mbase = sunxi_udc->regs;
	struct sunxi_udc_ep		*sunxi_udc_ep;
	void __iomem		*epio = sunxi_udc->endpoints[epnum].regs;
	struct dma_channel	*dma;
	struct sunxi_udc_hw_ep	*hw_ep = &sunxi_udc->endpoints[epnum];

	if (hw_ep->is_shared_fifo)
		sunxi_udc_ep = &hw_ep->ep_in;
	else
		sunxi_udc_ep = &hw_ep->ep_out;

	sunxi_udc_ep_select(mbase, epnum);

	req = next_request(sunxi_udc_ep);
	if (!req)
		return;

	request = &req->request;

	csr = usb_readw(epio, USB_RXCSR);
	dma = is_dma_capable() ? sunxi_udc_ep->dma : NULL;

	dev_dbg(sunxi_udc->controller, "<== %s, rxcsr %04x%s %p\n", sunxi_udc_ep->end_point.name,
			csr, dma ? " (dma)" : "", request);

	if (csr & USB_RXCSR_SENTSTALL) {
		csr |= USB_RXCSR_P_WZC_BITS;
		csr &= ~USB_RXCSR_SENTSTALL;
		usb_writew(epio, USB_RXCSR, csr);
		return;
	}

	if (csr & USB_RXCSR_OVERRUN) {
		csr &= ~USB_RXCSR_OVERRUN;
		usb_writew(epio, USB_RXCSR, csr);

		dev_dbg(sunxi_udc->controller, "%s iso overrun on %p\n", sunxi_udc_ep->name, request);
		if (request->status == -EINPROGRESS)
			request->status = -EOVERFLOW;
	}
	if (csr & USB_RXCSR_INCOMPRX) {
		dev_dbg(sunxi_udc->controller, "%s, incomprx\n", sunxi_udc_ep->end_point.name);
	}

	if (dma_channel_status(dma) == USB_DMA_STATUS_BUSY) {
		dev_dbg(sunxi_udc->controller, "%s busy, csr %04x\n",
			sunxi_udc_ep->end_point.name, csr);
		return;
	}

	if (dma && (csr & USB_RXCSR_DMAREQENAB)) {
		csr &= ~(USB_RXCSR_AUTOCLEAR
				| USB_RXCSR_DMAREQENAB
				| USB_RXCSR_DMAMODE);
		usb_writew(epio, USB_RXCSR,
			USB_RXCSR_P_WZC_BITS | csr);

		request->actual += sunxi_udc_ep->dma->actual_len;

		dev_dbg(sunxi_udc->controller, "RXCSR%d %04x, dma off, %04x, len %zu, req %p\n",
			epnum, csr,
			usb_readw(epio, USB_RXCSR),
			sunxi_udc_ep->dma->actual_len, request);

		sunxi_udc_g_giveback(sunxi_udc_ep, request, 0);
		sunxi_udc_ep_select(mbase, epnum);

		req = next_request(sunxi_udc_ep);
		if (!req)
			return;
	}

	rxstate(sunxi_udc, req);
}

/* ------------------------------------------------------------ */

static int sunxi_udc_gadget_enable(struct usb_ep *ep,
			const struct usb_endpoint_descriptor *desc)
{
	unsigned long		flags;
	struct sunxi_udc_ep		*sunxi_udc_ep;
	struct sunxi_udc_hw_ep	*hw_ep;
	void __iomem		*regs;
	struct sunxi_udc		*sunxi_udc;
	void __iomem	*mbase;
	u8		epnum;
	u16		csr;
	unsigned	tmp;
	int		status = -EINVAL;

	if (!ep || !desc)
		return -EINVAL;

	sunxi_udc_ep = to_sunxi_udc_ep(ep);
	hw_ep = sunxi_udc_ep->hw_ep;
	regs = hw_ep->regs;
	sunxi_udc = sunxi_udc_ep->sunxi_udc;
	mbase = sunxi_udc->regs;
	epnum = sunxi_udc_ep->current_epnum;

	spin_lock_irqsave(&sunxi_udc->lock, flags);

	if (sunxi_udc_ep->desc) {
		status = -EBUSY;
		goto fail;
	}
	sunxi_udc_ep->type = usb_endpoint_type(desc);

	if (usb_endpoint_num(desc) != epnum)
		goto fail;

	tmp = usb_endpoint_maxp(desc);
	if (tmp & ~0x07ff) {
		int ok;

		if (usb_endpoint_dir_in(desc))
			ok = sunxi_udc->hb_iso_tx;
		else
			ok = sunxi_udc->hb_iso_rx;

		if (!ok) {
			dev_dbg(sunxi_udc->controller, "no support for high bandwidth ISO\n");
			goto fail;
		}
		sunxi_udc_ep->hb_mult = (tmp >> 11) & 3;
	} else {
		sunxi_udc_ep->hb_mult = 0;
	}

	sunxi_udc_ep->packet_sz = tmp & 0x7ff;
	tmp = sunxi_udc_ep->packet_sz * (sunxi_udc_ep->hb_mult + 1);

	sunxi_udc_ep_select(mbase, epnum);
	if (usb_endpoint_dir_in(desc)) {
		u16 int_txe = usb_readw(mbase, USB_INTRTXE);

		if (hw_ep->is_shared_fifo)
			sunxi_udc_ep->is_in = 1;
		if (!sunxi_udc_ep->is_in)
			goto fail;

		if (tmp > hw_ep->max_packet_sz_tx) {
			dev_dbg(sunxi_udc->controller, "packet size beyond hardware FIFO size\n");
			goto fail;
		}

		int_txe |= (1 << epnum);
		usb_writew(mbase, USB_INTRTXE, int_txe);

		if (sunxi_udc->double_buffer_not_ok)
			usb_writew(regs, USB_TXMAXP, hw_ep->max_packet_sz_tx);
		else
			usb_writew(regs, USB_TXMAXP, sunxi_udc_ep->packet_sz
					| (sunxi_udc_ep->hb_mult << 11));

		csr = USB_TXCSR_MODE | USB_TXCSR_CLRDATATOG;
		if (usb_readw(regs, USB_TXCSR)
				& USB_TXCSR_FIFONOTEMPTY)
			csr |= USB_TXCSR_FLUSHFIFO;
		if (sunxi_udc_ep->type == USB_ENDPOINT_XFER_ISOC)
			csr |= USB_TXCSR_ISO;

		usb_writew(regs, USB_TXCSR, csr);
		usb_writew(regs, USB_TXCSR, csr);

	} else {
		u16 int_rxe = usb_readw(mbase, USB_INTRRXE);

		if (hw_ep->is_shared_fifo)
			sunxi_udc_ep->is_in = 0;
		if (sunxi_udc_ep->is_in)
			goto fail;

		if (tmp > hw_ep->max_packet_sz_rx) {
			dev_dbg(sunxi_udc->controller, "packet size beyond hardware FIFO size\n");
			goto fail;
		}

		int_rxe |= (1 << epnum);
		usb_writew(mbase, USB_INTRRXE, int_rxe);

		if (sunxi_udc->double_buffer_not_ok)
			usb_writew(regs, USB_RXMAXP, hw_ep->max_packet_sz_tx);
		else
			usb_writew(regs, USB_RXMAXP, sunxi_udc_ep->packet_sz
					| (sunxi_udc_ep->hb_mult << 11));

		if (hw_ep->is_shared_fifo) {
			csr = usb_readw(regs, USB_TXCSR);
			csr &= ~(USB_TXCSR_MODE | USB_TXCSR_TXPKTRDY);
			usb_writew(regs, USB_TXCSR, csr);
		}

		csr = USB_RXCSR_FLUSHFIFO | USB_RXCSR_CLRDATATOG;
		if (sunxi_udc_ep->type == USB_ENDPOINT_XFER_ISOC)
			csr |= USB_RXCSR_ISO;
		else if (sunxi_udc_ep->type == USB_ENDPOINT_XFER_INT)
			csr |= USB_RXCSR_DISNYET;

		usb_writew(regs, USB_RXCSR, csr);
		usb_writew(regs, USB_RXCSR, csr);
	}

	if (is_dma_capable() && sunxi_udc->dma_controller) {
		struct dma_controller	*c = sunxi_udc->dma_controller;

		sunxi_udc_ep->dma = c->channel_alloc(c, hw_ep,
				(desc->bEndpointAddress & USB_DIR_IN));
	} else
		sunxi_udc_ep->dma = NULL;

	sunxi_udc_ep->end_point.desc = desc;
	sunxi_udc_ep->desc = desc;
	sunxi_udc_ep->busy = 0;
	sunxi_udc_ep->wedged = 0;
	status = 0;

	pr_debug("%s periph: enabled %s for %s, %smaxpacket %d\n",
			sunxi_udc_driver_name, sunxi_udc_ep->end_point.name,
			sunxi_udc_ep->is_in ? "IN" : "OUT",
			sunxi_udc_ep->dma ? "dma, " : "",
			sunxi_udc_ep->packet_sz);

	schedule_work(&sunxi_udc->irq_work);

fail:
	spin_unlock_irqrestore(&sunxi_udc->lock, flags);
	return status;
}

static int sunxi_udc_gadget_disable(struct usb_ep *ep)
{
	unsigned long	flags;
	struct sunxi_udc	*sunxi_udc;
	u8		epnum;
	struct sunxi_udc_ep	*sunxi_udc_ep;
	void __iomem	*epio;
	int		status = 0;

	sunxi_udc_ep = to_sunxi_udc_ep(ep);
	sunxi_udc = sunxi_udc_ep->sunxi_udc;
	epnum = sunxi_udc_ep->current_epnum;
	epio = sunxi_udc->endpoints[epnum].regs;

	spin_lock_irqsave(&sunxi_udc->lock, flags);
	sunxi_udc_ep_select(sunxi_udc->regs, epnum);

	if (sunxi_udc_ep->is_in) {
		u16 int_txe = usb_readw(sunxi_udc->regs, USB_INTRTXE);
		int_txe &= ~(1 << epnum);
		usb_writew(sunxi_udc->regs, USB_INTRTXE, int_txe);
		usb_writew(epio, USB_TXMAXP, 0);
	} else {
		u16 int_rxe = usb_readw(sunxi_udc->regs, USB_INTRRXE);
		int_rxe &= ~(1 << epnum);
		usb_writew(sunxi_udc->regs, USB_INTRRXE, int_rxe);
		usb_writew(epio, USB_RXMAXP, 0);
	}

	sunxi_udc_ep->desc = NULL;
	sunxi_udc_ep->end_point.desc = NULL;

	/* abort all pending DMA and requests */
	nuke(sunxi_udc_ep, -ESHUTDOWN);

	schedule_work(&sunxi_udc->irq_work);

	spin_unlock_irqrestore(&(sunxi_udc->lock), flags);

	dev_dbg(sunxi_udc->controller, "%s\n", sunxi_udc_ep->end_point.name);

	return status;
}

struct usb_request *sunxi_udc_alloc_request(struct usb_ep *ep, gfp_t gfp_flags)
{
	struct sunxi_udc_ep		*sunxi_udc_ep = to_sunxi_udc_ep(ep);
	struct sunxi_udc		*sunxi_udc = sunxi_udc_ep->sunxi_udc;
	struct sunxi_udc_request	*request = NULL;

	request = kzalloc(sizeof *request, gfp_flags);
	if (!request) {
		dev_dbg(sunxi_udc->controller, "not enough memory\n");
		return NULL;
	}

	request->request.dma = DMA_ADDR_INVALID;
	request->epnum = sunxi_udc_ep->current_epnum;
	request->ep = sunxi_udc_ep;

	return &request->request;
}

void sunxi_udc_free_request(struct usb_ep *ep, struct usb_request *req)
{
	kfree(to_sunxi_udc_request(req));
}

static LIST_HEAD(buffers);

struct free_record {
	struct list_head	list;
	struct device		*dev;
	unsigned		bytes;
	dma_addr_t		dma;
};

void sunxi_udc_ep_restart(struct sunxi_udc *sunxi_udc, struct sunxi_udc_request *req)
{
	dev_dbg(sunxi_udc->controller, "<== %s request %p len %u on hw_ep%d\n",
		req->tx ? "TX/IN" : "RX/OUT",
		&req->request, req->request.length, req->epnum);

	sunxi_udc_ep_select(sunxi_udc->regs, req->epnum);
	if (req->tx)
		txstate(sunxi_udc, req);
	else
		rxstate(sunxi_udc, req);
}

static int sunxi_udc_gadget_queue(struct usb_ep *ep, struct usb_request *req,
			gfp_t gfp_flags)
{
	struct sunxi_udc_ep		*sunxi_udc_ep;
	struct sunxi_udc_request	*request;
	struct sunxi_udc		*sunxi_udc;
	int			status = 0;
	unsigned long		lockflags;

	if (!ep || !req)
		return -EINVAL;
	if (!req->buf)
		return -ENODATA;

	sunxi_udc_ep = to_sunxi_udc_ep(ep);
	sunxi_udc = sunxi_udc_ep->sunxi_udc;

	request = to_sunxi_udc_request(req);
	request->sunxi_udc = sunxi_udc;

	if (request->ep != sunxi_udc_ep)
		return -EINVAL;

	dev_dbg(sunxi_udc->controller, "<== to %s request=%p\n", ep->name, req);

	request->request.actual = 0;
	request->request.status = -EINPROGRESS;
	request->epnum = sunxi_udc_ep->current_epnum;
	request->tx = sunxi_udc_ep->is_in;

	map_dma_buffer(request, sunxi_udc, sunxi_udc_ep);

	spin_lock_irqsave(&sunxi_udc->lock, lockflags);

	/* don't queue if the ep is down */
	if (!sunxi_udc_ep->desc) {
		dev_dbg(sunxi_udc->controller, "req %p queued to %s while ep %s\n",
				req, ep->name, "disabled");
		status = -ESHUTDOWN;
		goto cleanup;
	}

	/* add request to the list */
	list_add_tail(&request->list, &sunxi_udc_ep->req_list);

	/* it this is the head of the queue, start i/o ... */
	if (!sunxi_udc_ep->busy && &request->list == sunxi_udc_ep->req_list.next)
		sunxi_udc_ep_restart(sunxi_udc, request);

cleanup:
	spin_unlock_irqrestore(&sunxi_udc->lock, lockflags);
	return status;
}

static int sunxi_udc_gadget_dequeue(struct usb_ep *ep, struct usb_request *request)
{
	struct sunxi_udc_ep		*sunxi_udc_ep = to_sunxi_udc_ep(ep);
	struct sunxi_udc_request	*req = to_sunxi_udc_request(request);
	struct sunxi_udc_request	*r;
	unsigned long		flags;
	int			status = 0;
	struct sunxi_udc		*sunxi_udc = sunxi_udc_ep->sunxi_udc;

	if (!ep || !request || to_sunxi_udc_request(request)->ep != sunxi_udc_ep)
		return -EINVAL;

	spin_lock_irqsave(&sunxi_udc->lock, flags);

	list_for_each_entry(r, &sunxi_udc_ep->req_list, list) {
		if (r == req)
			break;
	}
	if (r != req) {
		dev_dbg(sunxi_udc->controller, "request %p not queued to %s\n", request, ep->name);
		status = -EINVAL;
		goto done;
	}

	if (sunxi_udc_ep->req_list.next != &req->list || sunxi_udc_ep->busy)
		sunxi_udc_g_giveback(sunxi_udc_ep, request, -ECONNRESET);

	else if (is_dma_capable() && sunxi_udc_ep->dma) {
		struct dma_controller	*c = sunxi_udc->dma_controller;

		sunxi_udc_ep_select(sunxi_udc->regs, sunxi_udc_ep->current_epnum);
		if (c->channel_abort)
			status = c->channel_abort(sunxi_udc_ep->dma);
		else
			status = -EBUSY;
		if (status == 0)
			sunxi_udc_g_giveback(sunxi_udc_ep, request, -ECONNRESET);
	} else {
		sunxi_udc_g_giveback(sunxi_udc_ep, request, -ECONNRESET);
	}

done:
	spin_unlock_irqrestore(&sunxi_udc->lock, flags);
	return status;
}

static int sunxi_udc_gadget_set_halt(struct usb_ep *ep, int value)
{
	struct sunxi_udc_ep		*sunxi_udc_ep = to_sunxi_udc_ep(ep);
	u8			epnum = sunxi_udc_ep->current_epnum;
	struct sunxi_udc		*sunxi_udc = sunxi_udc_ep->sunxi_udc;
	void __iomem		*epio = sunxi_udc->endpoints[epnum].regs;
	void __iomem		*mbase;
	unsigned long		flags;
	u16			csr;
	struct sunxi_udc_request	*request;
	int			status = 0;

	if (!ep)
		return -EINVAL;
	mbase = sunxi_udc->regs;

	spin_lock_irqsave(&sunxi_udc->lock, flags);

	if ((USB_ENDPOINT_XFER_ISOC == sunxi_udc_ep->type)) {
		status = -EINVAL;
		goto done;
	}

	sunxi_udc_ep_select(mbase, epnum);

	request = next_request(sunxi_udc_ep);
	if (value) {
		if (request) {
			dev_dbg(sunxi_udc->controller, "request in progress, cannot halt %s\n",
			    ep->name);
			status = -EAGAIN;
			goto done;
		}
		/* Cannot portably stall with non-empty FIFO */
		if (sunxi_udc_ep->is_in) {
			csr = usb_readw(epio, USB_TXCSR);
			if (csr & USB_TXCSR_FIFONOTEMPTY) {
				dev_dbg(sunxi_udc->controller, "FIFO busy, cannot halt %s\n", ep->name);
				status = -EAGAIN;
				goto done;
			}
		}
	} else
		sunxi_udc_ep->wedged = 0;

	dev_dbg(sunxi_udc->controller, "%s: %s stall\n", ep->name, value ? "set" : "clear");
	if (sunxi_udc_ep->is_in) {
		csr = usb_readw(epio, USB_TXCSR);
		csr |= USB_TXCSR_P_WZC_BITS
			| USB_TXCSR_CLRDATATOG;
		if (value)
			csr |= USB_TXCSR_SENDSTALL;
		else
			csr &= ~(USB_TXCSR_SENDSTALL
				| USB_TXCSR_SENTSTALL);
		csr &= ~USB_TXCSR_TXPKTRDY;
		usb_writew(epio, USB_TXCSR, csr);
	} else {
		csr = usb_readw(epio, USB_RXCSR);
		csr |= USB_RXCSR_P_WZC_BITS
			| USB_RXCSR_FLUSHFIFO
			| USB_RXCSR_CLRDATATOG;
		if (value)
			csr |= USB_RXCSR_SENDSTALL;
		else
			csr &= ~(USB_RXCSR_SENDSTALL
				| USB_RXCSR_SENTSTALL);
		usb_writew(epio, USB_RXCSR, csr);
	}

	/* maybe start the first request in the queue */
	if (!sunxi_udc_ep->busy && !value && request) {
		dev_dbg(sunxi_udc->controller, "restarting the request\n");
		sunxi_udc_ep_restart(sunxi_udc, request);
	}

done:
	spin_unlock_irqrestore(&sunxi_udc->lock, flags);
	return status;
}

static int sunxi_udc_gadget_fifo_status(struct usb_ep *ep)
{
	struct sunxi_udc_ep		*sunxi_udc_ep = to_sunxi_udc_ep(ep);
	void __iomem		*epio = sunxi_udc_ep->hw_ep->regs;
	int			retval = -EINVAL;

	if (sunxi_udc_ep->desc && !sunxi_udc_ep->is_in) {
		struct sunxi_udc		*sunxi_udc = sunxi_udc_ep->sunxi_udc;
		int			epnum = sunxi_udc_ep->current_epnum;
		void __iomem		*mbase = sunxi_udc->regs;
		unsigned long		flags;

		spin_lock_irqsave(&sunxi_udc->lock, flags);

		sunxi_udc_ep_select(mbase, epnum);
		/* FIXME return zero unless RXPKTRDY is set */
		retval = usb_readw(epio, USB_RXCOUNT);

		spin_unlock_irqrestore(&sunxi_udc->lock, flags);
	}
	return retval;
}

static void sunxi_udc_gadget_fifo_flush(struct usb_ep *ep)
{
	struct sunxi_udc_ep	*sunxi_udc_ep = to_sunxi_udc_ep(ep);
	struct sunxi_udc	*sunxi_udc = sunxi_udc_ep->sunxi_udc;
	u8		epnum = sunxi_udc_ep->current_epnum;
	void __iomem	*epio = sunxi_udc->endpoints[epnum].regs;
	void __iomem	*mbase;
	unsigned long	flags;
	u16		csr, int_txe;

	mbase = sunxi_udc->regs;

	spin_lock_irqsave(&sunxi_udc->lock, flags);
	sunxi_udc_ep_select(mbase, (u8) epnum);

	/* disable interrupts */
	int_txe = usb_readw(mbase, USB_INTRTXE);
	usb_writew(mbase, USB_INTRTXE, int_txe & ~(1 << epnum));

	if (sunxi_udc_ep->is_in) {
		csr = usb_readw(epio, USB_TXCSR);
		if (csr & USB_TXCSR_FIFONOTEMPTY) {
			csr |= USB_TXCSR_FLUSHFIFO | USB_TXCSR_P_WZC_BITS;
			csr &= ~USB_TXCSR_TXPKTRDY;
			usb_writew(epio, USB_TXCSR, csr);
			usb_writew(epio, USB_TXCSR, csr);
		}
	} else {
		csr = usb_readw(epio, USB_RXCSR);
		csr |= USB_RXCSR_FLUSHFIFO | USB_RXCSR_P_WZC_BITS;
		usb_writew(epio, USB_RXCSR, csr);
		usb_writew(epio, USB_RXCSR, csr);
	}

	/* re-enable interrupt */
	usb_writew(mbase, USB_INTRTXE, int_txe);
	spin_unlock_irqrestore(&sunxi_udc->lock, flags);
}

static const struct usb_ep_ops sunxi_udc_ep_ops = {
	.enable		= sunxi_udc_gadget_enable,
	.disable	= sunxi_udc_gadget_disable,
	.alloc_request	= sunxi_udc_alloc_request,
	.free_request	= sunxi_udc_free_request,
	.queue		= sunxi_udc_gadget_queue,
	.dequeue	= sunxi_udc_gadget_dequeue,
	.set_halt	= sunxi_udc_gadget_set_halt,
	.fifo_status	= sunxi_udc_gadget_fifo_status,
	.fifo_flush	= sunxi_udc_gadget_fifo_flush
};

/* ----------------------------------------------------------------------- */

static int sunxi_udc_gadget_get_frame(struct usb_gadget *gadget)
{
	struct sunxi_udc	*sunxi_udc = gadget_to_sunxi_udc(gadget);

	return (int)usb_readw(sunxi_udc->regs, USB_FNUM);
}

static int sunxi_udc_gadget_wakeup(struct usb_gadget *gadget)
{
	return 0;
}

static int
sunxi_udc_gadget_set_self_powered(struct usb_gadget *gadget, int is_selfpowered)
{
	struct sunxi_udc	*sunxi_udc = gadget_to_sunxi_udc(gadget);

	sunxi_udc->is_self_powered = !!is_selfpowered;
	return 0;
}

static void sunxi_udc_pullup(struct sunxi_udc *sunxi_udc, int is_on)
{
	u8 power;

	power = usb_readb(sunxi_udc->regs, USB_GCS);
	if (is_on)
		power |= USB_GCS_SOFTCONN;
	else
		power &= ~USB_GCS_SOFTCONN;

	dev_dbg(sunxi_udc->controller, "gadget D+ pullup %s\n",
		is_on ? "on" : "off");
	usb_writeb(sunxi_udc->regs, USB_GCS, power);
}

static int sunxi_udc_gadget_vbus_session(struct usb_gadget *gadget, int is_active)
{
	struct sunxi_udc	*sunxi_udc = gadget_to_sunxi_udc(gadget);

	dev_dbg(sunxi_udc->controller, "<= %s =>\n", __func__);

	return -EINVAL;
}

static int sunxi_udc_gadget_vbus_draw(struct usb_gadget *gadget, unsigned mA)
{
	return 0;
}

static int sunxi_udc_gadget_pullup(struct usb_gadget *gadget, int is_on)
{
	struct sunxi_udc	*sunxi_udc = gadget_to_sunxi_udc(gadget);
	unsigned long	flags;

	is_on = !!is_on;

	pm_runtime_get_sync(sunxi_udc->controller);

	spin_lock_irqsave(&sunxi_udc->lock, flags);
	if (is_on != sunxi_udc->softconnect) {
		sunxi_udc->softconnect = is_on;
		sunxi_udc_pullup(sunxi_udc, is_on);
	}
	spin_unlock_irqrestore(&sunxi_udc->lock, flags);

	pm_runtime_put(sunxi_udc->controller);

	return 0;
}

static int sunxi_udc_gadget_stop(struct usb_gadget *g)
{
	struct sunxi_udc	*sunxi_udc = gadget_to_sunxi_udc(g);

	sunxi_udc_stop(sunxi_udc);
	return 0;
}

static const struct usb_gadget_ops sunxi_udc_gadget_operations = {
	.get_frame		= sunxi_udc_gadget_get_frame,
	.wakeup			= sunxi_udc_gadget_wakeup,
	.set_selfpowered	= sunxi_udc_gadget_set_self_powered,
	.vbus_session		= sunxi_udc_gadget_vbus_session,
	.vbus_draw		= sunxi_udc_gadget_vbus_draw,
	.pullup			= sunxi_udc_gadget_pullup,
	.udc_start		= sunxi_udc_gadget_start,
	.udc_stop		= sunxi_udc_gadget_stop,
};

static void __devinit
init_peripheral_ep(struct sunxi_udc *sunxi_udc, struct sunxi_udc_ep *ep, u8 epnum, int is_in)
{
	struct sunxi_udc_hw_ep	*hw_ep = sunxi_udc->endpoints + epnum;

	memset(ep, 0, sizeof *ep);

	ep->current_epnum = epnum;
	ep->sunxi_udc = sunxi_udc;
	ep->hw_ep = hw_ep;
	ep->is_in = is_in;

	INIT_LIST_HEAD(&ep->req_list);

	sprintf(ep->name, "ep%d%s", epnum,
			(!epnum || hw_ep->is_shared_fifo) ? "" : (
				is_in ? "in" : "out"));
	ep->end_point.name = ep->name;
	INIT_LIST_HEAD(&ep->end_point.ep_list);
	if (!epnum) {
		ep->end_point.maxpacket = 64;
		ep->end_point.ops = &sunxi_udc_g_ep0_ops;
		sunxi_udc->g.ep0 = &ep->end_point;
	} else {
		if (is_in)
			ep->end_point.maxpacket = hw_ep->max_packet_sz_tx;
		else
			ep->end_point.maxpacket = hw_ep->max_packet_sz_rx;
		ep->end_point.ops = &sunxi_udc_ep_ops;
		list_add_tail(&ep->end_point.ep_list, &sunxi_udc->g.ep_list);
	}
}

static inline void __devinit sunxi_udc_g_init_endpoints(struct sunxi_udc *sunxi_udc)
{
	u8			epnum;
	struct sunxi_udc_hw_ep	*hw_ep;
	unsigned		count = 0;

	/* initialize endpoint list just once */
	INIT_LIST_HEAD(&(sunxi_udc->g.ep_list));

	for (epnum = 0, hw_ep = sunxi_udc->endpoints;
			epnum < sunxi_udc->nr_endpoints;
			epnum++, hw_ep++) {
		if (hw_ep->is_shared_fifo /* || !epnum */) {
			init_peripheral_ep(sunxi_udc, &hw_ep->ep_in, epnum, 0);
			count++;
		} else {
			if (hw_ep->max_packet_sz_tx) {
				init_peripheral_ep(sunxi_udc, &hw_ep->ep_in,
							epnum, 1);
				count++;
			}
			if (hw_ep->max_packet_sz_rx) {
				init_peripheral_ep(sunxi_udc, &hw_ep->ep_out,
							epnum, 0);
				count++;
			}
		}
	}
}

int __devinit sunxi_udc_gadget_setup(struct sunxi_udc *sunxi_udc)
{
	sunxi_udc->g.ops = &sunxi_udc_gadget_operations;
	sunxi_udc->g.speed = USB_SPEED_UNKNOWN;
	sunxi_udc->g.name = sunxi_udc_driver_name;

	sunxi_udc_g_init_endpoints(sunxi_udc);

	sunxi_udc->is_active = 0;
	sunxi_udc_platform_try_idle(sunxi_udc, 0);

	return 0;
}

void sunxi_udc_gadget_cleanup(struct sunxi_udc *sunxi_udc)
{
	dev_dbg(sunxi_udc->controller, "<= %s =>\n", __func__);
}

int sunxi_udc_gadget_start(struct usb_gadget *g,
		struct usb_gadget_driver *driver)
{
	struct sunxi_udc	*sunxi_udc = gadget_to_sunxi_udc(g);
	unsigned long		flags;

	pm_runtime_get_sync(sunxi_udc->controller);

	sunxi_udc->softconnect = 0;
	sunxi_udc->gadget_driver = driver;

	spin_lock_irqsave(&sunxi_udc->lock, flags);
	sunxi_udc->is_active = 1;

	sunxi_udc_start(sunxi_udc);

	spin_unlock_irqrestore(&sunxi_udc->lock, flags);

	return 0;
}

void sunxi_udc_g_resume(struct sunxi_udc *sunxi_udc)
{
	dev_dbg(sunxi_udc->controller, "<= %s =>\n", __func__);
}

void sunxi_udc_g_suspend(struct sunxi_udc *sunxi_udc)
{
	dev_dbg(sunxi_udc->controller, "<= %s =>\n", __func__);
}

void sunxi_udc_g_wakeup(struct sunxi_udc *sunxi_udc)
{
	sunxi_udc_gadget_wakeup(&sunxi_udc->g);
}

void sunxi_udc_g_disconnect(struct sunxi_udc *sunxi_udc)
{
	(void) sunxi_udc_gadget_vbus_draw(&sunxi_udc->g, 0);

	sunxi_udc->g.speed = USB_SPEED_UNKNOWN;
	if (sunxi_udc->gadget_driver && sunxi_udc->gadget_driver->disconnect) {
		spin_unlock(&sunxi_udc->lock);
		sunxi_udc->gadget_driver->disconnect(&sunxi_udc->g);
		spin_lock(&sunxi_udc->lock);
	}

	sunxi_udc->is_active = 0;
}

void sunxi_udc_g_reset(struct sunxi_udc *sunxi_udc)
__releases(sunxi_udc->lock)
__acquires(sunxi_udc->lock)
{
	void __iomem	*mbase = sunxi_udc->regs;
	u8		power;

	if (sunxi_udc->g.speed != USB_SPEED_UNKNOWN)
		sunxi_udc_g_disconnect(sunxi_udc);

	power = usb_readb(mbase, USB_GCS);
	sunxi_udc->g.speed = (power & USB_GCS_HSFLAG)
			? USB_SPEED_HIGH : USB_SPEED_FULL;

	sunxi_udc->is_active = 1;
	sunxi_udc->is_suspended = 0;
	USB_DEV_MODE(sunxi_udc);
	sunxi_udc->address = 0;
	sunxi_udc->ep0_state = USB_EP0_STAGE_SETUP;

	sunxi_udc->may_wakeup = 0;
	sunxi_udc->g.b_hnp_enable = 0;
	sunxi_udc->g.a_alt_hnp_support = 0;
	sunxi_udc->g.a_hnp_support = 0;
}
