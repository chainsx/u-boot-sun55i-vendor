
/* SPDX-License-Identifier: GPL-2.0+
 * Copyright (C) 2023 Allwinnertech
*/

#include <command.h>
#include <common.h>
#include <fdt_support.h>
#include <asm/global_data.h>
#include <android_image.h>
#include <part.h>
#include <display_options.h>
#include <private_uboot.h>
#include <spare_head.h>
#include <sunxi_keybox.h>
#include <sunxi_boot_param.h>
#include <sunxi_board.h>
#include <fs.h>
#include <sunxi_logo_display.h>
#include <sunxi_mmc.h>
#include <sunxi_mmc.h>
#include <sunxi_flash.h>
#ifdef CONFIG_SUNXI_ENABLE_FEL_VERIFY
#include <sunxi_efuse_map.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

#define PARTITION_SETS_MAX_SIZE 1024
#define PARTITION_NAME_MAX_SIZE 16
#define ROOT_PART_NAME_MAX_SIZE (PARTITION_NAME_MAX_SIZE + 5)
#define DEV_PART_NAME_MAX_SIZE (PARTITION_NAME_MAX_SIZE + sizeof("/dev/"))

extern int get_boot_storage_type_ext(void);
extern int get_boot_storage_type(void);
extern int get_boot_storage_str(char *buff);
extern int script_parser_fetch(char *node_path, char *prop_name, int value[], int def_val);
extern uint sunxi_partition_get_offset_byname(const char *part_name);
extern void update_bootargs(void);
extern int sunxi_drm_kernel_para_flush(void);

/*
* name    :  sunxi_str_replace
* fucntion:  replace a word in string  which separated by a space
* note      :  sunxi_str_replace("abc def gh", "def", "replace")    get    "abc replace gh"
*/
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

/*
 * sunxi_parsed_specific_string() - Parse the string skipped by skip_character at intervals of space_character
 * @intput_string: the string to be parsed
 * @output_para[][16]: store the parsed string
 * @space_character: characters separated by space_character
 * @skip_character: skip when encountering skip_character, 0 is invalid
 *
 * exp:
 * 	the string to be parsed:"bootloader, env,boot,vendor_boot, dtbo"
 * 	space_character = ','
 * 	skip_character = ' '
 * output:
 * 	output_para[0] = bootloader
 * 	output_para[1] = env
 * 	output_para[2] = boot
 * 	output_para[3] = vendor_boot
 * 	output_para[4] = dtbo
 *
 */
int sunxi_parsed_specific_string(char *intput_string, char output_para[][16], char space_character, char skip_character)
{
	int i, j, k;
	for (i = 0, j = 0, k = 0;; i++) {
		if ((skip_character != 0) && (intput_string[i] == skip_character)) {
			continue;
		} else if ((intput_string[i] == space_character) || (intput_string[i] == 0)) {
			output_para[k][j] = 0;
			k++;
			j = 0;
			if (intput_string[i] == 0)
				break;
		} else {
			output_para[k][j] = intput_string[i];
			j++;
		}
	}
	/* for (i = 0; i < k; i++)
	 *         printf("output_para[%d]:%s\n", i, output_para[i]); */
	return 0;
}

