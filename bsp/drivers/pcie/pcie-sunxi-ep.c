// SPDX-License-Identifier: GPL-2.0+
/*
 * sunxi DesignWare based PCIe EP controller driver
 *
 * Copyright (c) 2021 sunxi, Inc.
 */

#include <common.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <dm/devres.h>
#include <errno.h>
#include <pci_ep.h>
#include <asm/global_data.h>
#include <linux/sizes.h>
#include <linux/log2.h>
#include "pcie-sunxi.h"
#include <power-domain.h>
#include <power/regulator.h>
#include <asm/gpio.h>
#include <asm/io.h>

DECLARE_GLOBAL_DATA_PTR;

static void sunxi_pcie_setup_ep(struct sunxi_pcie *pci)
{
	sunxi_pcie_plat_set_rate(pci);
}

static unsigned int sunxi_pcie_ep_func_select(struct sunxi_pcie_ep *ep, u8 func_no)
{
	unsigned int func_offset = 0;

	if (ep->ops->func_conf_select)
		func_offset = ep->ops->func_conf_select(ep, func_no);

	return func_offset;
}

static u8 __sunxi_pcie_ep_find_next_cap(struct sunxi_pcie_ep *ep, u8 func_no,
						u8 cap_ptr, u8 cap)
{
	struct sunxi_pcie *pci = to_sunxi_pcie_from_ep(ep);
	unsigned int func_offset = 0;
	u8 cap_id, next_cap_ptr;
	u16 reg;

	if (!cap_ptr)
		return 0;

	func_offset = sunxi_pcie_ep_func_select(ep, func_no);

	reg = sunxi_pcie_readw_dbi(pci, func_offset + cap_ptr);
	cap_id = (reg & 0x00ff);

	if (cap_id > PCI_CAP_ID_MAX)
		return 0;

	if (cap_id == cap)
		return cap_ptr;

	next_cap_ptr = (reg & 0xff00) >> 8;
	return __sunxi_pcie_ep_find_next_cap(ep, func_no, next_cap_ptr, cap);
}

static u8 sunxi_pcie_ep_find_capability(struct sunxi_pcie_ep *ep, u8 func_no, u8 cap)
{
	struct sunxi_pcie *pci = to_sunxi_pcie_from_ep(ep);

	unsigned int func_offset = 0;
	u8 next_cap_ptr;
	u16 reg;

	func_offset = sunxi_pcie_ep_func_select(ep, func_no);

	reg = sunxi_pcie_readw_dbi(pci, func_offset + PCI_CAPABILITY_LIST);
	next_cap_ptr = (reg & 0x00ff);

	return __sunxi_pcie_ep_find_next_cap(ep, func_no, next_cap_ptr, cap);
}

static unsigned int sunxi_pcie_ep_find_ext_capability(struct sunxi_pcie *pci, int cap)
{
	u32 header;
	int pos = PCI_CFG_SPACE_SIZE;

	while (pos) {
		header = sunxi_pcie_readl_dbi(pci, pos);
		if (PCI_EXT_CAP_ID(header) == cap)
			return pos;

		pos = PCI_EXT_CAP_NEXT(header);
		if (!pos)
			break;
	}

	return 0;
}

struct sunxi_pcie_ep_func *sunxi_pcie_ep_get_func_from_ep(struct sunxi_pcie_ep *ep, u8 func_no)
{
	struct sunxi_pcie_ep_func *ep_func;

	list_for_each_entry(ep_func, &ep->func_list, list) {
		if (ep_func->func_no == func_no)
			return ep_func;
	}

	return NULL;
}

static void __sunxi_pcie_ep_reset_bar(struct sunxi_pcie *pci, u8 func_no,
					enum pci_barno bar, int flags)
{
	u32 reg;
	unsigned int func_offset = 0;
	struct sunxi_pcie_ep *ep = &pci->ep;

	func_offset = sunxi_pcie_ep_func_select(ep, func_no);

	reg = func_offset + PCI_BASE_ADDRESS_0 + (4 * bar);

	sunxi_pcie_dbi_ro_wr_en(pci);
	sunxi_pcie_writel_dbi(pci, reg, 0x0);

