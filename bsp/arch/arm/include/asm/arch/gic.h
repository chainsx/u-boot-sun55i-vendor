// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2023-2027
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Tom Cubie <tangliang@allwinnertech.com>
 *
 * Some board init for the Allwinner A10-evb board.
 */
#ifndef __SUNXI_INTC_H__
#define __SUNXI_INTC_H__

#include <linux/types.h>
#include <cpu.h>
#include <irq_func.h>

/* processer target */
#define GIC_CPU_TARGET(_n)	(1 << (_n))
#define GIC_CPU_TARGET0		GIC_CPU_TARGET(0)
#define GIC_CPU_TARGET1		GIC_CPU_TARGET(1)
#define GIC_CPU_TARGET2		GIC_CPU_TARGET(2)
#define GIC_CPU_TARGET3		GIC_CPU_TARGET(3)
#define GIC_CPU_TARGET4		GIC_CPU_TARGET(4)
#define GIC_CPU_TARGET5		GIC_CPU_TARGET(5)
#define GIC_CPU_TARGET6		GIC_CPU_TARGET(6)
#define GIC_CPU_TARGET7		GIC_CPU_TARGET(7)
/* trigger mode */
#define GIC_SPI_LEVEL_TRIGGER	(0)	//2b'00
#define GIC_SPI_EDGE_TRIGGER	(2)	//2b'10

#if defined(CONFIG_MACH_SUN8IW21) || defined(CONFIG_MACH_SUN65IW1)
/* GIC registers */
#define GIC_DIST_BASE       (SUNXI_GIC400_BASE+0x1000)
#define GIC_CPUIF_BASE      (SUNXI_GIC400_BASE+0x2000)

#define GIC_CPU_IF_CTRL                 (GIC_CPUIF_BASE + 0x000) // 0x8000
#define GIC_INT_PRIO_MASK               (GIC_CPUIF_BASE + 0x004) // 0x8004
#define GIC_BINARY_POINT                (GIC_CPUIF_BASE + 0x008) // 0x8008
#define GIC_INT_ACK_REG                 (GIC_CPUIF_BASE + 0x00c) // 0x800c
#define GIC_END_INT_REG                 (GIC_CPUIF_BASE + 0x010) // 0x8010
#define GIC_RUNNING_PRIO                (GIC_CPUIF_BASE + 0x014) // 0x8014
#define GIC_HIGHEST_PENDINT             (GIC_CPUIF_BASE + 0x018) // 0x8018
#define GIC_DEACT_INT_REG               (GIC_CPUIF_BASE + 0x1000)// 0x1000
#define GIC_AIAR_REG                    (GIC_CPUIF_BASE + 0x020) // 0x8020
#define GIC_AEOI_REG                    (GIC_CPUIF_BASE + 0x024) // 0x8024
#define GIC_AHIGHEST_PENDINT    (GIC_CPUIF_BASE + 0x028) // 0x8028
#define GIC_IRQ_MOD_CFG(_n)	(GIC_DIST_BASE + 0xc00 + 4 * (_n))
#elif defined(CONFIG_MACH_SUN55IW6)
#include <asm/arch/plat-sun55iw6p1/sunxi_gic.h>
#else
#error "platform not support"
#endif

#define GIC_DIST_CON		(GIC_DIST_BASE + 0x0000)
#define GIC_CON_TYPE		(GIC_DIST_BASE + 0x0004)
#define GIC_CON_IIDR		(GIC_DIST_BASE + 0x0008)

#define GIC_CON_IGRP(n)		(GIC_DIST_BASE + 0x0080 + (n)*4)
#define GIC_SET_EN(_n)		(GIC_DIST_BASE + 0x100 + 4 * (_n))
#define GIC_CLR_EN(_n)		(GIC_DIST_BASE + 0x180 + 4 * (_n))
#define GIC_PEND_SET(_n)	(GIC_DIST_BASE + 0x200 + 4 * (_n))
#define GIC_PEND_CLR(_n)	(GIC_DIST_BASE + 0x280 + 4 * (_n))
#define GIC_ACT_SET(_n)		(GIC_DIST_BASE + 0x300 + 4 * (_n))
#define GIC_ACT_CLR(_n)		(GIC_DIST_BASE + 0x380 + 4 * (_n))
#define GIC_SGI_PRIO(_n)	(GIC_DIST_BASE + 0x400 + 4 * (_n))
#define GIC_PPI_PRIO(_n)	(GIC_DIST_BASE + 0x410 + 4 * (_n))
#define GIC_SPI_PRIO(_n)	(GIC_DIST_BASE + 0x420 + 4 * (_n))
#define GIC_SPI_PROC_TARG(_n)(GIC_DIST_BASE + 0x820 + 4 * (_n))

/* software generated interrupt */
#define GIC_SRC_SGI(_n)		(_n)
/* private peripheral interrupt */
#define GIC_SRC_PPI(_n)		(16 + (_n))
/* external peripheral interrupt */
#define GIC_SRC_SPI(_n)		(32 + (_n))

int  arch_interrupt_init (void);
int  arch_interrupt_exit (void);
int  irq_enable(int irq_no);
int  irq_disable(int irq_no);
void irq_install_handler (int irq, interrupt_handler_t handle_irq, void *data);
void irq_free_handler(int irq);
int  sunxi_gic_cpu_interface_init(int cpu);
int  sunxi_gic_cpu_interface_exit(void);

#endif
