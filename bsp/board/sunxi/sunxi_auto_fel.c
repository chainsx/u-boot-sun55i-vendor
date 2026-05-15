/*
 * (C) Copyright 2018-2020
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * wangwei <wangwei@allwinnertech.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */
#include <common.h>
//#include <sys_config.h>
#include <private_uboot.h>
#include <sunxi_board.h>
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

extern int do_fel_from_boot(struct cmd_tbl *cmdtp, int flag, int argc,
			    char *const argv[]);
extern int script_parser_fetch(char *node_path, char *prop_name, int value[],
			       int def_val);

int sunxi_auto_fel_by_usb(void)
{
	int auto_fel = 0;

	if (WORK_MODE_BOOT != get_boot_work_mode())
		return 0;

	script_parser_fetch("/soc/target", "auto_fel", &auto_fel, 1);

	if (auto_fel != 1) {
		debug("auto fel not enabled\n");
		return 0;
	}

	return do_fel_from_boot(NULL, 0, 0, NULL);
}
