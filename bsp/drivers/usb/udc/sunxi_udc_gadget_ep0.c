// SPDX-License-Identifier: GPL-2.0

#include <common.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <asm/processor.h>
#include <linux-compat.h>

#include "sunxi_udc_core.h"

#define	next_ep0_request(sunxi_udc)	next_in_request(&(sunxi_udc)->endpoints[0])

static char *decode_ep0stage(u8 stage)
{
	switch (stage) {
	case USB_EP0_STAGE_IDLE:	return "idle";
	case USB_EP0_STAGE_SETUP:	return "setup";
	case USB_EP0_STAGE_TX:		return "in";
	case USB_EP0_STAGE_RX:		return "out";
	case USB_EP0_STAGE_ACKWAIT:	return "wait";
	case USB_EP0_STAGE_STATUSIN:	return "in/status";
	case USB_EP0_STAGE_STATUSOUT:	return "out/status";
	default:			return "?";
	}
}

static int service_tx_status_request(
	struct sunxi_udc *sunxi_udc,
	const struct usb_ctrlrequest *ctrlrequest)
{
	void __iomem	*mbase = sunxi_udc->regs;
	int handled = 1;
	u8 result[2], epnum = 0;
	const u8 recip = ctrlrequest->bRequestType & USB_RECIP_MASK;

	result[1] = 0;

	switch (recip) {
	case USB_RECIP_DEVICE:
		result[0] = sunxi_udc->is_self_powered << USB_DEVICE_SELF_POWERED;
		result[0] |= sunxi_udc->may_wakeup << USB_DEVICE_REMOTE_WAKEUP;
		if (sunxi_udc->g.is_otg) {
			result[0] |= sunxi_udc->g.b_hnp_enable
				<< USB_DEVICE_B_HNP_ENABLE;
			result[0] |= sunxi_udc->g.a_alt_hnp_support
				<< USB_DEVICE_A_ALT_HNP_SUPPORT;
			result[0] |= sunxi_udc->g.a_hnp_support
				<< USB_DEVICE_A_HNP_SUPPORT;
		}
		break;

	case USB_RECIP_INTERFACE:
		result[0] = 0;
		break;

	case USB_RECIP_ENDPOINT: {
		int		is_in;
		struct sunxi_udc_ep	*ep;
		u16		tmp;
		void __iomem	*regs;

		epnum = (u8) ctrlrequest->wIndex;
		if (!epnum) {
			result[0] = 0;
			break;
		}

		if (epnum >= USB_C_NUM_EPS) {
			dev_err(sunxi_udc->controller, "error epnum\n");
			handled = -EINVAL;
			break;
		}

		is_in = epnum & USB_DIR_IN;
		if (is_in) {
			epnum &= 0x0f;
			ep = &sunxi_udc->endpoints[epnum].ep_in;
		} else {
			ep = &sunxi_udc->endpoints[epnum].ep_out;
		}
		regs = sunxi_udc->endpoints[epnum].regs;

		if (!ep->desc) {
			handled = -EINVAL;
			break;
		}

		sunxi_udc_ep_select(mbase, epnum);
		if (is_in)
			tmp = usb_readw(regs, USB_TXCSR)
						& USB_TXCSR_SENDSTALL;
		else
			tmp = usb_readw(regs, USB_RXCSR)
						& USB_RXCSR_SENDSTALL;
		sunxi_udc_ep_select(mbase, 0);

		result[0] = tmp ? 1 : 0;
		} break;

	default:
		/* class, vendor, etc ... delegate */
		handled = 0;
		break;
	}

	/* fill up the fifo; caller updates csr0 */
	if (handled > 0) {
		u16	len = le16_to_cpu(ctrlrequest->wLength);

		if (len > 2)
			len = 2;
		usb_write_fifo(&sunxi_udc->endpoints[0], len, result);
	}

	return handled;
}

static int
service_in_request(struct sunxi_udc *sunxi_udc, const struct usb_ctrlrequest *ctrlrequest)
{
	int handled = 0;	/* not handled */

	if ((ctrlrequest->bRequestType & USB_TYPE_MASK)
			== USB_TYPE_STANDARD) {
		switch (ctrlrequest->bRequest) {
		case USB_REQ_GET_STATUS:
			handled = service_tx_status_request(sunxi_udc,
					ctrlrequest);
			break;

		/* case USB_REQ_SYNC_FRAME: */

		default:
			break;
		}
	}
	return handled;
}

