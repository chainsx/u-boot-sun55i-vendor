// SPDX-License-Identifier: GPL-2.0+
/*
 * SUNXI TWI Controller Driver
 *
 * Author: Chen Mingxi <chenmingxi@allwinnertech.com>
 * Copyright (c) 2010 Albert Aribaud.
 */

#include <common.h>
#include <i2c.h>
#include <log.h>
#include <asm/global_data.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <linux/compat.h>
#include <clk.h>
#include <dm.h>
#include <reset.h>
#include <asm/arch/i2c.h>

DECLARE_GLOBAL_DATA_PTR;

/* status or interrupt source */
/*------------------------------------------------------------------------------
* Code   Status
* 00h    Bus error
* 08h    START condition transmitted
* 10h    Repeated START condition transmitted
* 18h    Address + Write bit transmitted, ACK received
* 20h    Address + Write bit transmitted, ACK not received
* 28h    Data byte transmitted in master mode, ACK received
* 30h    Data byte transmitted in master mode, ACK not received
* 38h    Arbitration lost in address or data byte
* 40h    Address + Read bit transmitted, ACK received
* 48h    Address + Read bit transmitted, ACK not received
* 50h    Data byte received in master mode, ACK transmitted
* 58h    Data byte received in master mode, not ACK transmitted
* 60h    Slave address + Write bit received, ACK transmitted
* 68h    Arbitration lost in address as master, slave address + Write bit received, ACK transmitted
* 70h    General Call address received, ACK transmitted
* 78h    Arbitration lost in address as master, General Call address received, ACK transmitted
* 80h    Data byte received after slave address received, ACK transmitted
* 88h    Data byte received after slave address received, not ACK transmitted
* 90h    Data byte received after General Call received, ACK transmitted
* 98h    Data byte received after General Call received, not ACK transmitted
* A0h    STOP or repeated START condition received in slave mode
* A8h    Slave address + Read bit received, ACK transmitted
* B0h    Arbitration lost in address as master, slave address + Read bit received, ACK transmitted
* B8h    Data byte transmitted in slave mode, ACK received
* C0h    Data byte transmitted in slave mode, ACK not received
* C8h    Last byte transmitted in slave mode, ACK received
* D0h    Second Address byte + Write bit transmitted, ACK received
* D8h    Second Address byte + Write bit transmitted, ACK not received
* F8h    No relevant status information or no interrupt
*-----------------------------------------------------------------------------*/
#define TWI_START_TRANSMIT	(0x08)
#define TWI_RESTART_TRANSMIT	(0x10)
#define TWI_ADDRWRITE_ACK	(0x18)
#define TWI_ADDRREAD_ACK	(0x40)
#define TWI_DATAWRITE_ACK	(0x28)
#define TWI_STAT_IDLE		(0xf8)
#define TWI_DATAREAD_NACK	(0x58)
#define TWI_DATAREAD_ACK	(0x50)
#define TWI_LCR_NORM_STATUS	(0x30)

/* Customize TWI operation return value */
#define SUNXI_TWI_OK		0
#define SUNXI_TWI_FAIL		-1
#define SUNXI_TWI_TOUT		-2

/* TWI Control Register Bit Fields & Masks, default value: 0x0000_0000*/
#define TWI_CTL_ACK		(0x1<<2)
#define TWI_CTL_INTFLG		(0x1<<3)
#define TWI_CTL_STP		(0x1<<4)
#define TWI_CTL_STA		(0x1<<5)
#define TWI_CTL_BUSEN		(0x1<<6)
#define TWI_CTL_INTEN		(0x1<<7)
/* 31:8 bit reserved */

/* twi line control register -default value: 0x0000_003a */
#define TWI_LCR_SDA_EN          (0x01<<0)
#define TWI_LCR_SDA_CTL         (0x01<<1)
#define TWI_LCR_SCL_EN          (0x01<<2)
#define TWI_LCR_SCL_CTL         (0x01<<3)
#define TWI_LCR_SDA_STATE_MASK  (0x01<<4)
#define TWI_LCR_SCL_STATE_MASK  (0x01<<5)

#define TWI_WRITE		0
#define TWI_READ		1

#define DRIVER_VERSION "1.0.0"

