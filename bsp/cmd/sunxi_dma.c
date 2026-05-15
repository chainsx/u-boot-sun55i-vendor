/* SPDX-License-Identifier: GPL-2.0+
 * Copyright (C) 2023 Allwinnertech
*/

#include <common.h>
#include <command.h>
#include <malloc.h>
#include <asm/arch/dma.h>
#include <dma-uclass.h>
#include <cpu_func.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <dm/device-internal.h>
#include <dm/root.h>
#include <dma.h>

static void sunxi_dma_isr(void *p_arg)
{
	printf("dma int occur\n");
}

struct dma dma_test;

static void pkt_hex_dump(char *pkt, unsigned int len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (!(i % 16))
			printf("\n");

		printf(" %02x", pkt[i]);

	}
	printf("\n");
}

static int do_sunxi_dma_test(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	unsigned int src_addr = 0, dst_addr = 0;
	sunxi_dma_set dma_set;
	uint st = 0, ret;
	ulong timeout;
	struct uclass *uc;
	struct udevice *dev;

	src_addr = (unsigned int)memalign(CONFIG_SYS_CACHELINE_SIZE, 256);
	dst_addr = (unsigned int)memalign(CONFIG_SYS_CACHELINE_SIZE, 256);
	memset((void *)src_addr, 0xff, 256);
	memset((void *)dst_addr, 0x00, 256);
	pkt_hex_dump((void *)src_addr, 256);
	pkt_hex_dump((void *)dst_addr, 256);
	printf("src:0x%08x ====> dst:0x%08x\n", src_addr, dst_addr);

	ret = uclass_get(UCLASS_DMA, &uc);
	if (ret) {
		printf("uclass get fail. \n");
		return -1;
	}

	uclass_foreach_dev(dev, uc) {
		ret = device_probe(dev);
		if (ret) {
			printf("uclass probe fail. \n");
		}
	}

	/* dma */
	dma_set.loop_mode = 0;
	dma_set.wait_cyc  = 8;
	dma_set.iospeed = false;
	/* channal config (from dram to dram)*/
	dma_set.channal_cfg.src_drq_type     = DMAC_CFG_TYPE_DRAM ;  //dram
	dma_set.channal_cfg.src_addr_mode    = DMAC_CFG_DEST_ADDR_TYPE_LINEAR_MODE;
	dma_set.channal_cfg.src_burst_length = DMAC_CFG_SRC_1_BURST;
	dma_set.channal_cfg.src_data_width   = DMAC_CFG_SRC_DATA_WIDTH_32BIT;
	dma_set.channal_cfg.reserved0        = 0;

	dma_set.channal_cfg.dst_drq_type     = DMAC_CFG_TYPE_DRAM;  //dram
	dma_set.channal_cfg.dst_burst_length = DMAC_CFG_DEST_1_BURST;
	dma_set.channal_cfg.dst_addr_mode    = DMAC_CFG_DEST_ADDR_TYPE_LINEAR_MODE;
	dma_set.channal_cfg.dst_data_width   = DMAC_CFG_DEST_DATA_WIDTH_32BIT;
	dma_set.channal_cfg.reserved1        = 0;

	flush_cache(src_addr, 256);
	flush_cache(dst_addr, 256);

	ret = sunxi_dma_request(&dma_test);
	if (ret)
		printf("can't request dma\n");

	sunxi_dma_install_int(dma_test.id, sunxi_dma_isr, NULL);
	sunxi_dma_enable(&dma_test);
	sunxi_dma_setting(dma_test.id, &dma_set);
	sunxi_dma_start(dma_test.id, src_addr, dst_addr, 256);

	/* timeout : 1000 ms */
	timeout = get_timer(0);
	st = sunxi_dma_querystatus(dma_test.id);

	while ((get_timer(timeout) < 1000) && st)
		st = sunxi_dma_querystatus(dma_test.id);
	if (st) {
		printf("wait dma timeout!\n");
	}

	sunxi_dma_stop(dma_test.id);

	ret = sunxi_dma_free(&dma_test);
	if (ret)
		printf("can't free dma\n");

	ret = device_remove(dev, DM_REMOVE_NORMAL);
	if (ret) {
		printf("uclass remove fail. \n");
	}

	pkt_hex_dump((void *)src_addr, 256);
	pkt_hex_dump((void *)dst_addr, 256);

	kfree((void *)src_addr);
	kfree((void *)dst_addr);
	return 0;
}

U_BOOT_CMD(
	sunxi_dma,	3,	1,	do_sunxi_dma_test,
	"do dma test",
	"sunxi_dma src_addr dst_addr"
);
