/*
 * (C) Copyright 2017  <wangwei@allwinnertech.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <fs.h>
#include <fdt_support.h>
#include <asm/global_data.h>
#include <command.h>
#include <version.h>
#include <private_uboot.h>
#include <spare_head.h>
#include <sunxi_board.h>
#include <privatestorage.h>
#include <sys_partition.h>
#include <sunxi_flash.h>

DECLARE_GLOBAL_DATA_PTR;

extern int get_boot_storage_type_ext(void);
extern int get_boot_storage_type(void);

/**
 * fdt_getprop_u32 - Find a node and return it's property or a default
 *
 * @fdt: ptr to device tree
 * @nodeoffset:  by fdt_path_offset() function
 * @prop: property name
 * @val: return value if the property is found
 * @note return value is the num of  the u32 element
 */
int fdt_getprop_u32(const void *fdt, int nodeoffset,
			const char *prop, uint32_t *val)
{
	int len;
	const fdt32_t *data = NULL;

	data = fdt_getprop(fdt, nodeoffset, prop, &len);
	if ((data == NULL) || (len == 0) || (len % 4 != 0)) {
		return -FDT_ERR_INTERNAL;
	}
	if (val != NULL) {
		const uint32_t *p = data;
		int j;
		for (j = 0, p = data; j < len / 4; j++) {
			*val = fdt32_to_cpu(p[j]);
			val++;
		}
	}
	return len / 4;
}

int script_parser_fetch(char *node_path, char *prop_name, int value[], int def_val)
{
	int nodeoffset;
	int ret;

	//get property vaule by handle
	nodeoffset = fdt_path_offset(working_fdt, node_path);
	if (nodeoffset < 0) {
		pr_debug("fdt err returned %s\n", fdt_strerror(nodeoffset));
		value[0] = def_val;
		return -1;
	}

	ret = fdt_getprop_u32(working_fdt, nodeoffset, prop_name, (u32 *)value);
	if (ret < 0) {
		pr_debug("%s :%s|%s err returned %s\n", __func__, node_path, prop_name, fdt_strerror(ret));
		value[0] = def_val;
		return -1;
	}

	return 0;
}

#ifdef CONFIG_SUNXI_USER_KEY
char *IGNORE_ENV_VARIABLE[] = {
	"console",    "root",    "init",	   "loglevel",
	"partitions", "vmalloc", "earlyprintk",    "ion_reserve",
	"enforcing",  "cma",     "initcall_debug", "gpt",
};
#define NAME_SIZE 32
int USER_DATA_NUM;
char USER_DATA_NAME[32][NAME_SIZE] = { { '\0' } };

void check_user_data(void)
{
	char *command_p    = NULL;
	char temp_name[32] = { '\0' };
	char temp_value[32] = { '\0' };
	int i, j, k;

	if ((get_boot_storage_type() == STORAGE_SD) ||
	    (get_boot_storage_type() == STORAGE_EMMC) ||
	    (get_boot_storage_type() == STORAGE_EMMC0)) {
		command_p = env_get("setargs_mmc");
	} else {
		command_p = env_get("setargs_nand");
	}
	//printf("cmd line = %s\n", command_p);
	if (!command_p) {
		printf("cann't get the boot_base from the env\n");
		return;
	}

	while (*command_p != '\0' && *command_p != ' ') { //过滤第一个环境变酿
		command_p++;
	}
	command_p++;
	while (*command_p == ' ') { //过滤多余的空枿
		command_p++;
	}
	while (*command_p != '\0' && *command_p != ' ') { //过滤第二个环境变酿
		command_p++;
	}
	command_p++;
	while (*command_p == ' ') {
		command_p++;
	}

	USER_DATA_NUM = 0;
	while (*command_p != '\0') {
		i = 0;
		while (*command_p != '=' && *command_p != '\0') {
			temp_name[i++] = *command_p;
			command_p++;
		}
		temp_name[i] = '\0';

		k = 0;
		while (*command_p != ' ' && *command_p != '\0') {
			temp_value[k++] = *command_p;
			command_p++;
		}

		if (i != 0) {
			for (j = 0; j < sizeof(IGNORE_ENV_VARIABLE) /
						sizeof(IGNORE_ENV_VARIABLE[0]);
			     j++) {
				if (!strcmp(IGNORE_ENV_VARIABLE[j],
					    temp_name)) { //查词典库，排除系统的环境变量，得到用户的数据
					break;
				}
			}
			if (j >= sizeof(IGNORE_ENV_VARIABLE) /
					 sizeof(IGNORE_ENV_VARIABLE[0])) {
				if (!strcmp(temp_name, "mac_addr")) { //处理mac_addr和mac不相等的情况（特殊情况）
					strcpy(USER_DATA_NAME[USER_DATA_NUM], "mac");
					USER_DATA_NUM++;
				} else {
					if (temp_value[1] == '$') {
						/* Remove excess characters: =${xxx} -> xxx */
						memmove(temp_value, &temp_value[3], k - 3);
						temp_value[k - 4] = '\0';
						if (!env_get(temp_value)) {
							strcpy(USER_DATA_NAME[USER_DATA_NUM], temp_name);
							USER_DATA_NUM++;
						}
					}
				}
			}
		}
		while (*command_p == ' ') {
			command_p++;
		}
	}
	/*
	printf("USER_DATA_NUM = %d\n", USER_DATA_NUM);
	for (i = 0; i < USER_DATA_NUM; i++) {
		printf("user data = %s\n", USER_DATA_NAME[i]);
	}
*/
}