static void sunxi_udc_g_ep0_giveback(struct sunxi_udc *sunxi_udc, struct usb_request *req)
{
	sunxi_udc_g_giveback(&sunxi_udc->endpoints[0].ep_in, req, 0);
}

static int
service_zero_data_request(struct sunxi_udc *sunxi_udc,
		struct usb_ctrlrequest *ctrlrequest)
__releases(sunxi_udc->lock)
__acquires(sunxi_udc->lock)
{
	int handled = -EINVAL;
	void __iomem *mbase = sunxi_udc->regs;
	const u8 recip = ctrlrequest->bRequestType & USB_RECIP_MASK;

	if ((ctrlrequest->bRequestType & USB_TYPE_MASK)
			== USB_TYPE_STANDARD) {
		switch (ctrlrequest->bRequest) {
		case USB_REQ_SET_ADDRESS:
			sunxi_udc->set_address = true;
			sunxi_udc->address = (u8) (ctrlrequest->wValue & 0x7f);
			handled = 1;
			break;

		case USB_REQ_CLEAR_FEATURE:
			switch (recip) {
			case USB_RECIP_DEVICE:
				if (ctrlrequest->wValue
						!= USB_DEVICE_REMOTE_WAKEUP)
					break;
				sunxi_udc->may_wakeup = 0;
				handled = 1;
				break;
			case USB_RECIP_INTERFACE:
				break;
			case USB_RECIP_ENDPOINT:{
				const u8		epnum =
					ctrlrequest->wIndex & 0x0f;
				struct sunxi_udc_ep		*sunxi_udc_ep;
				struct sunxi_udc_hw_ep	*ep;
				struct sunxi_udc_request	*request;
				void __iomem		*regs;
				int			is_in;
				u16			csr;

				if (epnum == 0 || epnum >= USB_C_NUM_EPS ||
				    ctrlrequest->wValue != USB_ENDPOINT_HALT)
					break;

				ep = sunxi_udc->endpoints + epnum;
				regs = ep->regs;
				is_in = ctrlrequest->wIndex & USB_DIR_IN;
				if (is_in)
					sunxi_udc_ep = &ep->ep_in;
				else
					sunxi_udc_ep = &ep->ep_out;
				if (!sunxi_udc_ep->desc)
					break;

				handled = 1;
				if (sunxi_udc_ep->wedged)
					break;

				sunxi_udc_ep_select(mbase, epnum);
				if (is_in) {
					csr  = usb_readw(regs, USB_TXCSR);
					csr |= USB_TXCSR_CLRDATATOG |
					       USB_TXCSR_P_WZC_BITS;
					csr &= ~(USB_TXCSR_SENDSTALL |
						 USB_TXCSR_SENTSTALL |
						 USB_TXCSR_TXPKTRDY);
					usb_writew(regs, USB_TXCSR, csr);
				} else {
					csr  = usb_readw(regs, USB_RXCSR);
					csr |= USB_RXCSR_CLRDATATOG |
					       USB_RXCSR_P_WZC_BITS;
					csr &= ~(USB_RXCSR_SENDSTALL |
						 USB_RXCSR_SENTSTALL);
					usb_writew(regs, USB_RXCSR, csr);
				}

				/* Maybe start the first request in the queue */
				request = next_request(sunxi_udc_ep);
				if (!sunxi_udc_ep->busy && request) {
					dev_dbg(sunxi_udc->controller, "restarting the request\n");
					sunxi_udc_ep_restart(sunxi_udc, request);
				}

				sunxi_udc_ep_select(mbase, 0);
				} break;
			default:
				handled = 0;
				break;
			}
			break;

		case USB_REQ_SET_FEATURE:
			switch (recip) {
			case USB_RECIP_DEVICE:
				handled = 1;
				switch (ctrlrequest->wValue) {
				case USB_DEVICE_REMOTE_WAKEUP:
					sunxi_udc->may_wakeup = 1;
					break;
				case USB_DEVICE_TEST_MODE:
					if (sunxi_udc->g.speed != USB_SPEED_HIGH)
						goto stall;
					if (ctrlrequest->wIndex & 0xff)
						goto stall;

					switch (ctrlrequest->wIndex >> 8) {
					case 1:
						pr_debug("TEST_J\n");
						/* TEST_J */
						sunxi_udc->test_mode_nr =
							USB_TEST_J;
						break;
					case 2:
						/* TEST_K */
						pr_debug("TEST_K\n");
						sunxi_udc->test_mode_nr =
							USB_TEST_K;
						break;
					case 3:
						/* TEST_SE0_NAK */
						pr_debug("TEST_SE0_NAK\n");
						sunxi_udc->test_mode_nr =
							USB_TEST_SE0_NAK;
						break;
					case 4:
						/* TEST_PACKET */
						pr_debug("TEST_PACKET\n");
						sunxi_udc->test_mode_nr =
							USB_TEST_PACKET;
						break;

					case 0xc0:
						/* TEST_FORCE_HS */
						pr_debug("TEST_FORCE_HS\n");
						sunxi_udc->test_mode_nr =
							USB_TEST_FORCE_HS;
						break;
					case 0xc1:
						/* TEST_FORCE_FS */
						pr_debug("TEST_FORCE_FS\n");
						sunxi_udc->test_mode_nr =
							USB_TEST_FORCE_FS;
						break;
					case 0xc2:
						/* TEST_FIFO_ACCESS */
						pr_debug("TEST_FIFO_ACCESS\n");
						sunxi_udc->test_mode_nr =
							USB_TEST_FIFO_ACCESS;
						break;
					case 0xc3:
						/* TEST_FORCE_HOST */
						pr_debug("TEST_FORCE_HOST\n");
						sunxi_udc->test_mode_nr =
							USB_TEST_FORCE_HOST;
						break;
					default:
						goto stall;
					}

					/* enter test mode after irq */
					if (handled > 0)
						sunxi_udc->test_mode = true;
					break;

				case USB_DEVICE_B_HNP_ENABLE:
					if (!sunxi_udc->g.is_otg)
						goto stall;
					sunxi_udc->g.b_hnp_enable = 1;
					break;

				case USB_DEVICE_A_HNP_SUPPORT:
					if (!sunxi_udc->g.is_otg)
						goto stall;
					sunxi_udc->g.a_hnp_support = 1;
					break;
				case USB_DEVICE_A_ALT_HNP_SUPPORT:
					if (!sunxi_udc->g.is_otg)
						goto stall;
					sunxi_udc->g.a_alt_hnp_support = 1;
					break;
				case USB_DEVICE_DEBUG_MODE:
					handled = 0;
					break;
stall:
				default:
					handled = -EINVAL;
					break;
				}
				break;

			case USB_RECIP_INTERFACE:
				break;

			case USB_RECIP_ENDPOINT:{
				const u8		epnum =
					ctrlrequest->wIndex & 0x0f;
				struct sunxi_udc_ep		*sunxi_udc_ep;
				struct sunxi_udc_hw_ep	*ep;
				void __iomem		*regs;
				int			is_in;
				u16			csr;

				if (epnum == 0 || epnum >= USB_C_NUM_EPS ||
				    ctrlrequest->wValue	!= USB_ENDPOINT_HALT)
					break;

				ep = sunxi_udc->endpoints + epnum;
				regs = ep->regs;
				is_in = ctrlrequest->wIndex & USB_DIR_IN;
				if (is_in)
					sunxi_udc_ep = &ep->ep_in;
				else
					sunxi_udc_ep = &ep->ep_out;
				if (!sunxi_udc_ep->desc)
					break;

				sunxi_udc_ep_select(mbase, epnum);
				if (is_in) {
					csr = usb_readw(regs, USB_TXCSR);
					if (csr & USB_TXCSR_FIFONOTEMPTY)
						csr |= USB_TXCSR_FLUSHFIFO;
					csr |= USB_TXCSR_SENDSTALL
						| USB_TXCSR_CLRDATATOG
						| USB_TXCSR_P_WZC_BITS;
					usb_writew(regs, USB_TXCSR, csr);
				} else {
					csr = usb_readw(regs, USB_RXCSR);
					csr |= USB_RXCSR_SENDSTALL
						| USB_RXCSR_FLUSHFIFO
						| USB_RXCSR_CLRDATATOG
						| USB_RXCSR_P_WZC_BITS;
					usb_writew(regs, USB_RXCSR, csr);
				}

				sunxi_udc_ep_select(mbase, 0);
				handled = 1;
				} break;

			default:
				handled = 0;
				break;
			}
			break;
		default:
			handled = 0;
		}
	} else
		handled = 0;
	return handled;
}

