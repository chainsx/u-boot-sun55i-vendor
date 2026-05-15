/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __UDC_CORE_H__
#define __UDC_CORE_H__

#include <linux/errno.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <udc.h>

struct sunxi_udc;
struct sunxi_udc_hw_ep;
struct sunxi_udc_ep;

#define USB_HWVERS_MAJOR(x)	((x >> 10) & 0x1f)
#define USB_HWVERS_MINOR(x)	(x & 0x3ff)
#define USB_HWVERS_RC		0x8000

#include <sunxi_udc_dma.h>
#include <sunxi_udc_regs.h>
#include "sunxi_udc_gadget.h"

#define	is_peripheral_enabled(sunxi_udc)	((sunxi_udc)->board_mode != USB_HOST)
#define	is_host_enabled(sunxi_udc)		((sunxi_udc)->board_mode != USB_PERIPHERAL)
#define	is_otg_enabled(sunxi_udc)		((sunxi_udc)->board_mode == USB_OTG)

#define is_peripheral_active(sunxi_udc)		(!(sunxi_udc)->is_host)
#define is_host_active(sunxi_udc)		((sunxi_udc)->is_host)

/****************************** DEBUG API *****************************/

#define yprintk(facility, format, args...) \
	do { printk(facility "%s %d: " format, \
	__func__, __LINE__, ## args); } while (0)
#define WARNING(fmt, args...) yprintk(KERN_WARNING, fmt, ## args)
#define INFO(fmt, args...) yprintk(KERN_INFO, fmt, ## args)
#define ERR(fmt, args...) yprintk(KERN_ERR, fmt, ## args)

/****************************** PERIPHERAL ROLE *****************************/

#ifdef CONFIG_AW_UDC_CONTROLLER
#define	is_peripheral_capable()	(1)
#else
#define	is_peripheral_capable()	(0)
#endif

extern irqreturn_t sunxi_udc_g_ep0_irq(struct sunxi_udc *);
extern void sunxi_udc_g_tx(struct sunxi_udc *, u8);
extern void sunxi_udc_g_rx(struct sunxi_udc *, u8);
extern void sunxi_udc_g_reset(struct sunxi_udc *);
extern void sunxi_udc_g_suspend(struct sunxi_udc *);
extern void sunxi_udc_g_resume(struct sunxi_udc *);
extern void sunxi_udc_g_wakeup(struct sunxi_udc *);
extern void sunxi_udc_g_disconnect(struct sunxi_udc *);

/****************************** HOST ROLE ***********************************/
#define	is_host_capable()	(0)

extern irqreturn_t sunxi_udc_h_ep0_irq(struct sunxi_udc *);
extern void sunxi_udc_host_tx(struct sunxi_udc *, u8);
extern void sunxi_udc_host_rx(struct sunxi_udc *, u8);

/****************************** CONSTANTS ********************************/

#ifndef USB_C_NUM_EPS
#define USB_C_NUM_EPS ((u8)16)
#endif

#ifndef USB_MAX_END0_PACKET
#define USB_MAX_END0_PACKET ((u16)USB_EP0_FIFOSIZE)
#endif

/* host side ep0 states */
enum sunxi_udc_h_ep0_state {
	USB_EP0_IDLE,
	USB_EP0_START,
	USB_EP0_IN,
	USB_EP0_OUT,
	USB_EP0_STATUS,
} __attribute__ ((packed));

/* peripheral side ep0 states */
enum sunxi_udc_g_ep0_state {
	USB_EP0_STAGE_IDLE,
	USB_EP0_STAGE_SETUP,
	USB_EP0_STAGE_TX,
	USB_EP0_STAGE_RX,
	USB_EP0_STAGE_STATUSIN,
	USB_EP0_STAGE_STATUSOUT,
	USB_EP0_STAGE_ACKWAIT,
} __attribute__ ((packed));

#define OTG_TIME_A_WAIT_VRISE	100		/* msec (max) */
#define OTG_TIME_A_WAIT_BCON	1100		/* min 1 second */
#define OTG_TIME_A_AIDL_BDIS	200		/* min 200 msec */
#define OTG_TIME_B_ASE0_BRST	100		/* min 3.125 ms */


/*************************** REGISTER ACCESS ********************************/
#define sunxi_udc_ep_select(_mbase, _epnum) \
	usb_writeb((_mbase), USB_INDEX, (_epnum))
#define	USB_EP_OFFSET			USB_INDEXED_OFFSET

/****************************** FUNCTIONS ********************************/

#define USB_HST_MODE(_sunxi_udc)\
	{ (_sunxi_udc)->is_host = true; }
#define USB_DEV_MODE(_sunxi_udc) \
	{ (_sunxi_udc)->is_host = false; }

#define USB_MODE(sunxi_udc) ((sunxi_udc)->is_host ? "Host" : "Peripheral")

/******************************** TYPES *************************************/
struct sunxi_udc_platform_ops {
	int	(*init)(struct sunxi_udc *sunxi_udc);
	int	(*exit)(struct sunxi_udc *sunxi_udc);

	int	(*enable)(struct sunxi_udc *sunxi_udc);

	void	(*disable)(struct sunxi_udc *sunxi_udc);

	int	(*set_mode)(struct sunxi_udc *sunxi_udc, u8 mode);
	void	(*try_idle)(struct sunxi_udc *sunxi_udc, unsigned long timeout);

	int	(*vbus_status)(struct sunxi_udc *sunxi_udc);
	void	(*set_vbus)(struct sunxi_udc *sunxi_udc, int on);

	int	(*adjust_channel_params)(struct dma_channel *channel,
				u16 packet_sz, u8 *mode,
				dma_addr_t *dma_addr, u32 *len);
	void	(*pre_root_reset_end)(struct sunxi_udc *sunxi_udc);
	void	(*post_root_reset_end)(struct sunxi_udc *sunxi_udc);
};

struct sunxi_udc_hw_ep {
	struct sunxi_udc		*sunxi_udc;
	void __iomem		*fifo;
	void __iomem		*regs;

	u8			epnum;

	bool			is_shared_fifo;
	bool			tx_double_buffered;
	bool			rx_double_buffered;
	u16			max_packet_sz_tx;
	u16			max_packet_sz_rx;

	struct dma_channel	*tx_channel;
	struct dma_channel	*rx_channel;

	void __iomem		*target_regs;

	struct sunxi_udc_qh		*in_qh;
	struct sunxi_udc_qh		*out_qh;

	u8			rx_reinit;
	u8			tx_reinit;

	/* peripheral side */
	struct sunxi_udc_ep		ep_in;			/* TX */
	struct sunxi_udc_ep		ep_out;			/* RX */
};

static inline struct sunxi_udc_request *next_in_request(struct sunxi_udc_hw_ep *hw_ep)
{
	return next_request(&hw_ep->ep_in);
}

static inline struct sunxi_udc_request *next_out_request(struct sunxi_udc_hw_ep *hw_ep)
{
	return next_request(&hw_ep->ep_out);
}

struct sunxi_udc_csr_regs {
	/* FIFO registers */
	u16 txmaxp, txcsr, rxmaxp, rxcsr;
	u16 rxfifoadd, txfifoadd;
	u8 txtype, txinterval, rxtype, rxinterval;
	u8 rxfifosz, txfifosz;
	u8 txfunaddr, txhubaddr, txhubport;
	u8 rxfunaddr, rxhubaddr, rxhubport;
};

struct sunxi_udc_context_registers {

	u8 power;
	u16 intrtxe, intrrxe;
	u8 intrusbe;
	u16 frame;
	u8 index, testmode;

	u8 busctl, misc;
	u32 otg_interfsel;

	struct sunxi_udc_csr_regs index_regs[USB_C_NUM_EPS];
};

/*
 * struct sunxi_udc - Driver instance data.
 */
struct sunxi_udc {
	/* device lock */
	spinlock_t		lock;

	const struct sunxi_udc_platform_ops *ops;
	struct sunxi_udc_context_registers context;

	irqreturn_t		(*isr)(int, void *);
	struct work_struct	irq_work;
	u16			hwvers;

/* this hub status bit is reserved by USB 2.0 and not seen by usbcore */
#define USB_PORT_STAT_RESUME	(1 << 31)

	u32			port1_status;

	unsigned long		rh_timer;

	enum sunxi_udc_h_ep0_state	ep0_stage;

	struct sunxi_udc_hw_ep	*bulk_ep;

	struct list_head	control;	/* of sunxi_udc_qh */
	struct list_head	in_bulk;	/* of sunxi_udc_qh */
	struct list_head	out_bulk;	/* of sunxi_udc_qh */

	struct timer_list	otg_timer;
	struct notifier_block	nb;

	struct dma_controller	*dma_controller;

	struct device		*controller;
	void __iomem		*ctrl_base;
	void __iomem		*regs;

	/* passed down from chip/board specific irq handlers */
	u8			int_usb;
	u16			int_rx;
	u16			int_tx;

	struct usb_phy		*xceiv;

	int nIrq;
	unsigned		irq_wake:1;

	struct sunxi_udc_hw_ep	 endpoints[USB_C_NUM_EPS];
#define control_ep		endpoints

#define VBUSERR_RETRY_COUNT	3
	u16			vbuserr_retry;
	u16 epmask;
	u8 nr_endpoints;

	u8 board_mode;		/* enum sunxi_udc_mode */
	int			(*board_set_power)(int state);

	u8			min_power;

	bool			is_host;

	int			a_wait_bcon;
	unsigned long		idle_timeout;

	unsigned		is_active:1;

	unsigned is_multipoint:1;
	unsigned ignore_disconnect:1;

	unsigned		hb_iso_rx:1;
	unsigned		hb_iso_tx:1;
	unsigned		dyn_fifo:1;

	unsigned		bulk_split:1;
#define	is_bulk_split(sunxi_udc, type) \
	(((type) == USB_ENDPOINT_XFER_BULK) && (sunxi_udc)->bulk_split)

	unsigned		bulk_combine:1;
#define	is_bulk_combine(sunxi_udc, type) \
	(((type) == USB_ENDPOINT_XFER_BULK) && (sunxi_udc)->bulk_combine)

	unsigned		is_suspended:1;

	unsigned		may_wakeup:1;

	unsigned		is_self_powered:1;
	unsigned		is_bus_powered:1;

	unsigned		set_address:1;
	unsigned		test_mode:1;
	unsigned		softconnect:1;

	u8			address;
	u8			test_mode_nr;
	u16			ackpend;		/* ep0 */
	enum sunxi_udc_g_ep0_state	ep0_state;
	struct usb_gadget	g;			/* the gadget */
	struct usb_gadget_driver *gadget_driver;	/* its driver */

	unsigned                double_buffer_not_ok:1;

	struct sunxi_udc_hdrc_config	*config;

};

static inline struct sunxi_udc *gadget_to_sunxi_udc(struct usb_gadget *g)
{
	return container_of(g, struct sunxi_udc, g);
}

static inline int usb_read_fifosize(struct sunxi_udc *sunxi_udc,
		struct sunxi_udc_hw_ep *hw_ep, u8 epnum)
{
	void *mbase = sunxi_udc->regs;
	u8 reg = 0;

	reg = usb_readb(mbase, USB_EP_OFFSET(epnum, USB_FIFOSIZE));
	if (!reg)
		return -ENODEV;

	sunxi_udc->nr_endpoints++;
	sunxi_udc->epmask |= (1 << epnum);

	hw_ep->max_packet_sz_tx = 1 << (reg & 0x0f);

	/* shared TX/RX FIFO? */
	if ((reg & 0xf0) == 0xf0) {
		hw_ep->max_packet_sz_rx = hw_ep->max_packet_sz_tx;
		hw_ep->is_shared_fifo = true;
		return 0;
	} else {
		hw_ep->max_packet_sz_rx = 1 << ((reg & 0xf0) >> 4);
		hw_ep->is_shared_fifo = false;
	}

	return 0;
}

static inline void sunxi_udc_configure_ep0(struct sunxi_udc *sunxi_udc)
{
	sunxi_udc->endpoints[0].max_packet_sz_tx = USB_EP0_FIFOSIZE;
	sunxi_udc->endpoints[0].max_packet_sz_rx = USB_EP0_FIFOSIZE;
	sunxi_udc->endpoints[0].is_shared_fifo = true;
}

/***************************** Glue it together *****************************/

extern const char sunxi_udc_driver_name[];

extern int sunxi_udc_start(struct sunxi_udc *sunxi_udc);
extern void sunxi_udc_stop(struct sunxi_udc *sunxi_udc);

extern void usb_write_fifo(struct sunxi_udc_hw_ep *ep, u16 len, const u8 *src);
extern void usb_read_fifo(struct sunxi_udc_hw_ep *ep, u16 len, u8 *dst);

extern void sunxi_udc_load_testpacket(struct sunxi_udc *);

extern irqreturn_t udc_interrupt(struct sunxi_udc *);

extern void sunxi_udc_hnp_stop(struct sunxi_udc *sunxi_udc);

static inline void sunxi_udc_platform_set_vbus(struct sunxi_udc *sunxi_udc, int is_on)
{
	if (sunxi_udc->ops->set_vbus)
		sunxi_udc->ops->set_vbus(sunxi_udc, is_on);
}

static inline int sunxi_udc_platform_enable(struct sunxi_udc *sunxi_udc)
{
	if (!sunxi_udc->ops->enable)
		return 0;

	return sunxi_udc->ops->enable(sunxi_udc);
}

static inline void sunxi_udc_platform_disable(struct sunxi_udc *sunxi_udc)
{
	if (sunxi_udc->ops->disable)
		sunxi_udc->ops->disable(sunxi_udc);
}

static inline int sunxi_udc_platform_set_mode(struct sunxi_udc *sunxi_udc, u8 mode)
{
	if (!sunxi_udc->ops->set_mode)
		return 0;

	return sunxi_udc->ops->set_mode(sunxi_udc, mode);
}

static inline void sunxi_udc_platform_try_idle(struct sunxi_udc *sunxi_udc,
		unsigned long timeout)
{
	if (sunxi_udc->ops->try_idle)
		sunxi_udc->ops->try_idle(sunxi_udc, timeout);
}

static inline int sunxi_udc_platform_get_vbus_status(struct sunxi_udc *sunxi_udc)
{
	if (!sunxi_udc->ops->vbus_status)
		return 0;

	return sunxi_udc->ops->vbus_status(sunxi_udc);
}

static inline int sunxi_udc_platform_init(struct sunxi_udc *sunxi_udc)
{
	if (!sunxi_udc->ops->init)
		return -EINVAL;

	return sunxi_udc->ops->init(sunxi_udc);
}

static inline int sunxi_udc_platform_exit(struct sunxi_udc *sunxi_udc)
{
	if (!sunxi_udc->ops->exit)
		return -EINVAL;

	return sunxi_udc->ops->exit(sunxi_udc);
}

struct sunxi_udc *
sunxi_udc_init_controller(struct sunxi_udc_hdrc_platform_data *plat, struct device *dev,
			     void *ctrl);

#endif	/* __UDC_CORE_H__ */