	if (flags & PCI_BASE_ADDRESS_MEM_TYPE_64) {
		sunxi_pcie_writel_dbi(pci, reg + 4, 0x0);
	}
	sunxi_pcie_dbi_ro_wr_dis(pci);
}

static void sunxi_pcie_ep_reset_bar(struct sunxi_pcie *pci, enum pci_barno bar)
{
	u8 func_no, funcs;

	funcs = pci->ep.max_functions;

	for (func_no = 0; func_no < funcs; func_no++)
		__sunxi_pcie_ep_reset_bar(pci, func_no, bar, 0);
}

static void sunxi_pcie_prog_inbound_atu(struct sunxi_pcie *pci, u8 func_no, int index,
						int type, u64 cpu_addr, u8 bar)
{
	sunxi_pcie_writel_dbi(pci, PCIE_ATU_LOWER_TARGET_INBOUND(index),
					lower_32_bits(cpu_addr));
	sunxi_pcie_writel_dbi(pci, PCIE_ATU_UPPER_TARGET_INBOUND(index),
					upper_32_bits(cpu_addr));
	sunxi_pcie_writel_dbi(pci, PCIE_ATU_CR1_INBOUND(index),
					type | PCIE_ATU_FUNC_NUM(func_no));
	sunxi_pcie_writel_dbi(pci, PCIE_ATU_CR2_INBOUND(index),
					PCIE_ATU_ENABLE | PCIE_ATU_FUNC_NUM_MATCH_EN |
					PCIE_ATU_BAR_MODE_ENABLE | (bar << 8));
}

// static void sunxi_pcie_prog_ep_outbound_atu(struct sunxi_pcie *pci, u8 func_no, int index,
// 						int type, u64 cpu_addr, u64 pci_addr,
// 						u64 size)
// {
// 	sunxi_pcie_writel_dbi(pci, PCIE_ATU_LOWER_BASE_OUTBOUND(index),
// 					lower_32_bits(cpu_addr));
// 	sunxi_pcie_writel_dbi(pci, PCIE_ATU_UPPER_BASE_OUTBOUND(index),
// 					upper_32_bits(cpu_addr));
// 	sunxi_pcie_writel_dbi(pci, PCIE_ATU_LIMIT_OUTBOUND(index),
// 					lower_32_bits(cpu_addr + size - 1));
// 	sunxi_pcie_writel_dbi(pci, PCIE_ATU_LOWER_TARGET_OUTBOUND(index),
// 					lower_32_bits(pci_addr));
// 	sunxi_pcie_writel_dbi(pci, PCIE_ATU_UPPER_TARGET_OUTBOUND(index), upper_32_bits(pci_addr));
// 	sunxi_pcie_writel_dbi(pci, PCIE_ATU_CR1_OUTBOUND(index),
// 					type | PCIE_ATU_FUNC_NUM(func_no));
// 	sunxi_pcie_writel_dbi(pci, PCIE_ATU_CR2_OUTBOUND(index),
// 					PCIE_ATU_ENABLE);
// }

static int sunxi_pcie_ep_inbound_atu(struct sunxi_pcie_ep *ep, u8 func_no, int type,
						dma_addr_t cpu_addr, enum pci_barno bar)
{
	u32 free_win = 0;
	struct sunxi_pcie *pci = to_sunxi_pcie_from_ep(ep);

	if (!ep->bar_to_atu[bar])
		free_win = find_first_zero_bit(ep->ib_window_map, ep->num_ib_windows);
	else
		free_win = ep->bar_to_atu[bar];

	if (free_win >= ep->num_ib_windows) {
		dev_err(pci->dev, "No free inbound window\n");
		return -EINVAL;
	}

	sunxi_pcie_prog_inbound_atu(pci, func_no, free_win, type,
						cpu_addr, bar);

	ep->bar_to_atu[bar] = free_win;
	test_and_set_bit(free_win, ep->ib_window_map);

	return 0;
}

// static int sunxi_pcie_ep_outbound_atu(struct sunxi_pcie_ep *ep, u8 func_no,
// 					phys_addr_t phys_addr,
// 					u64 pci_addr, size_t size)
// {
// 	struct sunxi_pcie *pci = to_sunxi_pcie_from_ep(ep);
// 	u32 free_win;

