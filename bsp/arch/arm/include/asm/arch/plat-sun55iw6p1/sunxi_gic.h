/*
 * (C) Copyright 2024-2027
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 *
 * huangrongcun <huangrongcun@allwinnertech.com>
 * SPDX-License-Identifier:     GPL-2.0+
 */

#ifndef __SUNXI_GIC_H__
#define __SUNXI_GIC_H__

#define AW_IRQ_GIC_START               (32)
#define AW_IRQ_USB_OTG                 (61)
#define AW_IRQ_USB_EHCI0               (62)
#define AW_IRQ_USB_OHCI0               (63)
#define AW_IRQ_USB_EHCI1               (64)
#define AW_IRQ_USB_OHCI1               (65)
#define AW_IRQ_DMA                     (82)
#define AW_IRQ_TIMER0                  (87)
#define AW_IRQ_TIMER1                  (88)
#define GIC_IRQ_NUM                    (287)


#define GIC_DIST_BASE        (SUNXI_GIC600_BASE)
#define GIC_IROUTR(_n)       (GIC_DIST_BASE + 8 * (_n) + 0x6000)
#define GICR_LPI_BASE(n)     (GIC_DIST_BASE + 0x60000 + n*0x20000)
#define GICR_WAKER(m)        (GICR_LPI_BASE(m) + 0x0014)
#define GICR_PWRR(m)         (GICR_LPI_BASE(m) + 0x0024)
#define put_wvalue(addr, v)  (*((volatile int *)(addr)) = (unsigned int)(v))
#define get_wvalue(addr)     (*((volatile int *)(addr)))
#define LEVEL_TRIGERRED      (0)
#define EDGE_TRIGERRED       (1)
#define GIC_IRQ_TYPE_CFG(_n)	(GIC_DIST_BASE + 0xc00 + 4 * (_n))
#define GIC_IRQ_MOD_CFG(_n)	(GIC_DIST_BASE + 0xd00 + 4 * (_n))

#endif //__SUNXI_GIC_H__