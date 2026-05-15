/*
 * (C) Copyright 2017-2018
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * wangwei <wangwei@allwinnertech.com>
 *
 * Configuration settings for the Allwinner sunxi series of boards.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <cpu_func.h>
#include <linux/arm-smccc.h>
#include <smc.h>
#include <sunxi_board.h>
#include <privatestorage.h>
#include <efuse_map.h>
#include <memalign.h>
#include <asm/global_data.h>
#include <sunxi_keybox.h>

DECLARE_GLOBAL_DATA_PTR;

#define ARM_SVC_CALL_COUNT          0x8000ff00
#define ARM_SVC_UID                 0x8000ff01
/* 0x8000ff02 reserved */
#define ARM_SVC_VERSION             0x8000ff03
#define ARM_SVC_RUNNSOS             0x8000ff04

#ifdef CONFIG_SUNXI_ARM64
#define ARM_SVC_READ_SEC_REG        0x8000ff05
#define ARM_SVC_WRITE_SEC_REG       0x8000ff06
#else
#define ARM_SVC_READ_SEC_REG        OPTEE_SMC_READ_REG
#define ARM_SVC_WRITE_SEC_REG       OPTEE_SMC_WRITE_REG
#endif

#define PSCI_CPU_OFF                0x84000002
#define PSCI_CPU_ON_AARCH32         0x84000003
#define PSCI_SYSTEM_OFF             0x84000008

#define SUNXI_CPU_ON_AARCH32        0x84000010
#define SUNXI_CPU_OFF_AARCH32       0x84000011
#define SUNXI_CPU_WFI_AARCH32       0x84000012
/* arisc */
#define ARM_SVC_ARISC_STARTUP                   0x8000ff10
#define ARM_SVC_ARISC_WAIT_READY                0x8000ff11
#define ARM_SVC_ARISC_READ_PMU                  0x8000ff12
#define ARM_SVC_ARISC_WRITE_PMU                 0x8000ff13
#define ARM_SVC_ARISC_FAKE_POWER_OFF_REQ_ARCH32 0x83000019
#define ARM_SVC_FAKE_POWER_OFF       0x8000ff14
#define ARM_SVC_UBOOT_POWER_OFF       0x8000ff15

/* efuse */
#define ARM_SVC_EFUSE_READ         (0x8000fe00)
#define ARM_SVC_EFUSE_WRITE        (0x8000fe01)
#define ARM_SVC_EFUSE_PROBE_SECURE_ENABLE_AARCH32    (0x8000fe03)
#define ARM_SVC_EFUSE_CUSTOMER_RESERVED_HANDLE		(0x8000fe05)

#define OPTEE_SMC_FAST_CALL_VAL(func_num) \
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32, \
			   ARM_SMCCC_OWNER_TRUSTED_OS, (func_num))

/*
 * Get revision of Trusted OS.
 *
 * Used by non-secure world to figure out which version of the Trusted OS
 * is installed. Note that the returned revision is the revision of the
 * Trusted OS, not of the API.
 *
 * Returns revision in 2 32-bit words in the same way as
 * OPTEE_MSG_CALLS_REVISION described above.
 */
#define OPTEE_MSG_FUNCID_GET_OS_REVISION	0x0001

 /*
  * Get revision of Trusted OS.
  *
  * Used by non-secure world to figure out which version of the Trusted OS
  * is installed. Note that the returned revision is the revision of the
  * Trusted OS, not of the API.
  *
  * Returns revision in a0-1 in the same way as OPTEE_SMC_CALLS_REVISION
  * described above. May optionally return a 32-bit build identifier in a2,
  * with zero meaning unspecified.
  */
 #define OPTEE_SMC_FUNCID_GET_OS_REVISION OPTEE_MSG_FUNCID_GET_OS_REVISION
 #define OPTEE_SMC_CALL_GET_OS_REVISION \
		OPTEE_SMC_FAST_CALL_VAL(OPTEE_SMC_FUNCID_GET_OS_REVISION)

#define SUNXI_OPTEE_SMC_OFFSET (0x200)
extern uint32_t sunxi_smc_call_offset;

#define OPTEE_SMC_FUNCID_GET_DRMINFO	(15 | sunxi_smc_call_offset)
#define OPTEE_SMC_GET_DRM_INFO \
	OPTEE_SMC_FAST_CALL_VAL(OPTEE_SMC_FUNCID_GET_DRMINFO)

