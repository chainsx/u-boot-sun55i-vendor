/*
 * (C) Copyright 2023-2025
 * allwinner Technology Co., Ltd. <www.allwinnertech.com>
 *
 * some simple description for this code
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */


#include <smc.h>
#include <linux/arm-smccc.h>
#include <linux/ctype.h>
#include <sys_partition.h>
#include <linux/types.h>
#include <common.h>
#include <command.h>
#include <cpu_func.h>
#include <inttypes.h>
#include <stdio_dev.h>
#include <asm/global_data.h>
#include <linux/libfdt.h>
#include <fdt_support.h>
#include <exports.h>
#include <fdtdec.h>
#include <elf_helpers.h>
#include <sys_partition.h>

#define AMP_I(fmt, args...)     printf("SUNXI AMP: "fmt, ##args)
#define AMP_E(fmt, args...)     printf("SUNXI AMP Error: "fmt, ##args)

#define LINUX_AMP_NODES        "/sunxi-amp"
#define UBOOT                  "uboot"

#define AMP_ARM64              BIT(31)

/* see atf-2.5 services/arm_arch_svc/arm_arch_svc_setup.c */
#define ARM_SVC_RUNAMP         (0x8000ff41)

#define AMP_MAX_NUM	       (8)

enum arch_type {
	ARM = 0,
	ARM64 = 1,
};

typedef struct boot_cpu {
	bool enable;
	u32 arch;
	u32 load_addr;
	u32 entry_addr;
	u32 cpu_id;
	u32 os_type;
	const char *desc;
	const char *image;
} boot_cpu_t;


static boot_cpu_t g_bootcpu[AMP_MAX_NUM];

int sunxi_amp_parse_args(int amp_id, void *fdt)
{
	int len;
	int node;
	int amp_node;
	u32 load_addr;
	char amp_name[32];
	const void *buffer;

	node = fdt_path_offset(fdt, LINUX_AMP_NODES);
	if (node < 0) {
		AMP_E("cant't find node \"%s\" \n", LINUX_AMP_NODES);
		return node;
	}

	sprintf(amp_name, "amp%d", amp_id);
	amp_node = fdt_subnode_offset(fdt, node, amp_name);
	if (amp_node < 0) {
		AMP_E("cant't find node %s \n", amp_name);
		return amp_node;
	}

	g_bootcpu[amp_id].desc = fdt_getprop(fdt, node, "description", NULL);

	buffer = fdt_getprop(fdt, amp_node, "boot-stage", &len);
	if (len < 0) {
		AMP_E("cant't get boot-stage node \"%s\" \n", amp_name);
		return len;
	}

	if (strncmp(buffer, UBOOT, strlen(UBOOT)) == 0)
		g_bootcpu[amp_id].enable = true;
	else
		g_bootcpu[amp_id].enable = false;

	g_bootcpu[amp_id].image = fdt_getprop(fdt, amp_node, "image", &len);
	if (len < 0) {
		AMP_E("cant't get image node \"%s\" \n", amp_name);
		return -1;
	}

	g_bootcpu[amp_id].cpu_id = fdt_getprop_u32_default_node(fdt, amp_node, 0, "cpu-id", -1);
	if (g_bootcpu[amp_id].cpu_id < 0) {
		AMP_E("cant't get cpu-id node \"%s\" \n", amp_name);
		return -1;
	}

	g_bootcpu[amp_id].entry_addr = fdt_getprop_u32_default_node(fdt, amp_node, 0, "entry-addr", -1);
	if (g_bootcpu[amp_id].entry_addr < 0) {
		AMP_E("cant't get cpu-id node \"%s\" \n", amp_name);
		return -1;
	}

	load_addr = fdt_getprop_u32_default_node(fdt, amp_node, 0, "load-addr", -1);
	if (load_addr < 0) {
		AMP_I("load_addr use default value %x \n", g_bootcpu[amp_id].load_addr);
	} else
		g_bootcpu[amp_id].load_addr = load_addr;

	g_bootcpu[amp_id].arch = fdt_getprop_u32_default_node(fdt, amp_node, 0, "arch", -1);
	if (g_bootcpu[amp_id].arch < 0) {
		AMP_E("cant't get arch node \"%s\" \n", amp_name);
		return -1;
	}

#if 0
	AMP_I("       desc: %s\n", g_bootcpu[amp_id].desc);
	AMP_I("     enable: %s\n", (g_bootcpu[amp_id].enable ? "Y" : "N"));
	AMP_I("       arch: %s\n", (g_bootcpu[amp_id].arch ? "ARM64" : "ARM"));
	AMP_I("        cpu: 0x%x\n", g_bootcpu[amp_id].cpu_id);
	AMP_I(" entry_addr: 0x%08x\n", g_bootcpu[amp_id].entry_addr);
	AMP_I("  load_addr: 0x%08x\n", g_bootcpu[amp_id].load_addr);
	AMP_I("    os_type: %d\n", g_bootcpu[amp_id].os_type);
	AMP_I("      image: %s\n", g_bootcpu[amp_id].image);
#endif

	return 0;
}

static int sunxi_amp_get_num(void)
{
	int ret;
	u32 amp_count = 0;
	int node;
	void *fdt = working_fdt;

	node = fdt_path_offset(fdt, LINUX_AMP_NODES);
	if (node < 0) {
		AMP_E("cant't find node \"%s\" \n", LINUX_AMP_NODES);
		return 0;
	}

	ret = fdtdec_get_is_enabled(fdt, node);
	if (!ret) {
		AMP_E("%s node status is not okay \n", LINUX_AMP_NODES);
		return 0;
	}

	amp_count = fdt_getprop_u32_default_node(fdt, node, 0, "amp-count", -1);
	if (amp_count < 0) {
		AMP_E("cant't get amp-count node \"%s\" \n", LINUX_AMP_NODES);
		return 0;
	}

	return amp_count;
}


