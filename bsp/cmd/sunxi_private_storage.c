/*
 * (C) Copyright 2018 allwinnertech  <wangwei@allwinnertech.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <privatestorage.h>
#include <sunxi_board.h>
#include <command.h>

static int save_user_data_to_private_storage(const char *name, char *data)
{
	char buffer[512];
	int data_len;
	int ret;
	memset(buffer, 0x00, sizeof(buffer));
	printf("Also save user data %s to secure storage\n", (char *)name);
	if (sunxi_private_storage_init()) {
		printf("secure storage init fail\n");
	} else {
		ret = sunxi_secure_object_read("key_burned_flag", buffer, 512,
					       &data_len);
		if (ret) {
			printf("sunxi secure storage has no flag\n");
		} else {
			if (!strcmp(buffer, "key_burned")) {
				printf("find key burned flag\n");
				return 0;
			}
			printf("do not find key burned flag\n");
		}
		sunxi_secure_object_write(name, data, strnlen(data, 512));
		sunxi_private_storage_exit();
	}
	return 0;
}

int do_probe_private_storage(struct cmd_tbl *cmdtp, int flag, int argc,
			    char *const argv[])
{
	char *cmd, *name;

	cmd = argv[1];
	name = argv[2];

	if (!strcmp(cmd, "read")) {
		if (argc == 2) {
			return sunxi_private_storage_list();
		}
		if (argc == 3) {
			char buffer[4096];
			int ret, data_len;

			memset(buffer, 0, 4096);
			ret = sunxi_private_storage_init();
			if (ret < 0) {
				printf("%s secure storage init err\n",
				       __func__);

				return -1;
			}
			ret = sunxi_secure_object_read(name, buffer, 4096,
						       &data_len);
			if (ret < 0) {
				printf("private data %s is not exist\n", name);

				return -1;
			}
			printf("private data:\n");
			sunxi_dump(buffer, strlen((const char *)buffer));

			return 0;
		}
	} else if (!strcmp(cmd, "erase")) {
		int ret;

		ret = sunxi_private_storage_init();
		if (ret < 0) {
			printf("%s secure storage init err\n", __func__);

			return -1;
		}

		if (argc == 2) {
			ret = sunxi_private_storage_erase_all();
			if (ret < 0) {
				printf("erase secure storage failed\n");
				return -1;
			}
		} else if (argc == 3) {
			if (!strcmp(name, "key_burned_flag")) {
				if (sunxi_private_storage_erase_data_only(
					    "key_burned_flag")) {
					printf("erase key_burned_flag failed\n");
				}
			}
		}
		sunxi_private_storage_exit();

		return 0;
	} else if (!strcmp(cmd, "write")) {
		if (argc == 4)
			save_user_data_to_private_storage(argv[2], argv[3]);
		else
			printf("para error!\n");

		return 0;
	}
	return -1;
}

static int do_cmd_secure_object(struct cmd_tbl *cmdtp, int flag, int argc,
				char *const argv[])
{
	int ret = -1;

	if (argc > 3 || argc < 1) {
		printf("wrong argc\n");
		return -1;
	}

	if (sunxi_private_storage_init() < 0) {
		printf("secure storage init fail\n");
		return -1;
	}

	if ((strncmp("clean", argv[1], strlen("clean")) == 0)) {
		if (strncmp("all", argv[2], strlen("all")) == 0) {
			ret = clear_secure_store(0xffff);
		} else {
			unsigned int index =
				simple_strtoul((const char *)argv[2], NULL, 10);
			ret = clear_secure_store(index);
		}
	} else if (strncmp("dump", argv[1], strlen("dump")) == 0) {
		ret = dump_secure_store(argv[2]);
	} else if ((strncmp("test", argv[1], strlen("test")) == 0)) {
#ifdef _SO_TEST_
		ret = secure_object_op_test();
#else
		ret = cmd_usage(cmdtp);
#endif
	} else if (strncmp("crypt", argv[1], strlen("crypt")) == 0) {
#ifdef CONFIG_SUNXI_SECURE_SYSTEM
		extern int smc_load_sst_test(void);
		smc_load_sst_test();
#endif
		ret = 0;
	} else
		ret = cmd_usage(cmdtp);

	if (sunxi_private_storage_exit() < 0) {
		printf("secure storage exit fail\n");
		return -1;
	}
	return ret;
}

U_BOOT_CMD(pst, 4, 1, do_probe_private_storage,
	   "read data from secure storage"
	   "erase flag in secure storage",
	   "pst read|erase [name]\n"
	   "pst read,  then dump all data\n"
	   "pst read name,  then dump the dedicate data\n"
	   "pst write name, string, write the name = string\n"
	   "pst erase,  then erase all secure storage data\n"
	   "pst erase key_burned_flag,  then erase the dedicate data\n"
	   "NULL");

U_BOOT_CMD(sunxi_so, 3, 1, do_cmd_secure_object, "sunxi_so sub-system",
	   "sunxi_so <cmd> \n"
	   "\t Allwinner secure object storage \n");
