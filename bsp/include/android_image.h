/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * This is from the Android Project,
 * Repository: https://android.googlesource.com/platform/system/tools/mkbootimg
 * File: include/bootimg/bootimg.h
 * Commit: e55998a0f2b61b685d5eb4a486ca3a0c680b1a2f
 *
 * Copyright (C) 2007 The Android Open Source Project
 */

#ifndef _ANDROID_IMAGE_H_
#define _ANDROID_IMAGE_H_

#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/libfdt.h>

typedef struct andr_img_hdr andr_img_hdr;
typedef struct boot_img_hdr boot_img_hdr;
typedef struct vendor_boot_img_hdr vendor_boot_img_hdr;

#define ANDR_BOOT_DTBO_MAIGC "androidboot.dtbo_idx"
#define ANDR_BOOT_MAGIC "ANDROID!"
#define ANDR_BOOT_MAGIC_SIZE 8
#define ANDR_BOOT_NAME_SIZE 16
#define ANDR_BOOT_ARGS_SIZE 512
#define ANDR_BOOT_EXTRA_ARGS_SIZE 1024

/* The bootloader expects the structure of andr_img_hdr with header
 * version 0 to be as follows: */
#define VENDOR_BOOT_MAGIC_SIZE 8
#define VENDOR_BOOT_MAGIC "VNDRBOOT"
#define VENDOR_BOOT_NAME_SIZE 16
#define VENDOR_BOOT_ARGS_SIZE 2048
#define VENDOR_BOOT_HEAD_SIZE 2108

/* new for v4 */
#define VENDOR_RAMDISK_TYPE_NONE 0
#define VENDOR_RAMDISK_TYPE_PLATFORM 1
#define VENDOR_RAMDISK_TYPE_RECOVERY 2
#define VENDOR_RAMDISK_TYPE_DLKM 3
#define VENDOR_RAMDISK_NAME_SIZE 32
#define VENDOR_RAMDISK_TABLE_ENTRY_BOARD_ID_SIZE 16

struct andr_img_hdr {
    /* Must be ANDR_BOOT_MAGIC. */
    char magic[ANDR_BOOT_MAGIC_SIZE];

    u32 kernel_size; /* size in bytes */
    u32 kernel_addr; /* physical load addr */

    u32 ramdisk_size; /* size in bytes */
    u32 ramdisk_addr; /* physical load addr */

    u32 second_size; /* size in bytes */
    u32 second_addr; /* physical load addr */

    u32 tags_addr; /* physical addr for kernel tags */
    u32 page_size; /* flash page size we assume */

    /* Version of the boot image header. */
    u32 header_version;

    /* Operating system version and security patch level.
     * For version "A.B.C" and patch level "Y-M-D":
     *   (7 bits for each of A, B, C; 7 bits for (Y-2000), 4 bits for M)
     *   os_version = A[31:25] B[24:18] C[17:11] (Y-2000)[10:4] M[3:0] */
    u32 os_version;

    char name[ANDR_BOOT_NAME_SIZE]; /* asciiz product name */

    char cmdline[ANDR_BOOT_ARGS_SIZE];

    u32 id[8]; /* timestamp / checksum / sha1 / etc */

    /* Supplemental command line data; kept here to maintain
     * binary compatibility with older versions of mkbootimg. */
    char extra_cmdline[ANDR_BOOT_EXTRA_ARGS_SIZE];

    /* Fields in boot_img_hdr_v1 and newer. */
    u32 recovery_dtbo_size;   /* size in bytes for recovery DTBO/ACPIO image */
    u64 recovery_dtbo_offset; /* offset to recovery dtbo/acpio in boot image */
    u32 header_size;

    /* Fields in boot_img_hdr_v2 and newer. */
    u32 dtb_size; /* size in bytes for DTB image */
    u64 dtb_addr; /* physical load address for DTB image */
} __attribute__((packed));

/*
 * Add allwinner certification to image file
 */
#define AW_CERT_MAGIC "AW_CERT!"
#define AW_CERT_DESC_SIZE		64
#define AW_PAGESIZE			2048
#define AW_IMAGE_PADDING_LEN	(AW_PAGESIZE-sizeof(struct andr_img_hdr)-\
									AW_CERT_DESC_SIZE)
#define AW_FIT_IMAGE_PADDING_LEN	(AW_PAGESIZE-sizeof(struct fdt_header)-\
									AW_CERT_DESC_SIZE)

/*
 * Pagesize (2048 bytes) offset
 */
struct boot_img_hdr_ex {
	struct andr_img_hdr std_hdr;
	unsigned char padding[AW_IMAGE_PADDING_LEN];

	/*Reserved AW_CERT_DESC_SIZE for allwinner specific data*/
	unsigned char cert_magic[ANDR_BOOT_MAGIC_SIZE];
	unsigned cert_size;
};


struct boot_img_fdt_ex {
	struct fdt_header std_hdr;
	unsigned char padding[AW_FIT_IMAGE_PADDING_LEN];

	/*Reserved AW_CERT_DESC_SIZE for allwinner specific data*/
	unsigned char cert_magic[ANDR_BOOT_MAGIC_SIZE];
	unsigned cert_size;
};