static int sunxi_update_partinfo(void)
{
	int index, ret;
	char partition_sets[PARTITION_SETS_MAX_SIZE];
	char part_name[PARTITION_NAME_MAX_SIZE];
	char root_part_name[ROOT_PART_NAME_MAX_SIZE];
	char *root_partition;
	char blkoops_part_name[DEV_PART_NAME_MAX_SIZE];
	char *blkoops_partition;
	char *partition_index = partition_sets;
	int offset = 0;
	int temp_offset = 0;
	int storage_type = get_boot_storage_type();
	struct blk_desc *desc;
	struct disk_partition info = { 0 };

	memset(root_part_name, 0, ROOT_PART_NAME_MAX_SIZE);
	root_partition = env_get("root_partition");
	if (root_partition)
		pr_notice("root_partition is %s\n", root_partition);

	memset(blkoops_part_name, 0, DEV_PART_NAME_MAX_SIZE);
	blkoops_partition = env_get("blkoops_partition");
	if (blkoops_partition)
		pr_notice("blkoops_partition is %s\n", blkoops_partition);

	memset(partition_sets, 0, PARTITION_SETS_MAX_SIZE);

	sunxi_flash_dev_get_blk(&desc);

	for (index = 1;; index++) {
		/* Get partitiones by GPT */
		ret = part_get_info(desc, index, &info);
		pr_debug("%s: try part %d, ret = %d\n", __func__, index, ret);
		if (ret < 0)
			break;

		memset(part_name, 0, PARTITION_NAME_MAX_SIZE);
		if (storage_type == STORAGE_NAND) {
			sprintf(part_name, "nand0p%d", index);
		} else if (storage_type == STORAGE_NOR) {
			sprintf(part_name, "mtdblock%d", index);
		} else if (storage_type == STORAGE_EMMC) {
			sprintf(part_name, "mmcblk2p%d", index);
		} else if (storage_type == STORAGE_SD) {
			sprintf(part_name, "mmcblk0p%d", index);
		}

		temp_offset = strlen((char *)info.name) + strlen(part_name) + 2;
		if (temp_offset >= PARTITION_SETS_MAX_SIZE) {
			pr_notice("partition_sets is too long, please reduces "
			       "partition name\n");
			break;
		}
		sprintf(partition_index, "%s@%s:", info.name, part_name);
		if (root_partition && strncmp(root_partition, (char *)info.name, sizeof(info.name)) == 0)
			sprintf(root_part_name, "/dev/%s", part_name);
		if (blkoops_partition && strncmp(blkoops_partition, (char *)info.name, sizeof(info.name)) == 0)
			sprintf(blkoops_part_name, "/dev/%s", part_name);
		offset += temp_offset;
		partition_index = partition_sets + offset;

	}
	partition_sets[offset - 1] = '\0';
	partition_sets[PARTITION_SETS_MAX_SIZE - 1] = '\0';
	env_set("partitions", partition_sets);

#ifdef CONFIG_SUNXI_DM_VERITY
	if (*root_part_name != 0) {
		if (gd->securemode) {
			pr_notice("set root to dm-0\n");
			env_set("verity_dev", root_part_name);
			if (storage_type == STORAGE_NAND)
				env_set("nand_root", "/dev/dm-0");
			else if (storage_type == STORAGE_NOR)
				env_set("nor_root", "/dev/dm-0");
			else
				env_set("mmc_root", "/dev/dm-0");
		} else {
			pr_notice("set root to %s\n", root_part_name);
			if (storage_type == STORAGE_NAND)
				env_set("nand_root", root_part_name);
			else if (storage_type == STORAGE_NOR)
				env_set("nor_root", root_part_name);
			else
				env_set("mmc_root", root_part_name);
		}
	}
#else
	if (*root_part_name != 0) {
		pr_notice("set root to %s\n", root_part_name);
		if (storage_type == STORAGE_NAND)
			env_set("nand_root", root_part_name);
		else if (storage_type == STORAGE_NOR)
			env_set("nor_root", root_part_name);
		else
			env_set("mmc_root", root_part_name);
	}
#endif
	if (*blkoops_part_name != 0) {
		pr_notice("set blkoops_blkdev to %s\n", blkoops_part_name);
		env_set("blkoops_blkdev", blkoops_part_name);
	}
	pr_notice("update part info\n");
	return 0;
}