#define OPTEE_SMC_FUNCID_CRYPT  (16 | sunxi_smc_call_offset)
#define OPTEE_SMC_CRYPT \
	OPTEE_SMC_FAST_CALL_VAL(OPTEE_SMC_FUNCID_CRYPT)


#define OPTEE_SMC_FUNCID_READ_REG  (17 | sunxi_smc_call_offset)
#define OPTEE_SMC_READ_REG \
	OPTEE_SMC_FAST_CALL_VAL(OPTEE_SMC_FUNCID_READ_REG)


#define OPTEE_SMC_FUNCID_WRITE_REG  (18 | sunxi_smc_call_offset)
#define OPTEE_SMC_WRITE_REG \
	OPTEE_SMC_FAST_CALL_VAL(OPTEE_SMC_FUNCID_WRITE_REG)

#define OPTEE_SMC_FUNCID_ARISC_COPY_PARAS  (19 | sunxi_smc_call_offset)
#define OPTEE_SMC_ARISC_COPY_PARAS \
	OPTEE_SMC_FAST_CALL_VAL(OPTEE_SMC_FUNCID_ARISC_COPY_PARAS)

#define OPTEE_SMC_FUNCID_GET_TEEADDR_PARAS  (20 | sunxi_smc_call_offset)
#define OPTEE_SMC_GET_TEEADDR_PARAS \
	OPTEE_SMC_FAST_CALL_VAL(OPTEE_SMC_FUNCID_GET_TEEADDR_PARAS)

#define OPTEE_SMC_FUNCID_EFUSE  (21 | sunxi_smc_call_offset)
#define OPTEE_SMC_EFUSE_OP \
	OPTEE_SMC_FAST_CALL_VAL(OPTEE_SMC_FUNCID_EFUSE)
#define EFUSE_OP_WR 1
#define EFUSE_OP_RD 0

#define OPTEE_SMC_FUNCIC_SUNXI_HASH (23 | sunxi_smc_call_offset)
#define OPTEE_SMC_SUNXI_HASH_OP \
		OPTEE_SMC_FAST_CALL_VAL(OPTEE_SMC_FUNCIC_SUNXI_HASH)

#define OPTEE_SMC_FUNCIC_SUNXI_SETUP_MIPS (24 | sunxi_smc_call_offset)
#define OPTEE_SMC_SUNXI_SETUP_MIPS \
		OPTEE_SMC_FAST_CALL_VAL(OPTEE_SMC_FUNCIC_SUNXI_SETUP_MIPS)

#define OPTEE_SMC_FUNCID_SUNXI_INFORM_FDT (25 | sunxi_smc_call_offset)
#define OPTEE_SMC_SUNXI_INFORM_FDT \
		OPTEE_SMC_FAST_CALL_VAL(OPTEE_SMC_FUNCID_SUNXI_INFORM_FDT)

#define OPTEE_SMC_FUNCID_SUNXI_RKP (30 | sunxi_smc_call_offset)
#define OPTEE_SMC_SUNXI_RKP \
		OPTEE_SMC_FAST_CALL_VAL(OPTEE_SMC_FUNCID_SUNXI_RKP)
/*PLATFORM_FASTCALL*/
#define OPTEE_SMC_FUNCID_PLATFORM  100
#define OPTEE_SMC_PLATFORM_CMD \
		OPTEE_SMC_FAST_CALL_VAL(OPTEE_SMC_FUNCID_PLATFORM)
#define TEE_SET_CPU_ENTRY 0xFFFF0001
#define TEE_SET_CPU_OFF 0xFFFF0002

#define TEESMC_SSK_ENCRYPT       0
#define TEESMC_KEYBOX_STORE      1
#define TEESMC_PROBE_SHM_BASE    2
#define TEESMC_SSK_DECRYPT       3
#define TEESMC_RSSK_ENCRYPT      4
#define TEESMC_RSSK_DECRYPT      5
#define TEESMC_AES_CBC           6
#define TEESMC_PKF_ENCRYPT       7
#define TEESMC_HUK_ENCRYPT       8
#define TEESMC_HUK_DECRYPT       9

/*
*note pResult is will
*/
u32 sunxi_smc_call_optee(ulong arg0, ulong arg1, ulong arg2,
		ulong arg3, ulong pResult)
{
	struct arm_smccc_res param = { 0 };
	arm_smccc_smc(arg0, arg1, arg2, arg3, 0, 0, 0, 0, &param);
	return param.a0;
}

