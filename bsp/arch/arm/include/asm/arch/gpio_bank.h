/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2024-2027
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Tom Cubie <huangrongcun@allwinnertech.com>
 */

#ifndef _SUNXI_GPIO_BANK_H
#define _SUNXI_GPIO_BANK_H

// PIO
#define SUNXI_GPIO_A	0
#define SUNXI_GPIO_B	1
#define SUNXI_GPIO_C	2
#define SUNXI_GPIO_D	3
#define SUNXI_GPIO_E	4
#define SUNXI_GPIO_F	5
#define SUNXI_GPIO_G	6
#define SUNXI_GPIO_H	7
#define SUNXI_GPIO_I	8
#define SUNXI_GPIO_J	9
#define SUNXI_GPIO_K	10

// R_PIO
#define SUNXI_GPIO_L	11
#define SUNXI_GPIO_M	12
#define SUNXI_GPIO_N	13

// PIO
#define PA SUNXI_GPIO_A
#define PB SUNXI_GPIO_B
#define PC SUNXI_GPIO_C
#define PD SUNXI_GPIO_D
#define PE SUNXI_GPIO_E
#define PF SUNXI_GPIO_F
#define PG SUNXI_GPIO_G
#define PH SUNXI_GPIO_H
#define PI SUNXI_GPIO_I
#define PJ SUNXI_GPIO_H
#define PK SUNXI_GPIO_I

// R_PIO
#define PL (SUNXI_GPIO_L - SUNXI_GPIO_L)
#define PM (SUNXI_GPIO_M - SUNXI_GPIO_L)
#define PN (SUNXI_GPIO_N - SUNXI_GPIO_L)

#endif //_SUNXI_GPIO_BANK_H