static int sunxi_update_sunxicmd(void)
{
	char  sunxi_command[128];
	int   storage_type = get_boot_storage_type();
	memset(sunxi_command, 0x0, 128);
	strncpy(sunxi_command, env_get("sunxicmd"), sizeof(sunxi_command)-1);
	pr_debug("base sunxicmd=%s\n", sunxi_command);

	if ((storage_type == STORAGE_SD) || (storage_type == STORAGE_EMMC) ||
	    (storage_type == STORAGE_EMMC3) || (storage_type == STORAGE_EMMC0)) {
		if (storage_type == STORAGE_EMMC) {
			sunxi_str_replace(sunxi_command, "setargs_nand", "setargs_emmc");
			sunxi_str_replace(sunxi_command, "setargs_mmc", "setargs_emmc");
			sunxi_str_replace(sunxi_command, "setargs_sd", "setargs_emmc");
			pr_debug("sunxicmd set setargs_emmc\n");
		} else if (storage_type == STORAGE_SD){
			sunxi_str_replace(sunxi_command, "setargs_nand", "setargs_sd");
			sunxi_str_replace(sunxi_command, "setargs_mmc", "setargs_sd");
			sunxi_str_replace(sunxi_command, "setargs_emmc", "setargs_sd");
			pr_debug("sunxicmd set setargs_sd\n");
		}
	} else if (storage_type == STORAGE_NOR) {
		sunxi_str_replace(sunxi_command, "setargs_nand", "setargs_nor");
	} else if (storage_type == STORAGE_NAND) {
		pr_debug("sunxicmd set setargs_nand\n");
	}

	env_set("sunxicmd", sunxi_command);
	pr_debug("to be run cmd=%s\n", sunxi_command);

	return 0;
}

struct os_memory_info_t {
	uint64_t shm_base;
	uint32_t shm_size;
	uint64_t ta_ram_base;
	uint32_t ta_ram_size;
	uint64_t tee_ram_base;
	uint32_t tee_ram_size;
} static os_memory_info;
int secure_os_memory_init(void)
{
	//some platforms uboot and kernel use different fdt
	//so dont add reserve memory until we are updating
	//kernel fdt, save the values for now
#if defined(CONFIG_SUNXI_EXTERN_SECURE_MM_LAYOUT)
	script_parser_fetch("/firmware/optee", "shm_base",
			    (int *)&os_memory_info.shm_base, 0);
	script_parser_fetch("/firmware/optee", "shm_size",
			    (int *)&os_memory_info.shm_size, 0);

	script_parser_fetch("/firmware/optee", "ta_ram_base",
			    (int *)&os_memory_info.ta_ram_base, 0);
	script_parser_fetch("/firmware/optee", "ta_ram_size",
			    (int *)&os_memory_info.ta_ram_size, 0);
#endif
	script_parser_fetch("/firmware/optee", "tee_ram_base",
			    (int *)&os_memory_info.tee_ram_base, 0);
	script_parser_fetch("/firmware/optee", "tee_ram_size",
			    (int *)&os_memory_info.tee_ram_size, 0);
	return 0;
}

#if CONFIG_IS_ENABLED(ANDROID_BOOT_IMAGE)
static int android_image_get_dtb(const struct andr_img_hdr *hdr,
			      ulong *dtb_data, ulong *dtb_len)
{
	u32 offset = 0;
	u32 part_start;
	if (!strncmp(hdr->magic, ANDR_BOOT_MAGIC, ANDR_BOOT_MAGIC_SIZE)) {
		offset += hdr->page_size;
		offset += ALIGN(hdr->kernel_size, hdr->page_size);
		offset += ALIGN(hdr->ramdisk_size, hdr->page_size);
		offset += ALIGN(hdr->second_size, hdr->page_size);
		offset += ALIGN(hdr->recovery_dtbo_size, hdr->page_size);

		*dtb_len = hdr->dtb_size;
		char *boot_name = env_get("boot_partition");
		if (boot_name == NULL) {
			part_start = sunxi_partition_get_offset_byname("boot");
		} else {
			part_start = sunxi_partition_get_offset_byname(boot_name);
		}
		if (part_start != 0) {
				sunxi_flash_read(part_start + offset /512, ALIGN(hdr->dtb_size, 512)/512, (char *)(*dtb_data));
		}
	}

	pr_debug("dtb address is 0x%lx\n", *dtb_data);
	return fdt_check_header((void *)*dtb_data);
}
#endif

