/*
 * sunxi_drm_dsi/sunxi_drm_dsi.c
 *
 * Copyright (c) 2007-2024 Allwinnertech Co., Ltd.
 * Author: zhengxiaobin <zhengxiaobin@allwinnertech.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <common.h>
#include <dm.h>
#include <dm/pinctrl.h>
#include <asm/arch/gic.h>
#include <drm/drm_modes.h>
#include <generic-phy.h>
#include <tcon_lcd.h>
#include <sunxi_device/sunxi_tcon.h>
#include <drm/drm_print.h>
#include <dm/device_compat.h>
#include <drm/drm_mipi_dsi.h>
#include "drm_mipi_dsi.h"

#include "sunxi_drm_phy.h"
#include "sunxi_drm_crtc.h"
#include "sunxi_drm_connector.h"
#include "sunxi_drm_drv.h"
#include "sunxi_drm_helper_funcs.h"

struct dsi_data {
	int id;
};
struct sunxi_drm_dsi {
	struct sunxi_drm_connector connector;
	struct udevice *tcon_dev;
	struct udevice *dev;
	/*struct mipi_dsi_host host;*/
	struct drm_display_mode mode;
	struct disp_dsi_para dsi_para;
	unsigned int tcon_id;
	unsigned int tcon_top_id;
	/*struct drm_panel *panel;*/
	bool bound;
	struct sunxi_drm_dsi *master;
	struct sunxi_drm_dsi *slave;
	struct phy *phy;
	struct sunxi_drm_phy_cfg phy_opts;

	uintptr_t reg_base;
	const struct dsi_data *dsi_data;
	struct sunxi_dsi_lcd dsi_lcd;
//	enum disp_lcd_tcon_mode lcd_tcon_mode;
	u32 dsi_id;
	u32 enable;
	interrupt_handler_t *irq_handler;
	void *irq_data;
	u32 irq_no;
	dev_t devid;

	struct clk *clk_bus;
	struct clk *clk;
	struct clk *clk_combphy;
	struct reset_ctl *rst_bus;
	unsigned long mode_flags;

	struct mutex mlock;
};

static const struct dsi_data dsi0_data = {
	.id = 0,
};

static const struct dsi_data dsi1_data = {
	.id = 1,
};

static const struct udevice_id sunxi_drm_dsi_match[] = {
	{ .compatible = "allwinner,dsi0", .data = (ulong)&dsi0_data },
	{ .compatible = "allwinner,dsi1", .data = (ulong)&dsi1_data },
	{},
};


static int sunxi_drm_dsi_host_attach(struct mipi_dsi_host *host,
				struct mipi_dsi_device *device)
{
	struct sunxi_drm_dsi *dsi = dev_get_priv(host->dev);
	/*struct drm_panel *panel = of_drm_find_panel(device->dev.of_node);*/

	DRM_INFO("[DSI]%s start\n", __FUNCTION__);

	/*dsi->panel = panel;*/
	dsi->dsi_para.dsi_div = 4;
	dsi->dsi_para.lanes = device->lanes;
	dsi->dsi_para.channel = device->channel;
	dsi->dsi_para.format = device->format;
	dsi->dsi_para.mode_flags = device->mode_flags;
	dsi->dsi_para.hs_rate = device->hs_rate;
	dsi->dsi_para.lp_rate = device->lp_rate;

	DRM_INFO("[DSI]%s finish\n", __FUNCTION__);
	return 0;
}

static int sunxi_drm_dsi_host_detach(struct mipi_dsi_host *host,
				struct mipi_dsi_device *device)
{
	struct sunxi_drm_dsi *dsi = dev_get_priv(host->dev);
	/*dsi->panel = NULL;*/
	dsi->dsi_para.lanes = 0;
	dsi->dsi_para.channel = 0;
	dsi->dsi_para.format = 0;
	dsi->dsi_para.mode_flags = 0;
	dsi->dsi_para.hs_rate = 0;
	dsi->dsi_para.lp_rate = 0;
	memset(&dsi->dsi_para.timings, 0, sizeof(struct disp_video_timings));

	return 0;
}


static s32 sunxi_dsi_read_para(struct sunxi_drm_dsi *dsi, const struct mipi_dsi_msg *msg)
{
	s32 ret;

	ret = dsi_dcs_rd(&dsi->dsi_lcd, msg->rx_buf, msg->rx_len);

	return ret;
}

