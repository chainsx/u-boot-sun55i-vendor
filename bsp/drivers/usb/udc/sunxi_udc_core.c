// SPDX-License-Identifier: GPL-2.0

#include <common.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <usb.h>
#include <linux/bitops.h>
#include <linux/bug.h>
#include <linux/errno.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/usb_urb_compat.h>
#include <asm/io.h>
#include <linux-compat.h>
#include <udc.h>
#include "sunxi_udc_core.h"

#define TA_WAIT_BCON(m) max_t(int, (m)->a_wait_bcon, OTG_TIME_A_WAIT_BCON)

#define DRIVER_AUTHOR "Allwinner"
#define DRIVER_DESC "Inventra Dual-Role USB Controller Driver"

#define USB_VERSION "6.0"

#define DRIVER_INFO DRIVER_DESC ", v" USB_VERSION

#define USB_DRIVER_NAME "sunxi_udc-hdrc"
const char sunxi_udc_driver_name[] = USB_DRIVER_NAME;

MODULE_DESCRIPTION(DRIVER_INFO);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" USB_DRIVER_NAME);


/*-------------------------------USB FIFO R/W API----------------------------------*/

void usb_write_fifo(struct sunxi_udc_hw_ep *hw_ep, u16 len, const u8 *src)
{
	struct sunxi_udc *sunxi_udc = hw_ep->sunxi_udc;
	void __iomem *fifo = hw_ep->fifo;

	prefetch((u8 *)src);

	dev_dbg(sunxi_udc->controller, "%cX ep%d fifo %p count %d buf %p\n",
			'T', hw_ep->epnum, fifo, len, src);

	/* We need to ensure data alignment */
	if (likely((0x01 & (unsigned long) src) == 0)) {
		u16	index = 0;

		/* best case is 32bit-aligned source address */
		if ((0x02 & (unsigned long) src) == 0) {
			if (len >= 4) {
				writesl(fifo, src + index, len >> 2);
				index += len & ~0x03;
			}
			if (len & 0x02) {
				usb_writew(fifo, 0, *(u16 *)&src[index]);
				index += 2;
			}
		} else {
			if (len >= 2) {
				writesw(fifo, src + index, len >> 1);
				index += len & ~0x01;
			}
		}
		if (len & 0x01)
			usb_writeb(fifo, 0, src[index]);
	} else  {
		/* byte aligned */
		writesb(fifo, src, len);
	}
}

void usb_read_fifo(struct sunxi_udc_hw_ep *hw_ep, u16 len, u8 *dst)
{
	struct sunxi_udc *sunxi_udc = hw_ep->sunxi_udc;
	void __iomem *fifo = hw_ep->fifo;

	dev_dbg(sunxi_udc->controller, "%cX ep%d fifo %p count %d buf %p\n",
			'R', hw_ep->epnum, fifo, len, dst);

	/* we can't assume unaligned writes work */
	if (likely((0x01 & (unsigned long) dst) == 0)) {
		u16	index = 0;

		/* best case is 32bit-aligned destination address */
		if ((0x02 & (unsigned long) dst) == 0) {
			if (len >= 4) {
				readsl(fifo, dst, len >> 2);
				index = len & ~0x03;
			}
			if (len & 0x02) {
				*(u16 *)&dst[index] = usb_readw(fifo, 0);
				index += 2;
			}
		} else {
			if (len >= 2) {
				readsw(fifo, dst, len >> 1);
				index = len & ~0x01;
			}
		}
		if (len & 0x01)
			dst[index] = usb_readb(fifo, 0);
	} else  {
		/* byte aligned */
		readsb(fifo, dst, len);
	}
}

/*-------------------------------USB TEST API----------------------------------*/
static const u8 sunxi_udc_test_packet[53] = {
	/* implicit SYNC then DATA0 to start */

	/* JKJKJKJK x9 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* JJKKJJKK x8 */
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	/* JJJJKKKK x8 */
	0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
	/* JJJJJJJKKKKKKK x8 */
	0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	/* JJJJJJJK x8 */
	0x7f, 0xbf, 0xdf, 0xef, 0xf7, 0xfb, 0xfd,
	/* JKKKKKKK x10, JK */
	0xfc, 0x7e, 0xbf, 0xdf, 0xef, 0xf7, 0xfb, 0xfd, 0x7e

	/* implicit CRC16 then EOP to end */
};

