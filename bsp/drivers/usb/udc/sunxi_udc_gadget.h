/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __UDC_GADGET_H
#define __UDC_GADGET_H

#include <linux/list.h>
#include <asm/byteorder.h>
#include <linux/errno.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>

enum buffer_map_state {
	UN_MAPPED = 0,
	PRE_MAPPED,
	USB_MAPPED
};

struct sunxi_udc_request {
	struct usb_request	request;
	struct list_head	list;
	struct sunxi_udc_ep	*ep;
	struct sunxi_udc	*sunxi_udc;
	u8 tx;			/* endpoint direction */
	u8 epnum;
	enum buffer_map_state map_state;
};

static inline struct sunxi_udc_request *to_sunxi_udc_request(struct usb_request *req)
{
	return req ? container_of(req, struct sunxi_udc_request, request) : NULL;
}

extern struct usb_request *
sunxi_udc_alloc_request(struct usb_ep *ep, gfp_t gfp_flags);

extern void sunxi_udc_free_request(struct usb_ep *ep, struct usb_request *req);

struct sunxi_udc_ep {
	struct usb_ep			end_point;
	char				name[12];
	struct sunxi_udc_hw_ep		*hw_ep;
	struct sunxi_udc		*sunxi_udc;
	u8				current_epnum;
	u8				type;
	u8				is_in;
	u16				packet_sz;
	const struct usb_endpoint_descriptor	*desc;
	struct dma_channel		*dma;
	struct list_head		req_list;
	u8				wedged;
	u8				busy;
	u8				hb_mult;
};

static inline struct sunxi_udc_ep *to_sunxi_udc_ep(struct usb_ep *ep)
{
	return ep ? container_of(ep, struct sunxi_udc_ep, end_point) : NULL;
}

static inline struct sunxi_udc_request *next_request(struct sunxi_udc_ep *ep)
{
	struct list_head	*queue = &ep->req_list;

	if (list_empty(queue))
		return NULL;
	return container_of(queue->next, struct sunxi_udc_request, list);
}

extern void sunxi_udc_g_tx(struct sunxi_udc *sunxi_udc, u8 epnum);
extern void sunxi_udc_g_rx(struct sunxi_udc *sunxi_udc, u8 epnum);

extern const struct usb_ep_ops sunxi_udc_g_ep0_ops;

extern int sunxi_udc_gadget_setup(struct sunxi_udc *);
extern void sunxi_udc_gadget_cleanup(struct sunxi_udc *);

extern void sunxi_udc_g_giveback(struct sunxi_udc_ep *, struct usb_request *, int);

extern void sunxi_udc_ep_restart(struct sunxi_udc *, struct sunxi_udc_request *);

int sunxi_udc_gadget_start(struct usb_gadget *g, struct usb_gadget_driver *driver);

#endif		/* __UDC_GADGET_H */
