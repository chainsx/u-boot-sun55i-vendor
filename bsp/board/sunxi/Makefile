# SPDX-License-Identifier: GPL-2.0+
#
# (C) Copyright 2012 Henrik Nordstrom <henrik@henriknordstrom.net>
#
# Based on some other board Makefile
#
# (C) Copyright 2000-2003
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.
obj-y	+= board.o
obj-$(CONFIG_SUN7I_GMAC)	+= gmac.o
obj-$(CONFIG_MACH_SUN4I)	+= dram_sun4i_auto.o
obj-$(CONFIG_MACH_SUN5I)	+= dram_sun5i_auto.o
obj-$(CONFIG_MACH_SUN7I)	+= dram_sun5i_auto.o
obj-$(CONFIG_CHIP_DIP_SCAN)	+= chip.o

obj-y += sunxi_challenge.o
obj-$(CONFIG_DM_PMIC)  += power_manage.o
obj-$(CONFIG_DM_PMIC)  += board_helper.o
obj-y	+= board_common.o

obj-$(CONFIG_SUNXI_IMAGE_VERIFIER) += sunxi_image_verifier.o
obj-$(CONFIG_SUNXI_SECURE_BOOT) += sunxi_cert_interface.o

obj-$(CONFIG_OPTEE25) += sunxi_smc_v2.o

obj-$(CONFIG_SUNXI_PRIVATE_STORAGE)  += privatestorage/
obj-$(CONFIG_SUNXI_KEYBOX)  += sunxi_keybox.o
obj-$(CONFIG_SUNXI_BOOT_PARAM) += sunxi_boot_param.o

obj-$(CONFIG_SUNXI_SPRITE) += sprite/

obj-$(CONFIG_CMD_SUNXI_BURN)    += key_burn.o
obj-$(CONFIG_CMD_SUNXI_AUTO_FEL)    += sunxi_auto_fel.o

obj-y += sunxi_part.o

obj-$(CONFIG_AW_EFUSE) += board_sunxi_efuse.o
obj-$(CONFIG_SUNXI_SERIAL) += sunxi_serial.o
obj-$(CONFIG_SUNXI_MAC) += sunxi_mac.o

obj-$(CONFIG_SUNXI_LOG) += sunxi_log.o
obj-$(CONFIG_SUNXI_IMAGE_HEADER) += sunxi_image_header.o
obj-$(CONFIG_SUNXI_ANDROID_OVERLAY) += ufdt_support.o

obj-$(CONFIG_SUNXI_SWITCH_SYSTEM) += sunxi_switch_system.o