// 	free_win = find_first_zero_bit(ep->ob_window_map, ep->num_ob_windows);
// 	if (free_win >= ep->num_ob_windows) {
// 		dev_err(pci->dev, "No free outbound window\n");
// 		return -EINVAL;
// 	}

// 	sunxi_pcie_prog_ep_outbound_atu(pci, func_no, free_win, PCIE_ATU_TYPE_MEM,
// 					   phys_addr, pci_addr, size);

// 	set_bit(free_win, ep->ob_window_map);
// 	ep->outbound_addr[free_win] = phys_addr;

// 	return 0;
// }

static void sunxi_ep_init_bar(struct sunxi_pcie_ep *ep)
{
	struct sunxi_pcie *pci = to_sunxi_pcie_from_ep(ep);
	struct sunxi_pcie_ep_func *ep_func;
	enum pci_barno bar;

	ep_func = sunxi_pcie_ep_get_func_from_ep(ep, 0);
	if (!ep_func)
		return;

	for (bar = 0; bar < PCI_STD_NUM_BARS; bar++)
		sunxi_pcie_ep_reset_bar(pci, bar);
}

static int sunxi_pcie_ep_write_header(struct udevice *dev, uint fn,
			     struct pci_ep_header *hdr)
{
	struct sunxi_pcie *pci = dev_get_plat(dev);
	unsigned int func_offset = 0;
	struct sunxi_pcie_ep *ep = &pci->ep;

	func_offset = sunxi_pcie_ep_func_select(ep, fn);

	sunxi_pcie_dbi_ro_wr_en(pci);
	sunxi_pcie_writew_dbi(pci, func_offset + PCI_VENDOR_ID, hdr->vendorid);
	sunxi_pcie_writew_dbi(pci, func_offset + PCI_DEVICE_ID, hdr->deviceid);
	sunxi_pcie_writeb_dbi(pci, func_offset + PCI_REVISION_ID, hdr->revid);
	sunxi_pcie_writeb_dbi(pci, func_offset + PCI_CLASS_PROG, hdr->progif_code);
	sunxi_pcie_writew_dbi(pci, func_offset + PCI_CLASS_DEVICE,
					hdr->subclass_code | hdr->baseclass_code << 8);
	sunxi_pcie_writeb_dbi(pci, func_offset + PCI_CACHE_LINE_SIZE,
					hdr->cache_line_size);
	sunxi_pcie_writew_dbi(pci, func_offset + PCI_SUBSYSTEM_VENDOR_ID,
					hdr->subsys_vendor_id);
	sunxi_pcie_writew_dbi(pci, func_offset + PCI_SUBSYSTEM_ID, hdr->subsys_id);
	sunxi_pcie_writeb_dbi(pci, func_offset + PCI_INTERRUPT_PIN,
					hdr->interrupt_pin);
	sunxi_pcie_dbi_ro_wr_dis(pci);

	return 0;
}

