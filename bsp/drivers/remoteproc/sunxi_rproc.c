// SPDX-License-Identifier: GPL-2.0+
/*
 * drivers/remoteproc/sunxi_rproc.c
 *
 * Copyright (c) 2007-2025 Allwinnertech Co., Ltd.
 * Author: shihongfu <shihongfu@allwinnertech.com>
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

#define pr_fmt(fmt) "" fmt
#include <common.h>
#include <errno.h>
#include <fdtdec.h>
#include <linux/libfdt.h>
#include <fdt_support.h>
#include <log.h>
#include <remoteproc.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <dm/device-internal.h>
#include <sunxi_rproc.h>
#include "sunxi_rproc_internal.h"
#if CONFIG_IS_ENABLED(AW_RPROC_RSC_HELPER)
#include "sunxi_rproc_rsc_helper.h"
#endif
#if CONFIG_IS_ENABLED(BOOT_RISCV)
#include "sunxi_rproc_riscv.h"
#endif

#define SUNXI_RPROC_VERSION "1.0.0"

#ifdef RPROC_DEBUG
#define SUNXI_RPROC_DEBUG
#endif

#define SUNXI_RPROC_PRINTF(_cond, _title, fmt, arg...) \
	do { \
		if (_cond) \
			printf(_title "%s " fmt "\n", __func__, ##arg); \
	} while (0)

#ifdef SUNXI_RPROC_DEBUG
#define SUNXI_RPROC_TRACE(fmt, arg...) SUNXI_RPROC_PRINTF(1, "trace ", fmt, ##arg)
#else
#define SUNXI_RPROC_TRACE(fmt, arg...)
#endif

struct sunxi_rproc_memory_mapping {
	u64 pa;				/* Address seen on the cpu side */
	u64 da;				/* Address seen on the remote processor side */
	u64 len;
};

struct sunxi_rproc_privdata {
	// object
	struct udevice *dev;
	struct dm_rproc_uclass_pdata *uc_pdata;
	const struct sunxi_rproc_driver_data *driver_data;
	bool running;
	// config from dts
	bool auto_boot;
	int mem_maps_cnt;
	struct sunxi_rproc_memory_mapping *mem_maps;
	u64 fw_mem;
	u64 fw_mem_size;
	// firmware
	struct firmware fw;
	struct resource_table *table_ptr;
	size_t table_sz;
	// ops
	int (*find_rsc_table)(const void *fw_addr, size_t fw_size, const void **table_ptr,
			      size_t *table_sz);
};

static int sunxi_rproc_set_fw_partitions(struct sunxi_rproc_privdata *priv,
					 const char **partitions, int count, int sectors)
{
	char prop_buf[128] = {0};
	int nodeoffset, len, off, i, ret;
	u32 part_sectors;
	const char *key;

	memset(prop_buf, 0, sizeof(prop_buf));
	snprintf(prop_buf, sizeof(prop_buf), "/soc/%s", priv->dev->name);

	nodeoffset = fdt_path_offset(working_fdt, prop_buf);
	if (nodeoffset < 0) {
		dev_err(priv->dev, "can't find node: %s\n", prop_buf);
		return -ENODATA;
	}

	key = "fw-partition-sectors";
	SUNXI_RPROC_TRACE("update %s: %d", key, sectors);
	part_sectors = cpu_to_fdt32(sectors);
	ret = fdt_setprop(working_fdt, nodeoffset, key, &part_sectors, sizeof(part_sectors));
	if (ret) {
		dev_err(priv->dev, "can't set val: %s\n", key);
		return -EINVAL;
	}

	key = "fw-partitions";
	SUNXI_RPROC_TRACE("update %s num: %d", key, count);
	if (!count)
		goto skip_update_fw_partitions;
	memset(prop_buf, 0, sizeof(prop_buf));
	off = 0;
	for (i = 0; i < count; i++) {
		len = strlen(partitions[i]);
		SUNXI_RPROC_TRACE("fw-partitions[%d]: %s(%d)", i, partitions[i], len);
		if ((off + len + 1) > sizeof(prop_buf)) {
			dev_err(priv->dev, "buf limit, drop fw-partitions[%d]: %s(%d)\n",
				i, partitions[i], len);
			continue;
		}
		memcpy(prop_buf + off, partitions[i], len);
		off += len + 1;
	}
	if (!off)
		goto skip_update_fw_partitions;
	ret = fdt_setprop(working_fdt, nodeoffset, key, prop_buf, off);
	if (ret) {
		dev_err(priv->dev, "can't set val: %s\n", key);
		return -EINVAL;
	}
skip_update_fw_partitions:

	return 0;
}

