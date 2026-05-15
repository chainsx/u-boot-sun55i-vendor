// SPDX-License-Identifier: GPL-2.0
/*
 * Allwinner PIPE USB3.0 PCIE Combo Phy driver
 *
 * Copyright(c) 2020 - 2024 Allwinner Technology Co.,Ltd. All rights reserved.
 *
 * sunxi-inno-combophy.c:	chenhuaqiang <chenhuaqiang@allwinnertech.com>
 */

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <generic-phy.h>
#include <regmap.h>
#include <reset-uclass.h>
#include <syscon.h>
#include <asm/io.h>
#include <dm/device_compat.h>
#include <dm/lists.h>
#include <power/regulator.h>
#include <dt-bindings/phy/phy.h>

/* PCIE USB3 Sub-System Registers */
/* Sub-System Version Reset Register */
#define PCIE_USB3_SYS_VER		0x00

/* Sub-System PCIE Bus Gating Reset Register */
#define PCIE_COMBO_PHY_BGR		0x04
#define   PCIE_SLV_ACLK_EN		BIT(18)
#define   PCIE_ACLK_EN			BIT(17)
#define   PCIE_HCLK_EN			BIT(16)
#define   PCIE_PERSTN			BIT(1)
#define   PCIE_PW_UP_RSTN		BIT(0)

/* Sub-System USB3 Bus Gating Reset Register */
#define USB3_COMBO_PHY_BGR		0x08
#define   USB3_ACLK_EN			BIT(17)
#define   USB3_HCLK_EN			BIT(16)
#define   USB3_U2_PHY_RSTN		BIT(4)
#define   USB3_U2_PHY_MUX_EN		BIT(3)
#define   USB3_U2_PHY_MUX_SEL		BIT(0)
#define   USB3_RESETN			BIT(0)

/* Sub-System PCIE PHY Control Register */
#define PCIE_COMBO_PHY_CTL		0x10
#define   PHY_USE_SEL			BIT(31)	/* 0:PCIE; 1:USB3 */
#define   PHY_CLK_SEL			BIT(30) /* 0:internal clk; 1:external clk */
#define   PHY_BIST_EN			BIT(16)
#define   PHY_PIPE_SW			BIT(9)
#define   PHY_PIPE_SEL			BIT(8)  /* 0:rstn by PCIE or USB3; 1:rstn by PHY_PIPE_SW */
#define   PHY_PIPE_CLK_INVERT		BIT(4)
#define   PHY_FPGA_SYS_RSTN		BIT(1)  /* for FPGA  */
#define   PHY_RSTN			BIT(0)

/* Registers */
#define  COMBO_REG_SYSVER(comb_base_addr)	((comb_base_addr) \
							+ PCIE_USB3_SYS_VER)
#define  COMBO_REG_PCIEBGR(comb_base_addr)	((comb_base_addr) \
							+ PCIE_COMBO_PHY_BGR)
#define  COMBO_REG_USB3BGR(comb_base_addr)	((comb_base_addr) \
							+ USB3_COMBO_PHY_BGR)
#define  COMBO_REG_PHYCTRL(comb_base_addr)	((comb_base_addr) \
							+ PCIE_COMBO_PHY_CTL)

/* Sub-System Version Number */
#define  COMBO_VERSION_01				(0x10000)
#define  COMBO_VERSION_ANY				(0x0)

#define  KEY_PHY_USE_SEL				"phy_use_sel"
#define  KEY_PHY_REFCLK_SEL				"phy_refclk_sel"

enum phy_use_sel {
	PHY_USE_BY_PCIE = 0, /* PHY used by PCIE */
	PHY_USE_BY_USB3, /* PHY used by USB3 */
	PHY_USE_BY_PCIE_USB3_U2,/* PHY used by PCIE & USB3_U2 */
};

enum phy_refclk_sel {
	INTER_SIG_REF_CLK = 0, /* PHY use internal single end reference clock */
	EXTER_DIF_REF_CLK, /* PHY use external single end reference clock */
};

struct sunxi_combophy_of_data {
	bool has_cfg_clk;
	bool has_slv_clk;
	bool has_phy_mbus_clk;
	bool has_phy_ahb_clk;
	bool has_pcie_axi_clk;
	bool has_u2_phy_mux;
	bool need_noppu_rst;
};

struct sunxi_combphy {
	struct device *dev;
	struct phy *phy;
	fdt_addr_t phy_ctl;  /* parse dts, control the phy mode, reset and power */
	fdt_addr_t phy_clk;  /* parse dts, set the phy clock */
	struct reset_ctl reset;
	struct reset_ctl noppu_reset;

