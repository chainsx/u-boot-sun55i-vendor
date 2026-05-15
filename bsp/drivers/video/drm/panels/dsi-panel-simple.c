// SPDX-License-Identifier: GPL-2.0+
/*
 * Generic dsi panel driver
 *
 * Copyright (C) 2023 Allwinner.
 *
 */


#include <common.h>
#include <dm.h>
#include <dm/devres.h>
#include <dm/device_compat.h>
#include <backlight.h>
#include <power/regulator.h>
#include <asm/gpio.h>
#include <linux/delay.h>
#include <drm/drm_modes.h>
#include "../sunxi_drm_panel.h"
#include "../sunxi_drm_helper_funcs.h"
#include <drm/drm_mipi_dsi.h>
#include "../drm_mipi_dsi.h"

#define POWER_MAX 3
#define GPIO_MAX  3

struct panel_cmd_header {
	u8 data_type;
	u8 delay;
	u8 payload_length;
} __packed;

struct panel_cmd_desc {
	struct panel_cmd_header header;
	u8 *payload;
};

struct panel_cmd_seq {
	struct panel_cmd_desc *cmds;
	unsigned int cmd_cnt;
};

struct panel_desc {

	struct videomode video_mode;
	struct {
		unsigned int width;
		unsigned int height;
	} size;

	 struct {
//		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
//		unsigned int unprepare;
		unsigned int reset;
//		unsigned int init;
	} delay;

	struct panel_cmd_seq *init_seq;
	struct panel_cmd_seq *exit_seq;
};

struct panel_dsi {
	struct sunxi_drm_panel panel;
	struct udevice *dev;
	struct mipi_dsi_device *dsi;

	const struct panel_desc *desc;

	struct udevice *backlight;
	struct udevice *supply[POWER_MAX];
	struct gpio_desc *enable_gpio[GPIO_MAX];
	struct gpio_desc *reset_gpio;

};

struct panel_desc_dsi {
	struct panel_desc desc;
	unsigned long flags;
	enum mipi_dsi_pixel_format format;
	unsigned int lanes;
};
/*
static const struct drm_display_mode auo_b080uan01_mode = {
	.clock = 154500,
	.hdisplay = 1200,
	.hsync_start = 1200 + 62,
	.hsync_end = 1200 + 62 + 4,
	.htotal = 1200 + 62 + 4 + 62,
	.vdisplay = 1920,
	.vsync_start = 1920 + 9,
	.vsync_end = 1920 + 9 + 2,
	.vtotal = 1920 + 9 + 2 + 8,
	.vrefresh = 60,
};

static const struct panel_desc_dsi auo_b080uan01 = {
	.desc = {
		.modes = &auo_b080uan01_mode,
		.bpc = 8,
		.size = {
			.width = 108,
			.height = 272,
		},
	},
	.flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_CLOCK_NON_CONTINUOUS,
	.format = MIPI_DSI_FMT_RGB888,
	.lanes = 4,
};
*/
static inline struct panel_dsi *to_panel_dsi(struct sunxi_drm_panel *panel)
{
	return container_of(panel, struct panel_dsi, panel);
}

static void panel_dsi_sleep(unsigned int msec)
{
	mdelay(msec);
}

static int panel_dsi_cmd_seq(struct panel_dsi *dsi_panel,
		struct panel_cmd_seq *seq)
{
	struct udevice *dev = dsi_panel->dev;
	struct mipi_dsi_device *dsi = dsi_panel->dsi;
	unsigned int i;
	int err;

	if (!seq)
		return -EINVAL;

	for (i = 0; i < seq->cmd_cnt; i++) {
		struct panel_cmd_desc *cmd = &seq->cmds[i];

		switch (cmd->header.data_type) {
		case MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM:
		case MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM:
		case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM:
		case MIPI_DSI_GENERIC_LONG_WRITE:
			err = mipi_dsi_generic_write(dsi, cmd->payload,
						cmd->header.payload_length);
			break;
		case MIPI_DSI_DCS_SHORT_WRITE:
		case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
		case MIPI_DSI_DCS_LONG_WRITE:
			err = mipi_dsi_dcs_write_buffer(dsi, cmd->payload,
					cmd->header.payload_length);
			break;
		default:
			return -EINVAL;
		}

		if (err < 0)
			dev_err(dev, "failed to write dcs cmd: %d\n", err);

		if (cmd->header.delay)
			panel_dsi_sleep(cmd->header.delay);
	}