static int sunxi_rproc_resource_get(struct sunxi_rproc_privdata *priv)
{
	char prop_path[128] = {0};
	int nodeoffset, nodeoffset2, len;
	const fdt32_t *data = NULL;
	u32 part_sectors;
	const u32 *p;
	int count, i;
	const char *part_name;

	SUNXI_RPROC_TRACE("priv: %p", priv);

	memset(prop_path, 0, sizeof(prop_path));
	snprintf(prop_path, sizeof(prop_path), "/soc/%s", priv->dev->name);

	nodeoffset = fdt_path_offset(working_fdt, prop_path);
	if (nodeoffset < 0) {
		dev_err(priv->dev, "can't find node: %s\n", prop_path);
		return -ENODATA;
	}

	data = fdt_getprop(working_fdt, nodeoffset, "auto-boot", &len);
	if (data)
		priv->auto_boot = true;
	else
		priv->auto_boot = false;
	SUNXI_RPROC_TRACE("auto-boot: %s", priv->auto_boot ? "true" : "false");

	data = fdt_getprop(working_fdt, nodeoffset, "memory-mappings", &len);
	if (!data || len % (sizeof(*p) * 3)) {
		dev_err(priv->dev, "can't find val: %s\n", "memory-mappings");
		return -EINVAL;
	}
	priv->mem_maps_cnt = len / (sizeof(*p) * 3);
	p = data;
	SUNXI_RPROC_TRACE("mem_maps_cnt: %d", priv->mem_maps_cnt);
	priv->mem_maps = malloc(priv->mem_maps_cnt * sizeof(*priv->mem_maps));
	if (!priv->mem_maps) {
		dev_err(priv->dev, "can't alloc memory for: %s\n", "memory-mappings");
		return -ENOMEM;
	}
	for (i = 0; i < priv->mem_maps_cnt; i++) {
		priv->mem_maps[i].da = fdt32_to_cpu(p[i * 3]);
		priv->mem_maps[i].len = fdt32_to_cpu(p[i * 3 + 1]);
		priv->mem_maps[i].pa = fdt32_to_cpu(p[i * 3 + 2]);
		SUNXI_RPROC_TRACE("mem_maps[%d]: da: %lx, pa: %lx, len: %lx", i,
				  (unsigned long)priv->mem_maps[i].da,
				  (unsigned long)priv->mem_maps[i].pa,
				  (unsigned long)priv->mem_maps[i].len);
	}

	data = fdt_getprop(working_fdt, nodeoffset, "fw-region", &len);
	if (!data) {
		dev_err(priv->dev, "can't find val: %s\n", "fw-region");
		return -EINVAL;
	}
	nodeoffset2 = fdt_node_offset_by_phandle(working_fdt, fdt32_to_cpu(*(uint32_t *)data));
	if (nodeoffset2 < 0) {
		dev_err(priv->dev, "can't get offset: %s\n", "fw-region");
		return -EINVAL;
	}
	data = fdt_getprop(working_fdt, nodeoffset2, "reg", &len);
	if (!data || len != (sizeof(*p) * 4)) {
		dev_err(priv->dev, "can't get reg: %s\n", "fw-region");
		return -EINVAL;
	}
	p = data;
	priv->fw_mem = ((u64)fdt32_to_cpu(p[0]) << 32) + fdt32_to_cpu(p[1]);
	priv->fw_mem_size = ((u64)fdt32_to_cpu(p[2]) << 32) + fdt32_to_cpu(p[3]);
	SUNXI_RPROC_TRACE("fw-region: %lx (+%lx)",
			  (unsigned long)priv->fw_mem, (unsigned long)priv->fw_mem_size);

	data = fdt_getprop(working_fdt, nodeoffset, "fw-partition-sectors", &len);
	if (!data || len != sizeof(part_sectors)) {
		dev_err(priv->dev, "can't find val: %s\n", "fw-partition-sectors");
		return -EINVAL;
	}
	part_sectors = fdt32_to_cpu(*data);
	SUNXI_RPROC_TRACE("fw-partition-sectors: %u", part_sectors);

	count = fdt_stringlist_count(working_fdt, nodeoffset, "fw-partitions");
	SUNXI_RPROC_TRACE("fw-partitions num: %d", count);
	for (i = 0; i < count; i++) {
		part_name = fdt_stringlist_get(working_fdt, nodeoffset, "fw-partitions", i, &len);
		if (!part_name) {
			dev_err(priv->dev, "can't find val: %s[%d]\n", "fw-partitions", i);
			return -EINVAL;
		}
		SUNXI_RPROC_TRACE("fw-partitions[%d]: %s(%d)", i, part_name, len);
	}

	return 0;
}