	struct clk phyclk_ref;
	struct clk refclk_par;
	struct clk phyclk_cfg;
	struct clk cfgclk_par;
	struct clk phy_mclk;
	struct clk phy_hclk;
	struct clk phy_axi;
	struct clk phy_axi_par;
	__u8 mode;
	__u32 vernum; /* version number */
	enum phy_use_sel user;
	enum phy_refclk_sel ref;
	struct notifier_block pwr_nb;
	const struct sunxi_combophy_of_data *drvdata;

	struct udevice *select3v3_supply;
	bool initialized;
};

static void sunxi_combphy_pcie_phy_enable(struct sunxi_combphy *combphy)
{
	u32 val;

	/* set the phy:
	 * bit(18): slv aclk enable
	 * bit(17): aclk enable
	 * bit(16): hclk enbale
	 * bit(1) : pcie_presetn
	 * bit(0) : pcie_power_up_rstn
	 */
	val = readl(combphy->phy_ctl + PCIE_COMBO_PHY_BGR);
	val &= (~(0x03<<0));
	val &= (~(0x03<<16));
	val |= (0x03<<0);
	if (combphy->drvdata->has_slv_clk)
		val |= (0x07<<16);
	else
		val |= (0x03<<16);
	writel(val, combphy->phy_ctl + PCIE_COMBO_PHY_BGR);


	/* select phy mode, phy assert */
	val = readl(combphy->phy_ctl + PCIE_COMBO_PHY_CTL);
	val &= (~PHY_USE_SEL);
	val &= (~(0x03<<8));
	val &= (~PHY_FPGA_SYS_RSTN);
	val &= (~PHY_RSTN);
	writel(val, combphy->phy_ctl + PCIE_COMBO_PHY_CTL);

	 /* phy De-assert */
	val = readl(combphy->phy_ctl + PCIE_COMBO_PHY_CTL);
	val &= (~PHY_CLK_SEL);
	val &= (~(0x03<<8));
	val &= (~PHY_FPGA_SYS_RSTN);
	val &= (~PHY_RSTN);
	val |= PHY_RSTN;
	writel(val, combphy->phy_ctl + PCIE_COMBO_PHY_CTL);

	val = readl(combphy->phy_ctl + PCIE_COMBO_PHY_CTL);
	val &= (~PHY_CLK_SEL);
	val &= (~(0x03<<8));
	val &= (~PHY_FPGA_SYS_RSTN);
	val &= (~PHY_RSTN);
	val |= PHY_RSTN;
	val |= (PHY_FPGA_SYS_RSTN);
	writel(val, combphy->phy_ctl + PCIE_COMBO_PHY_CTL);

}

