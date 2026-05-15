/* SPDX-License-Identifier: GPL-2.0+
 * Copyright (C) 2023 Allwinnertech
*/

#include <common.h>
#include <malloc.h>
#include <asm/arch/dma.h>
#include <asm/arch/gic.h>
#include <asm/arch/clock.h>
#include <asm/io.h>
#include <cpu_func.h>
#include <memalign.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <dm/devres.h>
#include <dma.h>
#include <dma-uclass.h>
#include <reset.h>
#include <linux/delay.h>
#include <linux/errno.h>


#ifdef CONFIG_MACH_SUN8IW18
#define SUNXI_DMA_MAX     10
#elif CONFIG_MACH_SUN60IW2
#define SUNXI_DMA_MAX	  16
#else
#define SUNXI_DMA_MAX     4
#endif

#define DMA_VERSION_REG_V2 0x30003

static int dma_int_cnt;
static int dma_init = -1;
static sunxi_dma_source   dma_channal_source[SUNXI_DMA_MAX];
sunxi_dma_reg  *dma_reg;
struct sunxi_dma_dev  *sdev;
struct udevice *sdma_dev;

static void sunxi_irq_enable(struct sunxi_dma_dev *sdev, u32 chan_num, u32 irq_val)
{
	writel(irq_val, sdev->base + DMA_IRQ_EN(chan_num));
}

static u32 sunxi_read_irq_enable(struct sunxi_dma_dev *sdev, u32 chan_num)
{
	u32 reg_val;

	reg_val = readl(sdev->base + DMA_IRQ_EN(chan_num));

	return reg_val;
}

static u32 sunxi_get_irq_status(struct sunxi_dma_dev *sdev, u32 chan_num)
{
	u32 status;

	status = readl(sdev->base + DMA_IRQ_STAT(chan_num));

	return status;
}

static void sunxi_clear_irq_status(struct sunxi_dma_dev *sdev, u32 chan_num, u32 status)
{
	writel(status, sdev->base + DMA_IRQ_STAT(chan_num));
}

static void sunxi_set_clock_autogate(struct sunxi_dma_dev *sdev, u32 reg_val)
{
	writel(reg_val, sdev->base + DMA_AUTO_GATE);
}

static void sunxi_dma_reg_func(void *p)
{
	int i;
	uint pending;
	unsigned int ver_reg;
	sunxi_dma_reg *dma_reg = (sunxi_dma_reg *)SUNXI_DMA_BASE;

	ver_reg = readl(&dma_reg->version);

	if (ver_reg < DMA_VERSION_REG_V2) {
		for (i = 0; i < 8 && i < SUNXI_DMA_MAX; i++) {
			pending = (DMA_PKG_END_INT << (i * 4));
			if (readl(&dma_reg->irq_pending0) & pending) {
				writel(pending, &dma_reg->irq_pending0);
				if (dma_channal_source[i].dma_func.m_func != NULL)
					dma_channal_source[i].dma_func.m_func(dma_channal_source[i].dma_func.m_data);
			}
		}
		for (i = 8; i < SUNXI_DMA_MAX; i++) {
			pending = (DMA_PKG_END_INT << ((i - 8) * 4));
			if (readl(&dma_reg->irq_pending1) & pending) {
				writel(pending, &dma_reg->irq_pending1);
				if (dma_channal_source[i].dma_func.m_func != NULL)
					dma_channal_source[i].dma_func.m_func(dma_channal_source[i].dma_func.m_data);
			}
		}
	} else if (ver_reg >= DMA_VERSION_REG_V2) {
#if defined(DMA_VERSION_V2)
		for (i = 0; i < SUNXI_DMA_MAX; i++) {
			if (readl(&(dma_reg->channal[i].irq_pend_reg)) & DMA_PKG_END_INT) {
				writel(DMA_PKG_END_INT, &(dma_reg->channal[i].irq_pend_reg));
				if (dma_channal_source[i].dma_func.m_func != NULL)
					dma_channal_source[i].dma_func.m_func(dma_channal_source[i].dma_func.m_data);
			}
		}
#endif
	}
}