void sunxi_udc_load_testpacket(struct sunxi_udc *sunxi_udc)
{
	void __iomem	*regs = sunxi_udc->endpoints[0].regs;

	sunxi_udc_ep_select(sunxi_udc->regs, 0);
	usb_write_fifo(sunxi_udc->control_ep,
			sizeof(sunxi_udc_test_packet), sunxi_udc_test_packet);
	usb_writew(regs, USB_CSR0, USB_CSR0_TXPKTRDY);
}

/*-------------------------------USB EP0 IRQ----------------------------------*/
static irqreturn_t sunxi_udc_stage0_irq(struct sunxi_udc *sunxi_udc, u8 int_usb, u8 power)
{
	irqreturn_t handled = IRQ_NONE;

	dev_dbg(sunxi_udc->controller, "<== Power=%02x, int_usb=0x%x\n", power, int_usb);

	schedule_work(&sunxi_udc->irq_work);

	return handled;
}

/*-------------------------------USB CORE API----------------------------------*/
int sunxi_udc_start(struct sunxi_udc *sunxi_udc)
{
	void __iomem	*regs = sunxi_udc->regs;
	int ret;

	/* enable interrupts */
	usb_writew(regs, USB_INTRTXE, sunxi_udc->epmask);
	usb_writew(regs, USB_INTRRXE, sunxi_udc->epmask & 0xfffe);
	usb_writeb(regs, USB_INTRUSBE, 0xf7);

	usb_writeb(regs, USB_TESTC, 0);

	usb_writeb(regs, USB_GCS, USB_GCS_ISOUPDATEEN
						| USB_GCS_HSEN
						/* | USB_GCS_SUSPENDEN */
						);

	sunxi_udc->is_active = 0;

	sunxi_udc->is_active = 1;

	ret = sunxi_udc_platform_enable(sunxi_udc);
	if (ret) {
		sunxi_udc->is_active = 0;
		return ret;
	}

	return 0;
}

static void sunxi_udc_generic_disable(struct sunxi_udc *sunxi_udc)
{
	void __iomem	*mbase = sunxi_udc->regs;
	u16	temp;

	/* disable interrupts */
	usb_writeb(mbase, USB_INTRUSBE, 0);
	usb_writew(mbase, USB_INTRTXE, 0);
	usb_writew(mbase, USB_INTRRXE, 0);

	/*  flush pending interrupts */
	temp = usb_readb(mbase, USB_INTRUSB);
	temp = usb_readw(mbase, USB_INTRTX);
	temp = usb_readw(mbase, USB_INTRRX);

}

void sunxi_udc_stop(struct sunxi_udc *sunxi_udc)
{
	/* stop IRQs and timers, and so on */
	sunxi_udc_platform_disable(sunxi_udc);
	sunxi_udc_generic_disable(sunxi_udc);
	dev_dbg(sunxi_udc->controller, "HDRC disabled\n");

	sunxi_udc_platform_try_idle(sunxi_udc, 0);
	sunxi_udc_platform_exit(sunxi_udc);
}

/*-------------------------------USB EP CONFIG STRUCT----------------------------------*/
static ushort __devinitdata fifo_mode;
/* "modprobe ... fifo_mode=1" etc */
module_param(fifo_mode, ushort, 0);
MODULE_PARM_DESC(fifo_mode, "initial endpoint configuration");

/* FIFO MOST 8K */
static struct sunxi_udc_fifo_cfg __devinitdata sunxi_fifo_cfg[] = {
{ .hw_ep_num = 1, .style = FIFO_TX,   .maxpacket = 512, },
{ .hw_ep_num = 1, .style = FIFO_RX,   .maxpacket = 512, },
{ .hw_ep_num = 2, .style = FIFO_TX,   .maxpacket = 512, },
{ .hw_ep_num = 2, .style = FIFO_RX,   .maxpacket = 512, },
{ .hw_ep_num = 3, .style = FIFO_RX,   .maxpacket = 1024, },
{ .hw_ep_num = 3, .style = FIFO_TX,   .maxpacket = 1024, },
{ .hw_ep_num = 4, .style = FIFO_RX,   .maxpacket = 1024, },
{ .hw_ep_num = 4, .style = FIFO_TX,   .maxpacket = 1024, },
{ .hw_ep_num = 5, .style = FIFO_RX,   .maxpacket = 1024, },
{ .hw_ep_num = 5, .style = FIFO_TX,   .maxpacket = 1024, },
};

