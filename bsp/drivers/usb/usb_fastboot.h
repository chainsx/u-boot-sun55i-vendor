/* SPDX-License-Identifier:	GPL-2.0+
 * (C) Copyright 2007-2013
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Jerry Wang <wangflord@allwinnertech.com>
 *
 */

#ifndef __USB_FASTBOOT_H__
#define __USB_FASTBOOT_H__

#include <common.h>

#define SUNXI_USB_FASTBOOT_DEV_MAX (6)

char sunxi_fastboot_normal_LangID[8] = { 0x04, 0x03, 0x09, 0x04, '\0' };
#define SUNXI_FASTBOOT_DEVICE_MANUFACTURER "USB Developer" /* 厂商信息 	*/
#define SUNXI_FASTBOOT_DEVICE_PRODUCT "sunxi" /* 产品信息 	*/
#define SUNXI_FASTBOOT_DEVICE_SERIAL_NUMBER "20080411" /* 产品序列号 	*/
#define SUNXI_FASTBOOT_DEVICE_CONFIG "Android Fastboot"
#define SUNXI_FASTBOOT_DEVICE_INTERFACE "Android Bootloader Interface"

/* String 0 is the language id */
#define SUNXI_FASTBOOT_DEVICE_STRING_MANUFACTURER_INDEX 1
#define SUNXI_FASTBOOT_DEVICE_STRING_PRODUCT_INDEX 2
#define SUNXI_FASTBOOT_DEVICE_STRING_SERIAL_NUMBER_INDEX 3
#define SUNXI_FASTBOOT_DEVICE_STRING_CONFIG_INDEX 4
#define SUNXI_FASTBOOT_DEVICE_STRING_INTERFACE_INDEX 5

/* fastboot */

#define FASTBOOT_TRANSFER_BUFFER CONFIG_FASTBOOT_TRANSFER_BUF_OFFSET
#define FASTBOOT_TRANSFER_BUFFER_SIZE (256 << 20)
#define FASTBOOT_ERASE_BUFFER SDRAM_OFFSET(0000000)
#define FASTBOOT_ERASE_BUFFER_SIZE (1 << 20)

char *sunxi_usb_fastboot_dev[SUNXI_USB_FASTBOOT_DEV_MAX] = {
	sunxi_fastboot_normal_LangID,  SUNXI_FASTBOOT_DEVICE_MANUFACTURER,
	SUNXI_FASTBOOT_DEVICE_PRODUCT, SUNXI_FASTBOOT_DEVICE_SERIAL_NUMBER,
	SUNXI_FASTBOOT_DEVICE_CONFIG,  SUNXI_FASTBOOT_DEVICE_INTERFACE
};

#define SUNXI_USB_FASTBOOT_BUFFER_MAX (32 * 1024 * 1024)

#define SUNXI_USB_FASTBOOT_IDLE (0)
#define SUNXI_USB_FASTBOOT_SETUP (1)
#define SUNXI_USB_FASTBOOT_SEND_DATA (2)
#define SUNXI_USB_FASTBOOT_RECEIVE_DATA (3)

typedef struct {
	char *base_recv_buffer; //存放接收到的数据
	char *act_recv_buffer;
	uint try_to_recv;
	uint act_recv;
	char *base_send_buffer; //存放预发送数据
	char *act_send_buffer;
	uint send_size; //需要发送数据的长度
} fastboot_trans_set_t;

#define SUNXI_FASTBOOT_SEND_MEM_SIZE (64 * 1024)

#define DEVICE_VENDOR_ID 0x1F3A
#define DEVICE_PRODUCT_ID 0x1010
#define DEVICE_BCD 0x0200

#define FASTBOOT_INTERFACE_SUB_CLASS 0x42
#define FASTBOOT_INTERFACE_PROTOCOL 0x03

#ifndef CFG_FASTBOOT_MKBOOTIMAGE_PAGE_SIZE
#define CFG_FASTBOOT_MKBOOTIMAGE_PAGE_SIZE 2048
#endif

#define FASTBOOT_BOOT_MAGIC_SIZE 8
#define FASTBOOT_BOOT_NAME_SIZE 16
#define FASTBOOT_BOOT_ARGS_SIZE (1024 + 256)

struct fastboot_boot_img_hdr {
    unsigned char magic[FASTBOOT_BOOT_MAGIC_SIZE];

    unsigned kernel_size; /* size in bytes */
    unsigned kernel_addr; /* physical load addr */

    unsigned ramdisk_size; /* size in bytes */
    unsigned ramdisk_addr; /* physical load addr */

    unsigned second_size; /* size in bytes */
    unsigned second_addr; /* physical load addr */

    unsigned tags_addr; /* physical addr for kernel tags */
    unsigned page_size; /* flash page size we assume */
    unsigned unused[2]; /* future expansion: should be 0 */

    unsigned char name[FASTBOOT_BOOT_NAME_SIZE]; /* asciiz product name */

    unsigned char cmdline[FASTBOOT_BOOT_ARGS_SIZE];

    unsigned id[8]; /* timestamp / checksum / sha1 / etc */
};

#define ANDROID_FORMAT_DETECT (1)

#endif
