/*
 * (C) Copyright 2018 allwinnertech  <wangwei@allwinnertech.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <console.h>
#include <command.h>
#include <spare_head.h>
#include <asm/global_data.h>
#include <privatestorage.h>
#include <sunxi_flash.h>
#include <sunxi_board.h>
//#include <sunxi_board.h>
//#include <linux/printk.h>

extern int sunxi_usb_dev_register(uint dev_name);
extern void sunxi_usb_main_loop(int delaytime);
extern int sunxi_usb_extern_loop(void);
extern int sunxi_usb_init(int delaytime);
extern int sunxi_usb_exit(void);

DECLARE_GLOBAL_DATA_PTR;

extern volatile int sunxi_usb_burn_from_boot_handshake,
	sunxi_usb_burn_from_boot_init, sunxi_usb_burn_from_boot_setup;

__weak void sunxi_update_subsequent_processing(int next_work)
{
	pr_err("__weak %s...%d\n", __func__, __LINE__);
}

int do_burn_from_boot(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	int ret;
	ulong begin_time = 0, over_time = 0;

	pr_debug("usb burn from boot\n");
	if (sunxi_usb_dev_register(4) < 0) {
		pr_err("usb burn fail: not support burn private data\n");
		return -1;
	}

	if (sunxi_usb_init(0)) {
		pr_err("%s usb init fail\n", __func__);
		sunxi_usb_exit();
		return 0;
	}

	pr_info("usb prepare ok\n");
	begin_time = get_timer(0);
	over_time = 300;
	while (1) {
		if (sunxi_usb_burn_from_boot_init) {
			pr_debug("usb sof ok\n");
			break;
		}
		if (get_timer(begin_time) > over_time) {
			pr_info("overtime\n");
			sunxi_usb_exit();
			pr_info("%s usb : no usb exist\n", __func__);

			return 0;
		}
	}
	pr_info("usb probe ok\n");
	pr_info("usb setup ok\n");

	begin_time = get_timer(0);
	over_time = 400;
	while (1) {
		ret = sunxi_usb_extern_loop();
		if (ret) {
			break;
		}
		if (!sunxi_usb_burn_from_boot_handshake) {
			if (get_timer(begin_time) > over_time) {
				sunxi_usb_exit();
				pr_info("%s usb : have no handshake\n",
					    __func__);
				return 0;
			}
		}
		if (ctrlc()) {
			ret = SUNXI_UPDATE_NEXT_ACTION_NORMAL;
			break;
		}
	}
	pr_info("exit usb burn from boot\n");
	sunxi_usb_exit();
	sunxi_update_subsequent_processing(ret);

	return 0;
}

U_BOOT_CMD(uburn, CONFIG_SYS_MAXARGS, 1, do_burn_from_boot,
	   "do a burn from boot",
	   "pburn [mode]"
	   "NULL");

/*
************************************************************************************************************
*
*                                             function
*
*    name          :
*
*    parmeters     :
*
*    return        :
*
*    note          :
*
*
************************************************************************************************************
*/
int do_read_from_boot(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
#ifdef CONFIG_SUNXI_PRIVATE_STORAGE
	if (argc == 1) {
		return sunxi_private_storage_list();
	}
	if (argc == 2) {
		char buffer[4096];
		int ret, data_len;

		memset(buffer, 0, 4096);
		ret = sunxi_private_storage_init();
		if (ret < 0) {
			pr_err("%s secure storage init err\n", __func__);

			return -1;
		}
		ret = sunxi_secure_object_read(argv[1], buffer, 4096,
					       &data_len);
		if (ret < 0) {
			pr_err("private data %s is not exist\n", argv[1]);

			return -1;
		}
		pr_debug("private data:\n");
		sunxi_dump(buffer, strlen((const char *)buffer));

		return 0;
	}
#endif
	return -1;
}

U_BOOT_CMD(pbread, CONFIG_SYS_MAXARGS, 1, do_read_from_boot,
	   "read data from private data",
	   "pread [name]"
	   "NULL");

