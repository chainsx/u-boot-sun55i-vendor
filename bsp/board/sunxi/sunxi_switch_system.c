// SPDX-License-Identifier: GPL-2.0+

#include <common.h>
#include <env.h>
#include <bootcount.h>
#include <command.h>

static int __check_systemAB(void)
{
	char *systemA = env_get("systemA");
	char *systemB = env_get("systemB");
	char *rootfsA = env_get("rootfsA");
	char *rootfsB = env_get("rootfsB");

	if (!systemA || !rootfsA) {
		pr_err("CHECK SYSTEM ERROR : env can't find systemA or rootfsA!!\n");
		return -1;
	}
	if (!systemB || !rootfsB) {
		pr_err("CHECK SYSTEM ERROR : env can't find systemB or rootfsB!!\n");
		return -1;
	}

	return 0;
}

//note:systemAB_now can only be modified by this function
static int __switch_systemAB(char *system)
{
	char tmp_system[5] = {0};
	char *systemA = NULL;
	char *systemB = NULL;
	char *rootfsA = NULL;
	char *rootfsB = NULL;
#ifdef CONFIG_SUNXI_ANDROID_OVERLAY
	char *dtboA = NULL;
	char *dtboB = NULL;
#endif
	char *systemAB_damage = env_get("systemAB_damage");
	char *systemAB_now = env_get("systemAB_now");
	memcpy(tmp_system, system, sizeof(tmp_system));

	if (!strcmp(tmp_system, "A")) {
		if (!strcmp(systemAB_damage, "A"))
			pr_warn("SWITCH WARNING : systemA is damaged\n");

		if ((!strcmp(systemAB_now, "B")) || (!systemAB_now)) {
			systemA = env_get("systemA");
			rootfsA = env_get("rootfsA");
			env_set("boot_partition", systemA);
			env_set("root_partition", rootfsA);
#ifdef CONFIG_SUNXI_ANDROID_OVERLAY
			dtboA = env_get("dtboA");
			env_set("dtbo_part_name", dtboA);
#endif
			env_set("systemAB_now", "A");
			env_save();
			pr_notice("boot system %s\n", tmp_system);
			return 0;
		}
	} else if (!strcmp(tmp_system, "B")) {
		if (!strcmp(systemAB_damage, "B"))
			pr_warn("SWITCH WARNING : systemB is damaged\n");

		if ((!strcmp(systemAB_now, "A")) || (!systemAB_now)) {
			systemB = env_get("systemB");
			rootfsB = env_get("rootfsB");
			env_set("boot_partition", systemB);
			env_set("root_partition", rootfsB);
#ifdef CONFIG_SUNXI_ANDROID_OVERLAY
			dtboB = env_get("dtboB");
			env_set("dtbo_part_name", dtboB);
#endif
			env_set("systemAB_now", "B");
			env_save();
			pr_notice("boot system %s\n", tmp_system);
			return 0;
		}
	} else {
		pr_err("SWITCH ERROR : input system is %s\n", tmp_system);
		pr_err("               Please input A or B system\n");
		return -1;
	}
	return 0;
}

//The system will switch according to systemab_next
int sunxi_auto_switch_system(void)
{
	char *systemAB_next = env_get("systemAB_next");

	if (!systemAB_next) {
		pr_warn("AUTO SWITCH WARNING : Can't get systemAB_next\n");
		pr_warn("                      Started system by default\n");
		return 0;
	}
	if (strcmp(systemAB_next, "A") && strcmp(systemAB_next, "B")) {
		pr_warn("AUTO SWITCH WARNING : systemAB_next(%s) is neither A nor B\n", systemAB_next);
		pr_warn("                      Started system by default\n");
		return 0;
	}

	if (__check_systemAB()) {
		pr_err("AUTO SWITCH ERROR : Check systemAB fail\n");
		return -1;
	}

	if (!strcmp(systemAB_next, "A")) {
		if (__switch_systemAB("A")) {
				pr_err("AUTO SWITCH ERROR : Switch to systemA fail\n");
				return -1;
			}
		}
	else {
		if (__switch_systemAB("B")) {
				pr_err("AUTO SWITCH ERROR : Switch to systemB fail\n");
				return -1;
			}
		}

	return 0;
}

//note:systemAB_damage can only be modified by this function
static int sunxi_damage_switch_system(void)
{
	pr_notice("==========DAMAGE SWITCH NOW==========\n");
	char *systemAB_next = env_get("systemAB_next");

	if (!systemAB_next) {
		pr_err("DAMAGE SWITCH ERROR : Can't get systemAB_next\n");
		pr_err("                      Please check the systemAB_next\n");
		return -1;
	}

	if (__check_systemAB()) {
		pr_err("DAMAGE SWITCH ERROR : Check systemAB fail\n");
		return -1;
	}

	if (!strcmp(systemAB_next, "A")) {
		pr_err("DAMAGE SWITCH : systemA is damaged;now switch to systemB\n");
		if (!__switch_systemAB("B")) {
			env_set("systemAB_next", "B");
			env_set("systemAB_damage", "A");
			bootcount_store(0);
#ifndef CONFIG_BOOTCOUNT_ENV
			/*
			 * If CONFIG_BOOTCOUNT_ENV is defined,
			 * there is no need to call env_save function to save the data to env,
			 * because the bootcount_store function contains the env_save function
			 *
			 * If CONFIG_BOOTCOUNT_ENV is not defined,
			 * need to call env_save function to save the data to env,
			 * because the bootcount_store function does not include
			 * the env_save function at this time.
			 */
			env_save();
#endif
		} else {
			pr_err("DAMAGE SWITCH ERROR:Switch to systemB fail\n");
			return -1;
		}
	} else {
		pr_err("DAMAGE SWITCH : systemB is damaged ;now switch to systemA\n");
		if (!__switch_systemAB("A")) {
			env_set("systemAB_next", "A");
			env_set("systemAB_damage", "B");
			bootcount_store(0);
#ifndef CONFIG_BOOTCOUNT_ENV
			/* The situation is the same as the comment above */
			env_save();
#endif
		} else {
			pr_err("DAMAGE SWITCH ERROR : Switch to systemA fail\n");
			return -1;
		}
	}

	return 0;
}

static int do_sunxi_switch_system(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	int do_reset(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[]);
	if (!sunxi_damage_switch_system()) {
		pr_notice("Damage switching succeeded, now reset the system!\n");
		do_reset(NULL, 0, 0, NULL);
	} else {
		pr_err("Damage switching failed!\n");
		return -1;
	}

	return 0;
}

U_BOOT_CMD(
	sunxi_switch_system,	1,	1,	do_sunxi_switch_system,
	"sunxi switch A/B system",
	""
);
