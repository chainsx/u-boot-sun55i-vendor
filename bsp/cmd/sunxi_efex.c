/*
 * SPDX-License-Identifier: GPL-2.0+
 * Sunxi RTC data area ops
 *
 * (C) Copyright 2018-2020
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * wangwei <wangwei@allwinnertech.com>
 */

#include <common.h>
#include <sunxi_board.h>
#include <command.h>
#include <asm/arch/rtc.h>

int do_efex (struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	pr_notice("## jump to efex ...\n");

	sunxi_board_run_fel();

	return 0;
}



U_BOOT_CMD(
	efex,	1,	0,	do_efex,
	"run to efex",
	"no parameters\n"
);