int sunxi_dma_probe(struct udevice *dev)
{
	int i, ret;
	unsigned int ver_reg;
	fdt_addr_t dma_addr;

	if (dma_init > 0)
		return -1;

	sdev = dev_get_priv(dev);

	dma_addr = dev_read_addr(dev);

	if (dma_addr == FDT_ADDR_T_NONE)
		return -EINVAL;

	sdev->base = (void __iomem *)dma_addr;

	sdma_dev = dev;

	dma_reg = (sunxi_dma_reg *)dma_addr;

	sdev->cfg  = (void *)dev_get_driver_data(dev);

	ret = reset_get_by_index(dev, 0, &sdev->reset);
	if (ret)
		return ret;

	reset_assert(&sdev->reset);
	udelay(20);
	reset_deassert(&sdev->reset);

	ret = clk_get_by_name(dev, "bus", &sdev->clk_bus);
	if (ret)
		return ret;

	ret = clk_get_by_name(dev, "mbus", &sdev->clk_mbus);
	if (ret)
		return ret;

	ret = clk_enable(&sdev->clk_bus);
	if (ret)
		goto free_clk_bus;

	ret = clk_enable(&sdev->clk_mbus);
	if (ret)
		goto free_clk_mbus;

	ver_reg = readl(&dma_reg->version);
	if (ver_reg == 0) {
		pr_debug("The sunxi dma version = 0, please check the error!\n");
		return -1;
	}

	if (ver_reg < DMA_VERSION_REG_V2) {
		writel(0, &dma_reg->irq_en0);
		writel(0, &dma_reg->irq_en1);

		writel(0xffffffff, &dma_reg->irq_pending0);
		writel(0xffffffff, &dma_reg->irq_pending1);
	}

	/*auto MCLK gating disable*/
	clrsetbits_le32(&dma_reg->auto_gate, 0x7 << 0, 0x7 << 0);

	memset((void *)dma_channal_source, 0, SUNXI_DMA_MAX * sizeof(struct sunxi_dma_source_t));

	for (i = 0; i < SUNXI_DMA_MAX; i++) {
#if defined(DMA_VERSION_V2)
		if (ver_reg >= DMA_VERSION_REG_V2) {
			writel(0, &(dma_reg->channal[i].irq_en_reg));
			writel(0xf, &(dma_reg->channal[i].irq_pend_reg));
		}
#endif

		dma_channal_source[i].used = 0;
		dma_channal_source[i].channal = &(dma_reg->channal[i]);
		dma_channal_source[i].desc  =
			(sunxi_dma_desc *)malloc_cache_aligned(sizeof(sunxi_dma_desc));
	}

	irq_install_handler(AW_IRQ_DMA, sunxi_dma_reg_func, 0);

	dma_int_cnt = 0;
	dma_init = 1;

	return 0;

free_clk_mbus:
	clk_free(&sdev->clk_mbus);

free_clk_bus:
	clk_free(&sdev->clk_bus);

	return ret;
}

ulong sunxi_dma_request_from_last(uint dmatype)
{
	int   i;

	for (i = SUNXI_DMA_MAX - 1; i >= 0; i--) {
		if (dma_channal_source[i].used == 0) {
			dma_channal_source[i].used = 1;
			dma_channal_source[i].channal_count = i;
			return (ulong)&dma_channal_source[i];
		}
	}

	return 0;
}

int sunxi_dma_request(struct dma *dma)
{
	int   i;

	for (i = 0; i < SUNXI_DMA_MAX; i++) {
		if (dma_channal_source[i].used == 0) {
			dma_channal_source[i].used = 1;
			dma_channal_source[i].channal_count = i;
			dma->id = (ulong)&dma_channal_source[i];
			return 0;
		}
	}

	return 0;
}