	return 0;
}

static int panel_dsi_disable(struct sunxi_drm_panel *panel)
{
	struct panel_dsi *dsi = to_panel_dsi(panel);

	if (dsi->backlight)
		backlight_set_brightness(dsi->backlight, 0);

	return 0;
}

static int panel_dsi_unprepare(struct sunxi_drm_panel *panel)
{
	struct panel_dsi *dsi_panel = to_panel_dsi(panel);
	int i;

	if (dsi_panel->desc->exit_seq)
		if (dsi_panel->dsi)
			panel_dsi_cmd_seq(dsi_panel, dsi_panel->desc->exit_seq);

	for (i = GPIO_MAX; i > 0; i--) {
		if (dsi_panel->enable_gpio[i - 1]) {
			dm_gpio_set_value(dsi_panel->enable_gpio[i], 0);
			if (dsi_panel->desc->delay.enable)
				panel_dsi_sleep(dsi_panel->desc->delay.disable);
		}
	}

	if (dsi_panel->reset_gpio)
		dm_gpio_set_value(dsi_panel->reset_gpio, 0);
	if (dsi_panel->desc->delay.reset)
		panel_dsi_sleep(dsi_panel->desc->delay.reset);

	for (i = POWER_MAX; i > 0; i--) {
		if (dsi_panel->supply[i - 1]) {
			regulator_set_enable(dsi_panel->supply[i - 1], false);
			mdelay(10);
		}
	}

	return 0;
}

static int panel_dsi_prepare(struct sunxi_drm_panel *panel)
{
	struct panel_dsi *dsi_panel = to_panel_dsi(panel);
	int err, i;

	for (i = 0; i < POWER_MAX; i++) {
		if (dsi_panel->supply[i]) {
			err = regulator_set_enable(dsi_panel->supply[i], true);
			if (err < 0) {
				dev_err(dsi_panel->dev, "failed to enable supply%d: %d\n",
					i, err);
				return err;
			}
			mdelay(10);
		}
	}

	for (i = 0; i < GPIO_MAX; i++) {
		if (dsi_panel->enable_gpio[i]) {
			dm_gpio_set_value(dsi_panel->enable_gpio[i], 1);

			if (dsi_panel->desc->delay.enable)
				panel_dsi_sleep(dsi_panel->desc->delay.enable);
		}
	}

	if (dsi_panel->reset_gpio)
		dm_gpio_set_value(dsi_panel->reset_gpio, 1);
	if (dsi_panel->desc->delay.reset)
		panel_dsi_sleep(dsi_panel->desc->delay.reset);
/*
	for (i = 0; i < dsi_panel->desc->reset_num; i++) {
		if (dsi_panel->reset_gpio)
			gpiod_set_value_cansleep(dsi_panel->reset_gpio, 1);
		if (dsi_panel->desc->delay.reset)
			panel_dsi_sleep(dsi_panel->desc->delay.reset);
	}
*/
	if (dsi_panel->desc->init_seq)
		if (dsi_panel->dsi)
			panel_dsi_cmd_seq(dsi_panel, dsi_panel->desc->init_seq);

	return 0;
}

static int panel_dsi_enable(struct sunxi_drm_panel *panel)
{
	struct panel_dsi *dsi = to_panel_dsi(panel);
	if (dsi->backlight)
		backlight_enable(dsi->backlight);

	return 0;
}

static const struct sunxi_drm_panel_funcs panel_dsi_funcs = {
	.unprepare = panel_dsi_unprepare,
	.disable = panel_dsi_disable,
	.prepare = panel_dsi_prepare,
	.enable = panel_dsi_enable,
	/*.get_modes = panel_dsi_get_modes,*/
//	.get_timings = panel_dsi_get_timings,
};

