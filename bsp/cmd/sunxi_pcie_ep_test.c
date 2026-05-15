/*
 * (C) Copyright 2019-2025
 * allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Author: chenhuaqiang <chenhuaqiang@allwinnertech.com>
 *
 * some simple description for this code
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */


#include <common.h>
#include <command.h>
#include <asm/global_data.h>
#include <dm.h>
#include <pci_ep.h>

DECLARE_GLOBAL_DATA_PTR;
static struct pci_ep_header ep_header = {
	.vendorid = 0x1F6D,
	.deviceid = 0x7788,
	.baseclass_code	= PCI_CLASS_OTHERS,
	.interrupt_pin	= PCI_INTERRUPT_INTA,
};

static struct pci_bar bar0 = {
	.phys_addr = 0x22100000,
	.size = 0x100000,
	.barno = BAR_0,
	.flags = PCI_BASE_ADDRESS_MEM_TYPE_64
};

static struct pci_bar bar3 = {
	.phys_addr = 0x22200000,
	.size = 0x100000,
	.barno = BAR_3,
	.flags = PCI_BASE_ADDRESS_MEM_TYPE_32
};

static struct pci_bar bar4 = {
	.phys_addr = 0x22300000,
	.size = 0x100000,
	.barno = BAR_4,
	.flags = PCI_BASE_ADDRESS_MEM_TYPE_64
};

/* process pcie ep command */
static int do_sunxi_pcie_ep(struct cmd_tbl *cmdtp, int flag, int argc,
		    char *const argv[])
{
	struct udevice *dev;

	printf("Initialise pcie ep func driver \n");

	uclass_get_device(UCLASS_PCI_EP, 0, &dev);

	pci_ep_write_header(dev, 0, &ep_header);

	pci_ep_set_bar(dev, 0, &bar0);

	pci_ep_set_bar(dev, 0, &bar3);

	pci_ep_set_bar(dev, 0, &bar4);

	pci_ep_start(dev);

	return 0;
}

U_BOOT_CMD(pcie_ep, CONFIG_SYS_MAXARGS, 1, do_sunxi_pcie_ep,
		"read data from private data",
		"pcie_ep [init]"
		"NULL");
