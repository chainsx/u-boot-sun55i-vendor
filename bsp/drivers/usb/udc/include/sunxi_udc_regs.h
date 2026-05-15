/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __UDC_REGS_H__
#define __UDC_REGS_H__

#include <asm/io.h>

#define USB_EP0_FIFOSIZE	64	/* EP0 Fixed at 64B*/

/*
 * ========================================================
 * USB Register bits
 */

/* USB_GCS 0x40 */
#define USB_GCS_ISOUPDATEEN	0x80
#define USB_GCS_SOFTCONN	0x40
#define USB_GCS_HSEN		0x20
#define USB_GCS_HSFLAG		0x10
#define USB_GCS_RESET		0x08
#define USB_GCS_RESUME		0x04
#define USB_GCS_SUSPENDM	0x02
#define USB_GCS_SUSPENDEN	0x01

/* USB_BUSINTF 0x4c */
#define USB_INTR_VBUSERROR	0x80
#define USB_INTR_SESSREQ	0x40
#define USB_INTR_DISCONNECT	0x20
#define USB_INTR_CONNECT	0x10
#define USB_INTR_SOF		0x08
#define USB_INTR_BABBLE		0x04
#define USB_INTR_RESET		0x04
#define USB_INTR_RESUME		0x02
#define USB_INTR_SUSPEND	0x01

/* USB_TESTC 0x7c*/
#define USB_TEST_FORCE_HOST	0x80
#define USB_TEST_FIFO_ACCESS	0x40
#define USB_TEST_FORCE_FS	0x20
#define USB_TEST_FORCE_HS	0x10
#define USB_TEST_PACKET		0x08
#define USB_TEST_K		0x04
#define USB_TEST_J		0x02
#define USB_TEST_SE0_NAK	0x01

/* USB_TXFIFO 0x90 */
/* USB_RXFIFO 0x94*/
/* Allocate for double-fifo */
#define USB_FIFOSZ_DPB		0x10
/* Allocation size (8, 16, 32, ... 4096) */
/* SZ[3:0] */
#define USB_FIFOSZ_SIZE		0x0f

/* USB_CSR0 0x82 */
#define USB_CSR0_FLUSHFIFO	0x0100
#define USB_CSR0_SVDSETUPEND	0x0080
#define USB_CSR0_SVDRXPKTRDY	0x0040
#define USB_CSR0_SENDSTALL	0x0020
#define USB_CSR0_SETUPEND	0x0010
#define USB_CSR0_DATAEND	0x0008
#define USB_CSR0_SENTSTALL	0x0004
#define USB_CSR0_TXPKTRDY	0x0002
#define USB_CSR0_RXPKTRDY	0x0001

/* USB_TXCSR 0x82 */
#define USB_TXCSR_AUTOSET		0x8000
#define USB_TXCSR_ISO			0x4000
#define USB_TXCSR_DMAREQENAB		0x1000
#define USB_TXCSR_FRCDATATOG		0x0800
#define USB_TXCSR_DMAMODE		0x0400
#define USB_TXCSR_INCOMPTX		0x0080
#define USB_TXCSR_CLRDATATOG		0x0040
#define USB_TXCSR_SENTSTALL		0x0020
#define USB_TXCSR_SENDSTALL		0x0010
#define USB_TXCSR_FLUSHFIFO		0x0008
#define USB_TXCSR_UNDERRUN		0x0004
#define USB_TXCSR_FIFONOTEMPTY		0x0002
#define USB_TXCSR_TXPKTRDY		0x0001

/* TXCSR bits to avoid zeroing */
#define USB_TXCSR_P_WZC_BITS	\
	(USB_TXCSR_INCOMPTX | USB_TXCSR_SENTSTALL \
	| USB_TXCSR_UNDERRUN | USB_TXCSR_FIFONOTEMPTY)

/* USB_RXCSR 0x86 */
#define USB_RXCSR_AUTOCLEAR		0x8000
#define USB_RXCSR_ISO			0x4000
#define USB_RXCSR_DMAREQENAB		0x2000
#define USB_RXCSR_DISNYET		0x1000
#define USB_RXCSR_PID_ERR		0x1000
#define USB_RXCSR_DMAMODE		0x0800
#define USB_RXCSR_INCOMPRX		0x0100
#define USB_RXCSR_CLRDATATOG		0x0080
#define USB_RXCSR_SENTSTALL		0x0040
#define USB_RXCSR_SENDSTALL		0x0020
#define USB_RXCSR_FLUSHFIFO		0x0010
#define USB_RXCSR_DATAERROR		0x0008
#define USB_RXCSR_OVERRUN		0x0004
#define USB_RXCSR_FIFOFULL		0x0002
#define USB_RXCSR_RXPKTRDY		0x0001

