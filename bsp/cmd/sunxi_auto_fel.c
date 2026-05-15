/*
 * (C) Copyright 2018 allwinnertech  <wangwei@allwinnertech.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 * wangwei@allwinnertech.com
 */

#include <common.h>
#include <console.h>
#include <command.h>
#include <sunxi_board.h>
#include <asm/global_data.h>

extern int sunxi_usb_dev_register(uint dev_name);
extern void sunxi_usb_main_loop(int delaytime);
extern int sunxi_usb_extern_loop(void);
extern int sunxi_usb_init(int delaytime);
extern int sunxi_usb_exit(void);

DECLARE_GLOBAL_DATA_PTR;

extern volatile int sunxi_auto_fel_from_boot_handshake,
	sunxi_usb_burn_from_boot_init, sunxi_usb_burn_from_boot_setup;
extern volatile int sunxi_usb_probe_update_action;
extern volatile int sunxi_auto_fel_runnning;

int do_fel_from_boot(struct cmd_tbl *cmdtp, int flag, int argc,
		     char *const argv[])
{
	int ret;
	ulong begin_time = 0, over_time = 0;

	pr_info("probe MP tools from boot\n");
	if (sunxi_usb_dev_register(4) < 0) {
		pr_err("usb burn fail: not support burn private data\n");
		return -1;
	}

	if (sunxi_usb_init(0)) {
		pr_err("%s usb init fail\n", __func__);
		sunxi_usb_exit();
		return 0;
	}

	pr_info("usb init ok\n");
	begin_time = get_timer(0);
	over_time = 500; /* ms */
	while (1) {
		if (sunxi_usb_burn_from_boot_init) {
			pr_info("usb sof ok\n");
			break;
		}
		if (get_timer(begin_time) > over_time) {
			pr_info("usb overtime\n");
			sunxi_usb_exit();
			return 0;
		}
	}
	pr_info("usb probe ok\n");

	begin_time = get_timer(0);
	over_time = 2000; /* ms */
	sunxi_auto_fel_runnning = 1;
	while (1) {
		ret = sunxi_usb_extern_loop();
		if (ret)
			break;

		if (!sunxi_auto_fel_from_boot_handshake) {
			if (get_timer(begin_time) > over_time) {
				sunxi_usb_exit();
				pr_err("usb: have no handshake\n");
				sunxi_auto_fel_runnning = 0;
				return 0;
			}
		}

		if (sunxi_usb_probe_update_action == 1) {
			pr_info("usb: enter fel mode\n");
			sunxi_board_run_fel();
		} else if (sunxi_usb_probe_update_action == 2) {
			pr_info("ready to boot normal \n");
			break;
		}
	}
	sunxi_auto_fel_runnning = 0;
	pr_info("exit usb fel from boot\n");
	sunxi_usb_exit();

	return 0;
}

U_BOOT_CMD(sunxi_fel, CONFIG_SYS_MAXARGS, 1, do_fel_from_boot,
	   "do fel from boot",
	   "sunxi_fel"
	   "NULL");