static s32 sunxi_dsi_write_para(struct sunxi_drm_dsi *dsi, struct mipi_dsi_packet *packet)
{
	u32 ecc, crc, para_num;
	u8 *para = NULL;

	para = kmalloc(packet->size + 2, GFP_ATOMIC);
	if (!para) {
	//	printk("%s %s %s :kmalloc fail\n", __FILE__, __func__, __LINE__);
		return -1;
	}
	ecc = packet->header[0] | (packet->header[1] << 8) | (packet->header[2] << 16);
	para[0] = packet->header[0];
	para[1] = packet->header[1];
	para[2] = packet->header[2];
	para[3] = dsi_ecc_pro(ecc);
	para_num = 4;

	if (packet->payload_length) {
		memcpy(para + 4, packet->payload, packet->payload_length);
		crc = dsi_crc_pro((u8 *)packet->payload, packet->payload_length + 1);
		para[packet->size] = (crc >> 0) & 0xff;
		para[packet->size + 1] = (crc >> 8) & 0xff;
		para_num = packet->size + 2;
	}
	dsi_dcs_wr(&dsi->dsi_lcd, para, para_num);

	kfree(para);
	para = NULL;

	return 0;
}

static ssize_t sunxi_drm_dsi_transfer(struct sunxi_drm_dsi *dsi,
				const struct mipi_dsi_msg *msg)
{
	struct mipi_dsi_packet packet;
	int ret;

	/* create a packet to the DSI protocol */
	ret = mipi_dsi_create_packet(&packet, msg);
	if (ret) {
		DRM_ERROR("failed to create packet\n");
		return ret;
	}
	sunxi_dsi_write_para(dsi, &packet);
	if (msg->rx_len) {
		ret = sunxi_dsi_read_para(dsi, msg);
		if (ret < 0)
			return ret;
	}

	if (dsi->slave)
		sunxi_drm_dsi_transfer(dsi->slave, msg);

	return msg->tx_len;
}


static ssize_t sunxi_drm_dsi_host_transfer(struct mipi_dsi_host *host,
				const struct mipi_dsi_msg *msg)
{
	struct sunxi_drm_dsi *dsi = dev_get_priv(host->dev);

	return sunxi_drm_dsi_transfer(dsi, msg);
}


static const struct mipi_dsi_host_ops sunxi_drm_dsi_host_ops = {
	.attach = sunxi_drm_dsi_host_attach,
	.detach = sunxi_drm_dsi_host_detach,
	.transfer = sunxi_drm_dsi_host_transfer,
};


static void sunxi_dsi_irq_event_proc(void *parg)
{
	struct sunxi_drm_dsi *dsi = parg;
	/* NOTE: only for dsi40 */
	dsi_irq_query(&dsi->dsi_lcd, DSI_IRQ_VIDEO_VBLK);

	return dsi->irq_handler(dsi->irq_data);
}


static void sunxi_dsi_enable_vblank(bool enable, void *data)
{
	struct sunxi_drm_dsi *dsi = (struct sunxi_drm_dsi *)data;

	if (dsi->slave || (dsi->dsi_para.mode_flags & MIPI_DSI_SLAVE_MODE))
		sunxi_tcon_enable_vblank(dsi->tcon_dev, enable);
	else
		dsi_enable_vblank(&dsi->dsi_lcd, enable);
}

static bool sunxi_dsi_fifo_check(void *data)
{
	struct sunxi_drm_dsi *dsi = (struct sunxi_drm_dsi *)data;
	int status;
	status = dsi_get_status(&dsi->dsi_lcd);

	return status ? true : false;
}

static int sunxi_dsi_connector_init(struct sunxi_drm_connector *conn,
					  struct display_state *state)
{
	struct sunxi_drm_dsi *dsi = dev_get_priv(conn->dev);
	struct crtc_state *scrtc_state = &state->crtc_state;


	scrtc_state->tcon_id = dsi->tcon_id;
	scrtc_state->tcon_top_id = dsi->tcon_top_id;
	scrtc_state->check_status = sunxi_dsi_fifo_check;
	scrtc_state->enable_vblank = sunxi_dsi_enable_vblank;
	scrtc_state->vblank_enable_data = dsi;
	return 0;
}


static int sunxi_dsi_clk_config_enable(struct sunxi_drm_dsi *dsi,
					const struct disp_dsi_para *para)
{
	unsigned long dsi_rate = 0;
	unsigned long dsi_rate_set = 150000000;
	int ret = 0;
	struct clk *clks[] = {dsi->clk_bus, dsi->clk};
	struct reset_ctl *rsts[] = {dsi->rst_bus};

	if (!IS_ERR_OR_NULL(dsi->clk)) {
		clk_set_rate(dsi->clk, dsi_rate_set);
		dsi_rate = clk_get_rate(dsi->clk);
		if (dsi_rate_set != dsi_rate)
			DRM_WARN("Dsi rate to be set:%lu, real clk rate:%lu\n",
				 dsi_rate, dsi_rate_set);
	}

	ret = sunxi_clk_enable(clks, ARRAY_SIZE(clks), rsts, ARRAY_SIZE(rsts));

	return ret;
}