/*
*note pResult is will
*/
u32 sunxi_smc_call_atf(ulong arg0, ulong arg1, ulong arg2, ulong arg3,
	ulong pResult)
{
	struct arm_smccc_res param = { 0 };
	arm_smccc_smc(arg0, arg1, arg2, arg3, 0, 0, 0, 0, &param);
	return param.a0;
}

u32 sunxi_smc_call(ulong arg0, ulong arg1, ulong arg2,
		ulong arg3, ulong pResult)
{
#ifdef CONFIG_SUNXI_ARM64
	return sunxi_smc_call_atf(arg0, arg1, arg2, arg3, pResult);
#else
	return sunxi_smc_call_optee(arg0, arg1, arg2, arg3, pResult);
#endif
}

int arm_svc_set_cpu_on(int cpu, uint entry)
{
	return sunxi_smc_call(PSCI_CPU_ON_AARCH32, cpu, entry, 0, 0);
}

int arm_svc_set_cpu_off(int cpu)
{
	return sunxi_smc_call(PSCI_CPU_OFF, cpu, 0, 0, 0);
}

/*for multi cluster*/
int sunxi_smc_set_cpu_entry(u32 entry, int cpu)
{
	struct arm_smccc_res res;
	arm_smccc_smc(OPTEE_SMC_PLATFORM_CMD,
		TEE_SET_CPU_ENTRY, entry, cpu, 0, 0, 0, 0, &res);
	return res.a0;
}

/*for multi cluster*/
int sunxi_smc_set_cpu_off(void)
{
	struct arm_smccc_res res;
	arm_smccc_smc(OPTEE_SMC_PLATFORM_CMD,
		TEE_SET_CPU_OFF, 0, 0, 0, 0, 0, 0, &res);
	return res.a0;
}


int arm_svc_version(u32 *major, u32 *minor)
{
	u32 *pResult = NULL;
	int ret = 0;
	ret = sunxi_smc_call_atf(ARM_SVC_VERSION, 0, 0, 0, (ulong)&pResult);
	if (ret < 0)
		return ret;

	*major = pResult[0];
	*minor = pResult[1];
	return ret;
}

int arm_svc_call_count(void)
{
	return sunxi_smc_call_atf(ARM_SVC_CALL_COUNT, 0, 0, 0, 0);
}

int arm_svc_uuid(u32 *uuid)
{
	return sunxi_smc_call_atf(ARM_SVC_UID, 0, 0, 0, (ulong)uuid);
}

int arm_svc_run_os(ulong kernel, ulong fdt, ulong arg2)
{
	return sunxi_smc_call_atf(ARM_SVC_RUNNSOS, kernel, fdt, arg2, 0);
}

u32 arm_svc_read_sec_reg(ulong reg)
{
	return sunxi_smc_call(ARM_SVC_READ_SEC_REG, reg, 0, 0, 0);
}

int arm_svc_write_sec_reg(u32 val, ulong reg)
{
	sunxi_smc_call(ARM_SVC_WRITE_SEC_REG, reg, val, 0, 0);
	return 0;
}

int arm_svc_arisc_startup(ulong cfg_base)
{
	return sunxi_smc_call_atf(ARM_SVC_ARISC_STARTUP, cfg_base, 0, 0, 0);
}

int arm_svc_arisc_wait_ready(void)
{
	return sunxi_smc_call_atf(ARM_SVC_ARISC_WAIT_READY, 0, 0, 0, 0);
}

int arm_svc_arisc_fake_poweroff(void)
{
	return sunxi_smc_call(ARM_SVC_ARISC_FAKE_POWER_OFF_REQ_ARCH32, 0, 0, 0, 0);
}

int arm_svc_fake_poweroff(ulong dtb_base)
{
	return sunxi_smc_call(ARM_SVC_FAKE_POWER_OFF, dtb_base, 0, 0, 0);
}

int arm_svc_poweroff(void)
{
	return sunxi_smc_call(ARM_SVC_UBOOT_POWER_OFF, 0, 0, 0, 0);
}

int arm_svc_poweroff_charge(void)
{
	return sunxi_smc_call(PSCI_SYSTEM_OFF, 0, 0, 0, 0);
}

u32 arm_svc_arisc_read_pmu(ulong addr)
{
	return sunxi_smc_call_atf(ARM_SVC_ARISC_READ_PMU, addr, 0, 0, 0);
}

