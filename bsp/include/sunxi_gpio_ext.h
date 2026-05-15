/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2023-2027
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Tom Cubie <huangrongcun@allwinnertech.com>
 */

#ifndef _SUNXI_GPIO_EXT_H
#define _SUNXI_GPIO_EXT_H

//sunxi_gpio_reg->gpio_bank[BANK_NUM]->cfg
static inline void sunxi_gpio_ext_set_bank_cfg(u32 *bank_cfg, u32 pin, u32 val)
{
	u32 index = GPIO_CFG_INDEX(pin);
	u32 offset = GPIO_CFG_OFFSET(pin);
	clrsetbits_le32(&bank_cfg[index], 0xf << offset, val << offset);
}

//sunxi_gpio_reg->gpio_bank[BANK_NUM]->cfg
static inline u32 sunxi_gpio_ext_get_bank_cfg(u32 *bank_cfg, u32 pin)
{
	u32 index = GPIO_CFG_INDEX(pin);
	u32 offset = GPIO_CFG_OFFSET(pin);
	u32 cfg;
	cfg = readl(&bank_cfg[index]);

	cfg >>= offset;
	return cfg & 0xf;
}

//sunxi_gpio_reg->gpio_bank[BANK_NUM]->pull
static inline void sunxi_gpio_ext_set_bank_pull(u32 *bank_pull, u32 pin, u32 val)
{
	u32 index = GPIO_PULL_INDEX(pin);
	u32 offset = GPIO_PULL_OFFSET(pin);
	clrsetbits_le32(&bank_pull[index], 0x3 << offset, val << offset);
}

//sunxi_gpio_reg->gpio_bank[BANK_NUM]->drv
static inline void sunxi_gpio_ext_set_bank_drv(u32 *bank_drv, u32 pin, u32 val)
{
	u32 index = GPIO_DRV_INDEX(pin);
	u32 offset = GPIO_DRV_OFFSET(pin);
	clrsetbits_le32(&bank_drv[index], 0x3 << offset, val << offset);
}

#endif /* _SUNXI_GPIO_EXT_H */