struct sunxi_twi_reg {
	volatile unsigned int addr;		/* slave address     */
	volatile unsigned int xaddr;		/* extend address    */
	volatile unsigned int data;		/* data              */
	volatile unsigned int ctl;		/* control           */
	volatile unsigned int status;		/* status            */
	volatile unsigned int clk;		/* clock             */
	volatile unsigned int srst;		/* soft reset        */
	volatile unsigned int eft;		/* enhanced future   */
	volatile unsigned int lcr;		/* line control      */
	volatile unsigned int dvfs;		/* dvfs control      */
};

struct sunxi_twi {
	struct sunxi_twi_reg *reg;
	int bus_num;
	unsigned int speed;
	struct reset_ctl reset;
	struct clk clk;
};

static void sunxi_twi_soft_reset(struct sunxi_twi_reg *reg)
{
	reg->eft  = 0;
	reg->srst = 1;
}

static void sunxi_twi_set_start(struct sunxi_twi_reg *reg)
{
	reg->ctl  |= TWI_CTL_STA;
}

static void sunxi_twi_clear_irq_flag(struct sunxi_twi_reg *reg)
{
	reg->ctl |= TWI_CTL_INTFLG;
	udelay(30);
}

static int sunxi_twi_wait_irq_flag(struct sunxi_twi_reg *reg, u32 time)
{
	while ((time--) && (!(reg->ctl & TWI_CTL_INTFLG)))
		;

	if (time <= 0)
		return SUNXI_TWI_TOUT;

	return SUNXI_TWI_OK;
}

static void sunxi_twi_enable_ack(struct sunxi_twi_reg *reg)
{
	reg->ctl |= TWI_CTL_ACK;
}

static void sunxi_twi_disable_ack(struct sunxi_twi_reg *reg)
{
	reg->ctl &= ~TWI_CTL_ACK;
}

static void sunxi_twi_set_stop(struct sunxi_twi_reg *reg)
{
	reg->ctl  |= TWI_CTL_STP;
}

static void sunxi_twi_enable_lcr(struct sunxi_twi_reg *reg)
{
	reg->lcr |= (TWI_LCR_SCL_EN | TWI_LCR_SDA_EN);
}

/* send 9 clock to release sda */
static int sunxi_twi_send_clk_9pulse(struct sunxi_twi *twi)
{
	struct sunxi_twi_reg *reg = twi->reg;
	int cycle = 10;

	sunxi_twi_enable_lcr(reg);
	udelay(500);

	/* toggle I2C SCL and SDA until bus idle */
	while ((cycle > 0) && ((reg->lcr & TWI_LCR_SDA_CTL) != TWI_LCR_SDA_CTL)) {
		/*control scl and sda output high level*/
		reg->lcr |= TWI_LCR_SCL_CTL;
		reg->lcr |= TWI_LCR_SDA_CTL;
		udelay(1000);
		/*control scl and sda output low level*/
		reg->lcr &= ~TWI_LCR_SCL_CTL;
		reg->lcr &= ~TWI_LCR_SDA_CTL;
		udelay(1000);
		cycle--;
	}

	if ((reg->lcr & TWI_LCR_SDA_CTL) != TWI_LCR_SDA_CTL) {
		pr_err("twi-%d: SDA is still Stuck Low, failed.\n", twi->bus_num);
		return SUNXI_TWI_FAIL;
	}

	reg->lcr = 0x0;
	udelay(500);

	return SUNXI_TWI_OK;
}

static int sunxi_twi_start(struct sunxi_twi *twi)
{
	struct sunxi_twi_reg *reg = twi->reg;
	u32 timeout = 0xff;

	sunxi_twi_soft_reset(reg);
	sunxi_twi_set_start(reg);

	if (sunxi_twi_wait_irq_flag(reg, timeout)) {
		pr_err("twi-%d: START can't sendout!\n", twi->bus_num);
		return SUNXI_TWI_FAIL;
	}

	if (reg->status != TWI_START_TRANSMIT) {
		pr_err("twi-%d: Status not is TWI_START_TRANSMIT! status = 0x%x\n", twi->bus_num, reg->status);
		return SUNXI_TWI_FAIL;
	}

	return SUNXI_TWI_OK;
}

static int sunxi_twi_restart(struct sunxi_twi *twi)
{
	struct sunxi_twi_reg *reg = twi->reg;
	u32 timeout = 0xff;

	sunxi_twi_set_start(reg);
	sunxi_twi_clear_irq_flag(reg);
	if (sunxi_twi_wait_irq_flag(reg, timeout)) {
		pr_err("twi-%d: Restart can't sendout!\n", twi->bus_num);
		return SUNXI_TWI_FAIL;
	}

	if (reg->status != TWI_RESTART_TRANSMIT) {
		pr_err("twi-%d: Status not is TWI_RESTART_TRANSMIT! status = 0x%x\n", twi->bus_num, reg->status);
		return SUNXI_TWI_FAIL;
	}

	return SUNXI_TWI_OK;
}