int arm_svc_arisc_write_pmu(ulong addr, u32 value)
{
	return sunxi_smc_call_atf(ARM_SVC_ARISC_WRITE_PMU, addr, value, 0, 0);
}

int arm_svc_efuse_read(void *key_buf, void *read_buf)
{
#if defined(CONFIG_SUNXI_ARM64)
	return sunxi_smc_call(ARM_SVC_EFUSE_READ,
		(ulong)key_buf, (ulong)read_buf, 0, 0);
#else
	struct arm_smccc_res res;
	efuse_key_info_t *efuse_info;
	int offset, len;
	memset(&res, 0, sizeof(res));
		/*probe share memory*/
	arm_smccc_smc(OPTEE_SMC_CRYPT, TEESMC_PROBE_SHM_BASE,
	0, 0, 0, 0, 0, 0, &res);
	if (res.a0 != 0) {
		printf("smc tee probe share memory base failed\n ");
		return -1;
	}

	efuse_info = (efuse_key_info_t *)res.a1;
	if (!efuse_info)
		return -1;

	memcpy(efuse_info, key_buf, sizeof(efuse_key_info_t));

	memset(&res, 0, sizeof(res));

	sunxi_smc_call(OPTEE_SMC_EFUSE_OP, EFUSE_OP_RD,
		(ulong)efuse_info, 0, 0);

	offset = ROUND(sizeof(efuse_key_info_t), 8);
	len = *(int *)((u32)efuse_info + SUNXI_KEY_NAME_LEN);
	memcpy(read_buf, (void *)((u32)efuse_info + offset), len);
	return len;
#endif
}

extern int set_efuse_voltage(int vol);
int arm_svc_efuse_write(void *key_buf)
{
	int ret = -1;

	/* set efuse voltage before burn efuse */
	set_efuse_voltage(1);

#if defined(CONFIG_SUNXI_ARM64)
	ret = sunxi_smc_call(ARM_SVC_EFUSE_WRITE,
				(ulong)key_buf, 0, 0, 0);
#else
	struct arm_smccc_res res;
	efuse_key_info_t *efuse_info;
	efuse_key_info_t *wd = (efuse_key_info_t *)key_buf;
	int offset;
	memset(&res, 0, sizeof(res));
	/*probe share memory*/
	arm_smccc_smc(OPTEE_SMC_CRYPT, TEESMC_PROBE_SHM_BASE,
	0, 0, 0, 0, 0, 0, &res);
	if (res.a0 != 0) {
		printf("smc tee probe share memory base failed\n ");
		goto out;
	}

	efuse_info = (efuse_key_info_t *)res.a1;
	if (!efuse_info)
		goto out;

	memcpy(efuse_info, key_buf, sizeof(efuse_key_info_t));
	offset = ROUND(sizeof(efuse_key_info_t), 8);
	memcpy((void *)((u32)efuse_info + offset), wd->key_data,
			wd->len);
	efuse_info->key_data = (u8 *)((u32)efuse_info + offset);
	memset(&res, 0, sizeof(res));

	/*For efuse, we can makesure 0x200 is enough*/
	flush_cache((ulong)efuse_info, 0x200);
	ret = sunxi_smc_call(OPTEE_SMC_EFUSE_OP, EFUSE_OP_WR,
		(ulong)efuse_info, 0, 0);
out:
#endif

	/* close efuse power after burning */
	set_efuse_voltage(0);

	return ret;
}

int arm_svc_probe_secure_mode(void)
{
	return sunxi_smc_call_atf(ARM_SVC_EFUSE_PROBE_SECURE_ENABLE_AARCH32,
		0, 0, 0, 0);
}

int arm_svc_customer_encrypt(u32 customer_reserved_id)
{
	return sunxi_smc_call(ARM_SVC_EFUSE_CUSTOMER_RESERVED_HANDLE,
		customer_reserved_id, 0, 0, 0);
}

static  u32 smc_readl_normal(ulong addr)
{
	return readl(addr);
}
static  int smc_writel_normal(u32 value, ulong addr)
{
	writel(value, addr);
	return 0;
}

u32 (*smc_readl_pt)(ulong addr) = smc_readl_normal;
int (*smc_writel_pt)(u32 value, ulong addr) = smc_writel_normal;
uint32_t sunxi_smc_call_offset = 0xFFFFFFFF;

u32 smc_readl(ulong addr)
{
	return smc_readl_pt(addr);
}

void smc_writel(u32 val, ulong addr)
{
	smc_writel_pt(val, addr);
}