/* RXCSR bits to avoid zeroing */
#define USB_RXCSR_P_WZC_BITS	\
	(USB_RXCSR_SENTSTALL | USB_RXCSR_OVERRUN \
	| USB_RXCSR_RXPKTRDY)

/*
 * Common USB registers
 */

#define USB_FADDR		0x0098
#define USB_GCS			0x0040

#define USB_INTRTX		0x0044
#define USB_INTRRX		0x0046
#define USB_INTRTXE		0x0048
#define USB_INTRRXE		0x004A
#define USB_INTRUSB		0x004C
#define USB_INTRUSBE		0x0050
#define USB_FNUM		0x0054
#define USB_INDEX		0x0042
#define USB_TESTC		0x007C

/* Get offset for a given FIFO from usb->regs */
#define USB_FIFO_OFFSET(epnum)	(0x00 + ((epnum) * 4))

/*
 * Additional Control Registers
 */
/* Regular Register */
#define USB_TXFIFO		0x0090
#define USB_RXFIFO		0x0094
#define USB_TXFIFOAD		0x0092
#define USB_RXFIFOAD		0x0096

/* Endpoint Registers */
#define USB_TXMAXP		0x0080
#define USB_TXCSR		0x0082
#define USB_CSR0		0x0082
#define USB_RXMAXP		0x0084
#define USB_RXCSR		0x0086
#define USB_RXCOUNT		0x0088
#define USB_COUNT0		0x0088
#define USB_FIFOSIZE		0x0090

/* Offsets to endpoint registers */
#define USB_INDEXED_OFFSET(_epnum, _offset) (_offset)

#define USB_TXCSR_MODE		0x2000

/* Endpoint is selected with USB_INDEX. */
#define USB_BUSCTL_OFFSET(_epnum, _offset) (_offset)

/****************************** RW API *****************************/

static inline u16 usb_readw(const void __iomem *addr, unsigned offset)
	{ return __raw_readw(addr + offset); }

static inline u32 usb_readl(const void __iomem *addr, unsigned offset)
	{ return __raw_readl(addr + offset); }


static inline void usb_writew(void __iomem *addr, unsigned offset, u16 data)
	{ __raw_writew(data, addr + offset); }

static inline void usb_writel(void __iomem *addr, unsigned offset, u32 data)
	{ __raw_writel(data, addr + offset); }

static inline u8 usb_readb(const void __iomem *addr, unsigned offset)
	{ return __raw_readb(addr + offset); }

static inline void usb_writeb(void __iomem *addr, unsigned offset, u8 data)
	{ __raw_writeb(data, addr + offset); }

/****************************** RW FIFO API *****************************/

static inline void usb_write_txfifosz(void __iomem *mbase, u8 c_size)
{
	usb_writeb(mbase, USB_TXFIFO, c_size);
}

static inline void usb_write_txfifoadd(void __iomem *mbase, u16 c_off)
{
	usb_writew(mbase, USB_TXFIFOAD, c_off);
}

static inline void usb_write_rxfifosz(void __iomem *mbase, u8 c_size)
{
	usb_writeb(mbase, USB_RXFIFO, c_size);
}

static inline void  usb_write_rxfifoadd(void __iomem *mbase, u16 c_off)
{
	usb_writew(mbase, USB_RXFIFOAD, c_off);
}

static inline void usb_write_ulpi_buscontrol(void __iomem *mbase, u8 val)
{
	return;
}

static inline u8 usb_read_txfifosz(void __iomem *mbase)
{
	return usb_readb(mbase, USB_TXFIFO);
}

static inline u16 usb_read_txfifoadd(void __iomem *mbase)
{
	return usb_readw(mbase, USB_TXFIFOAD);
}

static inline u8 usb_read_rxfifosz(void __iomem *mbase)
{
	return usb_readb(mbase, USB_RXFIFO);
}

static inline u16  usb_read_rxfifoadd(void __iomem *mbase)
{
	return usb_readw(mbase, USB_RXFIFOAD);
}

static inline u8 usb_read_ulpi_buscontrol(void __iomem *mbase)
{
	return 0;
}

static inline u8 usb_read_configdata(void __iomem *mbase)
{
	return 0xde;
}

static inline u16 usb_read_hwvers(void __iomem *mbase)
{
	return 0; /* Unknown version */
}

static inline void __iomem *usb_read_target_reg_base(u8 i, void __iomem *mbase)
{
	return (USB_BUSCTL_OFFSET(i, 0) + mbase);
}

#endif	/* __UDC_REGS_H__ */