static int sunxi_get_dtb(ulong *dtb_data, ulong *dtb_len)
{
	u32 part_start;
	char env_boot_normal[8][16];
	sunxi_parsed_specific_string(env_get("boot_normal"), env_boot_normal, ' ', 0);
	u32 boot_head_addr = (u32)simple_strtoul(env_boot_normal[2], NULL, 16);
	*dtb_data = CONFIG_SUNXI_FDT_ADDR;

	if (*dtb_data == 0) {
		// New platforms use this method by default
		*dtb_data = (ulong)memalign(PAGE_SIZE, CONFIG_SUNXI_FDT_MALLOC_SIZE);
		if (*dtb_data == 0) {
			pr_err("malloc kernel fdt failed\n");
		}
		pr_info("kernel dts addr:0x%lx\n", *dtb_data);
	}
	memset((void *)(*dtb_data), 0, 8);
	part_start = sunxi_partition_get_offset_byname("dtb");
	if (part_start != 0) {
		sunxi_flash_read(part_start, ALIGN(sizeof(struct fdt_header), 512)/512, (void *)(ulong)(*dtb_data));
		*dtb_len = fdt_totalsize((char *)(*dtb_data));
		sunxi_flash_read(part_start, ALIGN(*dtb_len, 512)/512, (char *)(*dtb_data));
	}
	if (fdt_check_header((void *)*dtb_data) < 0) {
		char *boot_name = env_get("boot_partition");
		if (boot_name == NULL) {
			part_start = sunxi_partition_get_offset_byname("boot");
		} else {
			part_start = sunxi_partition_get_offset_byname(boot_name);
		}
		sunxi_flash_read(part_start, ALIGN(sizeof(struct andr_img_hdr), 512)/512, (void *)(ulong)boot_head_addr);
#ifdef CONFIG_ANDROID_BOOT_IMAGE
		android_image_get_dtb((const struct andr_img_hdr *)(ulong)boot_head_addr, dtb_data, dtb_len);
#endif
	}
	return 0;
}

static int sunxi_replace_fdt_v2(void)
{
	ulong dtb_data = 0, dtb_len = 0;
	sunxi_get_dtb(&dtb_data, &dtb_len);
	dtb_data = (fdt_check_header((void *)dtb_data) < 0 ? (CONFIG_SYS_TEXT_BASE + SUNXI_DTB_OFFSET) : dtb_data);

	gd->sunxi_uboot_fdt = working_fdt;
	fdt_set_totalsize((void *)dtb_data, CONFIG_SUNXI_FDT_MALLOC_SIZE);
	set_working_fdt_addr((ulong)dtb_data);
	env_set_hex("sunxi_fdt_addr", dtb_data);
	pr_notice("change working_fdt 0x%lx to 0x%lx\n", (ulong)gd->sunxi_uboot_fdt, (ulong)working_fdt);

	return 0;
}

static int update_fdt_dram_para_from_bootpara(void *dtb_base)
{
	/*fix dram para*/
	int i, nodeoffset = 0;
	char dram_str[16]   = { 0 };
	uint32_t *dram_para = NULL;
	typedef_sunxi_boot_param *sunxi_boot_param = gd->sunxi_boot_param_addr;

	gd->bd->bi_dram[0].size =
		(unsigned long long)uboot_spare_head.boot_data.dram_scan_size * 1024 * 1024;

#ifdef CONFIG_SUNXI_BOOT_PARAM
	if (sunxi_bootparam_check_magic(sunxi_boot_param) < 0) {
		pr_err("%s:%d:bootparam magic error\n", __func__, __LINE__);
		return -1;
	}
#endif
	if (arch_fixup_fdt(dtb_base) < 0) {
		printf("ERROR: arch-specific fdt fixup failed\n");
		return -1;
	}

	nodeoffset = fdt_path_offset(dtb_base, "/dram");
	if (nodeoffset < 0) {
		pr_err("## error: %s : %s\n", __func__,
		       fdt_strerror(nodeoffset));
		return -1;
	}

	dram_para = (uint32_t *)sunxi_boot_param->ddr_info;
	memset(dram_str, 0, sizeof(dram_str));
	for (i = MAX_DRAMPARA_SIZE - 1; i >= 0; i--) {
		sprintf(dram_str, "dram_para%02d", i);
		fdt_setprop_u32(dtb_base, nodeoffset, dram_str, dram_para[i]);
	}
	return 0;
}