int sunxi_dma_free(struct dma *dma)
{
	ulong hdma = dma->id;
	struct sunxi_dma_source_t  *dma_source = (struct sunxi_dma_source_t *)hdma;

	if (!dma_source->used)
		return -1;

	sunxi_dma_disable(dma);
	sunxi_dma_free_int(hdma);

	dma_source->used   = 0;

	return 0;
}


int sunxi_dma_setting(ulong hdma, sunxi_dma_set *cfg)
{

	uint   commit_para;
	sunxi_dma_set     *dma_set = cfg;
	sunxi_dma_source  *dma_source = (sunxi_dma_source *)hdma;
	sunxi_dma_desc    *desc = dma_source->desc;
	uint channal_addr  = (ulong)(&(dma_set->channal_cfg));

	if (!dma_source->used)
		return -1;

	if (dma_set->loop_mode)
		desc->link = (ulong)(&dma_source->desc);
	else
		desc->link = SUNXI_DMA_LINK_NULL;

	commit_para  = (dma_set->wait_cyc & 0xff);
	if (dma_set->iospeed)
		commit_para |= BIT(8);

	writel(commit_para, &desc->commit_para);
	writel(readl((volatile void __iomem *)(ulong)channal_addr), &desc->config);

	pr_debug("config [%x] commit_para[%x] \n", readl(&desc->config), readl(&desc->commit_para));

	return 0;
}

int sunxi_dma_start(ulong hdma, uint saddr, uint daddr, uint bytes)
{
	sunxi_dma_source  	  *dma_source = (sunxi_dma_source *)hdma;
	sunxi_dma_channal_reg *channal = dma_source->channal;
	sunxi_dma_desc    *desc = dma_source->desc;

	if (!dma_source->used)
		return -1;

	/*config desc */
	writel(saddr, &desc->source_addr);
	writel(daddr, &desc->dest_addr);
	writel(bytes, &desc->byte_count);

	flush_cache((ulong)desc,
		    ALIGN(sizeof(sunxi_dma_desc), CONFIG_SYS_CACHELINE_SIZE));

	pr_debug("source_addr [%x] dest_addr [%x]  byte_count [%x]\n", readl(&desc->source_addr), readl(&desc->dest_addr), readl(&desc->byte_count));

	/* start dma */
	writel((ulong)(desc), &channal->desc_addr);
	writel(1, &channal->enable);

#if defined(sunxi_boot_dma_debug)

	sunxi_dma_reg *const dma_reg = (sunxi_dma_reg *)SUNXI_DMA_BASE;
	unsigned int ver_reg;
	int i = 0;

	ver_reg = readl(&dma_reg->version);

	if (ver_reg < DMA_VERSION_REG_V2) {
		for (i = 0; i < 1; i++) {

			pr_debug("desc [%p], channal [%p],  dma_reg [%p], auto_gate addr [%p], desc_addr addr [%p] \n \
					irq_en0 [%x], irq_pending0 [%x], auto_gate [%x], status [%x] \n  \
					enable [%x], desc_addr [%x], config [%x], cur_src_addr [%x]   \n  \
					cur_dst_addr [%x], left_bytes [%x], parameters [%x], mode [%x]\n", \
					desc,  channal, dma_reg, &dma_reg->auto_gate, &channal->desc_addr, \
					readl(&dma_reg->irq_en0), readl(&dma_reg->irq_pending0), readl(&dma_reg->auto_gate), readl(&dma_reg->status), \
					readl(&channal->enable), readl(&channal->desc_addr), readl(&channal->config), readl(&channal->cur_src_addr), \
					readl(&channal->cur_dst_addr), readl(&channal->left_bytes), readl(&channal->parameters), readl(&channal->mode));

			mdelay(200);
		}
	} else if (ver_reg >= DMA_VERSION_REG_V2) {
#if defined(DMA_VERSION_V2)
		for (i = 0; i < 5; i++) {

			pr_debug("desc [%p], channal [%p],  dma_reg [%p], auto_gate addr [%p], desc_addr addr [%p] \n \
					irq_en_reg [%x], irq_pend_reg [%x], auto_gate [%x], status [%x] \n  \
					enable [%x], desc_addr [%x], config [%x], cur_src_addr [%x]   \n  \
					cur_dst_addr [%x], left_bytes [%x], parameters [%x], mode [%x]\n", \
					desc,  channal, dma_reg, &dma_reg->auto_gate, &channal->desc_addr, \
					readl(&channal->irq_en_reg), readl(&channal->irq_pend_reg), readl(&dma_reg->auto_gate), readl(&dma_reg->status), \
					readl(&channal->enable), readl(&channal->desc_addr), readl(&channal->config), readl(&channal->cur_src_addr), \
					readl(&channal->cur_dst_addr), readl(&channal->left_bytes), readl(&channal->parameters), readl(&channal->mode));

			mdelay(200);
		}
#endif
	}
#endif

	return 0;
}