int smc_efuse_readl(void *key_buf, void *read_buf)
{
	printf("smc_efuse_readl is just a interface,you must override it\n");
	return 0;
}

int smc_efuse_writel(void *key_buf)
{
	printf("smc_efuse_writel is just a interface,you must override it\n");
	return 0;
}

int smc_tee_get_os_revision(uint32_t *major, uint32_t *minor)
{
	struct arm_smccc_res param = { 0 };

	arm_smccc_smc(OPTEE_SMC_CALL_GET_OS_REVISION, 0, 0, 0, 0, 0, 0, 0,
		      &param);
	*major = param.a0;
	*minor = param.a1;
	return 0;
}

int smc_tee_get_sunxi_call_offset(void)
{
	uint32_t major, minor;

	smc_tee_get_os_revision(&major, &minor);
	pr_info("optee version: major:%d minor:%d\n", major, minor);
	if ((major > 3) || ((major == 3) && (minor >= 5))) {
		return SUNXI_OPTEE_SMC_OFFSET;
	} else {
		return 0;
	}
}

int smc_init(void)
{
	if (sunxi_probe_secure_os()) {
		sunxi_smc_call_offset = smc_tee_get_sunxi_call_offset();
	}

	return 0;
}

typedef struct _aes_function_info {
	uint32_t  decrypt;
	uint32_t  key_addr;
	uint32_t  key_len;
	uint32_t  iv_map;
	uint32_t  key_mode;
} aes_function_info;

struct smc_tee_ssk_addr_group {
	uint32_t out_tee;
	uint32_t in_tee;
	uint32_t private;
};

int smc_tee_ssk_encrypt(char *out_buf, char *in_buf, int len, int *out_len)
{
	struct arm_smccc_res param = { 0 };
	struct smc_tee_ssk_addr_group *tee_addr;
	int align_len = 0;

	align_len = ALIGN(len, CACHE_LINE_SIZE);
	if (align_len > SUNXI_SECURE_STORTAGE_BLOCK_SIZE - SUNXI_SECURE_STORTAGE_INFO_HEAD_LEN) {
		return -1;
	}

	arm_smccc_smc(OPTEE_SMC_CRYPT, TEESMC_PROBE_SHM_BASE,
		0, 0, 0, 0, 0, 0, &param);
	if (param.a0 != 0) {
		printf("smc tee probe share memory base failed\n ");
		return -1;
	}

	tee_addr = (struct smc_tee_ssk_addr_group *)param.a1;
	tee_addr->in_tee = param.a1 + 0x100;
	tee_addr->out_tee = tee_addr->in_tee + 4096;
	memset((void *)(ulong)tee_addr->in_tee, 0x0, align_len);
	memcpy((void *)(ulong)tee_addr->in_tee, in_buf, len);
	flush_cache(tee_addr->in_tee, align_len);
	flush_cache(tee_addr->out_tee, align_len);
	flush_cache((ulong)tee_addr,
		    ALIGN(sizeof(struct smc_tee_ssk_addr_group),
			  CACHE_LINE_SIZE));

	memset(&param, 0, sizeof(param));
#ifdef CONFIG_SECURE_STORAGE_KEY_HUK
	arm_smccc_smc(OPTEE_SMC_CRYPT, TEESMC_HUK_ENCRYPT,
		(ulong)tee_addr, align_len, 0, 0, 0, 0, &param);
#else
	arm_smccc_smc(OPTEE_SMC_CRYPT, TEESMC_SSK_ENCRYPT,
		(ulong)tee_addr, align_len, 0, 0, 0, 0, &param);
#endif
	if (param.a0 != 0) {
		printf("smc tee encrypt with ssk failed with: %ld", param.a0);

		return -1;
	}
	memcpy(out_buf, (void *)(ulong)tee_addr->out_tee, align_len);
	*out_len = align_len;
	return 0;
}