static int fdt_enable_node(char *name, int onoff)
{
	int nodeoffset = 0;
	int ret = 0;

	nodeoffset = fdt_path_offset(working_fdt, name);
	ret = fdt_set_node_status(working_fdt, nodeoffset,
		onoff ? FDT_STATUS_OKAY : FDT_STATUS_DISABLED);

	if (ret < 0) {
		pr_err("enable node error: %s\n", fdt_strerror(ret));
	}
	return ret;
}

int sunxi_update_fdt_para_for_kernel(void)
{
	uint storage_type = 0;
	__maybe_unused int ret = 0;
#ifdef CONFIG_AW_MMC
	struct mmc *mmc = NULL;
	int dev_num = 0;
#endif

#if CONFIG_IS_ENABLED(AW_DRM)
	sunxi_drm_kernel_para_flush();
#endif

	storage_type = get_boot_storage_type_ext();

#ifdef CONFIG_AW_MMC
	/* update sdhc dbt para */
	if (storage_type == STORAGE_EMMC || storage_type == STORAGE_EMMC3
			|| (storage_type == STORAGE_EMMC0)) {
		if (storage_type == STORAGE_EMMC)
			dev_num = 2;
		else if (storage_type == STORAGE_EMMC0)
			dev_num = 0;
		else
			dev_num = 3;
	//	dev_num = (storage_type == STORAGE_EMMC) ? 2 : 3;
		mmc = find_mmc_device(0);
		if (mmc == NULL) {
			printf("can't find valid mmc %d\n", dev_num);
			return -1;
		}
		if (((struct sunxi_mmc_priv *)(mmc->priv))->cfg->sample_mode == AUTO_SAMPLE_MODE) {
				mmc_update_config_for_sdly(mmc);
		}
	}
#endif

	/* fix nand&sdmmc */
	switch (storage_type) {
	case STORAGE_SPI_NAND:
		fdt_enable_node("spi0", 1);
		fdt_enable_node("spi-nor0", 0);
		fdt_enable_node("spi-nand0", 1);
		break;
	case STORAGE_NOR:
		fdt_enable_node("spi0", 1);
		fdt_enable_node("spi-nor0", 1);
		fdt_enable_node("spi-nand0", 0);
		break;
	case STORAGE_EMMC:
		ret = fdt_enable_node("sunxi-mmc2", 1);
		if (ret)
			fdt_enable_node("mmc2", 1);
		break;
	case STORAGE_EMMC0:
		ret = fdt_enable_node("sunxi-mmc0", 1);
		if (ret)
			fdt_enable_node("mmc0", 1);
		break;
	case STORAGE_EMMC3:
		ret = fdt_enable_node("sunxi-mmc3", 1);
		if (ret)
			fdt_enable_node("mmc3", 1);
		break;
	case STORAGE_SD:
		ret = fdt_enable_node("sunxi-mmc0", 1);
		ret = fdt_enable_node("sunxi-mmc2", 1);
		if (ret)
			fdt_enable_node("mmc0", 1);
		break;
	default:
		break;
	}

#if defined(CONFIG_SUNXI_EXTERN_SECURE_MM_LAYOUT)
	if (os_memory_info.shm_size) {
		pr_debug("shm_base=0x%x\n", (uint32_t)os_memory_info.shm_base);
		pr_debug("shm_size=0x%x\n", os_memory_info.shm_size);
		ret = fdt_add_mem_rsv(working_fdt, os_memory_info.shm_base,
				      os_memory_info.shm_size);
		if (ret)
			pr_err("##add mem rsv error: %s : %s\n", __func__,
			       fdt_strerror(ret));
	}
	if (os_memory_info.ta_ram_size) {
		pr_debug("ta_ram_base=0x%x\n", (uint32_t)os_memory_info.ta_ram_base);
		pr_debug("ta_ram_size=0x%x\n", os_memory_info.ta_ram_size);
		ret = fdt_add_mem_rsv(working_fdt, os_memory_info.ta_ram_base,
				      os_memory_info.ta_ram_size);
		if (ret)
			pr_err("##add mem rsv error: %s : %s\n", __func__,
			       fdt_strerror(ret));
	}
#endif
	if (os_memory_info.tee_ram_size) {
		pr_debug("tee_ram_base=0x%llx\n", os_memory_info.tee_ram_base);
		pr_debug("tee_ram_size=0x%x\n", os_memory_info.tee_ram_size);
		ret = fdt_add_mem_rsv(working_fdt, os_memory_info.tee_ram_base,
				      os_memory_info.tee_ram_size);
		if (ret)
			pr_err("##add mem rsv error: %s : %s\n", __func__,
			       fdt_strerror(ret));
	}

	/* fix dram para */
	update_fdt_dram_para_from_bootpara(working_fdt);

#ifdef CONFIG_SUNXI_MAC
	extern int update_sunxi_mac(void);
	update_sunxi_mac();
#endif

	pr_notice("update dts\n");
	return 0;
}