static int sunxi_dsi_clk_config_disable(struct sunxi_drm_dsi *dsi)
{
	struct clk *clks[] = {dsi->clk_bus, dsi->clk};
	struct reset_ctl *rsts[] = {dsi->rst_bus};

	return sunxi_clk_disable(clks, ARRAY_SIZE(clks), rsts, ARRAY_SIZE(rsts));
}


static int sunxi_dsi_enable_output(struct sunxi_drm_dsi *dsi)
{
	struct disp_dsi_para *dsi_para = &dsi->dsi_para;
	sunxi_tcon_enable_output(dsi->tcon_dev);
	dsi_open(&dsi->dsi_lcd, dsi_para);
	if (dsi->slave)
		dsi_open(&dsi->slave->dsi_lcd, dsi_para);

	return 0;
}

static int sunxi_dsi_disable_output(struct sunxi_drm_dsi *dsi)
{

	dsi_close(&dsi->dsi_lcd);
	if (dsi->slave)
		dsi_close(&dsi->slave->dsi_lcd);

	return 0;
}

static int sunxi_dsi_connector_disable(struct sunxi_drm_connector *conn,
					  struct display_state *state)
{
	struct sunxi_drm_dsi *dsi = dev_get_priv(conn->dev);

	/*drm_panel_disable(dsi->panel);*/
	/*drm_panel_unprepare(dsi->panel);*/

	if (dsi->phy) {
		generic_phy_power_off(dsi->phy);
	}
	if (dsi->slave) {
		if (dsi->slave->phy)
			generic_phy_power_off(dsi->phy);
	}

	sunxi_dsi_clk_config_disable(dsi);
	if (dsi->slave)
		sunxi_dsi_clk_config_disable(dsi->slave);

	pinctrl_select_state(dsi->dev, "sleep");
	if (dsi->slave)
		pinctrl_select_state(dsi->slave->dev, "sleep");

	sunxi_dsi_disable_output(dsi);
	sunxi_tcon_mode_exit(dsi->dev);

	if (!(dsi->slave || (dsi->dsi_para.mode_flags & MIPI_DSI_SLAVE_MODE)))
		irq_free_handler(dsi->irq_no);

	DRM_DEBUG_DRIVER("%s finish\n", __FUNCTION__);
	return 0;

}

static int sunxi_dsi_connector_prepare(struct sunxi_drm_connector *conn,
					  struct display_state *state)
{
	int ret = -1, bpp = 0;
	struct sunxi_drm_dsi *dsi = dev_get_priv(conn->dev);
	struct crtc_state *scrtc_state = &state->crtc_state;
	struct disp_output_config disp_cfg;
	struct connector_state *conn_state = &state->conn_state;
	struct sunxi_drm_phy_cfg *phy_cfg = &dsi->phy_opts;

	memcpy(&dsi->mode, &conn_state->mode, sizeof(struct drm_display_mode));
	memset(&disp_cfg, 0, sizeof(struct disp_output_config));
	memset(phy_cfg, 0, sizeof(struct sunxi_drm_phy_cfg));

	drm_mode_to_sunxi_video_timings(&dsi->mode, &dsi->dsi_para.timings);

	memcpy(&disp_cfg.dsi_para, &dsi->dsi_para,
	       sizeof(struct disp_dsi_para));

	disp_cfg.type = INTERFACE_DSI;
	disp_cfg.de_id = sunxi_drm_crtc_get_hw_id(scrtc_state->crtc);
	disp_cfg.irq_handler = scrtc_state->crtc_irq_handler;
	disp_cfg.irq_data = state;
	disp_cfg.tcon_lcd_div = 1;

	if (dsi->slave || (dsi->dsi_para.mode_flags & MIPI_DSI_SLAVE_MODE))
		disp_cfg.slave_dsi = true;


	sunxi_tcon_mode_init(dsi->tcon_dev, &disp_cfg);

	bpp = mipi_dsi_pixel_format_to_bpp(dsi->dsi_para.format);
	if (dsi->phy) {
		phy_mipi_dphy_get_default_config(dsi->dsi_para.timings.pixel_clk,
					bpp, dsi->dsi_para.lanes, &dsi->phy_opts.mipi_dphy);
	}