static int sunxi_twi_send_slave_addr(struct sunxi_twi *twi, struct i2c_msg *msg, u32 rw)
{
	struct sunxi_twi_reg *reg = twi->reg;
	u32  time = 0xff;

	rw &= 1;
	reg->data = ((msg->addr & 0xff) << 1) | rw;
	sunxi_twi_clear_irq_flag(reg);

	if (sunxi_twi_wait_irq_flag(reg, time)) {
		pr_debug("twi-%d: wait clear irq flag timeout\n", twi->bus_num);
		return SUNXI_TWI_TOUT;
	}

	if ((rw == TWI_WRITE) && (reg->status != TWI_ADDRWRITE_ACK)) {
		pr_err("twi-%d: Status not is TWI_ADDRWRITE_ACK! status = 0x%x\n", twi->bus_num, reg->status);
		return SUNXI_TWI_FAIL;
	} else if ((rw == TWI_READ) && (reg->status != TWI_ADDRREAD_ACK)) {
		pr_err("twi-%d: Status not is TWI_ADDRREAD_ACK! status = 0x%x\n", twi->bus_num, reg->status);
		return SUNXI_TWI_FAIL;
	}

	return SUNXI_TWI_OK;
}

static int sunxi_twi_send_reg_addr(struct sunxi_twi *twi, struct i2c_msg *msg)
{
	struct sunxi_twi_reg *reg = twi->reg;
	const unsigned char reg_addr = msg->buf[0];
	int  time = 0xff;

	reg->data = reg_addr & 0xff;
	sunxi_twi_clear_irq_flag(reg);

	if (sunxi_twi_wait_irq_flag(reg, time)) {
		pr_err("twi-%d: wait clear irq flag timeout\n", twi->bus_num);
		return SUNXI_TWI_TOUT;
	}

	if (reg->status != TWI_DATAWRITE_ACK) {
		pr_err("twi-%d: Status not is TWI_DATAWRITE_ACK! status = 0x%x\n", twi->bus_num, reg->status);
		return SUNXI_TWI_FAIL;
	}

	return SUNXI_TWI_OK;
}

static int sunxi_twi_get_data(struct sunxi_twi *twi, struct i2c_msg *msg)
{
	struct sunxi_twi_reg *reg = twi->reg;
	int  time = 0xff;
	u32  i;
	u8 *read_data = msg->buf;

	if (msg->len == 1) {
		/* no need ack  */
		sunxi_twi_clear_irq_flag(reg);

		if (sunxi_twi_wait_irq_flag(reg, time)) {
			pr_debug("twi-%d: wait clear irq flag timeout\n", twi->bus_num);
			return SUNXI_TWI_TOUT;
		}

		*read_data = reg->data;

		if (reg->status != TWI_DATAREAD_NACK) {
			pr_err("twi-%d: Status not is TWI_DATAREAD_NACK! Status = 0x%x\n", twi->bus_num, reg->status);
			return -TWI_DATAREAD_NACK;
		}
	} else {
		for (i = 0; i < msg->len - 1; i++) {
			/* need ack  */
			sunxi_twi_enable_ack(reg);
			sunxi_twi_clear_irq_flag(reg);

			if (sunxi_twi_wait_irq_flag(reg, time)) {
				pr_debug("twi-%d: wait clear irq flag timeout\n", twi->bus_num);
				return SUNXI_TWI_TOUT;
			}

			read_data[i] = reg->data;

			while ((time--) && (reg->status != TWI_DATAREAD_ACK))
				;

			if (time <= 0)
				return SUNXI_TWI_TOUT;
		}

		/* received the last byte  */
		sunxi_twi_disable_ack(reg);
		sunxi_twi_clear_irq_flag(reg);

		if (sunxi_twi_wait_irq_flag(reg, time)) {
			pr_debug("twi-%d: wait clear irq flag timeout\n", twi->bus_num);
			return SUNXI_TWI_TOUT;
		}

		read_data[msg->len - 1] = reg->data;

		while ((time--) && (reg->status != TWI_DATAREAD_NACK))
			;

		if (time <= 0)
			return SUNXI_TWI_TOUT;
	}

	return SUNXI_TWI_OK;
}

