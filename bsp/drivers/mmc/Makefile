# SPDX-License-Identifier: GPL-2.0+
#
# (C) Copyright 2006
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.

ccflags-y += -I$(srctree)/drivers/mmc

obj-$(CONFIG_AW_MMC)			+= mmc.o sunxi_mmc.o sunxi_mmc_tuning.o sunxi_mmc_interface.o host/
obj-$(CONFIG_$(SPL_TPL_)MMC_WRITE)	+= mmc_write.o