static int panel_dsi_parse_dt(struct panel_dsi *dsi_panel)
{
	char power_name[40] = {0};
	char gpio_name[40] = {0};
	int ret, i;

	for (i = 0; i < GPIO_MAX; ++i) {
		dsi_panel->enable_gpio[i] =
			kmalloc(sizeof(struct gpio_desc), __GFP_ZERO);
	}
	dsi_panel->reset_gpio = kmalloc(sizeof(struct gpio_desc), __GFP_ZERO);

	ret = sunxi_of_get_panel_orientation(dsi_panel->dev, &dsi_panel->panel.orientation);

	for (i = 0; i < POWER_MAX; i++) {
		snprintf(power_name, 40, "power%d-supply", i);
		ret = uclass_get_device_by_phandle(UCLASS_REGULATOR, dsi_panel->dev,
						   power_name, &dsi_panel->supply[i]);
		if (!dsi_panel->supply[i] || ret) {
			if (ret != -ENODEV) {
				if (ret != -EPROBE_DEFER)
					dev_err(dsi_panel->dev,
						"failed to request regulator(%s): %d\n",
						power_name, ret);
			}

			dsi_panel->supply[i] = NULL;
		}
	}

	/* Get GPIOs and backlight controller. */
	for (i = 0; i < GPIO_MAX; i++) {
		snprintf(gpio_name, 40, "enable%d-gpios", i);
		ret = gpio_request_by_name(dsi_panel->dev, gpio_name, 0,
					   dsi_panel->enable_gpio[i], GPIOD_IS_OUT);
		if (!dsi_panel->enable_gpio[i] || ret) {
			dev_err(dsi_panel->dev, "failed to request %s GPIO: %d\n", gpio_name,
				ret);
		}
	}

	ret = gpio_request_by_name(dsi_panel->dev, "reset-gpios", 0,
				   dsi_panel->reset_gpio, GPIOD_IS_OUT);
	if (!dsi_panel->reset_gpio || ret) {
		dev_err(dsi_panel->dev, "failed to request %s GPIO: %d\n", "reset",
			ret);
	}

	return 0;
}
static int panel_simple_parse_cmd_seq(struct udevice *dev,
				const u8 *data, int length,
				struct panel_cmd_seq *seq)
{
	struct panel_cmd_header *header;
	struct panel_cmd_desc *desc;
	char *buf, *d;
	unsigned int i, cnt, len;

	if (!seq)
		return -EINVAL;

	buf = devm_kzalloc(dev, length, __GFP_ZERO);
	if (buf)
		memcpy(buf, data, length);
	else
		return -ENOMEM;

	d = buf;
	len = length;
	cnt = 0;
	while (len > sizeof(*header)) {
		header = (struct panel_cmd_header *)d;

		d += sizeof(*header);
		len -= sizeof(*header);

		if (header->payload_length > len)
			return -EINVAL;

		d += header->payload_length;
		len -= header->payload_length;
		cnt++;
	}

	if (len)
		return -EINVAL;

	seq->cmd_cnt = cnt;
	seq->cmds = devm_kcalloc(dev, cnt, sizeof(*desc), GFP_KERNEL);
	if (!seq->cmds)
		return -ENOMEM;

	d = buf;
	len = length;
	for (i = 0; i < cnt; i++) {
		header = (struct panel_cmd_header *)d;
		len -= sizeof(*header);
		d += sizeof(*header);

		desc = &seq->cmds[i];
		desc->header = *header;
		desc->payload = d;

		d += header->payload_length;
		len -= header->payload_length;
	}

	return 0;
}

static int panel_of_get_desc_data(struct udevice *dev,
					struct panel_desc *desc)
{
	const void *data;
	int len;
	int err;


