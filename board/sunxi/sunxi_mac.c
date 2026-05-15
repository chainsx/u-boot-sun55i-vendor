/*
 * (C) Copyright 2016
 *Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 *zhouhuacai <zhouhuacai@allwinnertech.com>
 *
 * SPDX-License-Identifier:›GPL-2.0+
 */
#include <common.h>
#include <fdt_support.h>
#include <asm/io.h>
#include <smc.h>

#define SUNXI_SID_BASE 0x03006000
#define SUNXI_SID_KEY0 0x0A0

static void generate_mac_from_cpuid(char *mac_addr, int index)
{
	u16 sunxi_soc_chipid[4];
	u8 byte1, byte2, byte3, byte4, byte5, byte6;

	/* Read CPU ID from SID */

	sunxi_soc_chipid[0] = smc_readl(SUNXI_SID_BASE + 0x200);
	sunxi_soc_chipid[1] = smc_readl(SUNXI_SID_BASE + 0x200 + 0x4);
	sunxi_soc_chipid[2] = smc_readl(SUNXI_SID_BASE + 0x200 + 0x8);
	sunxi_soc_chipid[3] = smc_readl(SUNXI_SID_BASE + 0x200 + 0xc);
	pr_err("chip id : 0x%08x 0x%08x 0x%08x 0x%08x\n",
	       sunxi_soc_chipid[0], sunxi_soc_chipid[1], sunxi_soc_chipid[2], sunxi_soc_chipid[3]);

	byte1 = 0x08;
	byte2 = ((sunxi_soc_chipid[0] >> 8) & 0xFF) ^ (sunxi_soc_chipid[0] & 0xFF);
	byte3 = ((sunxi_soc_chipid[1] >> 8) & 0xFF) ^ (sunxi_soc_chipid[1] & 0xFF);
	byte4 = ((sunxi_soc_chipid[2] >> 8) & 0xFF) ^ (sunxi_soc_chipid[2] & 0xFF);
	byte5 = ((sunxi_soc_chipid[3] >> 8) & 0xFF) ^ (sunxi_soc_chipid[3] & 0xFF);
	byte6 = index ^ (byte2 + byte3 + byte4 + byte5);

	/* Generate MAC address using CPU ID */
	sprintf(mac_addr, "%02x:%02x:%02x:%02x:%02x:%02x",
		byte1, byte2, byte3, byte4, byte5, byte6
	);
}

static int str2num(char *str, char *num)
{
	int val = 0, i;
	char *p = str;
	for (i = 0; i < 2; i++) {
		val *= 16;
		if (*p >= '0' && *p <= '9')
			val += *p - '0';
		else if (*p >= 'A' && *p <= 'F')
			val += *p - 'A' + 10;
		else if (*p >= 'a' && *p <= 'f')
			val += *p - 'a' + 10;
		else
			return -1;
		p++;
	}
	*num = val;
	return 0;
}

static int addr_parse(const char *addr_str, int check)
{
	char addr[6];
	char cmp_buf[6];
	char *p = (char *)addr_str;
	int i;
	if (!p || strlen(p) < 17)
		return -1;

	for (i = 0; i < 6; i++) {
		if (str2num(p, &addr[i]))
			return -1;

		p += 2;
		if ((i < 5) && (*p != ':'))
			return -1;

		p++;
	}

	if (check && (addr[0] & 0x3))
		return -1;

	memset(cmp_buf, 0x00, 6);
	if (memcmp(addr, cmp_buf, 6) == 0)
		return -1;

	memset(cmp_buf, 0xFF, 6);
	if (memcmp(addr, cmp_buf, 6) == 0)
		return -1;

	return 0;
}

struct addr_info_t {
	char *envname;
	char *dtsname;
	int   flag;
};

static struct addr_info_t addr[] = {
	{"mac",      "addr_eth",  1},
	{"mac1",     "addr_eth1", 1},
	{"wifi_mac", "addr_wifi", 1},
	{"bt_mac",   "addr_bt",   0},
};

int update_sunxi_mac(void)
{
	char *p = NULL;
	int   i = 0;
	int   nodeoffset = 0;
	char  mac_str[20];
	struct fdt_header *dtb_base = working_fdt;

	nodeoffset = fdt_path_offset(dtb_base, "/soc/addr_mgt");

	for (i = 0; i < ARRAY_SIZE(addr); i++) {
		p = env_get(addr[i].envname);

		/* Generate MAC address for mac and mac1 if not set */
		if (p == NULL && (strcmp(addr[i].envname, "mac") == 0 ||
                             strcmp(addr[i].envname, "mac1") == 0)) {

			generate_mac_from_cpuid(mac_str, i);
			env_set(addr[i].envname, mac_str);
			p = mac_str;
		}

		if (p != NULL) {
			pr_warn("mac addr => %s\n", p);
			if (addr_parse(p, addr[i].flag)) {
				/*if not pass, clean it, do not pass through cmdline*/
				pr_err("%s format illegal\n", addr[i].envname);
				env_set(addr[i].envname, "");
				continue;
			}
			if (nodeoffset >= 0)
				fdt_setprop_string(dtb_base, nodeoffset, addr[i].dtsname, p);
		}
	}

	return 0;
}