static void ep0_rxstate(struct sunxi_udc *sunxi_udc)
{
	void __iomem		*regs = sunxi_udc->control_ep->regs;
	struct sunxi_udc_request	*request;
	struct usb_request	*req;
	u16			count, csr;

	request = next_ep0_request(sunxi_udc);
	req = &request->request;

	if (req) {
		void		*buf = req->buf + req->actual;
		unsigned	len = req->length - req->actual;

		/* read the buffer */
		count = usb_readb(regs, USB_COUNT0);
		if (count > len) {
			req->status = -EOVERFLOW;
			count = len;
		}
		usb_read_fifo(&sunxi_udc->endpoints[0], count, buf);
		req->actual += count;
		csr = USB_CSR0_SVDRXPKTRDY;
		if (count < 64 || req->actual == req->length) {
			sunxi_udc->ep0_state = USB_EP0_STAGE_STATUSIN;
			csr |= USB_CSR0_DATAEND;
		} else
			req = NULL;
	} else
		csr = USB_CSR0_SVDRXPKTRDY | USB_CSR0_SENDSTALL;


	if (req) {
		sunxi_udc->ackpend = csr;
		sunxi_udc_g_ep0_giveback(sunxi_udc, req);
		if (!sunxi_udc->ackpend)
			return;
		sunxi_udc->ackpend = 0;
	}
	sunxi_udc_ep_select(sunxi_udc->regs, 0);
	usb_writew(regs, USB_CSR0, csr);
}