static int sunxi_pcie_ep_set_bar(struct udevice *dev, uint fn, struct pci_bar *ep_bar)
{
	struct sunxi_pcie *pci = dev_get_plat(dev);
	struct sunxi_pcie_ep *ep = &pci->ep;
	enum pci_barno bar = ep_bar->barno;
	size_t size = ep_bar->size;
	int flags = ep_bar->flags;
	unsigned int func_offset = 0;
	unsigned int offset;
	int ret, type, value;
	u32 reg, dbi2_reg;

	func_offset = sunxi_pcie_ep_func_select(ep, fn);

	reg = PCI_BASE_ADDRESS_0 + (4 * bar) + func_offset;
	dbi2_reg = PCI_BASE_ADDRESS_0 + (4 * bar) + fn * DBI2_FUNC_OFFSET;

	if (!(flags & PCI_BASE_ADDRESS_SPACE)) {
		type = PCIE_ATU_TYPE_MEM;
	} else {
		type = PCIE_ATU_TYPE_IO;
		dev_info(pci->dev, "IO type is not currently supported.");
		return -1;
	}

	ret = sunxi_pcie_ep_inbound_atu(ep, fn, type, ep_bar->phys_addr, bar);
	if (ret)
		return ret;

	if (ep->epf_bar[bar])
		return 0;

	sunxi_pcie_dbi_ro_wr_en(pci);

	sunxi_pcie_writel_dbi(pci, PCIE_TYPE0_STATUS_COMMAND_REG, 0x6);

	offset = sunxi_pcie_ep_find_ext_capability(pci, PCI_EXT_CAP_ID_REBAR);
	offset += func_offset;

	value = fls64(size - 1);
	if (value <= SIZE_OF_1MB) {
		value = 0;
		dev_info(pci->dev, "The min size default is 1MB, set 1MB for func%d bar%d\n",
				fn, bar);
	} else {
		value -= SIZE_OF_1MB;
		dev_info(pci->dev, "Only supports power-of-2 MB alignment, so func%d bar%d is set to %dMB",
				fn, bar, 1 << value);
	}

	value = (value << PCI_REBAR_CTRL_BAR_SHIFT) & PCI_REBAR_CTRL_BAR_SIZE;

	if (flags & PCI_BASE_ADDRESS_MEM_TYPE_64) {
		sunxi_pcie_writel_dbi(pci, reg, PCI_BASE_ADDRESS_MEM_TYPE_64);
		if (bar == 0) {
			sunxi_pcie_writel_dbi(pci, offset + RESBAR_CAP_REG, RESBAR_SIZE_MASK);
			sunxi_pcie_writel_dbi(pci, offset + RESBAR_CTL_REG, value);
		} else if (bar == 4) {
			sunxi_pcie_writel_dbi(pci, offset + RESBAR_CAP_REG + (3 * RESBAR_NEXT_BAR), RESBAR_SIZE_MASK);
			sunxi_pcie_writel_dbi(pci, offset + RESBAR_CTL_REG + (3 * RESBAR_NEXT_BAR), value);
		} else {
			dev_info(pci->dev, "No free 64bit bar.");
		}
	} else {
		/* 32bit and mem space mode. */
		sunxi_pcie_writel_dbi(pci, reg, 0x0);
		if (bar == 2 || bar == 3) {
			bar -= 1;
			sunxi_pcie_writel_dbi(pci, offset + RESBAR_CAP_REG + (bar * RESBAR_NEXT_BAR), RESBAR_SIZE_MASK);
			sunxi_pcie_writel_dbi(pci, offset + RESBAR_CTL_REG + (bar * RESBAR_NEXT_BAR), value);
		}
	}

	sunxi_pcie_writel_dbi(pci, PCIE_DBI2_BASE + dbi2_reg, BAR_ENABLE);

	ep->epf_bar[bar] = ep_bar;
	sunxi_pcie_dbi_ro_wr_dis(pci);

	return 0;
}

static int sunxi_pcie_ep_set_msi(struct udevice *dev, uint fn, uint mmc)
{
	struct sunxi_pcie *pci = dev_get_plat(dev);
	struct sunxi_pcie_ep *ep = &pci->ep;
	u32 val, reg;
	unsigned int func_offset = 0;
	struct sunxi_pcie_ep_func *ep_func;
	u8 func_no = 0;

	ep_func = sunxi_pcie_ep_get_func_from_ep(ep, func_no);
	if (!ep_func || !ep_func->msi_cap)
		return -EINVAL;

	func_offset = sunxi_pcie_ep_func_select(ep, func_no);

	reg = ep_func->msi_cap + func_offset + PCI_MSI_FLAGS;
	val = sunxi_pcie_readw_dbi(pci, reg);
	val &= ~PCI_MSI_FLAGS_QMASK;
	val |= (mmc << 1) & PCI_MSI_FLAGS_QMASK;
	sunxi_pcie_dbi_ro_wr_en(pci);
	sunxi_pcie_writew_dbi(pci, reg, val);
	sunxi_pcie_dbi_ro_wr_dis(pci);

	return 0;
}

static int sunxi_pcie_ep_start(struct udevice *dev)
{
	struct sunxi_pcie *pci = dev_get_plat(dev);

	sunxi_pcie_plat_ltssm_enable(pci);

	return 0;
}