int sunxi_dma_stop(ulong hdma)
{
	sunxi_dma_source *dma_source = (sunxi_dma_source *)hdma;
	sunxi_dma_channal_reg *channal = dma_source->channal;

	if (!dma_source->used)
		return -1;
	writel(0, &channal->enable);

	pr_debug("dma stop\n");

	return 0;
}

int sunxi_dma_querystatus(ulong hdma)
{
	uint  channal_count;
	sunxi_dma_source *dma_source = (sunxi_dma_source *)hdma;
	sunxi_dma_reg    *dma_reg = (sunxi_dma_reg *)SUNXI_DMA_BASE;

	if (!dma_source->used)
		return -1;

	channal_count = dma_source->channal_count;

	return (readl(&dma_reg->status) >> channal_count) & 0x01;
}

int sunxi_dma_install_int(ulong hdma, interrupt_handler_t dma_reg_func, void *p)
{
	sunxi_dma_source     *dma_channal = (sunxi_dma_source *)hdma;
	sunxi_dma_reg    *dma_reg  = (sunxi_dma_reg *)SUNXI_DMA_BASE;
	uint  channal_count;
	unsigned int ver_reg;

	if (!dma_channal->used)
		return -1;

	ver_reg = readl(&dma_reg->version);

	channal_count = dma_channal->channal_count;

	if (ver_reg < DMA_VERSION_REG_V2) {
		if (channal_count < 8)
			writel((7 << channal_count * 4), &dma_reg->irq_pending0);
		else
			writel((7 << (channal_count - 8) * 4), &dma_reg->irq_pending1);
	} else if (ver_reg >= DMA_VERSION_REG_V2) {
#if defined(DMA_VERSION_V2)
		sunxi_dma_channal_reg *channal = dma_channal->channal;
		writel(7, &channal->irq_pend_reg);
#endif
	}

	if (!dma_channal->dma_func.m_func) {
		dma_channal->dma_func.m_func = dma_reg_func;
		dma_channal->dma_func.m_data = p;
	} else {
		pr_debug("dma 0x%lx int is used already, you have to free it first\n", hdma);
	}

	return 0;
}