static void ep0_txstate(struct sunxi_udc *sunxi_udc)
{
	void __iomem		*regs = sunxi_udc->control_ep->regs;
	struct sunxi_udc_request	*req = next_ep0_request(sunxi_udc);
	struct usb_request	*request;
	u16			csr = USB_CSR0_TXPKTRDY;
	u8			*fifo_src;
	u8			fifo_count;

	if (!req) {
		/* WARN_ON(1); */
		dev_dbg(sunxi_udc->controller, "odd; csr0 %04x\n", usb_readw(regs, USB_CSR0));
		return;
	}

	request = &req->request;

	/* load the data */
	fifo_src = (u8 *) request->buf + request->actual;
	fifo_count = min((unsigned) USB_EP0_FIFOSIZE,
		request->length - request->actual);
	usb_write_fifo(&sunxi_udc->endpoints[0], fifo_count, fifo_src);
	request->actual += fifo_count;

	/* update the flags */
	if (fifo_count < USB_MAX_END0_PACKET
			|| (request->actual == request->length
				&& !request->zero)) {
		sunxi_udc->ep0_state = USB_EP0_STAGE_STATUSOUT;
		csr |= USB_CSR0_DATAEND;
	} else
		request = NULL;

	sunxi_udc_ep_select(sunxi_udc->regs, 0);
	usb_writew(regs, USB_CSR0, csr);

	if (request) {
		sunxi_udc->ackpend = csr;
		sunxi_udc_g_ep0_giveback(sunxi_udc, request);
		if (!sunxi_udc->ackpend)
			return;
		sunxi_udc->ackpend = 0;
	}
}

static void
usb_read_setup(struct sunxi_udc *sunxi_udc, struct usb_ctrlrequest *req)
{
	struct sunxi_udc_request	*r;
	void __iomem		*regs = sunxi_udc->control_ep->regs;

	usb_read_fifo(&sunxi_udc->endpoints[0], sizeof *req, (u8 *)req);

	dev_dbg(sunxi_udc->controller, "SETUP req%02x.%02x v%04x i%04x l%d\n",
		req->bRequestType,
		req->bRequest,
		le16_to_cpu(req->wValue),
		le16_to_cpu(req->wIndex),
		le16_to_cpu(req->wLength));

	/* clean up any leftover transfers */
	r = next_ep0_request(sunxi_udc);
	if (r)
		sunxi_udc_g_ep0_giveback(sunxi_udc, &r->request);

	sunxi_udc->set_address = false;
	sunxi_udc->ackpend = USB_CSR0_SVDRXPKTRDY;
	if (req->wLength == 0) {
		if (req->bRequestType & USB_DIR_IN)
			sunxi_udc->ackpend |= USB_CSR0_TXPKTRDY;
		sunxi_udc->ep0_state = USB_EP0_STAGE_ACKWAIT;
	} else if (req->bRequestType & USB_DIR_IN) {
		sunxi_udc->ep0_state = USB_EP0_STAGE_TX;
		usb_writew(regs, USB_CSR0, USB_CSR0_SVDRXPKTRDY);
		while ((usb_readw(regs, USB_CSR0)
				& USB_CSR0_RXPKTRDY) != 0)
			cpu_relax();
		sunxi_udc->ackpend = 0;
	} else
		sunxi_udc->ep0_state = USB_EP0_STAGE_RX;
}