static void sunxi_combphy_usb3_phy_set(struct sunxi_combphy *combphy, bool enable)
{
	u32 val, tmp = 0;

	val = readl(combphy->phy_clk + 0x1418);
	tmp = GENMASK(17, 16);
	if (enable) {
		val &= ~tmp;
		val |= BIT(25);
	} else {
		val |= tmp;
		val &= ~BIT(25);
	}
	writel(val, combphy->phy_clk + 0x1418);

	/* reg_rx_eq_bypass[3]=1, rx_ctle_res_cal_bypass */
	val = readl(combphy->phy_clk + 0x0674);
	if (enable)
		val |= BIT(3);
	else
		val &= ~BIT(3);
	writel(val, combphy->phy_clk + 0x0674);

	/* rx_ctle_res_cal=0xf, 0x4->0xf */
	val = readl(combphy->phy_clk + 0x0704);
	tmp = GENMASK(9, 8) | BIT(11);
	if (enable)
		val |= tmp;
	else
		val &= ~tmp;
	writel(val, combphy->phy_clk + 0x0704);

	/* CDR_div_fin_gain1 */
	val = readl(combphy->phy_clk + 0x0400);
	if (enable)
		val |= BIT(4);
	else
		val &= ~BIT(4);
	writel(val, combphy->phy_clk + 0x0400);

	/* CDR_div1_fin_gain1 */
	val = readl(combphy->phy_clk + 0x0404);
	tmp = GENMASK(3, 0) | BIT(5);
	if (enable)
		val |= tmp;
	else
		val &= ~tmp;
	writel(val, combphy->phy_clk + 0x0404);

	/* CDR_div3_fin_gain1 */
	val = readl(combphy->phy_clk + 0x0408);
	if (enable)
		val |= BIT(5);
	else
		val &= ~BIT(5);
	writel(val, combphy->phy_clk + 0x0408);

	val = readl(combphy->phy_clk + 0x109c);
	if (enable)
		val |= BIT(1);
	else
		val &= ~BIT(1);
	writel(val, combphy->phy_clk + 0x109c);

	/* SSC configure */
	val = readl(combphy->phy_clk + 0x107c);
	tmp = 0x3f << 12;
	val = val & (~tmp);
	val |= ((0x1 << 12) & tmp);              /* div_N */
	writel(val, combphy->phy_clk + 0x107c);

	val = readl(combphy->phy_clk + 0x1020);
	tmp = 0x1f << 0;
	val = val & (~tmp);
	val |= ((0x6 << 0) & tmp);               /* modulation freq div */
	writel(val, combphy->phy_clk + 0x1020);

	val = readl(combphy->phy_clk + 0x1034);
	tmp = 0x7f << 16;
	val = val & (~tmp);
	val |= ((0x9 << 16) & tmp);              /* spread[6:0], 400*9=4410ppm ssc */
	writel(val, combphy->phy_clk + 0x1034);

	val = readl(combphy->phy_clk + 0x101c);
	tmp = 0x1 << 27;
	val = val & (~tmp);
	val |= ((0x1 << 27) & tmp);              /* choose downspread */

	tmp = 0x1 << 28;
	val = val & (~tmp);
	if (enable)
		val |= ((0x0 << 28) & tmp);      /* don't disable ssc = 0 */
	else
		val |= ((0x1 << 28) & tmp);      /* don't enable ssc = 1 */
	writel(val, combphy->phy_clk + 0x101c);

#ifdef SUNXI_INNO_COMMBOPHY_DEBUG
	/* TX Eye configure bypass_en */
	val = readl(combphy->phy_clk + 0x0ddc);
	if (enable)
		val |= BIT(4);                   /*  0x0ddc[4]=1 */
	else
		val &= ~BIT(4);
	writel(val, combphy->phy_clk + 0x0ddc);

	/* Leg_cur[6:0] - 7'd84 */
	val = readl(combphy->phy_clk + 0x0ddc);
	val |= ((0x54 & BIT(6)) >> 3);           /* 0x0ddc[3] */
	writel(val, combphy->phy_clk + 0x0ddc);

	val = readl(combphy->phy_clk + 0x0de0);
	val |= ((0x54 & GENMASK(5, 0)) << 2);    /* 0x0de0[7:2] */
	writel(val, combphy->phy_clk + 0x0de0);

	/* Leg_curb[5:0] - 6'd18 */
	val = readl(combphy->phy_clk + 0x0de4);
	val |= ((0x12 & GENMASK(5, 1)) >> 1);    /* 0x0de4[4:0] */
	writel(val, combphy->phy_clk + 0x0de4);

	val = readl(combphy->phy_clk + 0x0de8);
	val |= ((0x12 & BIT(0)) << 7);           /* 0x0de8[7] */
	writel(val, combphy->phy_clk + 0x0de8);

	/* Exswing_isel */
	val = readl(combphy->phy_clk + 0x0028);
	val |= (0x4 << 28);                      /* 0x28[30:28] */
	writel(val, combphy->phy_clk + 0x0028);

	/* Exswing_en */
	val = readl(combphy->phy_clk + 0x0028);
	if (enable)
		val |= BIT(31);                  /* 0x28[31]=1 */
	else
		val &= ~BIT(31);
	writel(val, combphy->phy_clk + 0x0028);
#endif
}

static void sunxi_combphy_usb3_power_set(struct sunxi_combphy *combphy, bool enable)
{
	u32 val;

	dev_dbg(combphy->dev, "set power %s\n", enable ? "on" : "off");
	val = readl(combphy->phy_clk + 0x14);
	if (enable)
		val &= ~BIT(26);
	else
		val |= BIT(26);
	writel(val, combphy->phy_clk + 0x14);

	val = readl(combphy->phy_clk + 0x0);
	if (enable)
		val &= ~BIT(10);
	else
		val |= BIT(10);
	writel(val, combphy->phy_clk + 0x0);
}