	/* dual dsi use tcon's irq, single dsi use its own irq */
	if (!disp_cfg.slave_dsi) {
		dsi->irq_handler = scrtc_state->crtc_irq_handler;
		dsi->irq_data = state;
		pr_err("irq no:%d\n", dsi->irq_no);
		irq_install_handler(dsi->irq_no, sunxi_dsi_irq_event_proc, (void *)dsi);
		irq_enable(dsi->irq_no);
	}

	ret = sunxi_dsi_clk_config_enable(dsi, &dsi->dsi_para);
	if (ret) {
		DRM_ERROR("dsi clk enable failed\n");
		return -1;
	}

	if (dsi->slave) {
		ret = sunxi_dsi_clk_config_enable(dsi->slave, &dsi->dsi_para);
		if (ret) {
			DRM_ERROR("slave clk enable failed\n");
			return -2;
		}
	}

	ret = dsi_cfg(&dsi->dsi_lcd, &dsi->dsi_para);
	if (ret) {
		DRM_ERROR("dsi_cfg failed\n");
		return -3;
	}
	if (dsi->slave) {
		ret = dsi_cfg(&dsi->slave->dsi_lcd, &dsi->dsi_para);
		if (ret) {
			DRM_ERROR("dsi_cfg slave failed\n");
			return -4;
		}
	}

	pinctrl_select_state(dsi->dev, "active");
	if (dsi->slave) {
		pinctrl_select_state(dsi->slave->dev, "active");
	}

	phy_cfg->mode = PHY_MODE_MIPI_DPHY;
	phy_cfg->submode = PHY_SINGLE_ENABLE;
	if (dsi->phy) {
		ret = generic_phy_power_on(dsi->phy);
	}
	if ((dsi->slave) && (dsi->slave->phy)) {
		phy_cfg->submode = PHY_DUAL_ENABLE;
		ret = generic_phy_power_on(dsi->slave->phy);
		ret += generic_phy_configure(dsi->phy, phy_cfg);
		ret += generic_phy_configure(dsi->slave->phy, phy_cfg);
	} else {
		ret += generic_phy_configure(dsi->phy, phy_cfg);
	}

	return ret;
}

static int sunxi_dsi_connector_enable(struct sunxi_drm_connector *conn,
					  struct display_state *state)
{
	int ret = -1;
	struct sunxi_drm_dsi *dsi = dev_get_priv(conn->dev);

	/*drm_panel_prepare(dsi->panel);*/
	dsi_clk_enable(&dsi->dsi_lcd, &dsi->dsi_para, 1);
	if (dsi->slave)
		dsi_clk_enable(&dsi->slave->dsi_lcd, &dsi->dsi_para, 1);
	/*drm_panel_enable(dsi->panel);*/

	ret = sunxi_dsi_enable_output(dsi);
	if (ret < 0)
		DRM_ERROR("failed to enable dsi ouput\n");
	DRM_INFO("[DSI] %s finish\n", __FUNCTION__);
	return ret;

}

static const struct sunxi_drm_connector_funcs dsi_connector_funcs = {
	.init = sunxi_dsi_connector_init,
	.prepare = sunxi_dsi_connector_prepare,
	.enable = sunxi_dsi_connector_enable,
	.disable = sunxi_dsi_connector_disable,
	/*.check = sunxi_lvds_connector_check,*/
};


static int sunxi_drm_dsi_bind(struct udevice *dev)
{
	struct mipi_dsi_host *host = dev_get_plat(dev);

	host->dev = dev;
	host->ops = &sunxi_drm_dsi_host_ops;

	return dm_scan_fdt_dev(dev);
}