static int get_key_from_private(int partno, char *filename, char *data_buf, int *len)
{
	char part_info[16]  = { 0 }; /* format: "partno:0" */
	char file_info[64]  = { 0 };
	loff_t len_read;

	if (partno < 0)
		return -1;

	strcpy(file_info, filename);

	/* get data from file */
	sprintf(part_info, "0:%x", partno);
	memset(data_buf, 0, *len);

	//if (fs_set_blk_dev("sunxi_flash", part_info, FS_TYPE_FAT))
	if (fs_set_blk_dev("mmc", part_info, FS_TYPE_FAT))
		return 1;
	if (fs_read(filename, (ulong)data_buf, 0, 0, &len_read) < 0)
		return -1;
	data_buf[*len] = 0;

	return 0;
}

/*ret:0:not found, 1:found*/
int sunxi_bootargs_load_key(const char *name, int *data_len, char *buffer,
			    int buffer_size)
{
	static int partno     = -1;
	const char *file_path = "ULI/factory";
	int ret;
	int found      = 0;
	int sec_inited = !sunxi_private_storage_init();

	char fetch_name[64] = { 0 };
	char fetch_data[64] = { 0 };
	char full_name[64]  = { 0 };

	/* check private partition info */
	if (partno == -2) {
		/*already know private non exist*/
	} else if (partno == -1) {
		partno = sunxi_partition_get_partno_byname("private");
		if (partno >= 0) {
			//sprintf(buffer, "fatls sunxi_flash 0:%x %s", partno,
			sprintf(buffer, "fatls mmc 0:%x %s", partno,
				file_path);
			printf("List file under %s\n", file_path);
			if (run_command(buffer, 0)) {
				partno = -2;
			}
		}
	}

	found = 0;
	memset(buffer, 0, 512);
	if (sec_inited) {
		ret = sunxi_secure_object_read(name, buffer, 512, data_len);
		if (!ret && *data_len < 512) {
			found = 1;
		}
	}

	if (!found) {
		*data_len = 512;

		if (partno >= 0) {
			sprintf(fetch_name, "%s_filename", name);
			ret = script_parser_fetch("/soc/serial_feature",
						  fetch_name, (int *)fetch_data,
						  sizeof(fetch_data) / 4);
			if ((ret < 0) || (strlen(fetch_data) == 0))
				sprintf(full_name, "%s/%s.txt", file_path,
					name);
			else
				sprintf(full_name, "%s", fetch_data);

			ret = get_key_from_private(partno, full_name, buffer,
						   data_len);
			if (!ret)
				found = 2;
		}
	}

	if (found) {
		env_set(name, buffer);
		pr_info("update %s = %s, source:%s\n", name, buffer,
		       found == 2 ? "private" : "secure");
		memset(buffer, 0, 512);
	}
	return found;
}

int update_user_data(void)
{
	if (get_boot_work_mode() != WORK_MODE_BOOT) {
		return 0;
	}

	check_user_data(); //从env中检测用户的环境变量

	int data_len;
	int k;
	char buffer[512];
	int updata_data_num = 0;

	for (k = 0; k < USER_DATA_NUM; k++) {
		if (sunxi_bootargs_load_key(USER_DATA_NAME[k], &data_len,
					    buffer, sizeof(buffer))) {
			updata_data_num++;
			strcpy(USER_DATA_NAME[k], "\0");
		}
	}
	return 0;
}

#endif