int sunxi_dma_enable(struct dma *dma)
{
	ulong hdma = dma->id;
	sunxi_dma_source     *dma_channal = (sunxi_dma_source *)hdma;
	sunxi_dma_reg    *dma_status  = (sunxi_dma_reg *)SUNXI_DMA_BASE;
	uint  channal_count;
	unsigned int ver_reg;

	if (!dma_channal->used)
		return -1;

	ver_reg = readl(&dma_status->version);

	channal_count = dma_channal->channal_count;

	if (ver_reg < DMA_VERSION_REG_V2) {
		if (channal_count < 8) {
			if (readl(&dma_status->irq_en0) & (DMA_PKG_END_INT << channal_count * 4)) {
				pr_debug("dma 0x%lx int is avaible already\n", hdma);
				return 0;
			}
			setbits_le32(&dma_status->irq_en0, (DMA_PKG_END_INT << channal_count * 4));
		} else {
			if (readl(&dma_status->irq_en1) & (DMA_PKG_END_INT << (channal_count - 8) * 4)) {
				pr_debug("dma 0x%lx int is avaible already\n", hdma);
				return 0;
			}
			setbits_le32(&dma_status->irq_en1, (DMA_PKG_END_INT << (channal_count - 8) * 4));
		}
	}

	if (ver_reg >= DMA_VERSION_REG_V2) {
#if defined(DMA_VERSION_V2)
		sunxi_dma_channal_reg *channal = dma_channal->channal;
		if (readl(&channal->irq_en_reg) & DMA_PKG_END_INT) {
			pr_debug("dma 0x%lx int is avaible already\n", hdma);
			return 0;
		}
		setbits_le32(&channal->irq_en_reg, DMA_PKG_END_INT);
#endif
	}

	if (!dma_int_cnt)
		irq_enable(AW_IRQ_DMA);

	dma_int_cnt++;

	return 0;
}

int sunxi_dma_disable(struct dma *dma)
{
	ulong hdma = dma->id;
	sunxi_dma_source     *dma_channal = (sunxi_dma_source *)hdma;
	sunxi_dma_reg    *dma_reg  = (sunxi_dma_reg *)SUNXI_DMA_BASE;
	uint  channal_count;
	unsigned int ver_reg;

	if (!dma_channal->used)
		return -1;

	ver_reg = readl(&dma_reg->version);

	channal_count = dma_channal->channal_count;

	if (ver_reg < DMA_VERSION_REG_V2) {
		if (channal_count < 8) {
			if (!(readl(&dma_reg->irq_en0) & (DMA_PKG_END_INT << channal_count * 4))) {
				pr_debug("dma 0x%lx int is not used yet\n", hdma);
				return 0;
			}
			clrbits_le32(&dma_reg->irq_en0, (DMA_PKG_END_INT << channal_count * 4));
		} else {
			if (!(readl((volatile void __iomem *)(ulong)dma_reg->irq_en1) & (DMA_PKG_END_INT << (channal_count - 8) * 4))) {
				pr_debug("dma 0x%lx int is not used yet\n", hdma);
				return 0;
			}
			clrbits_le32(&dma_reg->irq_en1, (DMA_PKG_END_INT << (channal_count - 8) * 4));
		}
	}

	if (ver_reg >= DMA_VERSION_REG_V2) {
#if defined(DMA_VERSION_V2)
		sunxi_dma_channal_reg *channal = dma_channal->channal;
		if (!(readl(&channal->irq_en_reg) & DMA_PKG_END_INT)) {
			pr_debug("dma 0x%lx int is not used yet!\n", hdma);
			return 0;
		}
		clrbits_le32(&channal->irq_en_reg, DMA_PKG_END_INT);
#endif
	}

	//disable golbal int
	if (dma_int_cnt > 0)
		dma_int_cnt--;

	if (!dma_int_cnt)
		irq_disable(AW_IRQ_DMA);

	return 0;
}

int sunxi_dma_free_int(ulong hdma)
{
	sunxi_dma_source     *dma_channal = (sunxi_dma_source *)hdma;
	sunxi_dma_reg    *dma_status  = (sunxi_dma_reg *)SUNXI_DMA_BASE;
	sunxi_dma_reg *const dma_reg = (sunxi_dma_reg *)SUNXI_DMA_BASE;
	uint  channal_count;
	unsigned int ver_reg;

	if (!dma_channal->used)
		return -1;

	ver_reg = readl(&dma_reg->version);

	channal_count = dma_channal->channal_count;

	if (ver_reg < DMA_VERSION_REG_V2) {
		if (channal_count < 8)
			writel((7 << channal_count), &dma_status->irq_pending0);
		else
			writel((7 << (channal_count - 8)), &dma_status->irq_pending1);
	} else if (ver_reg >= DMA_VERSION_REG_V2) {
#if defined(DMA_VERSION_V2)
		sunxi_dma_channal_reg *channal = dma_channal->channal;
		writel(0, &channal->irq_pend_reg);
#endif
	}

	if (dma_channal->dma_func.m_func) {
		dma_channal->dma_func.m_func = NULL;
		dma_channal->dma_func.m_data = NULL;
	} else {
		pr_debug("dma 0x%lx int is free, you do not need to free it again\n", hdma);
		return -1;
	}

	return 0;
}