static int
forward_to_driver(struct sunxi_udc *sunxi_udc, const struct usb_ctrlrequest *ctrlrequest)
__releases(sunxi_udc->lock)
__acquires(sunxi_udc->lock)
{
	int retval;
	if (!sunxi_udc->gadget_driver)
		return -EOPNOTSUPP;
	spin_unlock(&sunxi_udc->lock);
	retval = sunxi_udc->gadget_driver->setup(&sunxi_udc->g, ctrlrequest);
	spin_lock(&sunxi_udc->lock);
	return retval;
}

irqreturn_t sunxi_udc_g_ep0_irq(struct sunxi_udc *sunxi_udc)
{
	u16		csr;
	u16		len;
	void __iomem	*mbase = sunxi_udc->regs;
	void __iomem	*regs = sunxi_udc->endpoints[0].regs;
	irqreturn_t	retval = IRQ_NONE;

	sunxi_udc_ep_select(mbase, 0);	/* select ep0 */
	csr = usb_readw(regs, USB_CSR0);
	len = usb_readb(regs, USB_COUNT0);

	dev_dbg(sunxi_udc->controller, "csr %04x, count %d, myaddr %d, ep0stage %s\n",
			csr, len,
			usb_readb(mbase, USB_FADDR),
			decode_ep0stage(sunxi_udc->ep0_state));

	if (csr & USB_CSR0_DATAEND) {
		return IRQ_HANDLED;
	}

	if (csr & USB_CSR0_SENTSTALL) {
		usb_writew(regs, USB_CSR0,
				csr & ~USB_CSR0_SENTSTALL);
		retval = IRQ_HANDLED;
		sunxi_udc->ep0_state = USB_EP0_STAGE_IDLE;
		csr = usb_readw(regs, USB_CSR0);
	}

	if (csr & USB_CSR0_SETUPEND) {
		usb_writew(regs, USB_CSR0, USB_CSR0_SVDSETUPEND);
		retval = IRQ_HANDLED;
		/* Transition into the early status phase */
		switch (sunxi_udc->ep0_state) {
		case USB_EP0_STAGE_TX:
			sunxi_udc->ep0_state = USB_EP0_STAGE_STATUSOUT;
			break;
		case USB_EP0_STAGE_RX:
			sunxi_udc->ep0_state = USB_EP0_STAGE_STATUSIN;
			break;
		default:
			ERR("SetupEnd came in a wrong ep0stage %s\n",
			    decode_ep0stage(sunxi_udc->ep0_state));
		}
		csr = usb_readw(regs, USB_CSR0);
	}

	switch (sunxi_udc->ep0_state) {

	case USB_EP0_STAGE_TX:
		/* irq on clearing txpktrdy */
		if ((csr & USB_CSR0_TXPKTRDY) == 0) {
			ep0_txstate(sunxi_udc);
			retval = IRQ_HANDLED;
		}
		break;

	case USB_EP0_STAGE_RX:
		/* irq on set rxpktrdy */
		if (csr & USB_CSR0_RXPKTRDY) {
			ep0_rxstate(sunxi_udc);
			retval = IRQ_HANDLED;
		}
		break;

	case USB_EP0_STAGE_STATUSIN:
		if (sunxi_udc->set_address) {
			sunxi_udc->set_address = false;
			usb_writeb(mbase, USB_FADDR, sunxi_udc->address);
		}

		else if (sunxi_udc->test_mode) {
			dev_dbg(sunxi_udc->controller, "entering TESTMODE\n");

			if (USB_TEST_PACKET == sunxi_udc->test_mode_nr)
				sunxi_udc_load_testpacket(sunxi_udc);

			usb_writeb(mbase, USB_TESTC,
					sunxi_udc->test_mode_nr);
		}

	case USB_EP0_STAGE_STATUSOUT:
		{
			struct sunxi_udc_request	*req;

			req = next_ep0_request(sunxi_udc);
			if (req)
				sunxi_udc_g_ep0_giveback(sunxi_udc, &req->request);
		}

		if (csr & USB_CSR0_RXPKTRDY)
			goto setup;

		retval = IRQ_HANDLED;
		sunxi_udc->ep0_state = USB_EP0_STAGE_IDLE;
		break;

	case USB_EP0_STAGE_IDLE:
		retval = IRQ_HANDLED;
		sunxi_udc->ep0_state = USB_EP0_STAGE_SETUP;

	case USB_EP0_STAGE_SETUP:
setup:
		if (csr & USB_CSR0_RXPKTRDY) {
			struct usb_ctrlrequest	setup;
			int			handled = 0;

			if (len != 8) {
				ERR("SETUP packet len %d != 8 ?\n", len);
				break;
			}
			usb_read_setup(sunxi_udc, &setup);
			retval = IRQ_HANDLED;

			/* sometimes the RESET won't be reported */
			if (unlikely(sunxi_udc->g.speed == USB_SPEED_UNKNOWN)) {
				u8	power;

				printk(KERN_NOTICE "%s: peripheral reset irq\n",
						sunxi_udc_driver_name);
				power = usb_readb(mbase, USB_GCS);
				sunxi_udc->g.speed = (power & USB_GCS_HSFLAG)
					? USB_SPEED_HIGH : USB_SPEED_FULL;

			}

			switch (sunxi_udc->ep0_state) {

			case USB_EP0_STAGE_ACKWAIT:
				handled = service_zero_data_request(
						sunxi_udc, &setup);

				sunxi_udc->ackpend |= USB_CSR0_DATAEND;

				if (handled > 0)
					sunxi_udc->ep0_state =
						USB_EP0_STAGE_STATUSIN;
				break;

			case USB_EP0_STAGE_TX:
				handled = service_in_request(sunxi_udc, &setup);
				if (handled > 0) {
					sunxi_udc->ackpend = USB_CSR0_TXPKTRDY
						| USB_CSR0_DATAEND;
					sunxi_udc->ep0_state =
						USB_EP0_STAGE_STATUSOUT;
				}
				break;

			default:		/* USB_EP0_STAGE_RX */
				break;
			}

			dev_dbg(sunxi_udc->controller, "handled %d, csr %04x, ep0stage %s\n",
				handled, csr,
				decode_ep0stage(sunxi_udc->ep0_state));

			if (handled < 0)
				goto stall;
			else if (handled > 0)
				goto finish;

			handled = forward_to_driver(sunxi_udc, &setup);
			if (handled < 0) {
				sunxi_udc_ep_select(mbase, 0);
stall:
				dev_dbg(sunxi_udc->controller, "stall (%d)\n", handled);
				sunxi_udc->ackpend |= USB_CSR0_SENDSTALL;
				sunxi_udc->ep0_state = USB_EP0_STAGE_IDLE;
finish:
				usb_writew(regs, USB_CSR0,
						sunxi_udc->ackpend);
				sunxi_udc->ackpend = 0;
			}
		}
		break;

	case USB_EP0_STAGE_ACKWAIT:
		retval = IRQ_HANDLED;
		break;

	default:
		/* "can't happen" */
		assert_noisy(false);
		usb_writew(regs, USB_CSR0, USB_CSR0_SENDSTALL);
		sunxi_udc->ep0_state = USB_EP0_STAGE_IDLE;
		break;
	}

	return retval;
}