/* When a boot header is of version 0, the structure of boot image is as
 * follows:
 *
 * +-----------------+
 * | boot header     | 1 page
 * +-----------------+
 * | kernel          | n pages
 * +-----------------+
 * | ramdisk         | m pages
 * +-----------------+
 * | second stage    | o pages
 * +-----------------+
 *
 * n = (kernel_size + page_size - 1) / page_size
 * m = (ramdisk_size + page_size - 1) / page_size
 * o = (second_size + page_size - 1) / page_size
 *
 * 0. all entities are page_size aligned in flash
 * 1. kernel and ramdisk are required (size != 0)
 * 2. second is optional (second_size == 0 -> no second)
 * 3. load each element (kernel, ramdisk, second) at
 *    the specified physical address (kernel_addr, etc)
 * 4. prepare tags at tag_addr.  kernel_args[] is
 *    appended to the kernel commandline in the tags.
 * 5. r0 = 0, r1 = MACHINE_TYPE, r2 = tags_addr
 * 6. if second_size != 0: jump to second_addr
 *    else: jump to kernel_addr
 */

struct boot_img_hdr {
	u8 magic[ANDR_BOOT_MAGIC_SIZE];
	u32 kernel_size; /* size in bytes */
	u32 ramdisk_size; /* size in bytes */
	u32 os_version;
	u32 header_size; /* size of boot image header in bytes */
	u32 reserved[4];
	u32 header_version; /* offset remains constant for version check */
	char cmdline[ANDR_BOOT_ARGS_SIZE + ANDR_BOOT_EXTRA_ARGS_SIZE];
} __attribute__((packed));

struct vendor_boot_img_hdr {
	u8 magic[VENDOR_BOOT_MAGIC_SIZE];
	u32 header_version;
	u32 page_size; /* flash page size we assume */
	u32 kernel_addr; /* physical load addr */
	u32 ramdisk_addr; /* physical load addr */
	u32 vendor_ramdisk_size; /* size in bytes */
	u8 cmdline[VENDOR_BOOT_ARGS_SIZE];
	u32 tags_addr; /* physical addr for kernel tags */
	u8 name[VENDOR_BOOT_NAME_SIZE]; /* asciiz product name */
	u32 header_size; /* size of vendor boot image header in
	* bytes */
	u32 dtb_size; /* size of dtb image */
	u64 dtb_addr; /* physical load address */
	/* new for v4 */
	uint32_t vendor_ramdisk_table_size; /* size in bytes for the vendor ramdisk table */
	uint32_t vendor_ramdisk_table_entry_num; /* number of entries in the vendor ramdisk table */
	uint32_t vendor_ramdisk_table_entry_size;
	uint32_t vendor_bootconfig_size; /* size in bytes for bootconfig image */
} __attribute__((packed));

/* new for v4 */
struct vendor_ramdisk_table_entry_v4 {
	uint32_t ramdisk_size; /* size in bytes for the ramdisk image */
	uint32_t ramdisk_offset; /* offset to the ramdisk image in vendor ramdisk section */
	uint32_t ramdisk_type; /* type of the ramdisk */
	uint8_t ramdisk_name[VENDOR_RAMDISK_NAME_SIZE]; /* asciiz ramdisk name */

	// Hardware identifiers describing the board, soc or platform which this
	// ramdisk is intended to be loaded on.
	uint32_t board_id[VENDOR_RAMDISK_TABLE_ENTRY_BOARD_ID_SIZE];
} __attribute__((packed));

/* When the boot image header has a version of 2, the structure of the boot
 * image is as follows:
 *
 * +---------------------+
 * | boot header         | 1 page
 * +---------------------+
 * | kernel              | n pages
 * +---------------------+
 * | ramdisk             | m pages
 * +---------------------+
 * | second stage        | o pages
 * +---------------------+
 * | recovery dtbo/acpio | p pages
 * +---------------------+
 * | dtb                 | q pages
 * +---------------------+
 *
 * n = (kernel_size + page_size - 1) / page_size
 * m = (ramdisk_size + page_size - 1) / page_size
 * o = (second_size + page_size - 1) / page_size
 * p = (recovery_dtbo_size + page_size - 1) / page_size
 * q = (dtb_size + page_size - 1) / page_size
 *
 * 0. all entities are page_size aligned in flash
 * 1. kernel, ramdisk and DTB are required (size != 0)
 * 2. recovery_dtbo/recovery_acpio is required for recovery.img in non-A/B
 *    devices(recovery_dtbo_size != 0)
 * 3. second is optional (second_size == 0 -> no second)
 * 4. load each element (kernel, ramdisk, second, dtb) at
 *    the specified physical address (kernel_addr, etc)
 * 5. If booting to recovery mode in a non-A/B device, extract recovery
 *    dtbo/acpio and apply the correct set of overlays on the base device tree
 *    depending on the hardware/product revision.
 * 6. prepare tags at tag_addr.  kernel_args[] is
 *    appended to the kernel commandline in the tags.
 * 7. r0 = 0, r1 = MACHINE_TYPE, r2 = tags_addr
 * 8. if second_size != 0: jump to second_addr
 *    else: jump to kernel_addr
 */

#endif
