// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2023
 * allwinnertech.
 */
#include <ctype.h>
#include <common.h>
#include <console.h>
#include <bootretry.h>
#include <cli.h>
#include <command.h>
#include <console.h>
#include <log.h>
#include <dm.h>
#include <misc.h>
#include <asm/global_data.h>
#include <asm/io.h>

#include <sunxi_efuse_map.h>

DECLARE_GLOBAL_DATA_PTR;

static int hexstr_to_byte(const char *source, uint8_t *dest, int sourceLen)
{
	uint32_t i;
	uint8_t highByte, lowByte;

	for (i = 0; i < sourceLen; i += 2) {
		highByte = toupper(source[i]);
		lowByte  = toupper(source[i + 1]);

		if (highByte < '0' || (highByte > '9' && highByte < 'A') || highByte > 'F') {
			printf("input buf[%d] is %c, not in 0123456789ABCDEF\n", i, source[i]);
			return -1;
		}

		if (lowByte < '0' || (lowByte > '9' && lowByte < 'A') || lowByte > 'F') {
			printf("input buf[%d] is %c, not in 0123456789ABCDEF\n", i+1, source[i+1]);
			return -1;
		}

		if (highByte > 0x39)
			highByte -= 0x37;
		else
			highByte -= 0x30;


		if (lowByte > 0x39)
			lowByte -= 0x37;
		else
			lowByte -= 0x30;

		dest[i / 2] = (highByte << 4) | lowByte;
	}
	return 0;
}

static int do_sunxi_efuse(struct cmd_tbl *cmdtp, int flag, int argc,
		char *const argv[])
{
	int request = 0;
	char *key_name = NULL;
	char key_data[32] = {0};
	struct udevice *dev;
	int ret;
	int fret = -1;
	int data = -1;
	efuse_key_info_t key_info = {0};

	if ((argc < 2) || (argc > 4))
		return CMD_RET_USAGE;

	if (argc >= 2)
		request = (int)simple_strtoul(argv[1], NULL, 10);

	if (argc >= 3)
		key_name = argv[2];

	if (argc == 4)
		hexstr_to_byte(argv[3], key_data, strlen(argv[3]));

	//fret = uclass_get_device_by_name(UCLASS_MISC, "sid@3006000", &dev);
	fret = uclass_get_device_by_driver(UCLASS_MISC,
				DM_DRIVER_GET(sunxi_efuse), &dev);
	if (fret) {
		pr_err("Unable to find device: sunxi_efuse\n");
		return fret;
	}

	switch (request) {
	case SUNXI_EFUSE_IOCTL_SET_VOL:
		data = 1900;
		fret = misc_ioctl(dev, request, (void *)&data);
		if (fret)
			pr_err("request: %d failed!\n", request);
		else
			printf("request: %d success!\n", request);
		break;
	case SUNXI_EFUSE_IOCTL_READ_KEY:
		data = 0;
		ret = misc_ioctl(dev, request, (void *)&data);
		printf("request: %d read offset [0x%08x] = 0x%08x!\n", request, data, ret);
		break;
	case SUNXI_EFUSE_IOCTL_SET_SMODE:
		printf("skip request %d: set secure mode\n", request);
		break;
	case SUNXI_EFUSE_IOCTL_PROBE_SMODE:
		ret = misc_ioctl(dev, request, (void *)&data);
		printf("request: %d probe secure mode: %d!\n", request, ret);
		break;
	case SUNXI_EFUSE_IOCTL_GET_SSTATUS:
		ret = misc_ioctl(dev, request, (void *)&data);
		printf("request: %d get secure status: %d!\n", request, ret);
		break;
	case SUNXI_EFUSE_IOCTL_READ:
		strcpy(key_info.name, key_name);
		memset(key_data, 0, sizeof(key_data));
		key_info.key_data = key_data;
		key_info.len = 16;
		fret = misc_ioctl(dev, request, (void *)&key_info);
		if (fret) {
			pr_err("request: %d read key: %s failed!\n", request, key_info.name);
		} else {
			printf("request: %d read key: %s!\n", request, key_info.name);
			printf("0x%08x 0x%08x 0x%08x 0x%08x\n", *(int *)(key_data + 0), *(int *)(key_data + 4), *(int *)(key_data + 8), *(int *)(key_data + 12));
		}
		break;
	case SUNXI_EFUSE_IOCTL_WRITE:
		strcpy(key_info.name, key_name);
		key_info.key_data = key_data;
		key_info.len = strlen(argv[3])/2;
		fret = misc_ioctl(dev, request, (void *)&key_info);
		if (fret) {
			pr_err("request: %d write key: %s failed!\n", request, key_info.name);
		} else {
			printf("request: %d write key: %s!\n", request, key_info.name);
			printf("0x%08x 0x%08x 0x%08x 0x%08x\n", *(int *)(key_data + 0), *(int *)(key_data + 4), *(int *)(key_data + 8), *(int *)(key_data + 12));
		}
		break;
	case SUNXI_EFUSE_IOCTL_GET_ROTPK_STATUS:
		ret = misc_ioctl(dev, request, (void *)&data);
		printf("request: %d get rotpk status: %d!\n", request, ret);
		break;
	case SUNXI_EFUSE_IOCTL_VERIFY_ROTPK:
		printf("skip request %d: verify rotpk\n", request);
		break;
	case SUNXI_EFUSE_IOCTL_GET_SOC_VER:
		ret = misc_ioctl(dev, request, (void *)&data);
		printf("request: %d get soc ver: %d!\n", request, ret);
		break;
	default:
		pr_err("%s unsupported request: %d\n", __func__, request);
		fret = -1;
		break;
	}

	return fret;
}


/**************************************************/
U_BOOT_CMD(
	sunxi_efuse,     4,      0,      do_sunxi_efuse,
	"test sunxi efuse dm",
	"sunxi_efuse <request_id> <key_name> <key_data>"
);
