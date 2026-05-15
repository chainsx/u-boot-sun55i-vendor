// SPDX-License-Identifier: GPL-2.0+
/*
 * drivers/riscv/sun8iw21/riscv.c
 *
 * Copyright (c) 2007-2025 Allwinnertech Co., Ltd.
 * Author: wujiayi <wujiayi@allwinnertech.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details
 *
 */

#include <asm/io.h>
#include <common.h>
#include <sys_config.h>
#include <sunxi_image_verifier.h>

#include "platform.h"
#include "elf.h"
#include "fdt_support.h"
#include "riscv_reg.h"
#include "../common/riscv_fdt.h"
#include "../common/riscv_img.h"
#include "../common/riscv_ic.h"
#ifdef CONFIG_AW_REMOTEPROC
#include <remoteproc.h>
#include <sunxi_rproc.h>

#ifdef CONFIG_SUNXI_IMAGE_HEADER
#include <sunxi_image_header.h>
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr)		(sizeof(arr) / sizeof((arr)[0]))
#endif
#endif

#define readl_riscv(addr)	readl((const volatile void*)(addr))
#define writel_riscv(val, addr)	writel((u32)(val), (volatile void*)(addr))

/*
 * riscv need to remap addresses for some addr.
 */
static struct vaddr_range_t addr_mapping[] = {
	{ 0x00044000, 0x0006BFFF, 0x00044000 },
	{ 0x40000000, 0x7FFFFFFF, 0x40000000},
};

static int get_image_len(ulong img_addr, u32 *img_len)
{
	int i = 0;
	int ret = -1;
	struct spare_rtos_head_t *prtos = NULL;
	Elf32_Ehdr *ehdr = NULL; /* Elf header structure pointer */
	Elf32_Phdr *phdr = NULL; /* Program header structure pointer */

	ehdr = (Elf32_Ehdr *)img_addr;
	phdr = (Elf32_Phdr *)(img_addr + ehdr->e_phoff);

	for (i = 0; i < ehdr->e_phnum; ++i) {
		if (!(unsigned long)phdr->p_paddr) {
			prtos = (struct spare_rtos_head_t *)(img_addr
					+ phdr->p_offset);
			*img_len = prtos->rtos_img_hdr.image_size;
			ret = 0;
			break;
		}
		++phdr;
	}

	return ret;
}

