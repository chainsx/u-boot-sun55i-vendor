/* SPDX-License-Identifier: GPL-2.0+
 * Copyright (C) 2023 Allwinnertech
*/

#include <common.h>
#include <sunxi_board.h>
#include <asm/io.h>
#include <asm/arch/efuse.h>
#include <display_options.h>
#include <private_uboot.h>
#include <env.h>
#include <linux/string.h>
#include <asm/arch/rtc.h>
#include <spare_head.h>
#include <malloc.h>

extern uint sunxi_partition_get_offset_byname(const char *part_name);

void sunxi_dump(void *addr, unsigned int size)
{
	print_buffer((ulong)addr, addr, 1, ALIGN(size, 16)/1, 16/1);
}
#ifndef CONFIG_SUNXI_RTOS
int sunxi_force_rotpk(void)
{
	return ((uboot_spare_head.boot_data.func_mask &
		 UBOOT_FUNC_MASK_BIT_FORCE_ROTPK) ==
		UBOOT_FUNC_MASK_BIT_FORCE_ROTPK);
}

int sunxi_update_rotpk_info(void)
{
	char rotpk_status[16] = "";
	int ret;
#ifndef CONFIG_DM
	ret = sunxi_efuse_get_rotpk_status();
#else
	ret = sunxi_efuse_get_rotpk_status_dm();
#endif
	if (ret >= 0) {
		sprintf(rotpk_status, "%d", ret);
		env_set("rotpk_status", rotpk_status);
	}

	if (sunxi_force_rotpk()) {
		if (ret < 1) {
			pr_err("rotpk required but not burned\n");
			return -1;
		}
	}
	return 0;
}
#endif

static int sunxi_str_replace(char *dest_buf, char *goal, char *replace)
{
	char tmp[128];
	char tmp_str[16];
	int  goal_len, rep_len, dest_len;
	int  i, j, k;

	if ((goal == NULL) || (dest_buf == NULL))
		return -1;

	memset(tmp, 0, 128);
	strcpy(tmp, dest_buf);

	goal_len = strlen(goal);
	dest_len = strlen(dest_buf);

	if (replace != NULL)
		rep_len = strlen(replace);
	else
		rep_len = 0;

	j = 0;
	for (i = 0; tmp[i]; ) {
		k = 0;
		while (((tmp[i] != ' ') && (tmp[i] != 0)) || (tmp[i + 1] == ' ')) {
			tmp_str[k++] = tmp[i];
			i++;
			if (i >= dest_len)
				break;
		}
		i++;
		tmp_str[k] = 0;
		if (!strcmp(tmp_str, goal)) {
			if (rep_len) {
				strcpy(dest_buf + j, replace);
				if (tmp[j + goal_len]) {
					memcpy(dest_buf + j + rep_len, tmp + j + goal_len, dest_len - j - goal_len);
					dest_buf[dest_len - goal_len + rep_len] = 0;
				}
			} else {
				if (tmp[j + goal_len]) {
					memcpy(dest_buf + j, tmp + j + goal_len, dest_len - j - goal_len);
					dest_buf[dest_len - goal_len + rep_len] = 0;
				}
			}

			return 0;
		}
		j = i;
	}

	return 0;
}

#ifndef CONFIG_BOOTCMD_SKIP_RTC
static int sunxi_get_bootcmd_from_rtc(void)
{
	u8 bootmode_flag = 0;

	bootmode_flag = rtc_get_bootmode_flag();
	debug("bootmode_flag = 0x%x", bootmode_flag);
	/*clear rtc*/
	rtc_set_bootmode_flag(0);
	switch (bootmode_flag) {
	case SUNXI_EFEX_CMD_FLAG:
		return SUNXI_EFEX_CMD_FLAG;
	case SUNXI_BOOT_RECOVERY_FLAG:
		return SUNXI_BOOT_RECOVERY_FLAG;
	case SUNXI_FASTBOOT_FLAG:
		return SUNXI_FASTBOOT_FLAG;
	case SUNXI_UBOOT_FLAG:
		return SUNXI_UBOOT_FLAG;
	default:
		return 0;
		break;
	}
}
#endif