void update_bootargs(void)
{
	int dram_clk = 0;
	char *str;
	char cmdline[2048]	   = { 0 };
	char tmpbuf[256]	     = { 0 };
	str			     = env_get("bootargs");
	__attribute__((unused)) char disp_reserve[80];
	strncpy(cmdline, str, sizeof(cmdline) - 1);

#ifdef CONFIG_SUNXI_SERIAL
	//serial info
	str = env_get("snum");
	sprintf(tmpbuf, " androidboot.serialno=%s", str);
	strncat(cmdline, tmpbuf, sizeof(cmdline) - strlen(cmdline) - 1);
#endif
#ifdef CONFIG_SUNXI_MAC
	str = env_get("mac");
	if (str && !strstr(cmdline, " mac_addr=")) {
		sprintf(tmpbuf, " mac_addr=%s", str);
		strncat(cmdline, tmpbuf, sizeof(cmdline) - strlen(cmdline) - 1);
	}

	str = env_get("wifi_mac");
	if (str && !strstr(cmdline, " wifi_mac=")) {
		sprintf(tmpbuf, " wifi_mac=%s", str);
		strncat(cmdline, tmpbuf, sizeof(cmdline) - strlen(cmdline) - 1);
	}

	str = env_get("bt_mac");
	if (str && !strstr(cmdline, " bt_mac=")) {
		sprintf(tmpbuf, " bt_mac=%s", str);
		strncat(cmdline, tmpbuf, sizeof(cmdline) - strlen(cmdline) - 1);
	}
#endif
	//harware info
	sprintf(tmpbuf, " androidboot.hardware=%s", CONFIG_SYS_CONFIG_NAME);
	strncat(cmdline, tmpbuf, sizeof(cmdline) - strlen(cmdline) - 1);
	/*boot type*/
	sprintf(tmpbuf, " boot_type=%d", get_boot_storage_type_ext());
	strncat(cmdline, tmpbuf, sizeof(cmdline) - strlen(cmdline) - 1);
	sprintf(tmpbuf, " androidboot.boot_type=%d", get_boot_storage_type_ext());
	strncat(cmdline, tmpbuf, sizeof(cmdline) - strlen(cmdline) - 1);

#if defined(CONFIG_SUNXI_SPINOR_BMP)
	if (env_get("disp_reserve")) {
		snprintf(disp_reserve, 80, " disp_reserve=%s",
			env_get("disp_reserve"));
		strncat(cmdline, disp_reserve,
			sizeof(cmdline) - strlen(cmdline) - 1);
	}
#endif


	/*dram_clk*/
	script_parser_fetch("soc/dram_para", "dram_clk", (int *)&dram_clk, 1);
	/* gpt support */
	sprintf(tmpbuf, " gpt=1");
	strncat(cmdline, tmpbuf, sizeof(cmdline) - strlen(cmdline) - 1);

	sprintf(tmpbuf, " uboot_message=%s", PLAIN_VERSION);
	strncat(cmdline, tmpbuf, sizeof(cmdline) - strlen(cmdline) - 1);
#ifdef CONFIG_SPINOR_LOGICAL_OFFSET
	/*spi-nor logical offset */
	sprintf(tmpbuf, " mbr_offset=%d", (sunxi_flash_get_logical_offset() * 512));
	strncat(cmdline, tmpbuf, sizeof(cmdline) - strlen(cmdline) - 1);
#endif /*CONFIG_SPINOR_LOGICAL_OFFSET*/

	uint32_t *dram_para = NULL;
	dram_para	   = (uint32_t *)uboot_spare_head.boot_data.dram_para;

	sprintf(tmpbuf, " androidboot.dramfreq=%d",
		(unsigned int)(dram_para[0]));
	strncat(cmdline, tmpbuf, sizeof(cmdline) - strlen(cmdline) - 1);
	sprintf(tmpbuf, " androidboot.dramsize=%d",
		(unsigned int)(uboot_spare_head.boot_data.dram_scan_size));

	strncat(cmdline, tmpbuf, sizeof(cmdline) - strlen(cmdline) - 1);

	env_set("bootargs", cmdline);
	pr_debug("android.hardware = %s\n", CONFIG_SYS_CONFIG_NAME);

}

#ifdef CONFIG_SUNXI_DM_VERITY
void update_dm_verity_bootargs(void)
{
	char *str;
	char cmdline[2048] = { 0 };
	char dm_mod[256] = {0};

	str = env_get("bootargs");
	strncpy(cmdline, str, sizeof(cmdline) - 1);

	str = env_get("dm_mod");
	if (str) {
		snprintf(dm_mod, 256, " dm-mod.create=%s", str);
		strncat(cmdline, dm_mod, sizeof(cmdline) - strlen(cmdline) - 1);
	}

	env_set("bootargs", cmdline);
}
#endif