int disp_fat_load(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	loff_t read;
	loff_t read_len = 0;
	ulong addr = 0;

	if (argc < 5)
		return -1;
	if (argc >= 6)
		read_len = simple_strtoul(argv[5], NULL, 16);

	addr = simple_strtoul(argv[3], NULL, 16);

	fs_set_blk_dev(argv[1], argv[2], FS_TYPE_FAT);
	return fs_read(argv[4], (ulong)addr, 0, read_len, &read);
}

static int sunxi_update_bootlogo(void)
{
#ifdef CONFIG_SUNXI_SPINOR_BMP
#if defined(CONFIG_CMD_FAT)
	fat_read_logo_to_kernel("bootlogo.bmp");
#else
	read_bmp_to_kernel("bootlogo");
#endif
#endif
#if CONFIG_IS_ENABLED(AW_DRM)
	run_command("sunxi_show_logo", 0);
#endif
	return 0;
}

int do_sunxi_preboot(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	pr_info("do sunxi preboot\n");

#ifdef CONFIG_CMD_SUNXI_AUTO_FEL
	sunxi_auto_fel_by_usb();
#endif
#ifdef CONFIG_SUNXI_USER_KEY
	/* update mac/wifi serial info in env */
	extern int update_user_data(void);
	update_user_data();
#endif
#ifdef CONFIG_AW_NAND
	if (get_boot_storage_type() == STORAGE_NAND)
		ubi_nand_update_ubi_env();
	else
#endif
		sunxi_update_partinfo();
	sunxi_update_sunxicmd();
#ifdef CONFIG_SOUND_SUNXI_BOOT_TONE
	sunxi_boot_tone_play();
#endif
	sunxi_update_bootlogo();

	if (sunxi_probe_android_kernel() == 0) {
		//only android image use
		sunxi_replace_fdt_v2();
		sunxi_update_fdt_para_for_kernel();
	}

#ifdef CONFIG_CMD_SUNXI_BURN
	sunxi_keydata_burn_by_usb();
#endif
#ifdef CONFIG_SUNXI_KEYBOX
	sunxi_keybox_init();
#endif
#ifdef CONFIG_SUNXI_ENABLE_FEL_VERIFY
	sid_enable_verify_fel();
#endif

#ifdef CONFIG_SUNXI_SERIAL
	//set serial to env
	sunxi_set_serial_num();
#endif

	return 0;
}

int do_sunxi_update(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	pr_info("do sunxi update\n");

	update_bootargs();
	return 0;
}

U_BOOT_CMD(
	sunxi_preboot, 1, 0, do_sunxi_preboot,
	"update sunxi_cmd, replace dts and so on", "no parameters\n"
)

U_BOOT_CMD(
	sunxi_update, 1, 0, do_sunxi_update,
	"update kernel cmdline, dts and so on", "no parameters\n"
)
