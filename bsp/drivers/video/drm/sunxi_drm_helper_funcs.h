/*
 * sunxi_drm_helper_funcs.h
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
#ifndef _SUNXI_DRM_HELPER_FUNCS
#define _SUNXI_DRM_HELPER_FUNCS

#include <dm/ofnode_decl.h>
#include <linux/types.h>
#include <clk.h>
#include <reset.h>

struct udevice;
enum uclass_id;
enum drm_panel_orientation;
struct device_node;

enum {
	PORT_DIR_IN,
	PORT_DIR_OUT,
};

struct device_node *
sunxi_of_graph_get_endpoint_by_regs(ofnode node, int port, int endpoint);

struct device_node *
sunxi_of_graph_get_remote_node(ofnode node, int port, int endpoint);

struct device_node *sunxi_of_graph_get_port_parent(ofnode port);

struct device_node *sunxi_of_graph_get_remote_endpoint(struct device_node *endpoint);

int sunxi_of_get_irq_number(struct udevice *dev, u32 index);

struct udevice *sunxi_of_graph_get_remote_device(enum uclass_id id, struct udevice dev);

int sunxi_of_get_panel_orientation(struct udevice *dev,
				 enum drm_panel_orientation *orientation);

struct device_node *sunxi_of_graph_get_port_by_id(ofnode node, int id);

int sunxi_clk_enable(struct clk **clk_array, u32 no_of_clk, struct reset_ctl **rst_array, u32 no_of_rst);

int sunxi_clk_disable(struct clk **clk_array, u32 no_of_clk, struct reset_ctl **rst_array, u32 no_of_rst);

#endif /*End of file*/