static void sunxi_combphy_pcie_phy_100M(struct sunxi_combphy *combphy)
{
	u32 val;

	val = readl(combphy->phy_clk + 0x1004);
	val &= ~(0x3<<3);
	val &= ~(0x1<<0);
	val |= (0x1<<0);
	val |= (0x1<<2);
	val |= (0x1<<4);
	writel(val, combphy->phy_clk + 0x1004);

	val = readl(combphy->phy_clk + 0x1018);
	val &= ~(0x3<<4);
	val |= (0x3<<4);
	writel(val, combphy->phy_clk + 0x1018);

	val = readl(combphy->phy_clk + 0x101c);
	val &= ~(0x0fffffff);
	writel(val, combphy->phy_clk + 0x101c);

	val = readl(combphy->phy_clk + 0x107c);
	val &= ~(0x3ffff);
	val |= (0x2<<12);
	val |= 0x32;
	writel(val, combphy->phy_clk + 0x107c);

	val = readl(combphy->phy_clk + 0x1030);
	val &= ~(0x3<<20);
	writel(val, combphy->phy_clk + 0x1030);

	val = readl(combphy->phy_clk + 0x1050);
	val &= ~(0x7<<5);
	val |= (0x1<<5);
	writel(val, combphy->phy_clk + 0x1050);

	val = readl(combphy->phy_clk + 0x1054);
	val &= ~(0x7<<5);
	val |= (0x1<<5);
	writel(val, combphy->phy_clk + 0x1054);

	val = readl(combphy->phy_clk + 0x0804);
	val &= ~(0xf<<4);
	val |= (0xc<<4);
	writel(val, combphy->phy_clk + 0x0804);

	val = readl(combphy->phy_clk + 0x109c);
	val &= ~(0x3<<8);
	val |= (0x1<<1);
	writel(val, combphy->phy_clk + 0x109c);

	writel(0x80540a0a, combphy->phy_clk + 0x1418);
}

static int sunxi_combphy_pcie_init(struct sunxi_combphy *combphy)
{
	sunxi_combphy_pcie_phy_100M(combphy);

	sunxi_combphy_pcie_phy_enable(combphy);

	return 0;
}
static int sunxi_combphy_pcie_exit(struct sunxi_combphy *combphy)
{
	u32 val;

	/* set the phy:
	 * bit(17): aclk enable
	 * bit(16): hclk enbale
	 * bit(1) : pcie_presetn
	 * bit(0) : pcie_power_up_rstn
	 */
	val = readl(combphy->phy_ctl + PCIE_COMBO_PHY_BGR);
	val &= (~(0x03<<0));
	val &= (~(0x03<<16));
	writel(val, combphy->phy_ctl + PCIE_COMBO_PHY_BGR);

	/* Assert the phy */
	val = readl(combphy->phy_ctl + PCIE_COMBO_PHY_CTL);
	val &= (~PHY_USE_SEL);
	val &= (~(0x03<<8));
	val &= (~PHY_RSTN);
	writel(val, combphy->phy_ctl + PCIE_COMBO_PHY_CTL);

	return 0;
}

static int sunxi_combphy_usb3_init(struct sunxi_combphy *combphy)
{
	sunxi_combphy_usb3_phy_set(combphy, true);

	return 0;
}

static int sunxi_combphy_usb3_exit(struct sunxi_combphy *combphy)
{
	sunxi_combphy_usb3_phy_set(combphy, false);

	return 0;
}

static int sunxi_combphy_usb3_power_on(struct sunxi_combphy *combphy)
{
	int ret;

	sunxi_combphy_usb3_power_set(combphy, true);

	if (!IS_ERR(combphy->select3v3_supply)) {
		ret = regulator_set_value(combphy->select3v3_supply, 3300000);
		if (ret) {
			dev_err(combphy->dev, "set select3v3-supply failed\n");
			goto err0;
		}

		ret = regulator_set_enable(combphy->select3v3_supply, true);
		if (ret) {
			dev_err(combphy->dev, "enable select3v3-supply failed\n");
			goto err0;
		}
	}

	return 0;
err0:
	sunxi_combphy_usb3_power_set(combphy, false);

	return ret;
}

static int sunxi_combphy_usb3_power_off(struct sunxi_combphy *combphy)
{
	sunxi_combphy_usb3_power_set(combphy, false);

	if (!IS_ERR(combphy->select3v3_supply))
		regulator_set_enable(combphy->select3v3_supply, false);

	return 0;
}