static int
sunxi_udc_g_ep0_enable(struct usb_ep *ep, const struct usb_endpoint_descriptor *desc)
{
	/* always enabled */
	return -EINVAL;
}

static int sunxi_udc_g_ep0_disable(struct usb_ep *e)
{
	/* always enabled */
	return -EINVAL;
}

static int
sunxi_udc_g_ep0_queue(struct usb_ep *e, struct usb_request *r, gfp_t gfp_flags)
{
	struct sunxi_udc_ep		*ep;
	struct sunxi_udc_request	*req;
	struct sunxi_udc		*sunxi_udc;
	int			status;
	unsigned long		lockflags;
	void __iomem		*regs;

	if (!e || !r)
		return -EINVAL;

	ep = to_sunxi_udc_ep(e);
	sunxi_udc = ep->sunxi_udc;
	regs = sunxi_udc->control_ep->regs;

	req = to_sunxi_udc_request(r);
	req->sunxi_udc = sunxi_udc;
	req->request.actual = 0;
	req->request.status = -EINPROGRESS;
	req->tx = ep->is_in;

	spin_lock_irqsave(&sunxi_udc->lock, lockflags);

	if (!list_empty(&ep->req_list)) {
		status = -EBUSY;
		goto cleanup;
	}

	switch (sunxi_udc->ep0_state) {
	case USB_EP0_STAGE_RX:		/* control-OUT data */
	case USB_EP0_STAGE_TX:		/* control-IN data */
	case USB_EP0_STAGE_ACKWAIT:	/* zero-length data */
		status = 0;
		break;
	default:
		dev_dbg(sunxi_udc->controller, "ep0 request queued in state %d\n",
				sunxi_udc->ep0_state);
		status = -EINVAL;
		goto cleanup;
	}

	/* add request to the list */
	list_add_tail(&req->list, &ep->req_list);

	dev_dbg(sunxi_udc->controller, "queue to %s (%s), length=%d\n",
			ep->name, ep->is_in ? "IN/TX" : "OUT/RX",
			req->request.length);

	sunxi_udc_ep_select(sunxi_udc->regs, 0);

	if (sunxi_udc->ep0_state == USB_EP0_STAGE_TX)
		ep0_txstate(sunxi_udc);

	else if (sunxi_udc->ep0_state == USB_EP0_STAGE_ACKWAIT) {
		if (req->request.length)
			status = -EINVAL;
		else {
			sunxi_udc->ep0_state = USB_EP0_STAGE_STATUSIN;
			usb_writew(regs, USB_CSR0,
					sunxi_udc->ackpend | USB_CSR0_DATAEND);
			sunxi_udc->ackpend = 0;
			sunxi_udc_g_ep0_giveback(ep->sunxi_udc, r);
		}

	} else if (sunxi_udc->ackpend) {
		usb_writew(regs, USB_CSR0, sunxi_udc->ackpend);
		sunxi_udc->ackpend = 0;
	}

cleanup:
	spin_unlock_irqrestore(&sunxi_udc->lock, lockflags);
	return status;
}

