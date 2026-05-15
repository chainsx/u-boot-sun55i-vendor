// SPDX-License-Identifier: GPL-2.0

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <generic-phy.h>
#include <log.h>
#include <malloc.h>
#include <reset.h>
#include <asm/arch/cpu.h>
#include <asm/arch/clock.h>
#include <dm/device_compat.h>
#include <dm/lists.h>
#include <dm/root.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux-compat.h>
#include <sunxi-usb-phy.h>
#include <udc.h>
#include "sunxi_udc_core.h"

/* reg offsets */
#define  USBC_REG_o_ISCR	0x0400
#define  USBC_REG_o_PHYCTL	0x0404
#define  USBC_REG_o_PHYBIST	0x0408
#define  USBC_REG_o_PHYTUNE	0x040c

#define  USBC_REG_o_VEND0	0x0043

/* Interface Status and Control */
#define  USBC_BP_ISCR_VBUS_VALID_FROM_DATA	30
#define  USBC_BP_ISCR_VBUS_VALID_FROM_VBUS	29
#define  USBC_BP_ISCR_EXT_ID_STATUS		28
#define  USBC_BP_ISCR_EXT_DM_STATUS		27
#define  USBC_BP_ISCR_EXT_DP_STATUS		26
#define  USBC_BP_ISCR_MERGED_VBUS_STATUS	25
#define  USBC_BP_ISCR_MERGED_ID_STATUS		24

#define  USBC_BP_ISCR_ID_PULLUP_EN		17
#define  USBC_BP_ISCR_DPDM_PULLUP_EN		16
#define  USBC_BP_ISCR_FORCE_ID			14
#define  USBC_BP_ISCR_FORCE_VBUS_VALID		12
#define  USBC_BP_ISCR_VBUS_VALID_SRC		10

#define  USBC_BP_ISCR_HOSC_EN			7
#define  USBC_BP_ISCR_VBUS_CHANGE_DETECT	6
#define  USBC_BP_ISCR_ID_CHANGE_DETECT		5
#define  USBC_BP_ISCR_DPDM_CHANGE_DETECT	4
#define  USBC_BP_ISCR_IRQ_ENABLE		3
#define  USBC_BP_ISCR_VBUS_CHANGE_DETECT_EN	2
#define  USBC_BP_ISCR_ID_CHANGE_DETECT_EN	1
#define  USBC_BP_ISCR_DPDM_CHANGE_DETECT_EN	0

#ifdef CONFIG_USB_FUNCTION_FASTBOOT
int g_dnl_get_board_bcd_device_number(int gcnum)
{
	return 0x200;
}
#endif

struct sunxi_udc_data {
	struct sunxi_udc *sunxi_udc;
};

struct sunxi_udc_config {
	struct sunxi_udc_hdrc_config *config;
};

struct sunxi_glue {
	struct sunxi_udc_data data;
	struct clk clk;
	struct reset_ctl rst;
	struct sunxi_udc_config *cfg;
	struct device dev;
	struct phy phy;
};
#define to_sunxi_glue(d)	container_of(d, struct sunxi_glue, dev)

static u32 USBC_WakeUp_ClearChangeDetect(u32 reg_val)
{
	u32 temp = reg_val;

	temp &= ~BIT(USBC_BP_ISCR_VBUS_CHANGE_DETECT);
	temp &= ~BIT(USBC_BP_ISCR_ID_CHANGE_DETECT);
	temp &= ~BIT(USBC_BP_ISCR_DPDM_CHANGE_DETECT);

	return temp;
}

static void USBC_EnableIdPullUp(__iomem void *base)
{
	u32 reg_val;

	reg_val = usb_readl(base, USBC_REG_o_ISCR);
	reg_val |= BIT(USBC_BP_ISCR_ID_PULLUP_EN);
	reg_val = USBC_WakeUp_ClearChangeDetect(reg_val);
	usb_writel(base, USBC_REG_o_ISCR, reg_val);
}

static void USBC_EnableDpDmPullUp(__iomem void *base)
{
	u32 reg_val;

	reg_val = usb_readl(base, USBC_REG_o_ISCR);
	reg_val |= BIT(USBC_BP_ISCR_DPDM_PULLUP_EN);
	reg_val = USBC_WakeUp_ClearChangeDetect(reg_val);
	usb_writel(base, USBC_REG_o_ISCR, reg_val);
}