static int sunxi_pcie_ep_stop(struct udevice *dev)
{
	struct sunxi_pcie *pci = dev_get_plat(dev);

	sunxi_pcie_plat_ltssm_disable(pci);

	return 0;
}

static struct pci_ep_ops sunxi_pci_ep_ops = {
	.write_header	= sunxi_pcie_ep_write_header,
	.set_bar	= sunxi_pcie_ep_set_bar,
	.set_msi	= sunxi_pcie_ep_set_msi,
	.start		= sunxi_pcie_ep_start,
	.stop		= sunxi_pcie_ep_stop,
};

static int sunxi_pcie_plat_clk_get(struct udevice *dev)
{
    struct sunxi_pcie *pci = dev_get_plat(dev);
    int ret = 0;

	ret = clk_get_by_name(dev, "pclk_aux", &pci->pcie_aux);
	if (ret) {
		dev_err(pci->dev, "get clk pclk_aux failed %d.\n", ret);
		return -ENXIO;
	}

    if (pci->drvdata->has_pcie_slv_clk) {
		ret = clk_get_by_name(dev, "pclk_slv", &pci->pcie_slv);
		if (ret) {
			dev_err(pci->dev, "get clk pclk_slv failed %d.\n", ret);
			return -ENXIO;
		}
	}

	if (pci->drvdata->need_pcie_rst) {
		ret = reset_get_by_name(dev, "pclk_rst", &pci->pcie_rst);
		if (ret) {
			dev_err(pci->dev, "get reset pclk_rst failed %d.\n", ret);
			return -ENXIO;
		}

		ret = reset_get_by_name(dev, "pwrup_rst", &pci->pwrup_rst);
		if (ret) {
			dev_err(pci->dev, "get reset pwrup_rst failed %d.\n", ret);
			return -ENXIO;
		}
	}

	return 0;
}

static int sunxi_pcie_plat_clk_setup(struct sunxi_pcie *pci)
{
	int ret;

	if (pci->drvdata->need_pcie_rst) {
		ret = reset_deassert(&pci->pcie_rst);
		if (ret) {
			dev_err(pci->dev, "cannot reset pcie\n");
			return ret;
		}

		ret = reset_deassert(&pci->pwrup_rst);
		if (ret) {
			dev_err(pci->dev, "cannot pwrup_reset pcie\n");
			goto err0;
		}
	}

	ret = clk_enable(&pci->pcie_aux);
	if (ret) {
		dev_err(pci->dev, "cannot prepare/enable aux clock\n");
		goto err1;
	}

	if (pci->drvdata->has_pcie_slv_clk) {
		if (pci->drvdata->pcie_slv_clk_400m) {
			ret = clk_set_rate(&pci->pcie_slv, 400000000);
			if (ret) {
				dev_err(pci->dev, "cannot set slv clock\n");
				goto err2;
			}
		}
		ret = clk_enable(&pci->pcie_slv);
		if (ret) {
			dev_err(pci->dev, "cannot prepare/enable slv clock\n");
			goto err2;
		}
	}

	if (pci->drvdata->has_pcie_its_clk) {
		ret = reset_deassert(&pci->pcie_its_rst);
		if (ret) {
			dev_err(pci->dev, "cannot reset pcie its\n");
			goto err3;
		}

		ret = clk_enable(&pci->pcie_its);
		if (ret) {
			dev_err(pci->dev, "cannot prepare/enable its clock\n");
			goto err4;
		}
	}

	return 0;
err4:
	if (pci->drvdata->has_pcie_its_clk)
		reset_assert(&pci->pcie_its_rst);
err3:
	if (pci->drvdata->has_pcie_slv_clk)
		clk_disable(&pci->pcie_slv);
err2:
	clk_disable(&pci->pcie_aux);
err1:
	if (pci->drvdata->need_pcie_rst)
		reset_assert(&pci->pwrup_rst);
err0:
	if (pci->drvdata->need_pcie_rst)
		reset_assert(&pci->pcie_rst);

	return ret;
}