int sunxi_set_bootcmd(char *bootcmd)
{
	int i, bootmode[4] = {0};
	env_set_hex("force_normal_boot", 1);
#ifndef CONFIG_BOOTCMD_SKIP_RTC
	bootmode[2] = sunxi_get_bootcmd_from_rtc();
#endif
	for (i = 1; i < sizeof(bootmode)/sizeof(bootmode[0]); i++) {
		if (bootmode[i]) {
			bootmode[0] = bootmode[i];
			pr_notice("bootmode[%d]:0x%x\n", i, bootmode[i]);
			break;
		}
	}
	switch (bootmode[0]) {
	case SUNXI_EFEX_CMD_FLAG:
		sunxi_board_run_fel();
		break;
	case SUNXI_SYS_RECOVERY_FLAG:
		set_boot_work_mode(WORK_MODE_SPRITE_RECOVERY);
		env_set("sysrecovery", "sprite_test");
		strncpy(bootcmd, "run sysrecovery",
			sizeof("run sysrecovery"));
		break;
	case SUNXI_BOOT_RECOVERY_FLAG:
		if (sunxi_partition_get_offset_byname("recovery")) {
			sunxi_str_replace(bootcmd, "boot_normal", "boot_recovery");
		}
		env_set_hex("force_normal_boot", 0);
		break;
	case SUNXI_FASTBOOT_FLAG:
		sunxi_str_replace(bootcmd, "boot_normal", "boot_fastboot");
		break;
	case SUNXI_UBOOT_FLAG:
		sunxi_set_uboot_shell(1);
		break;
	default:
		break;
	}

	return 0;
}

int sunxi_update_bootcmd(void)
{
	char  boot_commond[128];
	memset(boot_commond, 0x0, 128);
	strncpy(boot_commond, env_get("bootcmd"), sizeof(boot_commond)-1);
	pr_info("base bootcmd=%s\n", boot_commond);

	sunxi_set_bootcmd(boot_commond);

	env_set("bootcmd", boot_commond);
	pr_info("to be run cmd=%s\n", boot_commond);
	pr_notice("update bootcmd\n");
	return 0;
}

#ifdef CONFIG_SUNXI_BOOT_ENV_INLINE
char *env_table[9] = {"preboot", "bootcmd", "boot_normal", "boot_recovery", "boot_fastboot",
		"boot_riscv",
		"altbootcmd", "sunxi_pre_cmd", "sunxicmd"};
int sunxi_boot_env_inline_init(void)
{
	int i, len;
	char env_str[32] = {'\0'};
	char *value = NULL, *str, *new_line_ptr;
	char *target_env = (char *)CONFIG_SUNXI_BOOT_ENV_STRING;
	pr_debug("target env:%s\n", target_env);


	value = (char *)malloc(1024);
	if (value == NULL) {
		pr_err("malloc env value buf failed\n");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(env_table); i++) {
		memset(env_str, '\0', 32);
		snprintf(env_str, strlen(env_table[i]) + 2, "%s=", env_table[i]); //add "=" to env_table[i]
		pr_debug("env_table:%s, len:%d, env_str:%s\n", env_table[i], strlen(env_table[i]), env_str);

		str = strstr(target_env, env_str);
		if (str == NULL) {
			pr_err("not found inline boot env:%s=\n", env_table[i]);
			goto err_exit;
		}

		str = str + strlen(env_str);
		//CONFIG_SUNXI_BOOT_ENV_STRING should have '\n' to separate string
		new_line_ptr = strstr(str, "\n");
		len = new_line_ptr - str;

		memset(value, '\0', 1024);
		memcpy(value, str, len);
		pr_debug("set env:%s to:%s\n", env_table[i], value);

		env_set(env_table[i], value);
	}

	free(value);
	return 0;
err_exit:
	free(value);
	return -1;
}

int sunxi_check_setargs(void)
{
	int j, len, set = 0;
	char *setargs_table[4] = {"setargs_nor", "setargs_nand", "setargs_nand_ubi", "setargs_mmc"};
	char *value = NULL;
	char *tmp_ptr = NULL;
	len = strlen("setenv");//setenv bootargs
	for (j = 0; j < ARRAY_SIZE(setargs_table); j++) {
		set = 0;
		value = env_get(setargs_table[j]);
		if (value == NULL) {
			continue;
		} else {
			set = 1;
		}
		//that means env is in setargs_table, only use setenv
		if (set == 1) {
			if (memcmp(value, "setenv", len)) {
				pr_err("error:%s is not setenv bootargs\n", setargs_table[j]);
				return 1;
			}

			tmp_ptr = value + len;//skip setenv
			while ((*tmp_ptr != '\0') && (*tmp_ptr == ' ')) {
				//skip blank space ' '
				tmp_ptr++;
			}
			//first string should be "setenv", second string should be "bootargs"
			if (memcmp(tmp_ptr, "bootargs", strlen("bootargs"))) {
				pr_err("error:%s second string must be bootargs\n", setargs_table[j]);
				return 1;
			}

			if (strstr(value, ";")) {
				pr_err("error:don't set ; in %s\n", setargs_table[j]);
				return 1;
			}
		}
	}

	return 0;
}
#endif
