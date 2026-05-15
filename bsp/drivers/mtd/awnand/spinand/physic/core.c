// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) "sunxi-spinand-phy: " fmt

#include <common.h>
#include <linux/kernel.h>
#include <linux/libfdt.h>
#include <linux/errno.h>
#include <linux/mtd/aw-spinand.h>
#include <fdt_support.h>
#include <linux/compat.h>
#include <dm.h>

#include "physic.h"

/**
 * aw_spinand_chip_update_cfg() - Update the configuration register
 * @chip: spinand chip structure
 *
 * Return: 0 on success, a negative error code otherwise.
 */
static int aw_spinand_chip_update_cfg(struct aw_spinand_chip *chip)
{
	int ret;
	struct aw_spinand_chip_ops *ops = chip->ops;
	struct aw_spinand_info *info = chip->info;
	unsigned char reg;

	reg = 0;

	if (!strcmp(info->manufacture(chip), "SkyHigh")) {
		ret = ops->set_block_lock(chip, SKYHIGH_UNLOCK_ALL_BLOCK_STEP1);
		if (ret)
			goto err;
		ret = ops->set_block_lock(chip, SKYHIGH_UNLOCK_ALL_BLOCK_STEP2);
		if (ret)
			goto err;
	} else {
		ret = ops->set_block_lock(chip, reg);
		if (ret)
			goto err;
	}
	ret = ops->get_block_lock(chip, &reg);
	if (ret)
		goto err;
	pr_info("block lock register: 0x%02x\n", reg);

	ret = ops->get_otp(chip, &reg);
	if (ret) {
		pr_err("get otp register failed: %d\n", ret);
		goto err;
	}
	/* FS35ND01G ECC_EN not on register 0xB0, but on 0x90 */
	if (!strcmp(info->manufacture(chip), "Foresee")) {
		ret = ops->write_reg(chip, SPI_NAND_SETSR, FORESEE_REG_ECC_CFG,
				CFG_ECC_ENABLE);
		if (ret) {
			pr_err("enable ecc for foresee failed: %d\n", ret);
			goto err;
		}
	} else {
		reg |= CFG_ECC_ENABLE;
	}
	if (!strcmp(info->manufacture(chip), "Winbond"))
		reg |= CFG_BUF_MODE;
	if (info->operation_opt(chip) & SPINAND_QUAD_READ ||
			info->operation_opt(chip) & SPINAND_QUAD_PROGRAM)
		reg |= CFG_QUAD_ENABLE;
	if (info->operation_opt(chip) & SPINAND_QUAD_NO_NEED_ENABLE)
		reg &= ~CFG_QUAD_ENABLE;

	ret = ops->set_otp(chip, reg);
	if (ret) {
		pr_err("set otp register failed: val %d, ret %d\n", reg, ret);
		goto err;
	}
	ret = ops->get_otp(chip, &reg);
	if (ret) {
		pr_err("get updated otp register failed: %d\n", ret);
		goto err;
	}
	pr_info("feature register: 0x%02x\n", reg);

	return 0;
err:
	pr_err("update config register failed\n");
	return ret;
}

static void aw_spinand_chip_clean(struct aw_spinand_chip *chip)
{
	aw_spinand_chip_cache_exit(chip);
	aw_spinand_chip_bbt_exit(chip);
}

static int aw_spinand_chip_init_last(struct aw_spinand_chip *chip)
{
	int ret;
	struct aw_spinand_info *info = chip->info;
	unsigned int val;

	/* initialize from spinand infomation */
	if (info->operation_opt(chip) & SPINAND_QUAD_PROGRAM)
		chip->tx_bit = SPI_NBITS_QUAD ;
	else
		chip->tx_bit = SPI_NBITS_SINGLE;

	if (info->operation_opt(chip) & SPINAND_QUAD_READ)
		chip->rx_bit = SPI_NBITS_QUAD;
	else if (info->operation_opt(chip) & SPINAND_DUAL_READ)
		chip->rx_bit = SPI_NBITS_DUAL;
	else
		chip->rx_bit = SPI_NBITS_SINGLE;

	val = dev_read_u32_default(chip->dev, "spi-rx-bus-width", SPI_NBITS_SINGLE);
	if (val < chip->rx_bit)
		chip->rx_bit = val;
	pr_info("%s rx bit width to %u\n", info->model(chip), chip->rx_bit);

	val = dev_read_u32_default(chip->dev, "spi-tx-bus-width", SPI_NBITS_SINGLE);
	if (val < chip->tx_bit)
		chip->tx_bit = val;

	if (chip->tx_bit == SPI_NBITS_DUAL) {
		pr_info("%s not support tx bit width = 2, reset tx bit width to 1\n",
			info->model(chip));
		chip->tx_bit = SPI_NBITS_SINGLE;
	} else
		pr_info("%s tx bit width to %u\n", info->model(chip), chip->tx_bit);

	/*val = dev_read_u32_default(chip->dev, "spi-max-frequency", CONFIG_SPINAND_DEFAULT_SPEED);
	if (ret < 0) {
		pr_err("can't get spi-max-frequency\n");
		return -EINVAL;
	}*/

	/* update spinand register */
	ret = aw_spinand_chip_update_cfg(chip);
	if (ret)
		return ret;

	/* do read/write cache init */
	ret = aw_spinand_chip_cache_init(chip);
	if (ret)
		return ret;

	/* do bad block table init */
	ret = aw_spinand_chip_bbt_init(chip);
	if (ret)
		return ret;

	return 0;
}

static int aw_spinand_chip_preinit(struct spi_slave *salve,
		struct aw_spinand_chip *chip)
{
	int ret;

	chip->slave = salve;

	ret = aw_spinand_chip_ecc_init(chip);
	if (unlikely(ret))
		return ret;

	ret = aw_spinand_chip_ops_init(chip);
	if (unlikely(ret))
		return ret;

	return 0;
}

int aw_spinand_chip_init(struct spi_slave *slave, struct aw_spinand_chip *chip)
{
	int ret;

	pr_info("AW SPINand Phy Layer Version: %x.%x %x\n",
			AW_SPINAND_PHY_VER_MAIN, AW_SPINAND_PHY_VER_SUB,
			AW_SPINAND_PHY_VER_DATE);

	ret = aw_spinand_chip_preinit(slave, chip);
	if (unlikely(ret))
		return ret;

	ret = aw_spinand_chip_detect(chip);
	if (ret)
		return ret;

	ret = aw_spinand_chip_init_last(chip);
	if (ret)
		goto err;

	pr_info("sunxi physic nand init end\n");
	return 0;
err:
	aw_spinand_chip_clean(chip);
	return ret;
}
EXPORT_SYMBOL(aw_spinand_chip_init);

void aw_spinand_chip_exit(struct aw_spinand_chip *chip)
{
	aw_spinand_chip_clean(chip);
}
EXPORT_SYMBOL(aw_spinand_chip_exit);