static void USBC_ForceIdToHigh(__iomem void *base)
{
	u32 reg_val;

	reg_val = usb_readl(base, USBC_REG_o_ISCR);
	reg_val &= ~(0x03 << USBC_BP_ISCR_FORCE_ID);
	reg_val |= (0x03 << USBC_BP_ISCR_FORCE_ID);
	reg_val = USBC_WakeUp_ClearChangeDetect(reg_val);
	usb_writel(base, USBC_REG_o_ISCR, reg_val);
}

static void USBC_ForceVbusValidToLow(__iomem void *base)
{
	u32 reg_val;

	reg_val = usb_readl(base, USBC_REG_o_ISCR);
	reg_val &= ~(0x03 << USBC_BP_ISCR_FORCE_VBUS_VALID);
	reg_val |= (0x02 << USBC_BP_ISCR_FORCE_VBUS_VALID);
	reg_val = USBC_WakeUp_ClearChangeDetect(reg_val);
	usb_writel(base, USBC_REG_o_ISCR, reg_val);
}

static void USBC_ForceVbusValidToHigh(__iomem void *base)
{
	u32 reg_val;

	reg_val = usb_readl(base, USBC_REG_o_ISCR);
	reg_val &= ~(0x03 << USBC_BP_ISCR_FORCE_VBUS_VALID);
	reg_val |= (0x03 << USBC_BP_ISCR_FORCE_VBUS_VALID);
	reg_val = USBC_WakeUp_ClearChangeDetect(reg_val);
	usb_writel(base, USBC_REG_o_ISCR, reg_val);
}

#if 0
 /* do not need: */

static void USBC_ConfigFIFO_Base(void)
{
	u32 reg_value;

	/* config usb fifo, 8kb mode */
	reg_value = readl(SUNXI_SRAMC_BASE + 0x04);
	reg_value &= ~(0x03 << 0);
	reg_value |= BIT(0);
	writel(reg_value, SUNXI_SRAMC_BASE + 0x04);
}
#endif

static u8 last_int_usb;

bool dfu_usb_get_reset(void)
{
	return !!(last_int_usb & USB_INTR_RESET);
}

static irqreturn_t sunxi_udc_interrupt(int irq, void *__hci)
{
	struct sunxi_udc	*sunxi_udc = __hci;
	irqreturn_t		retval = IRQ_NONE;

	/* read and flush interrupts */
	sunxi_udc->int_usb = usb_readb(sunxi_udc->regs, USB_INTRUSB);
	last_int_usb = sunxi_udc->int_usb;
	if (sunxi_udc->int_usb)
		usb_writeb(sunxi_udc->regs, USB_INTRUSB, sunxi_udc->int_usb);
	sunxi_udc->int_tx = usb_readw(sunxi_udc->regs, USB_INTRTX);
	if (sunxi_udc->int_tx)
		usb_writew(sunxi_udc->regs, USB_INTRTX, sunxi_udc->int_tx);
	sunxi_udc->int_rx = usb_readw(sunxi_udc->regs, USB_INTRRX);
	if (sunxi_udc->int_rx)
		usb_writew(sunxi_udc->regs, USB_INTRRX, sunxi_udc->int_rx);

	if ((sunxi_udc->int_usb) || sunxi_udc->int_tx || sunxi_udc->int_rx)
		retval |= udc_interrupt(sunxi_udc);

	return retval;
}

static bool enabled;

static int sunxi_udc_enable(struct sunxi_udc *sunxi_udc)
{
	struct sunxi_glue *glue = to_sunxi_glue(sunxi_udc->controller);
	int ret;

	pr_debug("%s():\n", __func__);

	sunxi_udc_ep_select(sunxi_udc->regs, 0);
	usb_writeb(sunxi_udc->regs, USB_FADDR, 0);

	if (enabled)
		return 0;

	/* select PIO mode */
	usb_writeb(sunxi_udc->regs, USBC_REG_o_VEND0, 0);

	if (is_host_enabled(sunxi_udc)) {
		ret = sunxi_usb_phy_id_detect(&glue->phy);
		if (ret == 1) {
			printf("No host cable detected: ");
			return -ENODEV;
		}

		ret = generic_phy_power_on(&glue->phy);
		if (ret) {
			pr_debug("failed to power on USB PHY\n");
			return ret;
		}
	}

	USBC_ForceVbusValidToHigh(sunxi_udc->regs);

	enabled = true;
	return 0;
}