static int sunxi_twi_send_data(struct sunxi_twi *twi, struct i2c_msg *msg)
{
	struct sunxi_twi_reg *reg = twi->reg;
	int  time = 0xff;
	u32  i;

	for (i = 1; i < msg->len; i++) {
		reg->data = msg->buf[i];
		sunxi_twi_clear_irq_flag(reg);

		if (sunxi_twi_wait_irq_flag(reg, time)) {
			pr_debug("twi-%d: wait clear irq flag timeout\n", twi->bus_num);
			return SUNXI_TWI_TOUT;
		}

		time = 0xff;
		while ((time--) && (reg->status != TWI_DATAWRITE_ACK))
			;
		if (time <= 0)
			return SUNXI_TWI_TOUT;
	}

	return SUNXI_TWI_OK;
}

static int sunxi_twi_stop(struct sunxi_twi *twi)
{
	struct sunxi_twi_reg *reg = twi->reg;
	int  time = 0xff;

	reg->ctl |= (0x01 << 4);
	reg->ctl |= (0x01 << 3);

	sunxi_twi_set_stop(reg);
	sunxi_twi_clear_irq_flag(reg);

	while ((reg->status != TWI_STAT_IDLE) && (--time))
		;

	if (time <= 0) {
		pr_err("twi-%d: state isn't idle(0xf8)\n", twi->bus_num);
		return SUNXI_TWI_FAIL;
	}

	return SUNXI_TWI_OK;
}

static int sunxi_twi_init_clk(struct sunxi_twi *twi, int speed)
{
	struct sunxi_twi_reg *reg = twi->reg;
	int timeout, clk_n, clk_m, i, pow_2_clk_n;
	timeout = 0xff;
	int ret = 0;

	if ((reg->lcr & TWI_LCR_NORM_STATUS) != TWI_LCR_NORM_STATUS) {
		pr_err("twi-%d: bus is busy, lcr = %x\n", twi->bus_num, reg->lcr);
		ret = sunxi_twi_send_clk_9pulse(twi);
		if (ret)
			return ret;
	}
	twi->speed = speed;
	speed /= 1000; /* khz */

	if (speed < 100)
		speed = 100;
	else if (speed > 400)
		speed = 400;
	/* Foscl=24000/(2^CLK_N*(CLK_M+1)*10) */
	clk_n = (speed == 100) ? 1 : 0;
	pow_2_clk_n = 1;
	for (i = 0; i < clk_n; ++i)
		pow_2_clk_n *= 2;
	clk_m = 2400 / (pow_2_clk_n * speed) - 1;

	reg->clk = (clk_m << 3) | clk_n;
	reg->ctl |= TWI_CTL_BUSEN;
	reg->eft = 0;

	return ret;
}

static int sunxi_twi_check_device_busy(struct sunxi_twi *twi)
{
	struct sunxi_twi_reg *reg = twi->reg;

	return (reg->status != TWI_STAT_IDLE);
}

static int sunxi_twi_read(struct sunxi_twi *twi, struct i2c_msg *msg)
{
	int  ret;

	ret = sunxi_twi_start(twi);
	if (ret) {
		pr_err("twi-%d: read start error\n", twi->bus_num);
		goto twi_read_err;
	}

	ret = sunxi_twi_send_slave_addr(twi, msg, TWI_WRITE);
	if (ret) {
		pr_err("twi-%d: read send slave addr error!\n", twi->bus_num);
		goto twi_read_err;
	}

	ret = sunxi_twi_send_reg_addr(twi, msg);
	if (ret) {
		pr_err("twi-%d: twi_read send addr error\n", twi->bus_num);
		goto twi_read_err;
	}

	ret = sunxi_twi_restart(twi);
	if (ret) {
		pr_err("twi-%d: sunxi_twi_restart error\n", twi->bus_num);
		goto twi_read_err;
	}

	ret = sunxi_twi_send_slave_addr(twi, ++msg, TWI_READ);
	if (ret) {
		pr_err("twi-%d: twi_read send slave addr error!\n", twi->bus_num);
		goto twi_read_err;
	}

	ret = sunxi_twi_get_data(twi, msg);
	if (ret) {
		pr_err("twi-%d: sunxi_twi_get_data error\n", twi->bus_num);
		goto twi_read_err;
	}

twi_read_err:
	sunxi_twi_stop(twi);
	return ret;

}