static int sunxi_combphy_set_mode(struct sunxi_combphy *combphy)
{
	switch (combphy->mode) {
	case PHY_TYPE_PCIE:
		sunxi_combphy_pcie_init(combphy);
		break;
	case PHY_TYPE_USB3:
		if (combphy->user == PHY_USE_BY_PCIE_USB3_U2) {
			sunxi_combphy_pcie_init(combphy);
		} else if (combphy->user == PHY_USE_BY_USB3) {
			sunxi_combphy_usb3_init(combphy);
		}
		break;
	default:
		dev_err(combphy->dev, "incompatible PHY type\n");
		return -EINVAL;
	}

	return 0;
}

static int sunxi_combphy_parse_dt(struct udevice *dev,
				     struct sunxi_combphy *combphy)
{
	int ret = -1;

	/* combo phy use sel */
	combphy->user = fdtdec_get_int(gd->fdt_blob, dev_of_offset(dev),
					      KEY_PHY_USE_SEL, 1);

	ret = clk_get_by_name(dev, "phyclk_ref", &combphy->phyclk_ref);
	if (ret) {
		dev_err(combphy->dev, "failed to get phyclk_ref\n");
		return -ENXIO;
	}

	ret = reset_get_by_name(dev, "phy_rst", &combphy->reset);
	if (ret) {
		dev_err(combphy->dev, "failed to get reset control %d.\n", ret);
		return -ENXIO;
	}


	if (combphy->drvdata->need_noppu_rst) {
		ret = reset_get_by_name(dev, "noppu_rst", &combphy->noppu_reset);
		if (ret) {
			dev_err(combphy->dev, "failed to get noppu_reset control %d.\n", ret);
			return -ENXIO;
		}
	}
	if (combphy->user == PHY_USE_BY_PCIE || combphy->user == PHY_USE_BY_PCIE_USB3_U2) {

		ret = clk_get_by_name(dev, "refclk_par", &combphy->refclk_par);
		if (ret) {
			dev_err(combphy->dev, "failed to get refclk_par\n");
			return -ENXIO;
		}

		ret = clk_set_parent(&combphy->phyclk_ref, &combphy->refclk_par);
		if (ret) {
			dev_err(combphy->dev, "failed to set refclk parent ret =%d \n", ret);
			return -EINVAL;
		}
	}

	if (combphy->drvdata->has_cfg_clk) {
		ret = clk_get_by_name(dev, "phyclk_cfg", &combphy->phyclk_cfg);
		if (ret) {
			dev_err(combphy->dev, "failed to get phyclk_cfg\n");
			return -ENXIO;
		}

		ret = clk_get_by_name(dev, "cfgclk_par", &combphy->cfgclk_par);
		if (ret) {
			dev_err(combphy->dev, "failed to get cfgclk_par\n");
			return -ENXIO;
		}

		ret = clk_set_parent(&combphy->phyclk_cfg, &combphy->cfgclk_par);
		if (ret) {
			dev_err(combphy->dev, "failed to set cfgclk parent ret =%d \n", ret);
			return -EINVAL;
		}
	}

	if (combphy->drvdata->has_pcie_axi_clk) {
		ret = clk_get_by_name(dev, "pclk_axi", &combphy->phy_axi);
		if (ret) {
			dev_err(combphy->dev, "failed to get pclk_axi\n");
			return -ENXIO;
		}

		ret = clk_get_by_name(dev, "pclk_axi_par", &combphy->phy_axi_par);
		if (ret) {
			dev_err(combphy->dev, "failed to get pcie_axi_par\n");
			return -ENXIO;
		}

		ret = clk_set_parent(&combphy->phy_axi, &combphy->phy_axi_par);
		if (ret) {
			dev_err(combphy->dev, "failed to set parent ret =%d \n", ret);
			return -EINVAL;
		}
	}

	if (combphy->drvdata->has_phy_mbus_clk) {
		ret = clk_get_by_name(dev, "phy_mclk", &combphy->phy_mclk);
		if (ret) {
			dev_err(combphy->dev, "fail to get phy_mclk\n");
			return -ENXIO;
		}
	}

	if (combphy->drvdata->has_phy_ahb_clk) {
		ret = clk_get_by_name(dev, "phy_hclk", &combphy->phy_hclk);
		if (ret) {
			dev_err(combphy->dev, "fail to get phy_hclk\n");
			return -ENXIO;
		}
	}

	combphy->phy_ctl = dev_read_addr_index(dev, 0);

	if (combphy->phy_ctl == FDT_ADDR_T_NONE) {
			debug("Can't get the phy-ctl base address\n");
			return -ENXIO;
	}

	combphy->phy_clk = dev_read_addr_index(dev, 1);

