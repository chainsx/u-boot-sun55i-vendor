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

#define BOOT_I(fmt, args...)     printf("SUNXI BOOT: "fmt, ##args)
#define BOOT_E(fmt, args...)     printf("SUNXI BOOT Error: "fmt, ##args)


typedef struct boot_arg {
	u8 class;
	ulong load_addr;
	ulong entry_addr;
	char *part_name;
} boot_args_t;


static boot_args_t boot_args;

#define ROUND_DOWN(a, b)	((a) & ~((b)-1))
#define ROUND_UP(a, b)		(((a) + (b)-1) & ~((b)-1))

#define ROUND_DOWN_CACHE(a) ROUND_DOWN(a, CONFIG_SYS_CACHELINE_SIZE)
#define ROUND_UP_CACHE(a)   ROUND_UP(a, CONFIG_SYS_CACHELINE_SIZE)

static ulong sunxi_load_elf_img(ulong elf_fw_addr)
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

	boot_args.class = class;

	return ret;
}

static int sunxi_load_rtos_img(void)
{
	uint img_offset = 0, img_size = 0;
	const char *part_name = boot_args.part_name;
	void *load_addr = (void *)boot_args.load_addr;
	u8 class_type;
	ulong img_entry;

	sunxi_partition_get_info_byname(part_name, &img_offset, &img_size);
	if (!img_offset) {
		pr_err("Can`t find img name %s \n", part_name);
		return -1;
	}

	sunxi_flash_read(img_offset, img_size, (void *)load_addr);

	class_type = fw_elf_get_class((ulong)load_addr);
	img_entry = elf_Ehdr_get_e_entry(class_type, (void *)load_addr);

	boot_args.entry_addr = img_entry;

	sunxi_load_elf_img((ulong)load_addr);

	return 0;
}

static int sunxi_cpu_bring_up(void)
{
	ulong ret = 0;
	unsigned int entry = boot_args.entry_addr;

#if 1
	BOOT_I(" entry_addr: 0x%lx\n", boot_args.entry_addr);
	BOOT_I("  load_addr: 0x%lx\n", boot_args.load_addr);
	BOOT_I("      class: %d\n", boot_args.class);
	BOOT_I("  part name: %s\n", boot_args.part_name);
#endif

	cleanup_before_linux();

	asm volatile("DSB");
	arm_svc_run_os((ulong)entry, 0, 1);

	/* don`t return */
	return ret;
}


/*******************************************************************/
/* boot_rtos - boot application image from image in memory */
/*******************************************************************/
int do_boot_rtos(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	/* at least two arguments please */

	boot_args.load_addr = simple_strtoul(argv[1], NULL, 16);

	boot_args.part_name = argv[2];

	if (sunxi_load_rtos_img())
		return -1;

	if (sunxi_cpu_bring_up())
		return -1;

	return 0;
}


U_BOOT_CMD(
	boot_rtos, 6, 1, do_boot_rtos,
	"boot arm rtos", "boot_amp load_addr part_name"
);
