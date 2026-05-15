// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 allwinner Co., Ltd.
 * Allwinner PCIe controller driver
 */

#include <common.h>
#include <dm.h>
#include <dm/device_compat.h>
#include "pcie-sunxi.h"

/* Indexed by PCI_EXP_LNKCAP_SLS, PCI_EXP_LNKSTA_CLS */
const unsigned char pcie_link_speed[] = {
	PCI_SPEED_UNKNOWN,		/* 0 */
	PCIE_SPEED_2_5GT,		/* 1 */
	PCIE_SPEED_5_0GT,		/* 2 */
	PCIE_SPEED_8_0GT,		/* 3 */
	PCIE_SPEED_16_0GT,		/* 4 */
	PCIE_SPEED_32_0GT,		/* 5 */
	PCI_SPEED_UNKNOWN,		/* 6 */
	PCI_SPEED_UNKNOWN,		/* 7 */
	PCI_SPEED_UNKNOWN,		/* 8 */
	PCI_SPEED_UNKNOWN,		/* 9 */
	PCI_SPEED_UNKNOWN,		/* A */
	PCI_SPEED_UNKNOWN,		/* B */
	PCI_SPEED_UNKNOWN,		/* C */
	PCI_SPEED_UNKNOWN,		/* D */
	PCI_SPEED_UNKNOWN,		/* E */
	PCI_SPEED_UNKNOWN		/* F */
};

int sunxi_pcie_cfg_read(void __iomem *addr, int size, u32 *val)
{
	if ((uintptr_t)addr & (size - 1)) {
		*val = 0;
		return PCIBIOS_BAD_REGISTER_NUMBER;
	}

	if (size == 4) {
		*val = readl(addr);
	} else if (size == 2) {
		*val = readw(addr);
	} else if (size == 1) {
		*val = readb(addr);
	} else {
		*val = 0;
		return PCIBIOS_BAD_REGISTER_NUMBER;
	}

	return PCIBIOS_SUCCESSFUL;
}

int sunxi_pcie_cfg_write(void __iomem *addr, int size, u32 val)
{
	if ((uintptr_t)addr & (size - 1))
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (size == 4)
		writel(val, addr);
	else if (size == 2)
		writew(val, addr);
	else if (size == 1)
		writeb(val, addr);
	else
		return PCIBIOS_BAD_REGISTER_NUMBER;

	return PCIBIOS_SUCCESSFUL;
}

void sunxi_pcie_writel(u32 val, struct sunxi_pcie *pcie, u32 offset)
{
	writel(val, pcie->app_base + offset);
}

u32 sunxi_pcie_readl(struct sunxi_pcie *pcie, u32 offset)
{
	return readl(pcie->app_base + offset);
}

void sunxi_pcie_write_dbi(struct sunxi_pcie *pci, u32 reg, size_t size, u32 val)
{
	int ret;

	ret = sunxi_pcie_cfg_write(pci->dbi_base + reg, size, val);
	if (ret)
		dev_err(pci->dev, "Write DBI address failed\n");
}

u32 sunxi_pcie_read_dbi(struct sunxi_pcie *pci, u32 reg, size_t size)
{
	int ret;
	u32 val;

	ret = sunxi_pcie_cfg_read(pci->dbi_base + reg, size, &val);
	if (ret)
		dev_err(pci->dev, "Read DBI address failed\n");

	return val;
}

void sunxi_pcie_writel_dbi(struct sunxi_pcie *pci, u32 reg, u32 val)
{
	sunxi_pcie_write_dbi(pci, reg, 0x4, val);
}

u32 sunxi_pcie_readl_dbi(struct sunxi_pcie *pci, u32 reg)
{
	return sunxi_pcie_read_dbi(pci, reg, 0x4);
}

void sunxi_pcie_writew_dbi(struct sunxi_pcie *pci, u32 reg, u16 val)
{
	sunxi_pcie_write_dbi(pci, reg, 0x2, val);
}

u16 sunxi_pcie_readw_dbi(struct sunxi_pcie *pci, u32 reg)
{
	return sunxi_pcie_read_dbi(pci, reg, 0x2);
}

void sunxi_pcie_writeb_dbi(struct sunxi_pcie *pci, u32 reg, u8 val)
{
	sunxi_pcie_write_dbi(pci, reg, 0x1, val);
}

u8 sunxi_pcie_readb_dbi(struct sunxi_pcie *pci, u32 reg)
{
	return sunxi_pcie_read_dbi(pci, reg, 0x1);
}

void sunxi_pcie_dbi_ro_wr_en(struct sunxi_pcie *pci)
{
	u32 val;

	val = sunxi_pcie_readl_dbi(pci, PCIE_MISC_CONTROL_1_CFG);
	val |= (0x1 << 0);
	sunxi_pcie_writel_dbi(pci, PCIE_MISC_CONTROL_1_CFG, val);
}

void sunxi_pcie_dbi_ro_wr_dis(struct sunxi_pcie *pci)
{
	u32 val;

	val = sunxi_pcie_readl_dbi(pci, PCIE_MISC_CONTROL_1_CFG);
	val &= ~(0x1 << 0);
	sunxi_pcie_writel_dbi(pci, PCIE_MISC_CONTROL_1_CFG, val);
}

void sunxi_pcie_plat_ltssm_enable(struct sunxi_pcie *pcie)
{
	u32 val;

	val = sunxi_pcie_readl(pcie, PCIE_LTSSM_CTRL);
	val |= PCIE_LINK_TRAINING;
	sunxi_pcie_writel(val, pcie, PCIE_LTSSM_CTRL);
}