static void sunxi_pcie_plat_clk_exit(struct sunxi_pcie *pci)
{
	if (pci->drvdata->has_pcie_its_clk) {
		clk_disable(&pci->pcie_its);
		reset_assert(&pci->pcie_its_rst);
	}

	if (pci->drvdata->has_pcie_slv_clk)
		clk_disable(&pci->pcie_slv);

	clk_disable(&pci->pcie_aux);

	if (pci->drvdata->need_pcie_rst) {
		reset_assert(&pci->pcie_rst);
		reset_assert(&pci->pwrup_rst);
	}
}

static int sunxi_pcie_plat_power_on(struct sunxi_pcie *pci)
{
	int ret = 0;

	if (!IS_ERR(pci->pcie3v3)) {
		ret = regulator_set_value(pci->pcie3v3, 3300000);
		if (ret)
			dev_warn(pci->dev, "failed to set regulator voltage\n");

		ret = regulator_set_enable(pci->pcie3v3, true);
		if (ret)
			dev_err(pci->dev, "failed to enable pcie3v3 regulator\n");
	}

	if (!IS_ERR(pci->pcie1v8)) {
		ret = regulator_set_value(pci->pcie1v8, 1800000);
		if (ret)
			dev_warn(pci->dev, "failed to set regulator voltage\n");

		ret = regulator_set_enable(pci->pcie1v8, true);
		if (ret)
			dev_err(pci->dev, "failed to enable pcie1v8 regulator\n");
	}

	return ret;
}

static void sunxi_pcie_plat_power_off(struct sunxi_pcie *pci)
{
	if (!IS_ERR(pci->pcie3v3))
		regulator_set_enable(pci->pcie3v3, false);
	if (!IS_ERR(pci->pcie1v8))
		regulator_set_enable(pci->pcie1v8, false);
}

static int sunxi_pcie_plat_combo_phy_init(struct sunxi_pcie *pci)
{
	int ret;

	ret = generic_phy_init(&pci->phy);
	if (ret) {
		dev_err(pci->dev, "fail to init phy, err %d\n", ret);
		return ret;
	}

	// ret = generic_phy_power_on(&pci->phy);
	// if (ret) {
	// 	pr_err("power on %s PHY failed\n", dev->name);
	// 	goto err_exit_phy;
	// }
	return 0;
}

static void sunxi_pcie_plat_combo_phy_deinit(struct sunxi_pcie *pci)
{
	// generic_phy_power_off(&pci->phy);
	generic_phy_exit(&pci->phy);
}

static int sunxi_pcie_plat_hw_init(struct sunxi_pcie *pci)
{
	int ret;

	ret = sunxi_pcie_plat_power_on(pci);
	if (ret)
		return ret;

	ret = sunxi_pcie_plat_clk_setup(pci);
	if (ret)
		goto err0;

	ret = sunxi_pcie_plat_combo_phy_init(pci);
	if (ret)
		goto err1;

	return 0;

err1:
	sunxi_pcie_plat_clk_exit(pci);
err0:
	sunxi_pcie_plat_power_off(pci);

	return ret;
}

static void sunxi_pcie_plat_hw_deinit(struct sunxi_pcie *pci)
{
	sunxi_pcie_plat_combo_phy_deinit(pci);
	sunxi_pcie_plat_power_off(pci);
	sunxi_pcie_plat_clk_exit(pci);
}

