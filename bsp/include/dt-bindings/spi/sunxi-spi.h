/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * Author: jingyanliang@allwinner.com
 */

#ifndef __DT_SUNXI_SPI_H
#define __DT_SUNXI_SPI_H

#define SUNXI_SPI_BUS_MASTER	(1 << 0)
#define SUNXI_SPI_BUS_FLASH		(1 << 1)

#define SUNXI_SPI_BUS_NOR   SUNXI_SPI_BUS_FLASH
#define SUNXI_SPI_BUS_NAND  SUNXI_SPI_BUS_FLASH

#define SUNXI_SPI_CS_AUTO	(0)
#define SUNXI_SPI_CS_SOFT	(1)

#endif /* __DT_SUNXI_SPI_H */