	if (combphy->phy_clk == FDT_ADDR_T_NONE) {
			debug("Can't get the phy-clk base address\n");
			return -ENXIO;
	}

	/* combo phy refclk sel */
	combphy->ref = fdtdec_get_int(gd->fdt_blob, dev_of_offset(dev),
					      "KEY_PHY_REFCLK_SEL", 0);

	/* select ic supply */
	ret = device_get_supply_regulator(dev, "pcie3v3-supply",
					  &combphy->select3v3_supply);
	if (ret && ret != -ENOENT) {
		dev_err(combphy->dev, "get select3v3-supply fail (ret=%d)\n", ret);
		return ret;
	}

	return 0;
}

/*  PCIE USB3 Sub-system Application */
static void combo_pcie_clk_set(struct sunxi_combphy *combphy, bool enable)
{
	u32 val, tmp = 0;

	val = readl(COMBO_REG_PCIEBGR(combphy->phy_ctl));
	if (combphy->drvdata->has_slv_clk)
		tmp = PCIE_SLV_ACLK_EN | PCIE_ACLK_EN | PCIE_HCLK_EN | PCIE_PERSTN | PCIE_PW_UP_RSTN;
	else
		tmp = PCIE_ACLK_EN | PCIE_HCLK_EN | PCIE_PERSTN | PCIE_PW_UP_RSTN;
	if (enable)
		val |= tmp;
	else
		val &= ~tmp;
	writel(val, COMBO_REG_PCIEBGR(combphy->phy_ctl));
}

static void combo_usb3_clk_set(struct sunxi_combphy *combphy, bool enable)
{
	u32 val, tmp = 0;

	val = readl(COMBO_REG_USB3BGR(combphy->phy_ctl));
	if (combphy->drvdata->has_u2_phy_mux)
		tmp = USB3_ACLK_EN | USB3_HCLK_EN | USB3_U2_PHY_MUX_SEL | USB3_U2_PHY_RSTN | USB3_U2_PHY_MUX_EN;
	else
		tmp = USB3_ACLK_EN | USB3_HCLK_EN | USB3_RESETN;
	if (enable)
		val |= tmp;
	else
		val &= ~tmp;
	writel(val, COMBO_REG_USB3BGR(combphy->phy_ctl));
}

static void combo_phy_mode_set(struct sunxi_combphy *combphy, bool enable)
{
	u32 val, tmp = 0;

	val = readl(COMBO_REG_PHYCTRL(combphy->phy_ctl));

	if (combphy->user == PHY_USE_BY_PCIE)
		tmp &= ~PHY_USE_SEL;
	else if (combphy->user == PHY_USE_BY_USB3)
		tmp |= PHY_USE_SEL;
	else if (combphy->user == PHY_USE_BY_PCIE_USB3_U2)
		tmp &= ~PHY_USE_SEL;

	if (combphy->ref == INTER_SIG_REF_CLK)
		tmp &= ~PHY_CLK_SEL;
	else if (combphy->ref == EXTER_DIF_REF_CLK)
		tmp |= PHY_CLK_SEL;

	if (enable) {
		tmp |= PHY_RSTN;
		val |= tmp;
	} else {
		tmp &= ~PHY_RSTN;
		val &= ~tmp;
	}
	writel(val, COMBO_REG_PHYCTRL(combphy->phy_ctl));
}

static u32 combo_sysver_get(struct sunxi_combphy *combphy)
{
	u32 reg;

	reg = readl(COMBO_REG_SYSVER(combphy->phy_ctl));

	return reg;
}

static void pcie_usb3_sub_system_enable(struct sunxi_combphy *combphy)
{
	combo_phy_mode_set(combphy, true);

	if (combphy->user == PHY_USE_BY_PCIE)
		combo_pcie_clk_set(combphy, true);
	else if (combphy->user == PHY_USE_BY_USB3)
		combo_usb3_clk_set(combphy, true);
	else if (combphy->user == PHY_USE_BY_PCIE_USB3_U2) {
		combo_pcie_clk_set(combphy, true);
		combo_usb3_clk_set(combphy, true);
	}

	combphy->vernum = combo_sysver_get(combphy);
}

// static void pcie_usb3_sub_system_disable(struct sunxi_combphy *combphy)
// {
// 	combo_phy_mode_set(combphy, false);