int sunxi_dma_remove(struct udevice *dev)
{
	int i;
	unsigned int ver_reg;
	sunxi_dma_reg *dma_reg = (sunxi_dma_reg *)SUNXI_DMA_BASE;
	struct sunxi_ccm_reg *const ccm =
		(struct sunxi_ccm_reg *)SUNXI_CCM_BASE;

	ver_reg = readl(&dma_reg->version);

	/* free dma channal if other module not free it */
	for (i = 0; i < SUNXI_DMA_MAX; i++) {
		if (dma_channal_source[i].used == 1) {
			struct dma temp;
			temp.id = (ulong)&dma_channal_source[i];
			sunxi_dma_disable(&temp);
			sunxi_dma_free_int(temp.id);
			writel(0, &dma_channal_source[i].channal->enable);
			dma_channal_source[i].used   = 0;
		}
	}
	/* irq disable */
	if (ver_reg < DMA_VERSION_REG_V2) {
		writel(0, &dma_reg->irq_en0);
		writel(0, &dma_reg->irq_en1);

		writel(0xffffffff, &dma_reg->irq_pending0);
		writel(0xffffffff, &dma_reg->irq_pending1);
	} else if (ver_reg >= DMA_VERSION_REG_V2) {
#if defined(DMA_VERSION_V2)
		for (i = 0; i < SUNXI_DMA_MAX; i++) {
			writel(0, &(dma_reg->channal[i].irq_en_reg));
			writel(0xf, &(dma_reg->channal[i].irq_pend_reg));
		}
#endif
	}

	irq_free_handler(AW_IRQ_DMA);

#if defined(CONFIG_SUNXI_VERSION1)
	clrbits_le32(&ccm->ahb_gate0, 1 << AHB_GATE_OFFSET_DMA);
#else
	/* close dma clock when dma exit */
	clrbits_le32(&ccm->dma_gate_reset, 1 << DMA_GATING_OFS | 1 << DMA_RST_OFS);
#endif

	dma_init--;

	pr_debug("dma exit\n");

	return 0;
}

static const struct dma_ops sunxi_dma_ops = {
	.request	= sunxi_dma_request,
	.rfree          = sunxi_dma_free,
	.enable         = sunxi_dma_enable,
	.disable        = sunxi_dma_disable,
};

static struct sunxi_dma_config sunxi_dma_v100 = {
	.irq_enable        = sunxi_irq_enable,
	.get_irq_status    = sunxi_get_irq_status,
	.read_irq_enable   = sunxi_read_irq_enable,
	.clear_irq_status  = sunxi_clear_irq_status,
	.set_auto_gate	   = sunxi_set_clock_autogate,
	.channum_per_reg   = DMA_IRQ_CHAN_NR,
};

static const struct udevice_id sunxi_dma_of_match[] = {
	{ .compatible = "allwinner,dma-v100",  .data = (ulong)&sunxi_dma_v100 },
	{ /* sentinel */ },
};

U_BOOT_DRIVER(sunxi_dma) = {
	.name		= "sunxi_dma",
	.id		= UCLASS_DMA,
	.of_match	= sunxi_dma_of_match,
	.probe		= sunxi_dma_probe,
	.remove		= sunxi_dma_remove,
	.ops		= &sunxi_dma_ops,
	.priv_auto	= sizeof(struct sunxi_dma_dev),
};
