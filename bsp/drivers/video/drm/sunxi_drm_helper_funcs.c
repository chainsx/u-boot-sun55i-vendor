/*
 * sunxi_drm_ofnode_helper/sunxi_drm_ofnode_helper.c
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
#include "sunxi_drm_helper_funcs.h"
#include <dm.h>
#include <drm/drm_connector.h>
#include <drm/drm_print.h>

struct device_node *sunxi_of_graph_get_port_by_id(ofnode node, int id)
{
	ofnode ports, port;
	u32 reg;

	ports = ofnode_find_subnode(node, "ports");
	if (!ofnode_valid(ports))
		return NULL;

	ofnode_for_each_subnode(port, ports) {
		if (ofnode_read_u32(port, "reg", &reg))
			continue;

		if (reg == id)
			break;
	}

	if (reg == id)
		return ofnode_to_np(port);

	return NULL;
}


struct device_node *
sunxi_of_graph_get_endpoint_by_regs(ofnode node, int port, int endpoint)
{
	struct device_node *port_node;
	ofnode ep;
	u32 reg;

	port_node = sunxi_of_graph_get_port_by_id(node, port);
	if (!port_node)
		return NULL;

	ofnode_for_each_subnode(ep, np_to_ofnode(port_node)) {
		if (ofnode_read_u32(ep, "reg", &reg))
			break;
		if (reg == endpoint)
			break;
	}

	if (!ofnode_valid(ep))
		return NULL;

	return ofnode_to_np(ep);
}

struct device_node *
sunxi_of_graph_get_remote_node(ofnode node, int port, int endpoint)
{
	struct device_node *ep_node;
	ofnode ep;
	uint phandle;

	ep_node = sunxi_of_graph_get_endpoint_by_regs(node, port, endpoint);
	if (!ep_node)
		return NULL;

	if (ofnode_read_u32(np_to_ofnode(ep_node), "remote-endpoint", &phandle))
		return NULL;

	ep = ofnode_get_by_phandle(phandle);
	if (!ofnode_valid(ep))
		return NULL;

	return ofnode_to_np(ep);
}


struct device_node *sunxi_of_graph_get_port_parent(ofnode port)
{
	ofnode parent;
	int is_ports_node;

	parent = ofnode_get_parent(port);

	is_ports_node = strstr(ofnode_get_name(parent), "ports") ? 1 : 0;
	if (is_ports_node)
		parent = ofnode_get_parent(parent);

	return ofnode_to_np(parent);
}


struct device_node *sunxi_of_graph_get_remote_endpoint(struct device_node *endpoint)
{
	unsigned int phandle;
	ofnode ep, rep;
	struct device_node *remote_endpoint;

	ofnode_for_each_subnode(ep, np_to_ofnode(endpoint)) {
		//get remote ep
		if (ofnode_read_u32(ep, "remote-endpoint", &phandle))
			continue;

		rep = ofnode_get_by_phandle(phandle);
		if (!ofnode_valid(rep))
			continue;
		remote_endpoint = ofnode_to_np(rep);

		break;
	}

	return remote_endpoint;
}

int sunxi_of_get_irq_number(struct udevice *dev, u32 index)
{
	ofnode main_node;
	u32 *value = NULL;
	size_t size = (index + 1) * 3;
	int ret = -1, start_i = 0, irq_num = 0;

	value = malloc(size);
	if (!value) {
		return -1;
	}

	main_node = dev_ofnode(dev);
	if (!ofnode_valid(main_node)) {
		goto OUT;
	}
	start_i = index * 3;
	ret = ofnode_read_u32_array(main_node, "interrupts", value, size);
	if (!ret) {

		if (0 == value[start_i])
			irq_num = (value[start_i + 1] + 32);
		else
			irq_num = value[start_i + 1];
	} else
		irq_num = ret;

OUT:
	if (value) {
		free(value);
	}
	return irq_num;
}


int sunxi_of_get_panel_orientation(struct udevice *dev,
				 enum drm_panel_orientation *orientation)
{
	int rotation, ret;

	ret = dev_read_u32(dev, "rotation", &rotation);
	if (ret == -EINVAL) {
		/* Don't return an error if there's no rotation property. */
		*orientation = DRM_MODE_PANEL_ORIENTATION_UNKNOWN;
		return 0;
	}

	if (ret < 0)
		return ret;

	if (rotation == 0)
		*orientation = DRM_MODE_PANEL_ORIENTATION_NORMAL;
	else if (rotation == 90)
		*orientation = DRM_MODE_PANEL_ORIENTATION_RIGHT_UP;
	else if (rotation == 180)
		*orientation = DRM_MODE_PANEL_ORIENTATION_BOTTOM_UP;
	else if (rotation == 270)
		*orientation = DRM_MODE_PANEL_ORIENTATION_LEFT_UP;
	else
		return -EINVAL;

	return 0;
}

int sunxi_clk_enable(struct clk **clk_array, u32 no_of_clk, struct reset_ctl **rst_array, u32 no_of_rst)
{
	int ret = 0, i = 0;

	for (i = 0; i < no_of_rst; ++i) {
		if (!IS_ERR_OR_NULL(rst_array[i])) {
			ret = reset_deassert(rst_array[i]);
			if (ret) {
				DRM_INFO("reset_deassert for NO.%d failed!\n", i);
			}
		}
	}


	for (i = 0; i < no_of_clk; ++i) {
		if (!IS_ERR_OR_NULL(clk_array[i])) {
			ret = clk_prepare_enable(clk_array[i]);
			if (ret != 0) {
				DRM_INFO("fail enable NO.%d clock!\n", i);
			}
		}
	}
	return 0;
}

int sunxi_clk_disable(struct clk **clk_array, u32 no_of_clk, struct reset_ctl **rst_array, u32 no_of_rst)
{
	int ret = 0, i = 0;

	for (i = 0; i < no_of_clk; ++i) {
		if (!IS_ERR_OR_NULL(clk_array[i])) {
			ret = clk_disable_unprepare(clk_array[i]);
			if (ret != 0) {
				DRM_INFO("fail enable NO.%d clock!\n", i);
			}
		}
	}

	for (i = 0; i < no_of_rst; ++i) {
		if (!IS_ERR_OR_NULL(rst_array[i])) {
			ret = reset_assert(rst_array[i]);
			if (ret) {
				DRM_INFO("reset_deassert for NO.%d failed!\n", i);
			}
		}
	}

	return 0;
}

//End of File