static void sunxi_udc_disable(struct sunxi_udc *sunxi_udc)
{
	struct sunxi_glue *glue = to_sunxi_glue(sunxi_udc->controller);
	int ret;

	pr_debug("%s():\n", __func__);

	if (!enabled)
		return;

	if (is_host_enabled(sunxi_udc)) {
		ret = generic_phy_power_off(&glue->phy);
		if (ret) {
			pr_debug("failed to power off USB PHY\n");
			return;
		}
	}

	USBC_ForceVbusValidToLow(sunxi_udc->regs);
	/* wait for timeout */
	mdelay(200);

	enabled = false;
}

static int sunxi_udc_init(struct sunxi_udc *sunxi_udc)
{
	struct sunxi_glue *glue = to_sunxi_glue(sunxi_udc->controller);
	int ret;

	pr_debug("%s():\n", __func__);

	ret = clk_enable(&glue->clk);
	if (ret) {
		dev_err(sunxi_udc->controller, "failed to enable clock: %p\n", &glue->clk);
		return ret;
	}

	if (reset_valid(&glue->rst)) {
		ret = reset_deassert(&glue->rst);
		if (ret) {
			dev_err(sunxi_udc->controller, "failed to deassert reset\n");
			goto err_clk;
		}
	}

	ret = generic_phy_init(&glue->phy);
	if (ret) {
		dev_dbg(sunxi_udc->controller, "failed to init USB PHY\n");
		goto err_rst;
	}

	sunxi_udc->isr = sunxi_udc_interrupt;

	//USBC_ConfigFIFO_Base();
	USBC_EnableDpDmPullUp(sunxi_udc->regs);
	USBC_EnableIdPullUp(sunxi_udc->regs);
	USBC_ForceIdToHigh(sunxi_udc->regs);

	USBC_ForceVbusValidToHigh(sunxi_udc->regs);

	return 0;

err_rst:
	if (reset_valid(&glue->rst))
		reset_assert(&glue->rst);
err_clk:
	clk_disable(&glue->clk);
	return ret;
}

static int sunxi_udc_exit(struct sunxi_udc *sunxi_udc)
{
	struct sunxi_glue *glue = to_sunxi_glue(sunxi_udc->controller);
	int ret = 0;

	if (generic_phy_valid(&glue->phy)) {
		ret = generic_phy_exit(&glue->phy);
		if (ret) {
			dev_dbg(sunxi_udc->controller,
				"failed to power off usb phy\n");
			return ret;
		}
	}

	if (reset_valid(&glue->rst))
		reset_assert(&glue->rst);
	clk_disable(&glue->clk);

	return 0;
}

static void sunxi_udc_pre_root_reset_end(struct sunxi_udc *sunxi_udc)
{
	struct sunxi_glue *glue = to_sunxi_glue(sunxi_udc->controller);

	sunxi_usb_phy_set_squelch_detect(&glue->phy, false);
}

static void sunxi_udc_post_root_reset_end(struct sunxi_udc *sunxi_udc)
{
	struct sunxi_glue *glue = to_sunxi_glue(sunxi_udc->controller);

	sunxi_usb_phy_set_squelch_detect(&glue->phy, true);
}

static const struct sunxi_udc_platform_ops sunxi_udc_ops = {
	.init			= sunxi_udc_init,
	.exit			= sunxi_udc_exit,
	.enable			= sunxi_udc_enable,
	.disable		= sunxi_udc_disable,
	.pre_root_reset_end 	= sunxi_udc_pre_root_reset_end,
	.post_root_reset_end 	= sunxi_udc_post_root_reset_end,
};

/* Allwinner OTG supports up to 5 endpoints */
#define SUNXI_USB_MAX_EP_NUM		6
#define SUNXI_USB_RAM_BITS		11

/* The max size of FIFO is 8K
 * Please refer to the spec for the FIFO size of different IC.
*/
static struct sunxi_udc_fifo_cfg sunxi_udc_mode_cfg[] = {
	USB_EP_FIFO_SINGLE(1, FIFO_TX, 512),
	USB_EP_FIFO_SINGLE(1, FIFO_RX, 512),
	USB_EP_FIFO_SINGLE(2, FIFO_TX, 512),
	USB_EP_FIFO_SINGLE(2, FIFO_RX, 512),
	USB_EP_FIFO_SINGLE(3, FIFO_TX, 512),
	USB_EP_FIFO_SINGLE(3, FIFO_RX, 512),
	USB_EP_FIFO_SINGLE(4, FIFO_TX, 512),
	USB_EP_FIFO_SINGLE(4, FIFO_RX, 512),
	USB_EP_FIFO_SINGLE(5, FIFO_TX, 512),
	USB_EP_FIFO_SINGLE(5, FIFO_RX, 512),
};