static int sunxi_pcie_ep_of_to_plat(struct udevice *dev)
{
    struct sunxi_pcie *pci = dev_get_plat(dev);
    struct sunxi_pcie_of_data *data = (struct sunxi_pcie_of_data *)dev_get_driver_data(dev);
    int ret;

    pci->drvdata = data;

    pci->dbi_base = dev_read_addr_ptr(dev);
    if (!pci->dbi_base)
		return -ENOMEM;
    pci->app_base = pci->dbi_base + PCIE_USER_DEFINED_REGISTER;

    pci->link_gen = fdtdec_get_int(gd->fdt_blob, dev_of_offset(dev),
					      "max-link-speed", 2);

    pci->lanes = fdtdec_get_int(gd->fdt_blob, dev_of_offset(dev),
					      "num-lanes", 1);

    if (!dm_gpio_is_valid(&pci->rst_gpio)) {
		ret = gpio_request_by_name(dev, "reset-gpio", 0,
		&pci->rst_gpio, GPIOD_IS_OUT);
		if (ret)
			dev_err(pci->dev, "pci get reset-gpio failed %d \n", ret);
	}

    if (!dm_gpio_is_valid(&pci->wake_gpio)) {
		ret = gpio_request_by_name(dev, "wake-gpio", 0,
		&pci->wake_gpio, GPIOD_IS_OUT);
		if (ret)
			dev_err(pci->dev, "pci get wake-gpio failed %d \n", ret);
	}

    ret = device_get_supply_regulator(dev, "pcie3v3-supply",
					  &pci->pcie3v3);
	if (ret && ret != -ENOENT) {
		dev_err(pci->dev, "failed to get pcie3v3 supply (ret=%d)\n", ret);
		return ret;
	}

	ret = device_get_supply_regulator(dev, "pcie1v8-supply",
					  &pci->pcie1v8);
	if (ret && ret != -ENOENT) {
		dev_err(pci->dev, "failed to get pcie1v8 supply (ret=%d)\n", ret);
		return ret;
	}

    ret = sunxi_pcie_plat_clk_get(dev);
	if (ret) {
		dev_err(pci->dev, "pcie get clk init failed\n");
		return -ret;
	}

	ret = generic_phy_get_by_name(dev, "pcie-phy", &pci->phy);
	if (ret) {
		dev_err(pci->dev, "pcie Unable to get phy \r\n");
		return ret;
	}

	return 0;
}

static int sunxi_pcie_parse_ep_dts(struct udevice *dev)
{
	int ret;
	void *addr;
	struct fdt_resource addr_res;
	struct sunxi_pcie *pci = dev_get_plat(dev);
	struct sunxi_pcie_ep *ep = &pci->ep;

	ret = fdt_get_named_resource(gd->fdt_blob, dev_of_offset(dev),
				     "reg", "reg-names",
				     "addr_space", &addr_res);
	if (ret) {
		dev_err(pci->dev, "%s: resource \"addr_space\" not found\n", dev->name);
		return ret;
	}

	ep->phys_base = addr_res.start;
	ep->addr_size = addr_res.end - addr_res.start + 1;
	ep->page_size = SZ_4K;

	ep->num_ib_windows = fdtdec_get_int(gd->fdt_blob, dev_of_offset(dev),
					      "num-ib-windows", 8);

	if (ep->num_ib_windows < 0) {
		dev_err(dev, "unable to read *num-ib-windows* property\n");
		return ret;
	}

	ep->num_ob_windows = fdtdec_get_int(gd->fdt_blob, dev_of_offset(dev),
					      "num-ob-windows", 8);

	if (ep->num_ob_windows < 0) {
		dev_err(dev, "unable to read *num-ib-windows* property\n");
		return ret;
	}

	ep->ib_window_map = devm_kcalloc(dev, ep->num_ib_windows, sizeof(unsigned long),
							GFP_KERNEL);
	if (!ep->ib_window_map)
		return -ENOMEM;

	ep->ob_window_map = devm_kcalloc(dev, ep->num_ob_windows, sizeof(unsigned long),
							GFP_KERNEL);
	if (!ep->ob_window_map)
		return -ENOMEM;

	addr = devm_kcalloc(dev, ep->num_ob_windows, sizeof(phys_addr_t),
							GFP_KERNEL);
	if (!addr)
		return -ENOMEM;
	ep->outbound_addr = addr;

	return 0;
}

int sunxi_plat_ep_init_end(struct sunxi_pcie_ep *ep)
{
	struct sunxi_pcie *pci = to_sunxi_pcie_from_ep(ep);
	u8 hdr_type;

	hdr_type = sunxi_pcie_readb_dbi(pci, PCI_HEADER_TYPE) &
							PCI_HEADER_TYPE_MASK;
	if (hdr_type != PCI_HEADER_TYPE_NORMAL) {
		dev_err(pci->dev,
			"PCIe controller is not set to EP mode (hdr_type:0x%x)!\n",
			hdr_type);
		return -EIO;
	}

	sunxi_pcie_setup_ep(pci);
	sunxi_pcie_dbi_ro_wr_dis(pci);

	return 0;
}

