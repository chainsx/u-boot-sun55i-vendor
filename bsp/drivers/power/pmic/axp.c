// SPDX-License-Identifier: GPL-2.0+

#include <axp_pmic.h>
#include <dm.h>
#include <dm/lists.h>
#include <i2c.h>
#include <power/pmic.h>
#include <sysreset.h>

int _axp_set_power_off(struct udevice *dev)
{
	int ret, id;

	id = dev_get_driver_data(dev);
	switch (id) {
	case AXP152_ID:
		ret = pmic_clrsetbits(dev, AXP152_SHUTDOWN, 0, AXP152_POWEROFF);
		if (ret < 0)
			return ret;
		asm volatile("b .");
		break;
	case AXP2202_ID:
		ret = pmic_clrsetbits(dev, AXP2202_OFF_CTL, AXP2202_POWEROFF, AXP2202_POWEROFF);
		if (ret < 0)
			return ret;
		asm volatile("b .");
		break;
	case AXP2101_ID:
		ret = pmic_clrsetbits(dev, AXP2101_OFF_CTL, AXP2101_POWEROFF, AXP2101_POWEROFF);
		if (ret < 0)
			return ret;
		asm volatile("b .");
		break;
	default:
		pr_notice("no axp poweroff function");
		return -EPROTONOSUPPORT;
	}

	return 0;
}

int axp_set_power_off(void)
{
	int ret, i;
	struct uclass *uc;
	struct udevice *dev;
	bool no_power_off;

	ret = uclass_get(UCLASS_PMIC, &uc);
	if (ret)
		return ret;

	for (i = 0; ; i++) {
		ret = uclass_get_device_by_seq(UCLASS_PMIC, i, &dev);
		if (ret == -ENODEV)
			break;
		no_power_off = dev_read_bool(dev, "axp-poweroff-disable");
		if (no_power_off)
			continue;
		ret = _axp_set_power_off(dev);
	}
	return ret;
}

#if CONFIG_IS_ENABLED(SYSRESET)
static int axp_sysreset_request(struct udevice *dev, enum sysreset_t type)
{
	if (type != SYSRESET_POWER_OFF)
		return -EPROTONOSUPPORT;

	return axp_set_power_off();
}

static struct sysreset_ops axp_sysreset_ops = {
	.request	= axp_sysreset_request,
};

U_BOOT_DRIVER(axp_sysreset) = {
	.name		= "axp_sysreset",
	.id		= UCLASS_SYSRESET,
	.ops		= &axp_sysreset_ops,
};
#endif

static int axp_pmic_reg_count(struct udevice *dev)
{
	/* TODO: Get the specific value from driver data. */
	return 0x100;
}

static struct dm_pmic_ops axp_pmic_ops = {
	.reg_count	= axp_pmic_reg_count,
	.read		= dm_i2c_read,
	.write		= dm_i2c_write,
};

int pmu_axp2202_reg_debug(struct udevice *dev)
{
	pr_notice("[AXP2202] comm status : 0x%x = 0x%x, 0x%x = 0x%x\n",
				AXP2202_COMM_STATUS0, pmic_reg_read(dev, AXP2202_COMM_STATUS0),
				AXP2202_MODE_CHGSTATUS, pmic_reg_read(dev, AXP2202_MODE_CHGSTATUS));
	pr_notice("[AXP2202] onoff status : 0x%x = 0x%x, 0x%x = 0x%x\n",
				AXP2202_PWRON_STATUS, pmic_reg_read(dev, AXP2202_PWRON_STATUS),
				AXP2202_PWROFF_STATUS, pmic_reg_read(dev, AXP2202_PWROFF_STATUS));
	pr_notice("[[AXP2202] reboot/charge status: 0x%x = 0x%x\n",
				AXP2202_DATA_BUFFER3, pmic_reg_read(dev, AXP2202_DATA_BUFFER3));
	pr_notice("[[AXP2202] reboot/charge status: 0x%x = 0x%x\n",
				AXP2202_IIN_LIM, pmic_reg_read(dev, AXP2202_IIN_LIM));

	return 0;
}

int axp_start_up_debug(struct udevice *dev)
{
	int id;

	id = dev_get_driver_data(dev);
	switch (id) {
	case AXP2202_ID:
		pmu_axp2202_reg_debug(dev);
	default:
		pr_notice("no axp start up debug function\n");
	}

	return 0;
}

static const struct pmic_child_info axp_pmic_child_info[] = {
	{ "aldo",	"axp_regulator" },
	{ "bldo",	"axp_regulator" },
	{ "cldo",	"axp_regulator" },
	{ "dc",		"axp_regulator" },
	{ "dldo",	"axp_regulator" },
	{ "eldo",	"axp_regulator" },
	{ "fldo",	"axp_regulator" },
	{ "ldo",	"axp_regulator" },
	{ "sw",		"axp_regulator" },
	{ }
};

static int axp_pmic_bind(struct udevice *dev)
{
	ofnode regulators_node;
	int ret;

	ret = dm_scan_fdt_dev(dev);
	if (ret)
		return ret;

	regulators_node = dev_read_subnode(dev, "regulators");
	if (ofnode_valid(regulators_node))
		pmic_bind_children(dev, regulators_node, axp_pmic_child_info);

	if (CONFIG_IS_ENABLED(SYSRESET)) {
		ret = device_bind_driver_to_node(dev, "axp_sysreset", "axp_sysreset",
						 dev_ofnode(dev), NULL);
		if (ret)
			return ret;
	}

	return 0;
}

static const struct udevice_id axp_pmic_ids[] = {
	{ .compatible = "x-powers,axp152", .data = AXP152_ID },
	{ .compatible = "x-powers,axp202", .data = AXP202_ID },
	{ .compatible = "x-powers,axp209", .data = AXP209_ID },
	{ .compatible = "x-powers,axp221", .data = AXP221_ID },
	{ .compatible = "x-powers,axp223", .data = AXP223_ID },
	{ .compatible = "x-powers,axp803", .data = AXP803_ID },
	{ .compatible = "x-powers,axp806", .data = AXP806_ID },
	{ .compatible = "x-powers,axp809", .data = AXP809_ID },
	{ .compatible = "x-powers,axp813", .data = AXP813_ID },
	{ .compatible = "x-powers,axp2101", .data = AXP2101_ID },
	{ .compatible = "x-powers,axp2202", .data = AXP2202_ID },
	{ .compatible = "x-powers,axp1530", .data = AXP1530_ID },
	{ }
};

U_BOOT_DRIVER(axp_pmic) = {
	.name		= "axp_pmic",
	.id		= UCLASS_PMIC,
	.of_match	= axp_pmic_ids,
	.bind		= axp_pmic_bind,
	.ops		= &axp_pmic_ops,
};