static int sunxi_drm_dsi_probe(struct udevice *dev)
{
	struct sunxi_drm_dsi *dsi = dev_get_priv(dev);
	dsi->dsi_data = (struct dsi_data *)dev_get_driver_data(dev);
	struct udevice *secondary = NULL;
	int ret = -1;
	u32 phandle;

	DRM_INFO("%s:%d\n", __func__, __LINE__);

	dsi->tcon_dev = sunxi_tcon_of_get_tcon_dev(dev);
	if (!dsi->tcon_dev) {
		DRM_ERROR("%s:Get tcon dev fail!\n", __func__);
		return -1;
	}

	dsi->reg_base = (uintptr_t)dev_read_addr_ptr(dev);
	if (!dsi->reg_base) {
		DRM_ERROR("unable to map dsi registers\n");
		return -EINVAL;
	}

	dsi->irq_no = sunxi_of_get_irq_number(dev, 0);
	if (!dsi->irq_no) {
		DRM_ERROR("get irq no of dsi failed\n");
		return -EINVAL;
	}

	dsi->clk = devm_clk_get(dev, "dsi_clk");
	if (IS_ERR(dsi->clk)) {
		DRM_ERROR("fail to get clk clk_mipi_dsi\n");
	}

	dsi->clk_bus = devm_clk_get(dev, "dsi_gating_clk");
	if (IS_ERR(dsi->clk_bus)) {
		DRM_ERROR("fail to get clk clk_bus_mipi_dsi\n");
	}

	dsi->rst_bus = devm_reset_control_get(dev, "dsi_rst_clk");
	if (IS_ERR(dsi->rst_bus)) {
		DRM_ERROR("fail to get reset rst_bus_mipi_dsi\n");
	}

	dsi->dev = dev;
	dsi->tcon_id = sunxi_tcon_of_get_id(dsi->tcon_dev);
	dsi->tcon_top_id = sunxi_tcon_of_get_id(dsi->tcon_dev);
	dsi->dsi_id = dsi->dsi_data->id;
	dsi->dsi_lcd.dsi_index = dsi->dsi_id;
	dsi->phy = kmalloc(sizeof(struct phy), __GFP_ZERO);
	ret = generic_phy_get_by_name(dev, "combophy", dsi->phy);
	if (IS_ERR_OR_NULL(dsi->phy))
		DRM_INFO("dsi%d's combophy not setting, maybe not used!\n", dsi->dsi_id);

	dsi_set_reg_base(&dsi->dsi_lcd, dsi->reg_base);

	ret = ofnode_read_u32(dev->node_, "dual-channel", &phandle);
	if (!ret) {
		uclass_get_device_by_ofnode(UCLASS_DISPLAY, ofnode_get_by_phandle(phandle), &secondary);
	}

	if (secondary) {
		dsi->slave = dev_get_priv(secondary);
		dsi->slave->master = dsi;
		dsi->dsi_para.dual_dsi = 1;
	}

	ret = sunxi_drm_connector_bind(&dsi->connector, dev, dsi->dsi_id, &dsi_connector_funcs,
				NULL, DRM_MODE_CONNECTOR_DSI);

	DRM_INFO("%s:%d\n", __func__, __LINE__);
	dsi->bound = true;
	return ret;

}


static int sunxi_drm_dsi_post_bind(struct udevice *dev)
{
	struct mipi_dsi_host *host = dev_get_plat(dev->parent);
	struct mipi_dsi_device *device = dev_get_parent_plat(dev);
	char name[20];

	DRM_INFO("%s:%d\n", __func__, __LINE__);
	sprintf(name, "%s.%d", host->dev->name, device->channel);
	device_set_name(dev, name);

	device->dev = dev;
	device->host = host;
	device->lanes = dev_read_u32_default(dev, "dsi,lanes", 4);
	device->format =
		dev_read_u32_default(dev, "dsi,format", MIPI_DSI_FMT_RGB888);
	device->mode_flags = dev_read_u32_default(
		dev, "dsi,flags",
		MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			MIPI_DSI_MODE_VIDEO_HBP | MIPI_DSI_MODE_LPM |
			MIPI_DSI_MODE_NO_EOT_PACKET);
	device->channel = dev_read_u32_default(dev, "reg", 0);

	return 0;
}


static int sunxi_drm_dsi_post_probe(struct udevice *dev)
{
	struct mipi_dsi_device *device = dev_get_parent_plat(dev);
	int ret;

	DRM_INFO(" %s:%d\n", __func__, __LINE__);
	ret = mipi_dsi_attach(device);
	if (ret) {
		dev_err(dev, "mipi_dsi_attach() failed: %d\n", ret);
		return ret;
	}

	return 0;
}



U_BOOT_DRIVER(sunxi_drm_dsi) = {
	.name = "sunxi_drm_dsi",
	.id = UCLASS_DISPLAY,
	.of_match = sunxi_drm_dsi_match,
	.bind = sunxi_drm_dsi_bind,
	.probe = sunxi_drm_dsi_probe,
	.priv_auto = sizeof(struct sunxi_drm_dsi),
	.plat_auto = sizeof(struct mipi_dsi_host),
	//child node is dsi panel
	.per_child_plat_auto = sizeof(struct mipi_dsi_device),
	.child_post_bind = sunxi_drm_dsi_post_bind,
	.child_pre_probe = sunxi_drm_dsi_post_probe,
};

//End of File
