/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * Allwinner SoCs display driver.
 *
 * Copyright (C) 2023 Allwinner.
 *
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#include <common.h>
#include <dm.h>
#include <generic-phy.h>
#include <drm/drm_print.h>
#include <phy-mipi-dphy.h>
#include <clk.h>
#include <reset.h>

#include "sunxi_dsi_combophy_reg.h"
#include "../sunxi_drm_phy.h"
#include "../sunxi_drm_helper_funcs.h"


struct dsi_combophy_data {
	unsigned int id;
	struct combodphy_config phy_config;
};

/* TODO:
     1. implement sunxi_dsi_dphy as a phy without disp2 lowlevel, for now, only clk enable work.
     2. remove dsi_combophy_data.id
 */

struct sunxi_dsi_combophy {
	uintptr_t reg_base;
	int usage_count;
	unsigned int id;
	struct sunxi_dphy_lcd dphy_lcd;
	struct phy *phy;
	struct clk *phy_gating;
	struct reset_ctl *phy_rst;
	struct mutex lock;
};


static int sunxi_dsi_combophy_clk_enable(struct sunxi_dsi_combophy *cphy)
{
	struct clk *clks[] = {cphy->phy_gating};
	struct reset_ctl *rsts[] = {cphy->phy_rst};

	return sunxi_clk_enable(clks, ARRAY_SIZE(clks), rsts, ARRAY_SIZE(rsts));
}

static int sunxi_dsi_combophy_clk_disable(struct sunxi_dsi_combophy *cphy)
{
	struct clk *clks[] = {cphy->phy_gating};
	struct reset_ctl *rsts[] = {cphy->phy_rst};


	return sunxi_clk_disable(clks, ARRAY_SIZE(clks), rsts, ARRAY_SIZE(rsts));
}


static int sunxi_dsi_combophy_power_on(struct phy *phy)
{
	struct sunxi_dsi_combophy *cphy = dev_get_priv(phy->dev);

	DRM_INFO("%s start\n", __func__);
	mutex_lock(&cphy->lock);

	//FIXME: remove it?
	if (cphy->usage_count == 0)
		sunxi_dsi_combophy_clk_enable(cphy);

	cphy->usage_count++;

	mutex_unlock(&cphy->lock);
	DRM_INFO("%s end\n", __func__);
	return 0;
}

static int sunxi_dsi_combophy_power_off(struct phy *phy)
{
	struct sunxi_dsi_combophy *cphy = dev_get_priv(phy->dev);

	mutex_lock(&cphy->lock);

	//FIXME: remove it?
	if (cphy->usage_count == 1)
		sunxi_dsi_combophy_clk_disable(cphy);

	cphy->usage_count--;

	mutex_unlock(&cphy->lock);

	return 0;
}


static int sunxi_dsi_combophy_configure(struct phy *phy, void *params)
{
	struct sunxi_drm_phy_cfg *config = (struct sunxi_drm_phy_cfg *)params;
	struct sunxi_dsi_combophy *cphy = dev_get_priv(phy->dev);

	DRM_INFO("[PHY] %s start\n", __FUNCTION__);
	if (config->mode == PHY_MODE_MIPI_DPHY) {
		sunxi_dsi_combophy_set_dsi_mode(&cphy->dphy_lcd, config->submode);
	} else if (config->mode == PHY_MODE_LVDS) {
		sunxi_dsi_combophy_set_lvds_mode(&cphy->dphy_lcd, config->submode ? true : false);
	} else {
		DRM_ERROR("Invalid phy mode :%d\n", config->mode);
	}
	sunxi_dsi_combophy_configure_dsi(&cphy->dphy_lcd, config->mode, &config->mipi_dphy);

	return 0;
}


static int sunxi_dsi_combophy_init(struct phy *phy)
{
	struct sunxi_dsi_combophy *cphy = dev_get_priv(phy->dev);
	phy->id = cphy->id;
	cphy->phy = phy;
	DRM_INFO("%s finish\n", __func__);
	return 0;
}





static const struct phy_ops sunxi_dsi_combophy_ops = {
	.init = sunxi_dsi_combophy_init,
	.power_on = sunxi_dsi_combophy_power_on,
	.power_off = sunxi_dsi_combophy_power_off,
	.configure = sunxi_dsi_combophy_configure,
};


static int sunxi_dsi_combophy_probe(struct udevice *dev)
{
	struct sunxi_dsi_combophy *cphy = dev_get_priv(dev);
	const struct dsi_combophy_data *cphy_data = (struct dsi_combophy_data *)dev_get_driver_data(dev);

	DRM_INFO("[PHY] %s start\n", __FUNCTION__);


	cphy->reg_base = (uintptr_t)dev_read_addr_ptr(dev);
	if (!cphy->reg_base) {
		DRM_ERROR("unable to map dsi combo phy registers\n");
		return -EINVAL;
	}


	cphy->phy_gating = devm_clk_get(dev, "phy_gating_clk");
	if (IS_ERR_OR_NULL(cphy->phy_gating)) {
		DRM_INFO("Maybe dsi clk gating is not need for dsi_combophy.\n");
	}

	cphy->phy_rst = devm_reset_control_get(dev, "phy_rst_clk");
	if (IS_ERR_OR_NULL(cphy->phy_rst)) {
		DRM_INFO("fail to get reset rst_bus_mipi_dsi\n");
	}

	cphy->id = cphy_data->id;
	cphy->dphy_lcd.dphy_index = cphy->id;
	cphy->dphy_lcd.phy_config = (struct combodphy_config *)&cphy_data->phy_config;
	sunxi_dsi_combo_phy_set_reg_base(&cphy->dphy_lcd, cphy->reg_base);

	cphy->usage_count = 0;
	mutex_init(&cphy->lock);

	DRM_INFO("[PHY]%s finish\n", __FUNCTION__);

	return 0;
}

static const struct dsi_combophy_data phy0_data = {
	.id = 0,
	.phy_config.combo_phy_reg1 = {
		.dwval = 0x43,
	},
};

static const struct dsi_combophy_data phy1_data = {
	.id = 1,
	.phy_config.combo_phy_reg1 = {
		.dwval = 0x43,
	},
};

static const struct dsi_combophy_data sun55iw6_data0 = {
	.id = 0,
	.phy_config.combo_phy_reg1 = {
		.dwval = 0x53,
	},
};

static const struct udevice_id sunxi_dsi_combophy_of_table[] = {
	{ .compatible = "allwinner,sunxi-dsi-combo-phy0", .data = (ulong)&phy0_data },
	{ .compatible = "allwinner,sunxi-dsi-combo-phy1", .data = (ulong)&phy1_data },
	{ .compatible = "allwinner,sunxi-dsi-combo-phy0,sun55iw6", .data = (ulong)&sun55iw6_data0 },
	{}
};


U_BOOT_DRIVER(sunxi_dsi_combo_phy) = {
	.name = "sunxi_dsi_combo_phy",
	.id = UCLASS_PHY,
	.of_match = sunxi_dsi_combophy_of_table,
	.probe = sunxi_dsi_combophy_probe,
	.ops = &sunxi_dsi_combophy_ops,
	.priv_auto = sizeof(struct sunxi_dsi_combophy),
};