extern int rv_fw_partition_sectors;
extern int rv_fw_partitions_num;
extern const char **rv_fw_partitions;
int sunxi_riscv_init(ulong img_addr, ulong run_addr, u32 riscv_id)
{
	u32 reg_val;
	u32 image_len = 0;
	int ret;
	const char *fw_version = NULL;
	int map_size;
#ifdef CONFIG_AW_REMOTEPROC
	int id;
	const char *name[1] = {"e907_rproc@1a00000"};
#endif

       image_len = get_image_len(img_addr, &image_len);

	/* A523 Only one riscv core, ignore riscv_id arguent */
	(void)riscv_id;

	/* update run addr */
	if (!run_addr)
		run_addr = get_elf_fw_entry(img_addr);

#ifdef CONFIG_RISCV_UPDATA_IRQ_TAB
	update_riscv_irq_tab(img_addr, 0);
#endif

	/* load image to ram */
	map_size = sizeof(addr_mapping) / sizeof(struct vaddr_range_t);
	ret = load_elf_fw(img_addr, addr_mapping, map_size);
	if (ret) {
		printf("load elf fw faild, ret: %d\n", ret);
		return -2;
	}

	fw_version = get_elf_fw_version(img_addr, addr_mapping, map_size);
	if (fw_version) {
		show_img_version(fw_version, riscv_id);
	} else {
		printf("get elf fw version failed\n");
	}

#ifdef CONFIG_AW_REMOTEPROC
	do {
		if (riscv_id >= ARRAY_SIZE(name)) {
			pr_err("get name failed: %d\n", riscv_id);
			break;
		}

		ret = sunxi_remoteproc_get_id_by_name(name[riscv_id]);
		if (ret < 0) {
			pr_err("%s - get_id failed: %d\n", name[riscv_id], ret);
			break;
		}
		id = ret;

		ret = sunxi_remoteproc_set_fw_partitions(id, rv_fw_partitions, rv_fw_partitions_num,
							 rv_fw_partition_sectors);
		if (ret < 0) {
			pr_err("%s - set_fw_partitions failed: %d\n", name[riscv_id], ret);
			break;
		}

		ret = rproc_load(id, img_addr, 0);
		if (ret) {
			pr_err("%s - load failed: %d\n", name[riscv_id], ret);
			break;
		}

		ret = rproc_start(id);
		if (ret) {
			pr_err("%s - start failed: %d\n", name[riscv_id], ret);
			continue;
		}

		pr_info("%s - boot ok\n", name[riscv_id]);
	} while (0);
#endif

	/* De-assert core & sys reset */
	reg_val = readl_riscv(RISCV_CCMU_BASE + CCMU_RV_SYS_BGR_REG);
	reg_val |= BIT_RISCV_SYS_RST;
	reg_val |= BIT_RISCV_CORE_RST;
	writel_riscv(reg_val, RISCV_CCMU_BASE + CCMU_RV_SYS_BGR_REG);

	/* De-assert RISCV Cfg Clock and Gating */
	reg_val = readl_riscv(RISCV_CCMU_BASE + CCMU_RV_CFG_BGR_REG);
	reg_val |= BIT_RISCV_CFG_RST;
	reg_val |= BIT_RISCV_CFG_GATEING;
	writel_riscv(reg_val, RISCV_CCMU_BASE + CCMU_RV_CFG_BGR_REG);

	/* set start addr */
	reg_val = run_addr;
	writel_riscv(reg_val, RISCV_CFG_BASE + RISCV_STA_ADD_REG);

	/* set timestamp clk & enable */
	reg_val = readl_riscv(RISCV_CCMU_BASE + CCMU_RV_TS_CLK_REG);
	reg_val |= BIT_RISCV_TS_CLK_SEL;
	reg_val |= BIT_RISCV_TS_CLK_GAIING;
	writel_riscv(reg_val, RISCV_CCMU_BASE + CCMU_RV_TS_CLK_REG);

	/* set clk & rst  RISCV_E907 CLK=SRC/M */
	reg_val = readl_riscv(RISCV_CCMU_BASE + CCMU_RV_CORE_CLK_REG);
	reg_val &= ~(0x1f << 0);
	/* core clk default use parent clk frequency, not scaling down */
	reg_val |= (0 << BIT_RISCV_CORE_CLK_FATOR_M); //FACTOR_M,  M=FACTOR_M+1
	/* axi clk default use halt of core clk frequency */
	reg_val |= (1 << BIT_RISCV_AXI_CLK_FATOR_N); //FACTOR_N,  N=FACTOR_N+1
	writel_riscv(reg_val, RISCV_CCMU_BASE + CCMU_RV_CORE_CLK_REG);
	/* SRC 0:HOSC  1:RC16M  2:CLK32K  3:PERI0_600M  4:PERI0_480M  5:PERI0_400M */
	/* core clk depend on vdd-sys, vol equal with 0.94v, core clk set as 600M,
		otherwise vol equla with 0.92v, set as 480m */
	reg_val |= (0x4 << BIT_RISCV_CORE_CLK_SRC);
	writel_riscv(reg_val, RISCV_CCMU_BASE + CCMU_RV_CORE_CLK_REG);
	reg_val |= 	BIT_RISCV_CORE_CLK_GAIING;
	writel_riscv(reg_val, RISCV_CCMU_BASE + CCMU_RV_CORE_CLK_REG);

	RISCV_DEBUG("RISCV start ok, img length %d, boot addr 0x%lx\n",
						image_len, run_addr);

	return 0;
}