static int __devinit
fifo_setup(struct sunxi_udc *sunxi_udc, struct sunxi_udc_hw_ep  *hw_ep,
		const struct sunxi_udc_fifo_cfg *cfg, u16 offset)
{
	void __iomem	*mbase = sunxi_udc->regs;
	int	size = 0;
	u16	maxpacket = cfg->maxpacket;
	u16	c_off = offset >> 3;
	u8	c_size;

	size = ffs(max(maxpacket, (u16) 8)) - 1;
	maxpacket = 1 << size;

	c_size = size - 3;
	if (cfg->mode == BUF_DOUBLE) {
		if ((offset + (maxpacket << 1)) >
				(1 << (sunxi_udc->config->ram_bits + 2)))
			return -EMSGSIZE;
		c_size |= USB_FIFOSZ_DPB;
	} else {
		if ((offset + maxpacket) > (1 << (sunxi_udc->config->ram_bits + 2)))
			return -EMSGSIZE;
	}

	/* configure the FIFO */
	usb_writeb(mbase, USB_INDEX, hw_ep->epnum);

	if (hw_ep->epnum == 1)
		sunxi_udc->bulk_ep = hw_ep;
	switch (cfg->style) {
	case FIFO_TX:
		usb_write_txfifosz(mbase, c_size);
		usb_write_txfifoadd(mbase, c_off);
		hw_ep->tx_double_buffered = !!(c_size & USB_FIFOSZ_DPB);
		hw_ep->max_packet_sz_tx = maxpacket;
		break;
	case FIFO_RX:
		usb_write_rxfifosz(mbase, c_size);
		usb_write_rxfifoadd(mbase, c_off);
		hw_ep->rx_double_buffered = !!(c_size & USB_FIFOSZ_DPB);
		hw_ep->max_packet_sz_rx = maxpacket;
		break;
	case FIFO_RXTX:
		usb_write_txfifosz(mbase, c_size);
		usb_write_txfifoadd(mbase, c_off);
		hw_ep->rx_double_buffered = !!(c_size & USB_FIFOSZ_DPB);
		hw_ep->max_packet_sz_rx = maxpacket;

		usb_write_rxfifosz(mbase, c_size);
		usb_write_rxfifoadd(mbase, c_off);
		hw_ep->tx_double_buffered = hw_ep->rx_double_buffered;
		hw_ep->max_packet_sz_tx = maxpacket;

		hw_ep->is_shared_fifo = true;
		break;
	}

	sunxi_udc->epmask |= (1 << hw_ep->epnum);

	return offset + (maxpacket << ((c_size & USB_FIFOSZ_DPB) ? 1 : 0));
}

static struct sunxi_udc_fifo_cfg __devinitdata ep0_cfg = {
	.style = FIFO_RXTX, .maxpacket = 64,
};

static int __devinit ep_config_from_table(struct sunxi_udc *sunxi_udc)
{
	const struct sunxi_udc_fifo_cfg	*cfg;
	unsigned		i, n;
	int			offset;
	struct sunxi_udc_hw_ep	*hw_ep = sunxi_udc->endpoints;

	if (sunxi_udc->config->fifo_cfg) {
		cfg = sunxi_udc->config->fifo_cfg;
		n = sunxi_udc->config->fifo_cfg_size;
		goto done;
	}

	cfg = sunxi_fifo_cfg;
	n = ARRAY_SIZE(sunxi_fifo_cfg);
	pr_debug("%s: setup fifo_mode %d\n", sunxi_udc_driver_name, fifo_mode);

done:
	offset = fifo_setup(sunxi_udc, hw_ep, &ep0_cfg, 0);

	for (i = 0; i < n; i++) {
		u8	epn = cfg->hw_ep_num;

		if (epn >= sunxi_udc->config->num_eps) {
			pr_debug("%s: invalid ep %d\n",
					sunxi_udc_driver_name, epn);
			return -EINVAL;
		}
		offset = fifo_setup(sunxi_udc, hw_ep + epn, cfg++, offset);
		if (offset < 0) {
			pr_debug("%s: mem overrun, ep %d\n",
					sunxi_udc_driver_name, epn);
			return -EINVAL;
		}
		epn++;
		sunxi_udc->nr_endpoints = max(epn, sunxi_udc->nr_endpoints);
	}

	pr_debug("%s: %d/%d max ep, %d/%d memory\n", sunxi_udc_driver_name, n + 1,
		 sunxi_udc->config->num_eps * 2 - 1, offset,
		 (1 << (sunxi_udc->config->ram_bits + 2)));

	if (!sunxi_udc->bulk_ep) {
		pr_debug("%s: missing bulk\n", sunxi_udc_driver_name);
		return -EINVAL;
	}

	return 0;
}

