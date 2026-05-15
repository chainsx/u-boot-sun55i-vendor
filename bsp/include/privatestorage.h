/*
 * (C) Copyright 2018 allwinnertech  <wangwei@allwinnertech.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __SECURE_STORAGE_H__
#define __SECURE_STORAGE_H__

extern int sunxi_private_storage_init(void);
extern int sunxi_private_storage_exit(void);

extern int sunxi_private_storage_list(void);
extern int sunxi_private_storage_probe(const char *item_name);
extern int sunxi_private_storage_read(const char *item_name, char *buffer,
				     int length, int *data_len);

extern int sunxi_private_storage_write(const char *item_name, char *buffer,
				      int length);
extern int sunxi_private_storage_erase(const char *item_name);
extern int sunxi_private_storage_erase_all(void);
extern int sunxi_private_storage_erase_data_only(const char *item_name);

extern int sunxi_secure_object_build(const char *name, char *buf, int len,
				     int encrypt, int write_protect,
				     char *secdata_buf);
extern int sunxi_secure_object_down(const char *name, char *buf, int len,
				    int encrypt, int write_protect);
extern int sunxi_secure_object_up(const char *name, char *buf, int len);

extern int sunxi_secure_object_set(const char *item_name, int encyrpt,
				   int replace, int, int, int);
extern int sunxi_secure_object_write(const char *item_name, char *buffer,
				     int length);
extern int sunxi_secure_object_read(const char *item_name, char *buffer,
				    int buffer_len, int *data_len);
extern int sunxi_private_storage_write_or_read(const char *item_name,
					      char *buffer, int length,
					      int dir);
int dump_secure_store(char *type);
int clear_secure_store(int index);
int sunxi_private_storage_check_map(void *buffer);
int sunxi_private_storage_check_key(unsigned char *buffer);

extern int smc_load_sst_encrypt(char *name, char *in, unsigned int len,
				char *out, unsigned int *outLen);
#define SUNXI_SECURE_STORTAGE_BLOCK_SIZE 4096
#define SUNXI_HDCP_BUFFER_LEN (320)
#define SUNXI_SECURE_STORTAGE_INFO_HEAD_LEN (64 + 4 + 4 + 4)
#define SUNXI_HDCP_KEY_LEN (288)
typedef struct {
	char name[64]; //key name
	uint32_t len; //the len fo key_data
	uint32_t encrypted;
	uint32_t write_protect;
	char key_data[4096 -
		      SUNXI_SECURE_STORTAGE_INFO_HEAD_LEN]; //the raw data of key
} sunxi_private_storage_info_t;

#define SUNXI_SECSTORE_VERSION 1

#define MAX_STORE_LEN 0xc00 /*3K payload*/
#define STORE_OBJECT_MAGIC 0x17253948
#define STORE_REENCRYPT_MAGIC 0x86734716
#define STORE_WRITE_PROTECT_MAGIC 0x8ad3820f
typedef struct {
	unsigned int magic; /* store object magic*/
	int id; /*store id, 0x01,0x02.. for user*/
	char name[64]; /*OEM name*/
	unsigned int re_encrypt; /*flag for OEM object*/
	unsigned int version;
	unsigned int
		write_protect; /*can be placed or not, =0, can be write_protectd*/
	unsigned int reserved[3];
	unsigned int actual_len; /*the actual len in data buffer*/
	unsigned char data[MAX_STORE_LEN]; /*the payload of secure object*/
	unsigned int crc; /*crc to check the sotre_objce valid*/
} store_object_t;

/* secure storage map, have the key info in the keysecure storage */
#define SEC_BLK_SIZE (4096)
#define MAP_KEY_NAME_SIZE	(64)
#define MAP_KEY_DATA_SIZE	(32)
struct map_info {
	unsigned char data[SEC_BLK_SIZE - sizeof(int) * 2];
	unsigned int magic;
	unsigned int crc;
};

#define SECURE_STORAGE_DUMMY_KEY_NAME "reserved_after_delete"

#endif
