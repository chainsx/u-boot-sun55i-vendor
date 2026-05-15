/*
 * (C) Copyright 2018-2020
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * wangwei <wangwei@allwinnertech.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <command.h>
//#include <sys_config.h>
#include <privatestorage.h>
#include <private_uboot.h>
#include <sunxi_board.h>
#include <asm/global_data.h>
#include "trace.h"

DECLARE_GLOBAL_DATA_PTR;

extern int do_burn_from_boot(struct cmd_tbl *cmdtp, int flag, int argc,
			    char *const argv[]);
extern int script_parser_fetch(char *node_path, char *prop_name, int value[],
			       int def_val);

//#define  SUNXI_SECURESTORAGE_TEST_ERASE

int sunxi_keydata_burn_by_usb(void)
{
	char buffer[512];
	int data_len;
	int ret = 0;

	int workmode	 = uboot_spare_head.boot_data.work_mode;
	int if_need_burn_key = 0;
	char *env_burn_key;

	env_burn_key = env_get("burn_key");
	if (env_burn_key) {
		if_need_burn_key = simple_strtoul(env_burn_key, NULL, 16);
	} else {
		ret = script_parser_fetch("/soc/target", "burn_key", &if_need_burn_key, 1);
	}

	if (if_need_burn_key != 1) {
		pr_err("out of usb burn from boot: not need burn key\n");
		return 0;
	}

	if (workmode != WORK_MODE_BOOT) {
		pr_err("out of usb burn from boot: not boot mode\n");
		return 0;
	}
	memset(buffer, 0, 512);
#ifdef CONFIG_SUNXI_PRIVATE_STORAGE
	if (sunxi_private_storage_init()) {
		pr_err("sunxi secure storage is not supported\n");
	} else {
#ifndef SUNXI_SECURESTORAGE_TEST_ERASE
		ret = sunxi_secure_object_read("key_burned_flag", buffer, 512,
					       &data_len);
		if (ret) {
			pr_debug("sunxi secure storage has no flag\n");
		} else {
			if (!strcmp(buffer, "key_burned")) {
				pr_debug("find key burned flag\n");
				return 0;
			}
			pr_debug("do not find key burned flag\n");
		}
#else
		if (!sunxi_private_storage_erase("key_burned_flag"))
			sunxi_private_storage_exit();

		return 0;
#endif
	}
#endif
#ifdef CONFIG_SUNXI_TRACE
		/* trace polling loop is pointless, just don't*/
		trace_set_enabled(0);
#endif
	ret = do_burn_from_boot(NULL, 0, 0, NULL);
#ifdef CONFIG_SUNXI_TRACE
		trace_set_enabled(1);
#endif
	return ret;
}