// 	if (combphy->user == PHY_USE_BY_PCIE)
// 		combo_pcie_clk_set(combphy, false);
// 	else if (combphy->user == PHY_USE_BY_USB3)
// 		combo_usb3_clk_set(combphy, false);
// 	else if (combphy->user == PHY_USE_BY_PCIE_USB3_U2) {
// 		combo_pcie_clk_set(combphy, false);
// 		combo_usb3_clk_set(combphy, false);
// 	}
// }

static int pcie_usb3_sub_system_init(struct udevice *dev)
{
	struct sunxi_combphy *combphy = dev_get_priv(dev);
	int ret;

	if (combphy->initialized)
		return 0;

	ret = reset_deassert(&combphy->reset);
	if (ret)
		return ret;

	if (combphy->drvdata->need_noppu_rst) {
		ret = reset_deassert(&combphy->noppu_reset);
		if (ret) {
			if (!IS_ERR(&combphy->reset))
				reset_assert(&combphy->reset);
			return ret;
		}
	}

	if (combphy->user == PHY_USE_BY_USB3) {
		if (!IS_ERR(&combphy->phyclk_ref)) {
			ret = clk_enable(&combphy->phyclk_ref);
			if (ret)
				return ret;
		}
	} else if (combphy->user == PHY_USE_BY_PCIE || combphy->user == PHY_USE_BY_PCIE_USB3_U2) {
		ret = clk_set_rate(&combphy->phyclk_ref, 100000000);
		if (ret) {
			dev_err(combphy->dev, "failed to set phyclk_ref freq 100M!\n");
		}

		if (!IS_ERR(&combphy->phyclk_ref)) {
			ret = clk_enable(&combphy->phyclk_ref);
			if (ret)
				return ret;
		}
	}

	if (combphy->drvdata->has_cfg_clk) {
		ret = clk_set_rate(&combphy->phyclk_cfg, 200000000);
		if (ret) {
			dev_err(combphy->dev, "failed to set phyclk_cfg freq 200M!\n");
		}

		if (!IS_ERR(&combphy->phyclk_cfg)) {
			ret = clk_enable(&combphy->phyclk_cfg);
			if (ret)
				return ret;
		}
	}

	if (combphy->drvdata->has_phy_ahb_clk) {
		ret = clk_enable(&combphy->phy_hclk);
		if (ret) {
			dev_err(combphy->dev, "cannot prepare/enable phy_hclk\n");
			return ret;
		}
	}

	if (combphy->drvdata->has_pcie_axi_clk) {
		ret = clk_enable(&combphy->phy_axi);
		if (ret) {
			dev_err(combphy->dev, "cannot prepare/enable phy_axi clock\n");
			return ret;
		}
	}

	if (combphy->drvdata->has_phy_mbus_clk) {
		ret = clk_enable(&combphy->phy_mclk);
		if (ret) {
			dev_err(combphy->dev, "cannot prepare/enable phy_mclk \n");
			return ret;
		}
	}

	pcie_usb3_sub_system_enable(combphy);

	if (combphy->vernum == COMBO_VERSION_ANY)
		dev_err(combphy->dev, "this is unknown version number\n");

	combphy->initialized = true;

	return 0;
}

// static int pcie_usb3_sub_system_exit(struct udevice *dev)
// {
// 	struct sunxi_combphy *combphy = dev_get_priv(dev);

// 	if (!combphy->initialized)
// 		return 0;

// 	pcie_usb3_sub_system_disable(combphy);

// 	if (combphy->drvdata->has_phy_mbus_clk) {
// 		if (!IS_ERR(&combphy->phy_mclk))
// 			clk_disable(&combphy->phy_mclk);
// 	}

// 	if (combphy->drvdata->has_pcie_axi_clk) {
// 		if (!IS_ERR(&combphy->phy_axi))
// 			clk_disable(&combphy->phy_axi);
// 	}

// 	if (combphy->drvdata->has_phy_ahb_clk) {
// 		if (!IS_ERR(&combphy->phy_hclk))
// 			clk_disable(&combphy->phy_hclk);
// 	}

// 	if (combphy->drvdata->has_cfg_clk) {
// 		if (!IS_ERR(&combphy->phyclk_cfg))
// 			clk_disable(&combphy->phyclk_cfg);
// 	}

// 	if (!IS_ERR(&combphy->phyclk_ref))
// 		clk_disable(&combphy->phyclk_ref);

// 	if (combphy->drvdata->need_noppu_rst) {
// 		if (!IS_ERR(&combphy->noppu_reset))
// 			reset_assert(&combphy->noppu_reset);
// 	}

// 	if (!IS_ERR(&combphy->reset))
// 		reset_assert(&combphy->reset);