#define ROUND_DOWN(a, b)	((a) & ~((b)-1))
#define ROUND_UP(a, b)		(((a) + (b)-1) & ~((b)-1))

#define ROUND_DOWN_CACHE(a) ROUND_DOWN(a, CONFIG_SYS_CACHELINE_SIZE)
#define ROUND_UP_CACHE(a)   ROUND_UP(a, CONFIG_SYS_CACHELINE_SIZE)


static ulong sunxi_amp_load_elf_img(ulong elf_fw_addr)
{
	void *ehdr;
	void *phdr;
	void *dst = NULL;
	void *src = NULL;
	int i = 0;
	ulong ret = 0;
	u8 class = fw_elf_get_class(elf_fw_addr);
	uint32_t flush_start_addr = 0, flush_cache_size = 0;

	ehdr = (void *)elf_fw_addr;
	phdr = (void *)elf_fw_addr + elf_Ehdr_get_e_phoff(class, ehdr);

	if (!elf_Ehdr_get_e_phnum(class, ehdr)) {
		pr_err("there is no program header, please check whether the ELF file is correct\n");
		return 0;
	}

	/* Load each program header */
	for (i = 0; i < elf_Ehdr_get_e_phnum(class, ehdr); ++i) {

		//remap addresses
		dst = (void *)(ulong)elf_Phdr_get_p_paddr(class, phdr);


		src = (void *)elf_fw_addr + elf_Phdr_get_p_offset(class, phdr);
		pr_err("Loading phdr %i from 0x%p to 0x%p (%lli bytes)\n",
		      i, src, dst, elf_Phdr_get_p_filesz(class, phdr));

		if (elf_Phdr_get_p_filesz(class, phdr)) {
			ret += elf_Phdr_get_p_filesz(class, phdr);
			memcpy(dst, src, elf_Phdr_get_p_filesz(class, phdr));
		}

		/*
		 * In order to speed up firmware loading, don't clear .bss section in here.
		 * Remote processor will clean it when boot up.
		 */
		/*
		if (elf_Phdr_get_p_filesz(class, phdr) != elf_Phdr_get_p_memsz(class, phdr))
			memset(dst + elf_Phdr_get_p_filesz(class, phdr), 0x00,
			elf_Phdr_get_p_memsz(class, phdr) - elf_Phdr_get_p_filesz(class, phdr));
		*/

		/*
		 * In order to avoid current cpu write back the cache content to memory after
		 * remote processor boot up(it will cause strange issue), we need to flush cache.
		 * And the range of flush cache is all memory which is used by remote processor.
		 */
		flush_start_addr = ROUND_DOWN_CACHE((unsigned long)dst);
		flush_cache_size = ROUND_UP_CACHE(elf_Phdr_get_p_memsz(class, phdr));

		//pr_err("flush_start_addr: 0x%08x, flush_cache_size: 0x%08x\n", flush_start_addr, flush_cache_size);
		flush_cache(flush_start_addr, flush_cache_size);
		phdr += elf_size_of_Phdr(class);

	}

	return ret;
}

static int sunxi_amp_load_img(int amp_id)
{
	uint img_offset = 0, img_size = 0;
	const char *zone_name = g_bootcpu[amp_id].image;
	void *load_addr = (void *)g_bootcpu[amp_id].load_addr;
	u8 class_type;
	ulong img_entry;

	sunxi_partition_get_info_byname(zone_name, &img_offset, &img_size);
	if (!img_offset) {
		pr_err("Can`t find img name %s \n", zone_name);
		return -1;
	}

	sunxi_flash_read(img_offset, img_size, (void *)load_addr);

	class_type = fw_elf_get_class((ulong)load_addr);
	img_entry = elf_Ehdr_get_e_entry(class_type, (void *)load_addr);

	if (img_entry == g_bootcpu[amp_id].entry_addr)
		sunxi_amp_load_elf_img((ulong)load_addr);
	else {
		AMP_E("amp cpu entry: %x ; img entry: %lx error \n",
				g_bootcpu[amp_id].entry_addr, img_entry);
		return -1;
	}

	return 0;
}

static int sunxi_amp_bring_up(int amp_id)
{
	ulong ret = 0;
	ulong flag = 0;
	unsigned int cpu_id = g_bootcpu[amp_id].cpu_id;
	unsigned int entry = g_bootcpu[amp_id].entry_addr;
	unsigned int arch = g_bootcpu[amp_id].arch;

	if (arch == ARM64)
		flag |= AMP_ARM64;

	sunxi_smc_call(ARM_SVC_RUNAMP, cpu_id, entry, flag, (ulong)(ulong *)ret);

	return ret;
}

/*******************************************************************/
/* boot_amp - boot application image from image in memory */
/*******************************************************************/
int do_boot_amp(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	int i;
	int num;
	u32 entry_point = simple_strtoul(argv[1], NULL, 16);

	num = sunxi_amp_get_num();

	for (i = 0; i < num; i++) {
		g_bootcpu[i].load_addr = entry_point;

		if (sunxi_amp_parse_args(i, working_fdt)) {
			AMP_E("parse amp-cpu args fail \n");
			return -1;
		}

		if (!g_bootcpu[i].enable)
			continue;

		if (sunxi_amp_load_img(i))
			return -1;

		if (sunxi_amp_bring_up(i))
			return -1;
	}

	AMP_I("sunxi amp cpu on finish \n");

	return 0;
}


U_BOOT_CMD(
	boot_amp, 2, 1, do_boot_amp,
	"boot arm amp", "boot_amp entry_addr"
);
