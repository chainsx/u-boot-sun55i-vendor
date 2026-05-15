// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2008-2011 Freescale Semiconductor, Inc.
 */

/* #define DEBUG */

#include <common.h>
#include <command.h>
#include <env.h>
#include <env_internal.h>
#include <fdtdec.h>
#include <linux/stddef.h>
#include <malloc.h>
#include <memalign.h>
#include <part.h>
#include <search.h>
#include <errno.h>
#include <asm/global_data.h>
#include <sunxi_flash.h>
#include <sunxi_board.h>

DECLARE_GLOBAL_DATA_PTR;

#define MTDIDS_MAXLEN              128
#define MTDPARTS_MAXLEN            512
#define PARTITION_MAXLEN           16

#define CONFIG_SUNXI_ENV_PARTITION              "env"
#define CONFIG_SUNXI_ENV_REDUNDAND_PARTITION    "env-redund"

extern int sunxi_partition_get_info(const char *part_name, struct disk_partition *info);

#if 1
const uchar sunxi_sprite_environment[] = {
#ifdef SUNXI_SPRITE_ENV_SETTINGS
	SUNXI_SPRITE_ENV_SETTINGS
#endif
	"\0"
};

static void use_sprite_env(void)
{
	extern struct hsearch_data env_htab;

	if (himport_r(&env_htab, (char *)sunxi_sprite_environment,
		      sizeof(sunxi_sprite_environment), '\0', H_INTERACTIVE, 0,
		      0, NULL) == 0)
		pr_err("Environment import failed: errno = %d\n", errno);
	gd->flags |= GD_FLG_ENV_READY;

	return;
}
#endif

#if defined(CONFIG_CMD_SAVEENV) && !defined(CONFIG_SPL_BUILD)
static inline int write_env(uint blk_start, uint blk_cnt,
			   const void *buffer)
{
	uint n;

	n = sunxi_flash_write(blk_start, blk_cnt, (uchar *)buffer);

	return (n == blk_cnt) ? 0 : -1;
}


#ifdef CONFIG_SUNXI_REDUNDAND_ENVIRONMENT
static int env_sunxi_flash_save(void)
{
	ALLOC_CACHE_ALIGN_BUFFER(env_t, env_new, 1);

	int ret;
	struct disk_partition info = { 0 };

	ret = env_export(env_new);
	if (ret)
		goto fini;

	if (gd->env_valid == ENV_VALID) {
		pr_err("Writing to redundant env... ");
		ret = sunxi_partition_get_info(CONFIG_SUNXI_ENV_REDUNDAND_PARTITION, &info);
		if (ret < 0)
			return -ENODEV;

		if (write_env((uint)info.start, (CONFIG_ENV_SIZE + 511) / 512,
			      (u_char *)env_new)) {
			pr_err("failed\n");
			ret = 1;
			goto fini;
		}
	} else {
		pr_err("Writing to env... ");
		ret = sunxi_partition_get_info(CONFIG_SUNXI_ENV_PARTITION, &info);
		if (ret < 0)
			return -ENODEV;

		if (write_env((uint)info.start, (CONFIG_ENV_SIZE + 511) / 512,
			      (u_char *)env_new)) {
			pr_err("failed\n");
			ret = 1;
			goto fini;
		}
	}

	sunxi_flash_write_end();
	sunxi_flash_flush();
	ret = 0;

	gd->env_valid = gd->env_valid == ENV_REDUND ? ENV_VALID : ENV_REDUND;

fini:
	return ret;
}
#else
static int env_sunxi_flash_save(void)
{
	ALLOC_CACHE_ALIGN_BUFFER(env_t, env_new, 1);

	int ret;
	struct disk_partition info = { 0 };

	ret = sunxi_partition_get_info("env", &info);
	if (ret < 0)
		return -ENODEV;
	ret = env_export(env_new);
	if (ret)
		goto fini;

	printf("Writing to env...\n");
	if (write_env((uint)info.start, (CONFIG_ENV_SIZE*2 + 511) / 512,
		      (u_char *)env_new)) {
		pr_err("failed\n");
		ret = 1;
		goto fini;
	}

	sunxi_flash_write_end();
	sunxi_flash_flush();
	ret = 0;

fini:
	return ret;
}
#endif /* CONFIG_SUNXI_REDUNDAND_ENVIRONMENT */
#endif /* CONFIG_CMD_SAVEENV && !CONFIG_SPL_BUILD */

static inline int read_env(uint blk_start, uint blk_cnt,
			   const void *buffer)
{
	uint n;

	n = sunxi_flash_read(blk_start, blk_cnt, (uchar *)buffer);

	return (n == blk_cnt) ? 0 : -1;
}