static int sunxi_twi_write(struct sunxi_twi *twi, struct i2c_msg *msg)
{
	int ret;

	ret = sunxi_twi_start(twi);
	if (ret) {
		pr_err("twi-%d: write start error\n", twi->bus_num);
		goto twi_write_err;
	}

	ret = sunxi_twi_send_slave_addr(twi, msg, TWI_WRITE);
	if (ret) {
		pr_err("twi-%d: write send slave addr error!\n", twi->bus_num);
		goto twi_write_err;
	}

	ret = sunxi_twi_send_reg_addr(twi, msg);
	if (ret) {
		pr_err("twi-%d: write send addr error!\n", twi->bus_num);
		goto twi_write_err;
	}

	ret = sunxi_twi_send_data(twi, msg);
	if (ret) {
		pr_err("twi-%d: write send data error!\n", twi->bus_num);
		goto twi_write_err;
	}

twi_write_err:
	sunxi_twi_stop(twi);
	return ret;

}

static int sunxi_twi_xfer(struct udevice *dev, struct i2c_msg *msg, int nmsgs)
{
	struct sunxi_twi *twi = dev_get_priv(dev);
	struct i2c_msg *dmsg;

	if (nmsgs > 2 || nmsgs == 0) {
		pr_err("twi-%d: Only support transfer one or two messages\n", twi->bus_num);
		return -1;
	}

	if (sunxi_twi_check_device_busy(twi)) {
		pr_err("twi-%d: Bus is busy\n", twi->bus_num);
		return -EBUSY;
	}

	dmsg = nmsgs == 1 ? msg : msg + 1;

	if (dmsg->flags & I2C_M_RD)
		return sunxi_twi_read(twi, msg);
	else
		return sunxi_twi_write(twi, msg);

	return 0;
}

static int sunxi_twi_of_to_plat(struct udevice *dev)
{
	struct sunxi_twi *twi = dev_get_priv(dev);

	twi->reg = dev_read_addr_ptr(dev);
	if (!twi->reg)
		return -ENOMEM;

	twi->speed = dev_read_u32_default(dev, "clock-frequency",
					I2C_SPEED_STANDARD_RATE);

	return 0;
}

static int sunxi_twi_set_bus_speed(struct udevice *dev, unsigned int speed)
{
	struct sunxi_twi *twi = dev_get_priv(dev);

	sunxi_twi_init_clk(twi, speed);

	return 0;
}

static int sunxi_twi_probe(struct udevice *dev)
{
	struct sunxi_twi *twi = dev_get_priv(dev);
	fdt_addr_t addr;
	int ret;

	twi->bus_num = dev_seq(dev);

	addr = dev_read_addr(dev);
	if (addr == FDT_ADDR_T_NONE)
		return -EINVAL;
	twi->reg = (struct sunxi_twi_reg *)addr;

#if 0
	ret = reset_get_by_index(dev, 0, &twi->reset);
	if (ret)
		return ret;

	reset_assert(&twi->reset);
	udelay(2);
	reset_deassert(&twi->reset);

	ret = clk_get_by_index(dev, 0, &twi->clk);
	if (ret)
		return ret;

	ret = clk_enable(&twi->clk);
	if (ret)
		goto free_clk;
#endif
	writel(0x10001, 0x701019c);
	writel(0x10001, 0x2002E88);	// TWI2_BGR_REG, TWI2_RST: De-assert, TWI2_GATING: Pass
	ret = sunxi_twi_init_clk(twi, twi->speed);
	if (ret)
		goto clk_disable;

	pr_info("twi-%d: v%s probe seccess!\n", twi->bus_num, DRIVER_VERSION);

	return 0;

clk_disable:
	clk_disable(&twi->clk);
//free_clk:
//	clk_free(&twi->clk);

	return ret;

}

static const struct dm_i2c_ops sunxi_twi_ops = {
	.xfer		= sunxi_twi_xfer,
	.set_bus_speed	= sunxi_twi_set_bus_speed,
};

static const struct udevice_id sunxi_twi_of_match[] = {
	{ .compatible = "allwinner,sunxi-twi", },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(sunxi_twi) = {
	.name		= "sunxi_twi",
	.id		= UCLASS_I2C,
	.of_match	= sunxi_twi_of_match,
	.probe		= sunxi_twi_probe,
	.of_to_plat	= sunxi_twi_of_to_plat,
	.priv_auto	= sizeof(struct sunxi_twi),
	.ops		= &sunxi_twi_ops,
};