// 	combphy->initialized = false;

// 	return 0;
// }

static int sunxi_combphy_probe(struct udevice *dev)
{
	struct sunxi_combphy *combphy = dev_get_priv(dev);
	int ret;
	struct sunxi_combophy_of_data *data = (struct sunxi_combophy_of_data *)dev_get_driver_data(dev);

	combphy->mode = PHY_NONE;
	combphy->drvdata = data;

	ret = sunxi_combphy_parse_dt(dev, combphy);
	if (ret) {
		dev_err(combphy->dev, "failed to parse dts of combphy\n");
		return ret;
	}

	ret = pcie_usb3_sub_system_init(dev);
	if (ret)
		dev_warn(combphy->dev, "failed to init sub system\n");

	dev_info(combphy->dev, "Sub System Version: 0x%x\n", combphy->vernum);

	return 0;
}
static int sunxi_combphy_init(struct phy *phy)
{
	struct sunxi_combphy *combphy = dev_get_priv(phy->dev);
	int ret;

	ret = sunxi_combphy_set_mode(combphy);
	if (ret) {
		dev_err(combphy->dev, "invalid number of arguments\n");
		return ret;
	}

	return ret;
}

static int sunxi_combphy_exit(struct phy *phy)
{
	struct sunxi_combphy *combphy = dev_get_priv(phy->dev);

	switch (combphy->mode) {
	case PHY_TYPE_PCIE:
		sunxi_combphy_pcie_exit(combphy);
		break;
	case PHY_TYPE_USB3:
		sunxi_combphy_usb3_exit(combphy);
		break;
	default:
		dev_err(combphy->dev, "incompatible PHY type\n");
		return -EINVAL;
	}

	return 0;
}

static int sunxi_combphy_power_on(struct phy *phy)
{
	struct sunxi_combphy *combphy = dev_get_priv(phy->dev);

	switch (combphy->mode) {
	case PHY_TYPE_PCIE:
		break;
	case PHY_TYPE_USB3:
		sunxi_combphy_usb3_power_on(combphy);
		break;
	default:
		dev_err(combphy->dev, "incompatible PHY type\n");
		return -EINVAL;
	}

	return 0;
}

static int sunxi_combphy_power_off(struct phy *phy)
{
	struct sunxi_combphy *combphy = dev_get_priv(phy->dev);

	switch (combphy->mode) {
	case PHY_TYPE_PCIE:
		break;
	case PHY_TYPE_USB3:
		sunxi_combphy_usb3_power_off(combphy);
		break;
	default:
		dev_err(combphy->dev, "incompatible PHY type\n");
		return -EINVAL;
	}

	return 0;
}

static int sunxi_combphy_xlate(struct phy *phy, struct ofnode_phandle_args *args)
{
	struct sunxi_combphy *combphy = dev_get_priv(phy->dev);

	if (args->args_count != 1) {
		dev_err(combphy->dev, "invalid number of arguments\n");
		return -EINVAL;
	}

	if (combphy->mode != PHY_NONE && combphy->mode != args->args[0])
		dev_warn(combphy->dev, "phy type select %d overwriting type %d\n",
			 args->args[0], combphy->mode);

	combphy->mode = args->args[0];

	return 0;
}

static struct phy_ops sunxi_combphy_ops = {
	.init = sunxi_combphy_init,
	.exit = sunxi_combphy_exit,
	.power_on = sunxi_combphy_power_on,
	.power_off = sunxi_combphy_power_off,
	.of_xlate = sunxi_combphy_xlate,
};

static const struct sunxi_combophy_of_data sunxi_inno_v2_of_data = {
	.has_cfg_clk = true,
	.has_slv_clk = true,
	.has_phy_mbus_clk = true,
	.has_phy_ahb_clk = true,
	.has_pcie_axi_clk = true,
	.has_u2_phy_mux = true,
	.need_noppu_rst = true,
};

static const struct udevice_id sunxi_combphy_of_match[] = {
	{
		.compatible = "allwinner,inno-v2-combphy",
		.data = (ulong)&sunxi_inno_v2_of_data,
	},
	{ },
};

U_BOOT_DRIVER(sunxi_combphy) = {
	.name		= "allwinner,inno-v2-combphy",
	.id		= UCLASS_PHY,
	.of_match	= sunxi_combphy_of_match,
	.ops		= &sunxi_combphy_ops,
	.probe		= sunxi_combphy_probe,
	.priv_auto	= sizeof(struct sunxi_combphy),
};
