/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Configuration settings for the Allwinner A7 (sun8i) CPU
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#ifdef CONFIG_USB_EHCI_HCD
#define CONFIG_USB_EHCI_SUNXI
#define CONFIG_USB_MAX_CONTROLLER_COUNT 1
#endif

#define CONFIG_SUNXI_USB_PHYS	1

#define AW_IRQ_USB_OTG                 61
#define AW_IRQ_USB_EHCI0               62
#define AW_IRQ_USB_OHCI0               63
#define AW_IRQ_DMA                     82
#define AW_IRQ_TIMER0                  91
#define AW_IRQ_TIMER1                  92
#define AW_IRQ_NMI                     168
#define GIC_IRQ_NUM                   	223

/* sram layout*/

#define SUNXI_SRAM_A1_BASE		(0x20000L)
#define SUNXI_SRAM_A1_SIZE		(0x8000)

#define SUNXI_SYS_SRAM_BASE		SUNXI_SRAM_A1_BASE
#define SUNXI_SYS_SRAM_SIZE		(SUNXI_SRAM_A1_SIZE)

#define PHOENIX_PRIV_DATA_ADDR      (SUNXI_SYS_SRAM_BASE + 0x20400)//给phoenix保留的空间

#ifdef CONFIG_SUNXI_MALLOC_LEN
#define SUNXI_SYS_MALLOC_LEN	CONFIG_SUNXI_MALLOC_LEN
#else
#define SUNXI_SYS_MALLOC_LEN	(32 << 20)
#endif

/*
 * Include common sunxi configuration where most the settings are
 */
#include <configs/sunxi-common.h>

#endif /* __CONFIG_H */