int smc_tee_ssk_decrypt(char *out_buf, char *in_buf, int len)
{
	struct arm_smccc_res param = { 0 };
	struct smc_tee_ssk_addr_group *tee_addr;

	if (len > SUNXI_SECURE_STORTAGE_BLOCK_SIZE - SUNXI_SECURE_STORTAGE_INFO_HEAD_LEN) {
		return -1;
	}
	arm_smccc_smc(OPTEE_SMC_CRYPT, TEESMC_PROBE_SHM_BASE,
		0, 0, 0, 0, 0, 0, &param);
	if (param.a0 != 0) {
		printf("smc tee probe share memory base failed\n ");
		return -1;
	}

	tee_addr = (struct smc_tee_ssk_addr_group *)param.a1;
	tee_addr->in_tee = param.a1 + 0x100;
	tee_addr->out_tee = tee_addr->in_tee + 4096;
	memcpy((void *)(ulong)tee_addr->in_tee, in_buf, len);
	memset(&param, 0, sizeof(param));
	flush_cache(tee_addr->in_tee, len);
	flush_cache(tee_addr->out_tee, len);
	flush_cache((ulong)tee_addr,
		    ALIGN(sizeof(struct smc_tee_ssk_addr_group),
			  CACHE_LINE_SIZE));

#ifdef CONFIG_SECURE_STORAGE_KEY_HUK
	arm_smccc_smc(OPTEE_SMC_CRYPT, TEESMC_HUK_DECRYPT,
		(ulong)tee_addr, len, 0, 0, 0, 0, &param);
#else
	arm_smccc_smc(OPTEE_SMC_CRYPT, TEESMC_SSK_DECRYPT,
		(ulong)tee_addr, len, 0, 0, 0, 0, &param);
#endif
	if (param.a0 != 0) {
		printf("smc tee decrypt with ssk failed with: %ld", param.a0);

		return param.a0;
	}
	memcpy(out_buf, (void *)(ulong)tee_addr->out_tee, len);

	return 0;
}

int smc_tee_rssk_encrypt(char *out_buf, char *in_buf, int len, int *out_len)
{
	struct smc_tee_ssk_addr_group *tee_addr;
	int align_len = 0;
	struct arm_smccc_res param = { 0 };

	align_len = ALIGN(len, CACHE_LINE_SIZE);

	if (align_len > SUNXI_SECURE_STORTAGE_BLOCK_SIZE - SUNXI_SECURE_STORTAGE_INFO_HEAD_LEN) {
		return -1;
	}
	arm_smccc_smc(OPTEE_SMC_CRYPT, TEESMC_PROBE_SHM_BASE,
		0, 0, 0, 0, 0, 0, &param);
	if (param.a0 != 0) {
		printf("smc tee probe share memory base failed\n ");
		return -1;
	}

	tee_addr = (struct smc_tee_ssk_addr_group *)param.a1;
	tee_addr->in_tee = param.a1 + 0x100;
	tee_addr->out_tee = tee_addr->in_tee + 4096;
	memset((void *)(ulong)tee_addr->in_tee, 0x0, align_len);
	memcpy((void *)(ulong)tee_addr->in_tee, in_buf, len);
	flush_cache(tee_addr->in_tee, align_len);
	flush_cache(tee_addr->out_tee, align_len);
	flush_cache((ulong)tee_addr,
		    ALIGN(sizeof(struct smc_tee_ssk_addr_group),
			  CACHE_LINE_SIZE));

	memset(&param, 0, sizeof(param));
	arm_smccc_smc(OPTEE_SMC_CRYPT, TEESMC_RSSK_ENCRYPT,
		(ulong)tee_addr, align_len, 0, 0, 0, 0, &param);
	if (param.a0 != 0) {
		printf("smc tee encrypt with ssk failed with: %ld", param.a0);

		return param.a0;
	}
	memcpy(out_buf, (void *)(ulong)tee_addr->out_tee, align_len);
	*out_len = align_len;
	return 0;
}

int smc_aes_rssk_decrypt_to_keysram(void)
{
	struct smc_tee_ssk_addr_group *tee_addr;
	struct arm_smccc_res param = { 0 };

	arm_smccc_smc(OPTEE_SMC_CRYPT, TEESMC_PROBE_SHM_BASE,
		0, 0, 0, 0, 0, 0, &param);
	if (param.a0 != 0) {
		printf("smc tee probe share memory base failed\n ");
		return -1;
	}

	tee_addr = (struct smc_tee_ssk_addr_group *)param.a1;
	tee_addr->in_tee = param.a1 + 0x100;
	tee_addr->out_tee = tee_addr->in_tee + 4096;
	memset(&param, 0, sizeof(param));
	flush_cache((ulong)tee_addr,
		    ALIGN(sizeof(struct smc_tee_ssk_addr_group),
			  CACHE_LINE_SIZE));
	arm_smccc_smc(OPTEE_SMC_CRYPT, TEESMC_RSSK_DECRYPT,
		0, SUNXI_HDCP_KEY_LEN, 0, 0, 0, 0, &param);

	if (param.a0 != 0) {
		printf("smc tee decrypt with ssk failed with: %ld", param.a0);
		return param.a0;
	}
	return 0;

}