	dev_read_u32(dev, "width-mm", &desc->size.width);
	dev_read_u32(dev, "height-mm", &desc->size.height);

//	dev_read_u32(dev, "prepare-delay-ms", &desc->delay.prepare);
	dev_read_u32(dev, "enable-delay-ms", &desc->delay.enable);
	dev_read_u32(dev, "disable-delay-ms", &desc->delay.disable);
//      dev_read_u32(dev, "unprepare-delay-ms", &desc->delay.unprepare);
	dev_read_u32(dev, "reset-delay-ms", &desc->delay.reset);
//	dev_read_u32(dev, "init-delay-ms", &desc->delay.init);


	data = dev_read_prop(dev, "panel-init-sequence", &len);
	if (data) {
		desc->init_seq = devm_kzalloc(dev, sizeof(*desc->init_seq),
					GFP_KERNEL);
		if (!desc->init_seq)
			return -ENOMEM;

		err = panel_simple_parse_cmd_seq(dev, data, len,
						 desc->init_seq);
		if (err) {
			dev_err(dev, "failed to parse init sequence\n");
			return err;
		}
	}

	data = dev_read_prop(dev, "panel-exit-sequence", &len);
	if (data) {
		desc->exit_seq = devm_kzalloc(dev, sizeof(*desc->exit_seq),
					GFP_KERNEL);
		if (!desc->exit_seq)
			return -ENOMEM;

		err = panel_simple_parse_cmd_seq(dev, data, len,
						desc->exit_seq);
		if (err) {
			dev_err(dev, "failed to parse exit sequence\n");
			return err;
		}
	}

	return 0;
}

static int panel_dsi_of_get_desc_data(struct udevice *dev,
					struct panel_desc_dsi *desc)
{
	u32 val;
	int ret;

	ret = panel_of_get_desc_data(dev, &desc->desc);
	if (ret)
		return ret;

	if (!dev_read_u32(dev, "dsi,flags", &val))
		desc->flags = val;
	if (!dev_read_u32(dev, "dsi,format", &val))
		desc->format = val;
	if (!dev_read_u32(dev, "dsi,lanes", &val))
		desc->lanes = val;

	return 0;
}

static const struct udevice_id dsi_of_match[] = {
	{
		.compatible = "panel-dsi",
	},
/*	{
		.compatible = "auo,b080uan01",
		.data = &auo_b080uan01
	}, */
	{ /* Sentinel */ },
};
static int panel_dsi_probe(struct udevice *dev)
{
	struct panel_dsi *dsi_panel = dev_get_priv(dev);
	struct mipi_dsi_device *dsi = dev_get_parent_plat(dev);
	const struct panel_desc_dsi *desc;
	struct panel_desc_dsi *d;
	struct sunxi_drm_panel *panel = &dsi_panel->panel;
	int ret;

	DRM_INFO("[DSI-PANEL] panel_dsi_probe start\n");

	desc = (struct panel_desc_dsi *)dev_get_driver_data(dev);

	if (!desc) {
		d = devm_kzalloc(dev, sizeof(*d), GFP_KERNEL);
		if (!d)
		return -ENOMEM;

		ret = panel_dsi_of_get_desc_data(dev, d);
		if (ret) {
			dev_err(dev, "failed to get desc data: %d\n", ret);
			return ret;
		}
		desc = d;
	}

	dsi_panel->dev = dev;
	dsi_panel->desc = &desc->desc;
	dsi_panel->dsi = dsi;

	ret = panel_dsi_parse_dt(dsi_panel);
	if (ret < 0)
		return ret;

	dev->driver_data = (ulong)panel;
	panel->dev = dev;
	panel->funcs = &panel_dsi_funcs;

	ret = uclass_get_device_by_phandle(UCLASS_PANEL_BACKLIGHT, dev,
					   "backlight", &dsi_panel->backlight);
	if (ret) {
		DRM_ERROR("%s: Cannot get backlight: %d\n", __func__, ret);
	}

	DRM_INFO("[DSI-PANEL] panel_dsi_probe finish\n");

	return 0;
}



U_BOOT_DRIVER(panel_dsi) = {
	.name	= "panel_dsi",
	.id	= UCLASS_PANEL,
	.of_match = dsi_of_match,
	.probe	= panel_dsi_probe,
	.priv_auto = sizeof(struct panel_dsi),
};