static int __devinit ep_config_from_hw(struct sunxi_udc *sunxi_udc)
{
	u8 epnum = 0;
	struct sunxi_udc_hw_ep *hw_ep;
	void *mbase = sunxi_udc->regs;
	int ret = 0;

	dev_dbg(sunxi_udc->controller, "<== static silicon ep config\n");

	for (epnum = 1; epnum < sunxi_udc->config->num_eps; epnum++) {
		sunxi_udc_ep_select(mbase, epnum);
		hw_ep = sunxi_udc->endpoints + epnum;

		ret = usb_read_fifosize(sunxi_udc, hw_ep, epnum);
		if (ret < 0)
			break;


		/* for bulk */
		if (hw_ep->max_packet_sz_tx < 512
				|| hw_ep->max_packet_sz_rx < 512)
			continue;

		if (sunxi_udc->bulk_ep)
			continue;
		sunxi_udc->bulk_ep = hw_ep;
	}

	if (!sunxi_udc->bulk_ep) {
		pr_debug("%s: missing bulk\n", sunxi_udc_driver_name);
		return -EINVAL;
	}

	return 0;
}

enum { USB_CONTROLLER_MHDRC, USB_CONTROLLER_HDRC, };

static int __devinit sunxi_udc_core_init(u16 sunxi_udc_type, struct sunxi_udc *sunxi_udc)
{
	u8 reg;
	char *type;
	void __iomem	*mbase = sunxi_udc->regs;
	int		status = 0;
	int		i;

	reg = usb_read_configdata(mbase);

	sunxi_udc->dyn_fifo = true;

	sunxi_udc->bulk_combine = false;
	sunxi_udc->bulk_split = false;

	sunxi_udc->hb_iso_rx = true;
	sunxi_udc->hb_iso_tx = true;

	pr_debug("%s:ConfigData=0x%02x\n", sunxi_udc_driver_name, reg);

	if (USB_CONTROLLER_MHDRC == sunxi_udc_type) {
		sunxi_udc->is_multipoint = 1;
		type = "M";
	} else {
		sunxi_udc->is_multipoint = 0;
		type = "";
#ifndef	CONFIG_USB_OTG_BLACKLIST_HUB
		printk(KERN_ERR
			"%s: kernel must blacklist external hubs\n",
			sunxi_udc_driver_name);
#endif
	}

	/* configure ep0 */
	sunxi_udc_configure_ep0(sunxi_udc);

	/* discover endpoint configuration */
	sunxi_udc->nr_endpoints = 1;
	sunxi_udc->epmask = 1;

	if (sunxi_udc->dyn_fifo)
		status = ep_config_from_table(sunxi_udc);
	else
		status = ep_config_from_hw(sunxi_udc);

	if (status < 0)
		return status;

	/* finish init, and print endpoint config */
	for (i = 0; i < sunxi_udc->nr_endpoints; i++) {
		struct sunxi_udc_hw_ep	*hw_ep = sunxi_udc->endpoints + i;

		hw_ep->fifo = USB_FIFO_OFFSET(i) + mbase;

		hw_ep->regs = USB_EP_OFFSET(i, 0) + mbase;
		hw_ep->target_regs = usb_read_target_reg_base(i, mbase);
		hw_ep->rx_reinit = 1;
		hw_ep->tx_reinit = 1;

		if (hw_ep->max_packet_sz_tx) {
			dev_dbg(sunxi_udc->controller,
				"%s: hw_ep %d%s, %smax %d\n",
				sunxi_udc_driver_name, i,
				hw_ep->is_shared_fifo ? "shared" : "tx",
				hw_ep->tx_double_buffered
					? "doublebuffer, " : "",
				hw_ep->max_packet_sz_tx);
		}
		if (hw_ep->max_packet_sz_rx && !hw_ep->is_shared_fifo) {
			dev_dbg(sunxi_udc->controller,
				"%s: hw_ep %d%s, %smax %d\n",
				sunxi_udc_driver_name, i,
				"rx",
				hw_ep->rx_double_buffered
					? "doublebuffer, " : "",
				hw_ep->max_packet_sz_rx);
		}
		if (!(hw_ep->max_packet_sz_tx || hw_ep->max_packet_sz_rx))
			dev_dbg(sunxi_udc->controller, "hw_ep %d not configured\n", i);
	}

	return 0;
}