static
int sunxi_rproc_da_to_pa(struct sunxi_rproc_privdata *priv, u64 da, size_t len, u64 *pa)
{
	struct sunxi_rproc_memory_mapping *map;
	int i;

	for (i = 0; i < priv->mem_maps_cnt; i++) {
		map = &priv->mem_maps[i];
		if (da < map->da || da >= map->da + map->len)
			continue;
		*pa = da - map->da + map->pa;
		dev_dbg(priv->dev, "translate da 0x%llx to pa %pa\n", da, pa);
		return 0;
	}

	dev_err(priv->dev, "Failed to translate da 0x%llx to pa\n", da);
	return -EINVAL;
}

static void *sunxi_rproc_da_to_va(struct sunxi_rproc_privdata *priv, u64 da, size_t len)
{
	u64 pa;
	int ret;

	/* first step: translate da to pa */
	ret = sunxi_rproc_da_to_pa(priv, da, len, &pa);
	if (ret) {
		dev_err(priv->dev, "invalid da 0x%llx\n", da);
		return NULL;
	}

	/* second step: get va from carveouts via pa */
	return (void *)(unsigned long)pa;
}

static int sunxi_rproc_find_rsc_table(struct udevice *dev, const struct firmware *fw)
{
	struct dm_rproc_uclass_pdata *uc_pdata;
	struct sunxi_rproc_privdata *priv;
	int ret;

	if (!dev || !fw || !fw->data || fw->size <= 0) {
		printf("%s parameter error!\n", __func__);
		return -EINVAL;
	}

	uc_pdata = dev_get_uclass_plat(dev);
	priv = dev_get_priv(dev);

	if (!priv->driver_data->find_rsc_table) {
		dev_err(priv->dev, "%s internal error!\n", __func__);
		return -ENODEV;
	}

	// note: fw has been loaded in ddr, not boot addr.
	ret = priv->driver_data->find_rsc_table(fw->data, fw->size,
						(const void **)&priv->table_ptr, &priv->table_sz);
	if (ret) {
		priv->table_ptr = NULL;
		priv->table_sz = 0;
		dev_err(priv->dev, "%s find_rsc_table failed! ret: %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static int sunxi_rproc_handle_vendor_src(struct udevice *dev, u32 rsc_type,
					 void *rsc, int offset, int avail)
{
#if CONFIG_IS_ENABLED(AW_RPROC_RSC_HELPER)
	int ret;
#endif

	SUNXI_RPROC_TRACE("");

#if CONFIG_IS_ENABLED(AW_RPROC_RSC_HELPER)
	ret = sunxi_rproc_rsc_helper_handle_rsc(dev, rsc_type, rsc, offset, avail);
	if (ret == RSC_HANDLED)
		return ret;
#endif

	dev_err(dev, "%s nobody handle rsc_type: %u\n", __func__, rsc_type);
	return RSC_IGNORED;
}

static int sunxi_rproc_handle_resources(struct udevice *dev)
{
	struct sunxi_rproc_privdata *priv = dev_get_priv(dev);
	struct resource_table *rsc_table;
	size_t table_sz;
	int ret = 0, i;

	SUNXI_RPROC_TRACE("priv: %p", priv);

	if (!priv->table_ptr || priv->table_sz <= 0) {
		dev_err(dev, "internal error!\n");
		return -ENODATA;
	}
	rsc_table = priv->table_ptr;
	table_sz = priv->table_sz;

	for (i = 0; i < rsc_table->num; i++) {
		int offset = rsc_table->offset[i];
		struct fw_rsc_hdr *hdr = (void *)rsc_table + offset;
		int avail = table_sz - offset - sizeof(*hdr);
		void *rsc = (void *)hdr + sizeof(*hdr);

		/* make sure table isn't truncated */
		if (avail < 0) {
			dev_err(dev, "rsc table is truncated\n");
			return -EINVAL;
		}

		SUNXI_RPROC_TRACE("rsc: type %d", hdr->type);

		if (hdr->type >= RSC_VENDOR_START && hdr->type <= RSC_VENDOR_END) {
			ret = sunxi_rproc_handle_vendor_src(dev, hdr->type, rsc,
							    offset + sizeof(*hdr), avail);
			if (ret == RSC_HANDLED)
				continue;
			else if (ret < 0)
				break;

			dev_err(dev, "unsupported vendor resource %d\n", hdr->type);
			continue;
		}

		if (hdr->type >= RSC_LAST) {
			dev_err(dev, "unsupported resource %d\n", hdr->type);
			continue;
		}

		// ignore std rsc
		ret = RSC_HANDLED;
		if (ret == RSC_HANDLED)
			continue;
		else if (ret < 0)
			break;
	}

	return (ret == RSC_HANDLED) ? 0 : -1;
}

static int sunxi_rproc_load(struct udevice *dev, ulong addr, ulong size)
{
	struct sunxi_rproc_privdata *priv = dev_get_priv(dev);
	int ret;

	SUNXI_RPROC_TRACE("priv: %p", priv);

	// using kernel's dts
	ret = sunxi_rproc_resource_get(priv);
	if (ret) {
		dev_err(dev, "sunxi_rproc_resource_get failed, ret: %d\n", ret);
		return ret;
	}

	if (!priv->auto_boot) {
		SUNXI_RPROC_TRACE("not need to load");
		return 0;
	}

#if 0
	priv->fw.data = (u8 *)addr;
	priv->fw.size = size;

	if ((unsigned long)priv->fw.data != (unsigned long)priv->fw_mem) {
		dev_err(dev, "fw.data(%p) != priv->fw_mem(%lx)\n",
			priv->fw.data, (unsigned long)priv->fw_mem);
	}
	if (priv->fw.size > priv->fw_mem_size) {
		dev_err(dev, "fw.size(%lx) > priv->fw_mem_size(%lx)\n",
			(unsigned long)priv->fw.size, (unsigned long)priv->fw_mem_size);
	}
#else
	priv->fw.data = (u8 *)(unsigned long)priv->fw_mem;
	priv->fw.size = priv->fw_mem_size;
#endif

	ret = sunxi_rproc_find_rsc_table(dev, &priv->fw);
	if (ret) {
		dev_err(dev, "sunxi_rproc_find_rsc_table failed, ret: %d\n", ret);
		return ret;
	}

	ret = sunxi_rproc_handle_resources(dev);
	if (ret) {
		dev_err(dev, "sunxi_rproc_handle_resources failed, ret: %d\n", ret);
		return ret;
	}

	return 0;
}

static int sunxi_rproc_start(struct udevice *dev)
{
	struct sunxi_rproc_privdata *priv = dev_get_priv(dev);

	SUNXI_RPROC_TRACE("priv: %p", priv);

	if (!priv->auto_boot) {
		SUNXI_RPROC_TRACE("not need to start");
		return 0;
	}

	// TODO

	priv->running = true;
	return 0;
}

static int sunxi_rproc_probe(struct udevice *dev)
{
	struct sunxi_rproc_privdata *priv = dev_get_priv(dev);

	SUNXI_RPROC_TRACE("priv: %p", priv);
	dev_info(dev, "version: %s\n", SUNXI_RPROC_VERSION);

	priv->dev = dev;
	priv->uc_pdata = dev_get_uclass_plat(dev);
	priv->driver_data = (void *)dev_get_driver_data(dev);

	return 0;
}

static int sunxi_rproc_is_running(struct udevice *dev)
{
	struct sunxi_rproc_privdata *priv = dev_get_priv(dev);

	SUNXI_RPROC_TRACE("priv: %p", priv);
	return priv->running ? 1 : 0;
}

static void *sunxi_rproc_device_to_virt(struct udevice *dev, ulong da, ulong size)
{
	struct sunxi_rproc_privdata *priv = dev_get_priv(dev);

	SUNXI_RPROC_TRACE("priv: %p, da: %lx, size: %lx", priv, da, size);
	return sunxi_rproc_da_to_va(priv, da, size);
}

static const struct dm_rproc_ops sunxi_rproc_ops = {
	.load = sunxi_rproc_load,
	.start = sunxi_rproc_start,
	.is_running = sunxi_rproc_is_running,
	.device_to_virt = sunxi_rproc_device_to_virt,
};

static const struct udevice_id sunxi_rproc_ids[] = {
#if CONFIG_IS_ENABLED(BOOT_RISCV)
	SUNXI_RPROC_RISCV_IDS,
#endif
	{}
};

U_BOOT_DRIVER(sunxi_rproc) = {
	.name = "sunxi_rproc",
	.of_match = sunxi_rproc_ids,
	.id = UCLASS_REMOTEPROC,
	.ops = &sunxi_rproc_ops,
	.probe = sunxi_rproc_probe,
	.priv_auto	= sizeof(struct sunxi_rproc_privdata),
};

int sunxi_remoteproc_get_id_by_name(const char *name)
{
	int ret;
	struct uclass *uc;
	struct udevice *dev;

	ret = uclass_get(UCLASS_REMOTEPROC, &uc);
	if (ret)
		return -ENODEV;

	uclass_foreach_dev(dev, uc) {
		if (!strcmp(dev->name, name))
			return dev->seq_;
	}

	return -ENODEV;
}

int sunxi_remoteproc_set_fw_partitions(int id, const char **partitions, int count, int sectors)
{
	int ret;
	struct uclass *uc;
	struct udevice *dev;
	struct sunxi_rproc_privdata *priv;

	ret = uclass_get(UCLASS_REMOTEPROC, &uc);
	if (ret)
		return -ENODEV;

	uclass_foreach_dev(dev, uc) {
		if (dev->seq_ == id)
			goto found;
	}
	return -ENODEV;

found:
	priv = dev_get_priv(dev);
	return sunxi_rproc_set_fw_partitions(priv, partitions, count, sectors);
}

int sunxi_remoteproc_init(void)
{
	int ret, id;
	struct uclass *uc;
	struct udevice *dev;

	ret = uclass_get(UCLASS_REMOTEPROC, &uc);
	if (ret)
		return -1;

	uclass_foreach_dev(dev, uc) {
		ret = device_probe(dev);
		if (ret) {
			pr_err("%s - probe failed: %d\n", dev->name, ret);
			continue;
		}
		id = dev->seq_;
		pr_info("%s - id: %d\n", dev->name, id);
	}

	return 0;
}