#ifdef CONFIG_SUNXI_REDUNDAND_ENVIRONMENT
static int env_sunxi_flash_load(void)
{
	ALLOC_CACHE_ALIGN_BUFFER(char, env1_buf, CONFIG_ENV_SIZE);
	ALLOC_CACHE_ALIGN_BUFFER(char, env2_buf, CONFIG_ENV_SIZE);
	struct disk_partition info = { 0 };
	int ret;
	char *errmsg = "!no device";
	char ids[MTDIDS_MAXLEN];
	char parts[MTDPARTS_MAXLEN];
	char partition[PARTITION_MAXLEN];

	int workmode = get_boot_work_mode();
	if ((workmode & WORK_MODE_PRODUCT) &&
	    (!(workmode & WORK_MODE_UPDATE))) {
		use_sprite_env();
		return 0;
	}

	int read1_fail, read2_fail;
	env_t *tmp_env1, *tmp_env2;

	memset(env1_buf, 0x0, CONFIG_ENV_SIZE);
	memset(env2_buf, 0x0, CONFIG_ENV_SIZE);

	tmp_env1 = (env_t *)env1_buf;
	tmp_env2 = (env_t *)env2_buf;

	ret = sunxi_partition_get_info(CONFIG_SUNXI_ENV_PARTITION, &info);
	if (ret < 0) {
		printf("Can't find %s partition\n", CONFIG_SUNXI_ENV_PARTITION);
		ret = -ENODEV;
		goto err;
	}
	read1_fail = read_env((uint)info.start, (CONFIG_ENV_SIZE + 511) / 512, env1_buf);
	if (read1_fail)
		printf("\n** Unable to read env data from %s partition **\n",
		       CONFIG_SUNXI_ENV_PARTITION);

	memset(&info, 0x0, sizeof(struct disk_partition));
	ret = sunxi_partition_get_info(CONFIG_SUNXI_ENV_REDUNDAND_PARTITION, &info);
	if (ret < 0) {
		printf("Can't find %s partition\n", CONFIG_SUNXI_ENV_REDUNDAND_PARTITION);
		ret = -ENODEV;
		goto err;
	}
	read2_fail = read_env((uint)info.start, (CONFIG_ENV_SIZE + 511) / 512, env2_buf);
	if (read2_fail)
		printf("\n** Unable to read env data from %s partition **\n",
		       CONFIG_SUNXI_ENV_REDUNDAND_PARTITION);

	strcpy(ids, env_get("mtdids"));
	strcpy(parts, env_get("mtdparts"));
	strcpy(partition, env_get("partition"));

	ret =  env_import_redund((char *)tmp_env1, read1_fail, (char *)tmp_env2,
							 read2_fail, H_EXTERNAL);

err:
	if (ret)
		env_set_default(errmsg, 0);

#ifndef CONFIG_SUNXI_RTOS
	env_set("mtdids", ids);
	env_set("mtdparts", parts);
	env_set("partition", partition);
#endif
	return ret;
}
#else


static int env_sunxi_flash_load(void)
{
	ALLOC_CACHE_ALIGN_BUFFER(char, buf, CONFIG_ENV_SIZE);
	struct disk_partition info = { 0 };
	int ret;
	char *errmsg = "!no device";
	char ids[MTDIDS_MAXLEN];
	char parts[MTDPARTS_MAXLEN];
	char partition[PARTITION_MAXLEN];

	int workmode = get_boot_work_mode();
	if ((workmode & WORK_MODE_PRODUCT) &&
	    (!(workmode & WORK_MODE_UPDATE))) {
		use_sprite_env();
		return 0;
	}

	ret = sunxi_partition_get_info("env", &info);
	if (ret < 0) {
		ret = -ENODEV;
		goto err;
	}

	strcpy(ids, env_get("mtdids"));
	strcpy(parts, env_get("mtdparts"));
	strcpy(partition, env_get("partition"));

	if (read_env((uint)info.start, (CONFIG_ENV_SIZE + 511) / 512,
		     buf)) {
		errmsg = "!read failed";
		ret    = -EIO;
		goto err;
	}

	ret = env_import(buf, 0, H_EXTERNAL);
err:
	if (ret)
		env_set_default(errmsg, 0);

	env_set("mtdids", ids);
	env_set("mtdparts", parts);
	env_set("partition", partition);

	return ret;
}
#endif /* CONFIG_SUNXI_REDUNDAND_ENVIRONMENT */

U_BOOT_ENV_LOCATION(sunxi_flash) = {
	.location		     = ENVL_SUNXI_FLASH,
	ENV_NAME("SUNXI_FLASH").load = env_sunxi_flash_load,
	.save			     = env_save_ptr(env_sunxi_flash_save),

};