/*-------------------------------USB IRQ API----------------------------------*/
/* If sunxi_udc ->isl is not defined, use this interrupt handling function
 */
#if 0
static irqreturn_t generic_interrupt(int irq, void *__hci)
{
	unsigned long	flags;
	irqreturn_t	retval = IRQ_NONE;
	struct sunxi_udc	*sunxi_udc = __hci;

	spin_lock_irqsave(&sunxi_udc->lock, flags);

	sunxi_udc->int_usb = usb_readb(sunxi_udc->regs, USB_INTRUSB);
	sunxi_udc->int_tx = usb_readw(sunxi_udc->regs, USB_INTRTX);
	sunxi_udc->int_rx = usb_readw(sunxi_udc->regs, USB_INTRRX);

	if (sunxi_udc->int_usb || sunxi_udc->int_tx || sunxi_udc->int_rx)
		retval = sunxi_udc_interrupt(sunxi_udc);

	spin_unlock_irqrestore(&sunxi_udc->lock, flags);

	return retval;
}
#else
#define generic_interrupt	NULL
#endif

irqreturn_t udc_interrupt(struct sunxi_udc *sunxi_udc)
{
	irqreturn_t	retval = IRQ_NONE;
	u8		power;
	int		ep_num;
	u32		reg;

	power = usb_readb(sunxi_udc->regs, USB_GCS);

	dev_dbg(sunxi_udc->controller, "** IRQ %s usb%04x tx%04x rx%04x\n",
		"peripheral", sunxi_udc->int_usb, sunxi_udc->int_tx, sunxi_udc->int_rx);

	if (sunxi_udc->int_usb)
		retval |= sunxi_udc_stage0_irq(sunxi_udc, sunxi_udc->int_usb, power);


	/* handle endpoint 0 first */
	if (sunxi_udc->int_tx & 1) {
		if (is_peripheral_capable())
				retval |= sunxi_udc_g_ep0_irq(sunxi_udc);
	}

	/* RX on endpoints 1-15 */
	reg = sunxi_udc->int_rx >> 1;
	ep_num = 1;
	while (reg) {
		if (reg & 1) {
			/* sunxi_udc_ep_select(sunxi_udc->regs, ep_num); */
			retval = IRQ_HANDLED;
			if (is_peripheral_capable())
					sunxi_udc_g_rx(sunxi_udc, ep_num);
		}

		reg >>= 1;
		ep_num++;
	}

	/* TX on endpoints 1-15 */
	reg = sunxi_udc->int_tx >> 1;
	ep_num = 1;
	while (reg) {
		if (reg & 1) {
			/* sunxi_udc_ep_select(sunxi_udc->regs, ep_num); */
			retval = IRQ_HANDLED;
			if (is_peripheral_capable())
				sunxi_udc_g_tx(sunxi_udc, ep_num);
		}
		reg >>= 1;
		ep_num++;
	}

	return retval;
}
EXPORT_SYMBOL_GPL(udc_interrupt);

#define use_dma			0

/*-------------------------------USB INIT API----------------------------------*/
static struct sunxi_udc *__devinit
allocate_instance(struct device *dev,
		struct sunxi_udc_hdrc_config *config, void __iomem *mbase)
{
	struct sunxi_udc		*sunxi_udc;
	struct sunxi_udc_hw_ep	*ep;
	int			epnum;

	sunxi_udc = calloc(1, sizeof(*sunxi_udc));
	if (!sunxi_udc)
		return NULL;

	INIT_LIST_HEAD(&sunxi_udc->control);
	INIT_LIST_HEAD(&sunxi_udc->in_bulk);
	INIT_LIST_HEAD(&sunxi_udc->out_bulk);

	sunxi_udc->vbuserr_retry = VBUSERR_RETRY_COUNT;
	sunxi_udc->a_wait_bcon = OTG_TIME_A_WAIT_BCON;
	dev_set_drvdata(dev, sunxi_udc);
	sunxi_udc->regs = mbase;
	sunxi_udc->ctrl_base = mbase;
	sunxi_udc->nIrq = -ENODEV;
	sunxi_udc->config = config;

	assert_noisy(sunxi_udc->config->num_eps <= USB_C_NUM_EPS);

	for (epnum = 0, ep = sunxi_udc->endpoints;
			epnum < sunxi_udc->config->num_eps;
			epnum++, ep++) {
		ep->sunxi_udc = sunxi_udc;
		ep->epnum = epnum;
	}