static int sunxi_udc_g_ep0_dequeue(struct usb_ep *ep, struct usb_request *req)
{
	/* we just won't support this */
	return -EINVAL;
}

static int sunxi_udc_g_ep0_halt(struct usb_ep *e, int value)
{
	struct sunxi_udc_ep		*ep;
	struct sunxi_udc		*sunxi_udc;
	void __iomem		*base, *regs;
	unsigned long		flags;
	int			status;
	u16			csr;

	if (!e || !value)
		return -EINVAL;

	ep = to_sunxi_udc_ep(e);
	sunxi_udc = ep->sunxi_udc;
	base = sunxi_udc->regs;
	regs = sunxi_udc->control_ep->regs;
	status = 0;

	spin_lock_irqsave(&sunxi_udc->lock, flags);

	if (!list_empty(&ep->req_list)) {
		status = -EBUSY;
		goto cleanup;
	}

	sunxi_udc_ep_select(base, 0);
	csr = sunxi_udc->ackpend;

	switch (sunxi_udc->ep0_state) {

	case USB_EP0_STAGE_TX:		/* control-IN data */
	case USB_EP0_STAGE_ACKWAIT:	/* STALL for zero-length data */
	case USB_EP0_STAGE_RX:		/* control-OUT data */
		csr = usb_readw(regs, USB_CSR0);
		/* FALLTHROUGH */

	case USB_EP0_STAGE_STATUSIN:	/* control-OUT status */
	case USB_EP0_STAGE_STATUSOUT:	/* control-IN status */

		csr |= USB_CSR0_SENDSTALL;
		usb_writew(regs, USB_CSR0, csr);
		sunxi_udc->ep0_state = USB_EP0_STAGE_IDLE;
		sunxi_udc->ackpend = 0;
		break;
	default:
		dev_dbg(sunxi_udc->controller, "ep0 can't halt in state %d\n", sunxi_udc->ep0_state);
		status = -EINVAL;
	}

cleanup:
	spin_unlock_irqrestore(&sunxi_udc->lock, flags);
	return status;
}

const struct usb_ep_ops sunxi_udc_g_ep0_ops = {
	.enable		= sunxi_udc_g_ep0_enable,
	.disable	= sunxi_udc_g_ep0_disable,
	.alloc_request	= sunxi_udc_alloc_request,
	.free_request	= sunxi_udc_free_request,
	.queue		= sunxi_udc_g_ep0_queue,
	.dequeue	= sunxi_udc_g_ep0_dequeue,
	.set_halt	= sunxi_udc_g_ep0_halt,
};