int smc_aes_algorithm(char *out_buf, char *in_buf, int data_len,
	char *pkey, int key_mode, int decrypt)
{
	struct smc_tee_ssk_addr_group *tee_addr;
	aes_function_info *p_aes_info = NULL;
	int len = data_len;
	uint8_t *aes_key = NULL;
	struct arm_smccc_res param = { 0 };

	arm_smccc_smc(OPTEE_SMC_CRYPT, TEESMC_PROBE_SHM_BASE,
		0, 0, 0, 0, 0, 0, &param);
	if (param.a0 != 0) {
		printf("smc tee probe share memory base failed\n ");
		return -1;
	}

	tee_addr = (struct smc_tee_ssk_addr_group *)param.a1;
	tee_addr->in_tee = param.a1 + 0x100;
	tee_addr->out_tee = tee_addr->in_tee + 4096;
	tee_addr->private = tee_addr->out_tee + 4096;
	aes_key = (uint8_t *)(ulong)(tee_addr->private + 256);

	p_aes_info = (aes_function_info *)(ulong)tee_addr->private;
	p_aes_info->decrypt = decrypt;
	p_aes_info->key_len = 128/8; /* key len is 128 bit */
	p_aes_info->key_addr = (ulong)aes_key;
	memcpy((void *)(ulong)p_aes_info->key_addr, pkey, p_aes_info->key_len);

	memcpy((void *)(ulong)tee_addr->in_tee, in_buf, len);
	memset(&param, 0, sizeof(param));
	flush_cache(tee_addr->in_tee, ALIGN(len, CACHE_LINE_SIZE));
	flush_cache(tee_addr->out_tee, ALIGN(len, CACHE_LINE_SIZE));
	flush_cache(tee_addr->private,
		    ALIGN(sizeof(aes_function_info), CACHE_LINE_SIZE));
	flush_cache(p_aes_info->key_addr,
		    ALIGN(p_aes_info->key_len, CACHE_LINE_SIZE));
	flush_cache((ulong)tee_addr,
		    ALIGN(sizeof(struct smc_tee_ssk_addr_group),
			  CACHE_LINE_SIZE));
	arm_smccc_smc(OPTEE_SMC_CRYPT, TEESMC_AES_CBC,
		(ulong)tee_addr, len, 0, 0, 0, 0, &param);

	if (param.a0 != 0) {
		printf("smc tee decrypt with ssk failed with: %ld", param.a0);
		return param.a0;
	}
	memcpy(out_buf, (void *)(ulong)tee_addr->out_tee, len);
	return 0;
}


int smc_tee_keybox_store(const char *name, char *in_buf, int len)
{
	sunxi_private_storage_info_t *key_box;
	struct arm_smccc_res param = { 0 };

	arm_smccc_smc(OPTEE_SMC_CRYPT, TEESMC_PROBE_SHM_BASE,
		0, 0, 0, 0, 0, 0, &param);
	if (param.a0 != 0) {
		printf("smc tee probe share memory base failed\n ");
		return -1;
	}

	if (len > SUNXI_SECURE_STORTAGE_BLOCK_SIZE) {
		return -1;
	}
	key_box = (sunxi_private_storage_info_t *)param.a1;
	memcpy(key_box, in_buf, len);
	/* debug info */
	pr_info("len=%d\n", len);
	pr_info("name=%s\n", key_box->name);
	pr_info("len=%d\n",  key_box->len);
	pr_info("encrypt=%d\n", key_box->encrypted);
	pr_info("write_protect=%d\n", key_box->write_protect);
	pr_info("******************\n");

	if (strcmp(name, key_box->name)) {
		pr_err("name of key %s not match, key data corrupted\n", name);
		return -1;
	}

#ifdef CONFIG_SECURE_STORAGE_KEY_HUK
	key_box->encrypted = (key_box->encrypted & KEYBOX_ENC_MASK)
		| (KEYBOX_ENC_PARAM_CBC_192_HUK << KEYBOX_ENC_PARAM_OFFSET);
#endif

	flush_cache((ulong)key_box, sizeof(sunxi_private_storage_info_t));

	memset(&param, 0, sizeof(param));
	arm_smccc_smc(OPTEE_SMC_CRYPT, TEESMC_KEYBOX_STORE,
		(ulong)key_box, 0, 0, 0, 0, 0, &param);

	if (param.a0 != 0) {
		printf("smc tee keybox store failed with: %ld", param.a0);

		return param.a0;
	}

	return 0;
}