	sunxi_udc->controller = dev;

	return sunxi_udc;
}

static void sunxi_udc_free(struct sunxi_udc *sunxi_udc)
{
	if (sunxi_udc->nIrq >= 0) {
		if (sunxi_udc->irq_wake)
			disable_irq_wake(sunxi_udc->nIrq);
		free_irq(sunxi_udc->nIrq, sunxi_udc);
	}
	if (is_dma_capable() && sunxi_udc->dma_controller) {
		struct dma_controller	*c = sunxi_udc->dma_controller;

		(void) c->stop(c);
		dma_controller_destroy(c);
	}

	kfree(sunxi_udc);
}

struct sunxi_udc *
sunxi_udc_init_controller(struct sunxi_udc_hdrc_platform_data *plat, struct device *dev,
			     void *ctrl)
{
	int			status;
	struct sunxi_udc		*sunxi_udc;
	int nIrq = 0;

	if (!plat) {
		dev_dbg(dev, "no platform_data?\n");
		status = -ENODEV;
		goto fail0;
	}

	/* allocate */
	sunxi_udc = allocate_instance(dev, plat->config, ctrl);
	if (!sunxi_udc) {
		status = -ENOMEM;
		goto fail0;
	}

	pm_runtime_use_autosuspend(sunxi_udc->controller);
	pm_runtime_set_autosuspend_delay(sunxi_udc->controller, 200);
	pm_runtime_enable(sunxi_udc->controller);

	spin_lock_init(&sunxi_udc->lock);
	sunxi_udc->board_mode = plat->mode;
	sunxi_udc->board_set_power = plat->set_power;
	sunxi_udc->min_power = plat->min_power;
	sunxi_udc->ops = plat->platform_ops;

	sunxi_udc->isr = generic_interrupt;
	status = sunxi_udc_platform_init(sunxi_udc);
	if (status < 0)
		goto fail1;

	if (!sunxi_udc->isr) {
		status = -ENODEV;
		goto fail2;
	}

	pm_runtime_get_sync(sunxi_udc->controller);

	/* disable controller before init */
	sunxi_udc_platform_disable(sunxi_udc);
	sunxi_udc_generic_disable(sunxi_udc);

	status = sunxi_udc_core_init(plat->config->multipoint
			? USB_CONTROLLER_MHDRC
			: USB_CONTROLLER_HDRC, sunxi_udc);
	if (status < 0)
		goto fail3;

	setup_timer(&sunxi_udc->otg_timer, sunxi_udc_otg_timer_func, (unsigned long) sunxi_udc);

	/* Init IRQ workqueue before request_irq */
	INIT_WORK(&sunxi_udc->irq_work, sunxi_udc_irq_work);

	/* attach to the IRQ */
	if (request_irq(nIrq, sunxi_udc->isr, 0, dev_name(dev), sunxi_udc)) {
		dev_err(dev, "request_irq %d failed!\n", nIrq);
		status = -ENODEV;
		goto fail3;
	}
	sunxi_udc->nIrq = nIrq;
	/* FIXME: this handles wakeup irqs wrong */
	if (enable_irq_wake(nIrq) == 0) {
		sunxi_udc->irq_wake = 1;
		device_init_wakeup(dev, 1);
	} else {
		sunxi_udc->irq_wake = 0;
	}

	if (is_peripheral_capable()) {
		USB_DEV_MODE(sunxi_udc);
		status = sunxi_udc_gadget_setup(sunxi_udc);
	}

	if (status < 0)
		goto fail3;

	pm_runtime_put(sunxi_udc->controller);

	pr_debug("USB %s mode controller at %p using %s, IRQ %d\n",
			"Peripheral", ctrl,
			(is_dma_capable() && sunxi_udc->dma_controller)
			? "DMA" : "PIO",
			sunxi_udc->nIrq);

	return status == 0 ? sunxi_udc : NULL;
#if 0
	sunxi_udc_gadget_cleanup(sunxi_udc);
#endif
fail3:
	pm_runtime_put_sync(sunxi_udc->controller);

fail2:
	if (sunxi_udc->irq_wake)
		device_init_wakeup(dev, 0);
	sunxi_udc_platform_exit(sunxi_udc);

fail1:
	dev_err(sunxi_udc->controller,
		"sunxi_udc_init_controller failed with status %d\n", status);

	sunxi_udc_free(sunxi_udc);

fail0:
	return status == 0 ? sunxi_udc : NULL;
}
