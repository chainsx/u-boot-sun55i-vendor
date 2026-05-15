# SPDX-License-Identifier: GPL-2.0+
#
# (C) Copyright 2012 Henrik Nordstrom <henrik@henriknordstrom.net>
#
# Based on some other Makefile
# (C) Copyright 2000-2003
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.

ifeq ($(CONFIG_SUNXI_GIC_V3),y)
obj-y	+= gic_v3.o
else
obj-y	+= gic.o
endif

ifdef CONFIG_ARM64
obj-$(CONFIG_AW_STACKTRACE)	+= stacktrace_64.o
else ifdef CONFIG_ARM
obj-$(CONFIG_AW_STACKTRACE)	+= stacktrace.o
endif

obj-$(CONFIG_CPU_V7A)	+= interrupts.o

obj-y	+= rtc.o

obj-y	+= board.o
obj-y	+= clock.o
obj-y	+= cpu_info.o
obj-y	+= dram_helpers.o
obj- += dummy.o

obj-$(CONFIG_SUNXI_GPIO_V1)$(CONFIG_SUNXI_GPIO_V2) += pinmux.o
obj-$(CONFIG_SUN6I_PRCM)	+= prcm.o
obj-$(CONFIG_AXP_PMIC_BUS)	+= pmic_bus.o
obj-$(CONFIG_MACH_SUNIV)	+= clock_sun6i.o
obj-$(CONFIG_MACH_SUN4I)	+= clock_sun4i.o
obj-$(CONFIG_MACH_SUN5I)	+= clock_sun4i.o
obj-$(CONFIG_MACH_SUN6I)	+= clock_sun6i.o
obj-$(CONFIG_MACH_SUN7I)	+= clock_sun4i.o
obj-$(CONFIG_MACH_SUN50I)	+= clock_sun6i.o
ifdef CONFIG_MACH_SUN8I_A83T
obj-y				+= clock_sun8i_a83t.o
else
obj-$(CONFIG_MACH_SUN8I)	+= clock_sun6i.o
endif
obj-$(CONFIG_MACH_SUN9I)	+= clock_sun9i.o gtbus_sun9i.o
obj-$(CONFIG_SUN50I_GEN_H6)	+= clock_sun50i_h6.o
ifndef CONFIG_ARM64
obj-y	+= timer.o
endif

extra-y	+= spare_head.o

ifdef CONFIG_SPL_BUILD
obj-$(CONFIG_MACH_SUNIV)	+= dram_suniv.o
obj-$(CONFIG_DRAM_SUN4I)	+= dram_sun4i.o
obj-$(CONFIG_DRAM_SUN6I)	+= dram_sun6i.o
obj-$(CONFIG_DRAM_SUN8I_A23)	+= dram_sun8i_a23.o
obj-$(CONFIG_DRAM_SUN8I_A33)	+= dram_sun8i_a33.o
obj-$(CONFIG_DRAM_SUN8I_A83T)	+= dram_sun8i_a83t.o
obj-$(CONFIG_DRAM_SUN9I)	+= dram_sun9i.o
obj-$(CONFIG_SPL_SPI_SUNXI)	+= spl_spi_sunxi.o
obj-$(CONFIG_SUNXI_DRAM_DW)	+= dram_sunxi_dw.o
obj-$(CONFIG_SUNXI_DRAM_DW)	+= dram_timings/
obj-$(CONFIG_DRAM_SUN50I_H6)	+= dram_sun50i_h6.o
obj-$(CONFIG_DRAM_SUN50I_H6)	+= dram_timings/
obj-$(CONFIG_DRAM_SUN50I_H616)	+= dram_sun50i_h616.o
obj-$(CONFIG_DRAM_SUN50I_H616)	+= dram_timings/
endif

obj-$(CONFIG_MACH_SUN8IW21) += clock_sun8iw21.o board_sun8iw21.o
obj-$(CONFIG_MACH_SUN55IW6) += clock_sun55iw6.o board_sun55iw6.o
obj-$(CONFIG_ARM64) += sunxi_timer.o
