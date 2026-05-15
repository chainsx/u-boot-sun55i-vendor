/*
 * panels/panel-lvds/panel-lvds.c
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
#include <dm/device_compat.h>
#include <backlight.h>
#include <power/regulator.h>
#include <asm/gpio.h>
#include <linux/delay.h>
#include <drm/drm_modes.h>
#include <media_bus_format.h>
#include "../sunxi_drm_panel.h"
#include "../sunxi_drm_helper_funcs.h"

#define POWER_MAX 3
#define GPIO_MAX 3


struct panel_lvds {
	struct sunxi_drm_panel panel;
	struct udevice *dev;

	const char *label;
	unsigned int width;
	unsigned int height;
	struct videomode video_mode;
	bool data_mirror;
	struct {
	//unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
//		unsigned int unprepare;
		unsigned int reset;
//		unsigned int init;
	} delay;

	struct udevice *backlight;
	struct udevice *supply[POWER_MAX];

	struct gpio_desc *enable_gpio[GPIO_MAX];
	struct gpio_desc *reset_gpio;
};


static void panel_lvds_sleep(unsigned int msec)
{
	mdelay(msec);
}


static inline struct panel_lvds *to_panel_lvds(struct sunxi_drm_panel *panel)
{
	return container_of(panel, struct panel_lvds, panel);
}


static int panel_lvds_prepare(struct sunxi_drm_panel *panel)
{
	struct panel_lvds *lvds = to_panel_lvds(panel);
	int err, i;

	DRM_INFO("%s\n", __func__);
	for (i = 0; i < POWER_MAX; i++) {
		if (lvds->supply[i]) {
			err = regulator_set_enable(lvds->supply[i], true);
			if (err < 0) {
				DRM_ERROR("failed to enable supply%d: %d\n",
					i, err);
				return err;
			}
			mdelay(10);
		}
	}

	for (i = 0; i < GPIO_MAX; i++) {
		if (lvds->enable_gpio[i]) {
			dm_gpio_set_value(lvds->enable_gpio[i], 1);

			if (lvds->delay.enable)
				mdelay(lvds->delay.enable);
		}
	}

	if (lvds->reset_gpio)
		dm_gpio_set_value(lvds->reset_gpio, 1);
	if (lvds->delay.reset)
		mdelay(lvds->delay.reset);

	return 0;
}

static int panel_lvds_enable(struct sunxi_drm_panel *panel)
{
	struct panel_lvds *lvds = to_panel_lvds(panel);

	DRM_INFO("%s\n", __func__);
	if (lvds->backlight) {
		/*backlight_set_brightness(lvds->backlight)*/
		backlight_enable(lvds->backlight);
	}

	return 0;
}


static int panel_lvds_disable(struct sunxi_drm_panel *panel)
{
	struct panel_lvds *lvds = to_panel_lvds(panel);

	if (lvds->backlight)
		backlight_set_brightness(lvds->backlight, 0);

	return 0;
}

static int panel_lvds_unprepare(struct sunxi_drm_panel *panel)
{
	struct panel_lvds *lvds = to_panel_lvds(panel);
	int i;


	for (i = GPIO_MAX; i > 0; i--) {
		if (lvds->enable_gpio[i - 1]) {
			dm_gpio_set_value(lvds->enable_gpio[i - 1], 0);
			if (lvds->delay.enable)
				panel_lvds_sleep(lvds->delay.disable);
		}
	}

	if (lvds->reset_gpio)
		dm_gpio_set_value(lvds->reset_gpio, 0);
	if (lvds->delay.reset)
		panel_lvds_sleep(lvds->delay.reset);

	for (i = POWER_MAX; i > 0; i--) {
		if (lvds->supply[i - 1]) {
			regulator_set_enable(lvds->supply[i - 1], false);
			mdelay(10);
		}
	}

	return 0;
}

static const struct sunxi_drm_panel_funcs panel_lvds_funcs = {
	.disable = panel_lvds_disable,
	.unprepare = panel_lvds_unprepare,
	.prepare = panel_lvds_prepare,
	.enable = panel_lvds_enable,
	/*.get_modes = panel_rgb_get_modes,*/
};


static int panel_lvds_parse_dt(struct panel_lvds *lvds)
{
	char power_name[40] = {0}, gpio_name[40] = {0};
	int ret = -1, i = 0;


	for (i = 0; i < GPIO_MAX; ++i) {
		lvds->enable_gpio[i] =
			kmalloc(sizeof(struct gpio_desc), __GFP_ZERO);
	}
	lvds->reset_gpio = kmalloc(sizeof(struct gpio_desc), __GFP_ZERO);

	ret = sunxi_of_get_panel_orientation(lvds->dev, &lvds->panel.orientation);

	for (i = 0; i < POWER_MAX; i++) {
		snprintf(power_name, 40, "power%d-supply", i);

		ret = uclass_get_device_by_phandle(UCLASS_REGULATOR, lvds->dev,
						   power_name, &lvds->supply[i]);
		if (ret) {
			pr_err("failed to request regulator(%s): %d\n", power_name, ret);
		}
	}

	/* Get GPIOs and backlight controller. */
	for (i = 0; i < GPIO_MAX; i++) {
		snprintf(gpio_name, 40, "enable%d-gpios", i);

		ret = gpio_request_by_name(lvds->dev, gpio_name, 0,
					   lvds->enable_gpio[i], GPIOD_IS_OUT);
		if (ret) {
			kfree(lvds->enable_gpio[i]);
			lvds->enable_gpio[i] = NULL;
			pr_err("failed to request %s GPIO: %d\n", gpio_name, ret);
		}
	}

	ret = gpio_request_by_name(lvds->dev, "reset-gpios", 0,
				   lvds->reset_gpio, GPIOD_IS_OUT);
	if (ret) {
		kfree(lvds->reset_gpio);
		lvds->reset_gpio = NULL;
		pr_err("failed to request %s GPIO: %d\n", "reset", ret);
	}


	lvds->panel.bus_format = dev_read_u32_default(lvds->dev, "bus-format", MEDIA_BUS_FMT_RGB888_1X7X4_JEIDA);

	lvds->data_mirror = dev_read_bool(lvds->dev, "data-mirror");

	return 0;

}

static int panel_lvds_probe(struct udevice *dev)
{
	struct panel_lvds *lvds = dev_get_priv(dev);
	struct sunxi_drm_panel *panel = &lvds->panel;
	int ret = 0;

	lvds->dev = dev;

	ret = panel_lvds_parse_dt(lvds);
	if (ret < 0)
		return ret;

	ret = uclass_get_device_by_phandle(UCLASS_PANEL_BACKLIGHT, dev,
					   "backlight", &lvds->backlight);
	if (ret) {
		pr_err("%s: Cannot get backlight: %d\n", __func__, ret);
	}


	dev->driver_data = (ulong)panel;
	panel->dev = dev;
	panel->funcs = &panel_lvds_funcs;

	return 0;
}

static const struct udevice_id panel_lvds_of_table[] = {
	{ .compatible = "sunxi-lvds", },
	{ /* Sentinel */ },
};

U_BOOT_DRIVER(sunxi_lvds) = {
	.name	= "sunxi_lvds",
	.id	= UCLASS_PANEL,
	.of_match = panel_lvds_of_table,
	.probe	= panel_lvds_probe,
	.priv_auto = sizeof(struct panel_lvds),
};

//End of File