void sunxi_pcie_plat_ltssm_disable(struct sunxi_pcie *pcie)
{
	u32 val;

	val = sunxi_pcie_readl(pcie, PCIE_LTSSM_CTRL);
	val &= ~PCIE_LINK_TRAINING;
	sunxi_pcie_writel(val, pcie, PCIE_LTSSM_CTRL);
}

static u8 __sunxi_pcie_find_next_cap(struct sunxi_pcie *pci, u8 cap_ptr,
						u8 cap)
{
	u8 cap_id, next_cap_ptr;
	u16 reg;

	if (!cap_ptr)
		return 0;

	reg = sunxi_pcie_readw_dbi(pci, cap_ptr);
	cap_id = (reg & CAP_ID_MASK);

	if (cap_id > PCI_CAP_ID_MAX)
		return 0;

	if (cap_id == cap)
		return cap_ptr;

	next_cap_ptr = (reg & NEXT_CAP_PTR_MASK) >> 8;
	return __sunxi_pcie_find_next_cap(pci, next_cap_ptr, cap);
}

static u8 sunxi_pcie_plat_find_capability(struct sunxi_pcie *pci, u8 cap)
{
	u8 next_cap_ptr;
	u16 reg;

	reg = sunxi_pcie_readw_dbi(pci, PCI_CAPABILITY_LIST);
	next_cap_ptr = (reg & CAP_ID_MASK);

	return __sunxi_pcie_find_next_cap(pci, next_cap_ptr, cap);
}


static void sunxi_pcie_plat_set_link_cap(struct sunxi_pcie *pci, u32 link_gen)
{
	u32 cap, ctrl2, link_speed;

	u8 offset = sunxi_pcie_plat_find_capability(pci, PCI_CAP_ID_EXP);

	cap = sunxi_pcie_readl_dbi(pci, offset + PCI_EXP_LNKCAP);
	ctrl2 = sunxi_pcie_readl_dbi(pci, offset + PCI_EXP_LNKCTL2);
	ctrl2 &= ~PCI_EXP_LNKCTL2_TLS;

	switch (pcie_link_speed[link_gen]) {
	case PCIE_SPEED_2_5GT:
		link_speed = PCI_EXP_LNKCTL2_TLS_2_5GT;
		break;
	case PCIE_SPEED_5_0GT:
		link_speed = PCI_EXP_LNKCTL2_TLS_5_0GT;
		break;
	case PCIE_SPEED_8_0GT:
		link_speed = PCI_EXP_LNKCTL2_TLS_8_0GT;
		break;
	case PCIE_SPEED_16_0GT:
		link_speed = PCI_EXP_LNKCTL2_TLS_16_0GT;
		break;
	default:
		/* Use hardware capability */
		// link_speed = FIELD_GET(PCI_EXP_LNKCAP_SLS, cap);
		ctrl2 &= ~PCI_EXP_LNKCTL2_HASD;
		break;
	}

	sunxi_pcie_writel_dbi(pci, offset + PCI_EXP_LNKCTL2, ctrl2 | link_speed);

	cap &= ~((u32)PCI_EXP_LNKCAP_SLS);
	sunxi_pcie_writel_dbi(pci, offset + PCI_EXP_LNKCAP, cap | link_speed);
}

void sunxi_pcie_plat_set_rate(struct sunxi_pcie *pci)
{
	u32 val;

	sunxi_pcie_plat_set_link_cap(pci, pci->link_gen);
	/* set the number of lanes */
	val = sunxi_pcie_readl_dbi(pci, PCIE_PORT_LINK_CONTROL);
	val &= ~PORT_LINK_MODE_MASK;

	switch (pci->lanes) {
	case 1:
		val |= PORT_LINK_MODE_1_LANES;
		break;
	case 2:
		val |= PORT_LINK_MODE_2_LANES;
		break;
	case 4:
		val |= PORT_LINK_MODE_4_LANES;
		break;
	default:
		dev_err(pci->dev, "num-lanes %u: invalid value\n", pci->lanes);
		return;
	}
	sunxi_pcie_writel_dbi(pci, PCIE_PORT_LINK_CONTROL, val);

	/* set link width speed control register */
	val = sunxi_pcie_readl_dbi(pci, PCIE_LINK_WIDTH_SPEED_CONTROL);
	val &= ~PORT_LOGIC_LINK_WIDTH_MASK;
	switch (pci->lanes) {
	case 1:
		val |= PORT_LOGIC_LINK_WIDTH_1_LANES;
		break;
	case 2:
		val |= PORT_LOGIC_LINK_WIDTH_2_LANES;
		break;
	case 4:
		val |= PORT_LOGIC_LINK_WIDTH_4_LANES;
		break;
	}
	sunxi_pcie_writel_dbi(pci, PCIE_LINK_WIDTH_SPEED_CONTROL, val);
}

void sunxi_pcie_plat_set_mode(struct sunxi_pcie *pci)
{
	u32 val;

	switch (pci->drvdata->mode) {
	case SUNXI_PCIE_EP_TYPE:
		val = sunxi_pcie_readl(pci, PCIE_LTSSM_CTRL);
		val &= ~DEVICE_TYPE_MASK;
		sunxi_pcie_writel(val, pci, PCIE_LTSSM_CTRL);
		break;
	case SUNXI_PCIE_RC_TYPE:
		val = sunxi_pcie_readl(pci, PCIE_LTSSM_CTRL);
		val |= DEVICE_TYPE_RC;
		sunxi_pcie_writel(val, pci, PCIE_LTSSM_CTRL);
		break;
	default:
		dev_err(pci->dev, "unsupported device type:%d\n", pci->drvdata->mode);
		break;
	}
}