int smc_tee_probe_drm_configure(ulong *drm_base, ulong *drm_size)
{
	struct arm_smccc_res param = { 0 };
	arm_smccc_smc(OPTEE_SMC_GET_DRM_INFO, 0, 0, 0, 0, 0, 0, 0, &param);

	if (param.a0 != 0) {
		printf("drm config service not available: %X", (uint)param.a0);
		return param.a0;
	}

	*drm_base = param.a1;
	*drm_size = param.a2;

	return 0;
}

int smc_tee_check_hash(const char *name, u8 *hash)
{
#define NAME_MAX_SIZE 16
	struct arm_smccc_res res;
	char *smc_name, *smc_hash;

	memset(&res, 0, sizeof(res));
	/*probe share memory*/
	arm_smccc_smc(OPTEE_SMC_CRYPT, TEESMC_PROBE_SHM_BASE, 0, 0, 0, 0, 0, 0,
		      &res);
	if (res.a0 != 0) {
		printf("smc tee probe share memory base failed\n ");
		return -1;
	}
	if (!res.a1)
		return -1;
	smc_name = (char *)res.a1;
	strncpy(smc_name, name, NAME_MAX_SIZE);
	smc_name[NAME_MAX_SIZE - 1] = 0;
	smc_hash		    = (char *)((ulong)smc_name + NAME_MAX_SIZE);
	memcpy(smc_hash, hash, 32);

	flush_cache((ulong)smc_name,
		    ALIGN(NAME_MAX_SIZE + 32, CACHE_LINE_SIZE));
	return sunxi_smc_call(OPTEE_SMC_SUNXI_HASH_OP, (ulong)smc_name,
			      (ulong)smc_hash, 0, 0);
}

int smc_tee_setup_mips(uint32_t load_addr, uint32_t mips_size)
{
	struct arm_smccc_res param = { 0 };
	arm_smccc_smc(OPTEE_SMC_SUNXI_SETUP_MIPS, load_addr, mips_size, 0, 0, 0,
		      0, 0, &param);
	if (param.a0 != 0) {
		pr_err("%s failed with: %ld", __func__, param.a0);
		return param.a0;
	}
	return 0;
}

int smc_tee_inform_fdt(uint64_t base, uint32_t size)
{
	struct arm_smccc_res param = { 0 };
	ulong fdt_blob =
		((ulong)gd->fdt_blob) & ~(CONFIG_SYS_CACHELINE_SIZE - 1);

	flush_cache(fdt_blob, ALIGN(size, CACHE_LINE_SIZE));

	arm_smccc_smc(OPTEE_SMC_SUNXI_INFORM_FDT, base, 0, size, 0, 0, 0, 0,
		      &param);

	invalidate_dcache_range(
		fdt_blob, (ulong)(ALIGN(((ulong)gd->fdt_blob + gd->fdt_size),
					CONFIG_SYS_CACHELINE_SIZE)));
	if (param.a0 != 0) {
		pr_err("%s failed with: %lx\n", __func__, param.a0);
		return param.a0;
	}
	return 0;
}

int smc_tee_rkp_op(int sub_cmd, void *input, size_t input_size, void *output)
{
	struct arm_smccc_res param = { 0 };
	uint8_t *buffer;

	arm_smccc_smc(OPTEE_SMC_CRYPT, TEESMC_PROBE_SHM_BASE, 0, 0, 0, 0, 0, 0,
		      &param);
	if (param.a0 != 0) {
		printf("smc tee probe share memory base failed\n ");
		return -1;
	}

	buffer = (uint8_t *)param.a1;
	memcpy(buffer, input, input_size);

	flush_cache((ulong)buffer, ALIGN(input_size, CACHE_LINE_SIZE));

	memset(&param, 0, sizeof(param));
	arm_smccc_smc(OPTEE_SMC_SUNXI_RKP, sub_cmd, (unsigned long)buffer, 0, 0,
		      0, 0, 0, &param);

	if (param.a0 > 0x80000000) {
		printf("%s failed with: %ld\n", __func__, param.a0);

		return param.a0;
	} else {
		if (param.a0 && output) {
			memcpy(output, buffer, param.a0);
		}
		return 0;
	}

	return 0;
}