static struct sunxi_udc_hdrc_config sunxi_udc_config = {
	.fifo_cfg       = sunxi_udc_mode_cfg,
	.fifo_cfg_size  = ARRAY_SIZE(sunxi_udc_mode_cfg),
	.multipoint	= true,
	.dyn_fifo	= true,
	.num_eps	= SUNXI_USB_MAX_EP_NUM,
	.ram_bits	= SUNXI_USB_RAM_BITS,
};

int dm_usb_gadget_handle_interrupts(struct udevice *dev)
{
	struct sunxi_glue *glue = dev_get_priv(dev);
	struct sunxi_udc_data *gadget = &glue->data;

	return gadget->sunxi_udc->isr(0, gadget->sunxi_udc);
}

struct sunxi_udc *sunxi_udc_register(struct sunxi_udc_hdrc_platform_data *plat, void *bdata,
			   void *ctl_regs)
{
	struct sunxi_udc *sunxi_udcp;

	sunxi_udcp = sunxi_udc_init_controller(plat, (struct device *)bdata, ctl_regs);
	if (IS_ERR(sunxi_udcp)) {
		printf("Failed to init the controller\n");
		return ERR_CAST(sunxi_udcp);
	}

	return sunxi_udcp;
}

static int sunxi_udc_usb_probe(struct udevice *dev)
{
	struct sunxi_glue *glue = dev_get_priv(dev);
	struct sunxi_udc_data *udc_data = &glue->data;
	struct sunxi_udc_hdrc_platform_data pdata;
	void *base = dev_read_addr_ptr(dev);
	int ret;

	if (!base)
		return -EINVAL;

	glue->cfg = (struct sunxi_udc_config *)dev_get_driver_data(dev);
	if (!glue->cfg)
		return -EINVAL;

	ret = clk_get_by_index(dev, 0, &glue->clk);
	if (ret) {
		dev_err(dev, "failed to get clock\n");
		return ret;
	}

	ret = reset_get_by_index(dev, 0, &glue->rst);
	if (ret && ret != -ENOENT) {
		dev_err(dev, "failed to get reset\n");
		return ret;
	}
	ret = generic_phy_get_by_name(dev, "usb", &glue->phy);
	if (ret) {
		pr_err("failed to get usb PHY\n");
		return ret;
	}

	memset(&pdata, 0, sizeof(pdata));
	pdata.power = 250;
	pdata.platform_ops = &sunxi_udc_ops;
	pdata.config = glue->cfg->config;

	pdata.mode = USB_PERIPHERAL;
	udc_data->sunxi_udc = sunxi_udc_register(&pdata, &glue->dev, base);
	if (!udc_data->sunxi_udc)
		return -EIO;

	printf("Allwinner USB Device Controller Init!\n");

	ret = usb_add_gadget_udc((struct device *)dev, &udc_data->sunxi_udc->g);
	if (ret) {
		pr_err("failed to add gadget udc\n");
		return ret;
	}
	return 0;
}

static int sunxi_udc_usb_remove(struct udevice *dev)
{
	struct sunxi_glue *glue = dev_get_priv(dev);
	struct sunxi_udc_data *udc_data = &glue->data;

	usb_del_gadget_udc(&udc_data->sunxi_udc->g);
	sunxi_udc_stop(udc_data->sunxi_udc);
	free(udc_data->sunxi_udc);
	udc_data->sunxi_udc = NULL;

	return 0;
}

static const struct sunxi_udc_config sunxi_cfg = {
	.config = &sunxi_udc_config,
};

static const struct udevice_id sunxi_udc_ids[] = {
	{ .compatible = "allwinner,sunxi-udc",
			.data = (ulong)&sunxi_cfg },
	{ }
};

U_BOOT_DRIVER(usb_sunxi_udc) = {
	.name		= "sunxi_udc",
	.id		= UCLASS_USB_GADGET_GENERIC,
	.of_match	= sunxi_udc_ids,
	.probe		= sunxi_udc_usb_probe,
	.remove		= sunxi_udc_usb_remove,
	.plat_auto	= sizeof(struct usb_plat),
	.priv_auto	= sizeof(struct sunxi_glue),
};