int sunxi_pcie_ep_init(struct udevice *dev)
{
	int ret;
	u8 func_no;

	struct sunxi_pcie *pci = dev_get_plat(dev);
	struct sunxi_pcie_ep *ep = &pci->ep;
	struct sunxi_pcie_ep_func *ep_func;

	INIT_LIST_HEAD(&ep->func_list);

	ret = sunxi_pcie_parse_ep_dts(dev);
	if (ret) {
		dev_err(dev, "failed to parse ep dts\n");
		return ret;
	}

	ep->max_functions = fdtdec_get_int(gd->fdt_blob, dev_of_offset(dev),
					      "max-functions", 1);

	for (func_no = 0; func_no < ep->max_functions; func_no++) {
		ep_func = devm_kzalloc(dev, sizeof(*ep_func), GFP_KERNEL);
		if (!ep_func)
			return -ENOMEM;

		ep_func->func_no = func_no;
		ep_func->msi_cap = sunxi_pcie_ep_find_capability(ep, func_no,
							      PCI_CAP_ID_MSI);
		ep_func->msix_cap = sunxi_pcie_ep_find_capability(ep, func_no,
							       PCI_CAP_ID_MSIX);

		list_add_tail(&ep_func->list, &ep->func_list);
	}

	sunxi_ep_init_bar(ep);

	ret = sunxi_plat_ep_init_end(ep);

	return ret;
}
EXPORT_SYMBOL_GPL(sunxi_pcie_ep_init);

static unsigned int sunxi_pcie_ep_func_conf_select(struct sunxi_pcie_ep *ep,
						u8 func_no)
{
	struct sunxi_pcie *pcie = to_sunxi_pcie_from_ep(ep);

	return pcie->drvdata->func_offset * func_no;
}

static const struct sunxi_pcie_ep_ops sunxi_ep_ops = {
	.func_conf_select = sunxi_pcie_ep_func_conf_select,
};

static const struct sunxi_pcie_of_data sunxi_pcie_ep_v210_of_data = {
	.mode = SUNXI_PCIE_EP_TYPE,
	.func_offset = 0x10000,
	.ops = &sunxi_ep_ops,
	.has_pcie_slv_clk = true,
	.need_pcie_rst = true,
	.has_pcie_ecc = true,
};

const struct udevice_id sunxi_pci_ep_of_match[] = {
	{
		.compatible = "allwinner,sunxi-pcie-v210-ep",
		.data = (ulong)&sunxi_pcie_ep_v210_of_data,
	},
	{ },
};


static int sunxi_pci_ep_probe(struct udevice *dev)
{
    struct sunxi_pcie *pci = dev_get_plat(dev);
    int ret;

    ret = sunxi_pcie_plat_hw_init(pci);
	if (ret)
		return ret;

	switch (pci->drvdata->mode) {
	case SUNXI_PCIE_RC_TYPE:
		// ret = sunxi_pcie_host_add_port(pci, pdev);
		break;
	case SUNXI_PCIE_EP_TYPE:
		sunxi_pcie_plat_set_mode(pci);
		pci->ep.ops = &sunxi_ep_ops;
		ret = sunxi_pcie_ep_init(dev);
		break;
	default:
		dev_err(pci->dev, "INVALID device type %d\n", pci->drvdata->mode);
		ret = -EINVAL;
		break;
	}
	if (ret)
		goto err;

	dev_info(pci->dev, "driver version: %s\n", SUNXI_PCIE_MODULE_VERSION);

	return 0;

err:
	sunxi_pcie_plat_hw_deinit(pci);

	return ret;
}

static int sunxi_pci_ep_remove(struct udevice *dev)
{
	return 0;
}

U_BOOT_DRIVER(sunxi_pcie) = {
	.name	= "allwinner,pcie-ep",
	.id	= UCLASS_PCI_EP,
	.of_match = sunxi_pci_ep_of_match,
	.of_to_plat	= sunxi_pcie_ep_of_to_plat,
	.ops = &sunxi_pci_ep_ops,
	.probe = sunxi_pci_ep_probe,
	.remove = sunxi_pci_ep_remove,
	.priv_auto	= sizeof(struct sunxi_pcie_ep),
    .plat_auto	= sizeof(struct sunxi_pcie),
};
