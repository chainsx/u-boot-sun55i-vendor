/* SPDX-License-Identifier: GPL-2.0+ */
#include <asm/arch/plat-sun65iw1p1/cpu_autogen.h>
#define SUNXI_TIMER_BASE              (SUNXI_TIMER0_BASE)

#define SUNXI_PIO_BASE                (SUNXI_GPIO_BASE)
#define SUNXI_R_PIO_BASE              (SUNXI_S_GPIO_BASE)
#define SUNXI_RTC_DATA_BASE           (SUNXI_SYSRTC_BASE+0x100)
/*#define SUNXI_RSB_BASE*/
#define SUNXI_CCM_BASE                (SUNXI_CCMU_BASE)
#define SUNXI_EHCI1_BASE              (SUNXI_USB0_BASE + 0x1000)
#define SUNXI_USBOTG_BASE             (SUNXI_USB0_BASE)
#define SUNXI_DMA_BASE                (SUNXI_DMAC_BASE)
#define SUNXI_MMC0_BASE               (SUNXI_SMHC0_BASE)
#define SUNXI_MMC1_BASE               (SUNXI_SMHC1_BASE)
#define SUNXI_MMC2_BASE               (SUNXI_SMHC2_BASE)
#define SUNXI_NFC_BASE                (SUNXI_NAND_BASE)
#define SUNXI_SS_BASE                 (SUNXI_CE_NS_BASE)
#define SUNXI_WDT_BASE                (0x02050000)

#define SUNXI_RTWI_BRG_REG            (SUNXI_PRCM_BASE+0x019c)
#define SUNXI_RTC_BASE                   (SUNXI_SYSRTC_BASE)

#define SUNXI_GIC400_BASE		SUNXI_CPU_GIC400_BASE

