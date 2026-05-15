// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2023-2027
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Tom Cubie <tangliang@allwinnertech.com>
 *
 * Some board init for the Allwinner A10-evb board.
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/gic.h>
#include <irq_func.h>
#include <dm.h>
#include <irq.h>

int interrupt_init(void)
{
	/*
	 * setup up stacks if necessary
	 */
	IRQ_STACK_START = gd->irq_sp - 4;
	IRQ_STACK_START_IN = gd->irq_sp + 8;
	FIQ_STACK_START = IRQ_STACK_START - SUNXI_STACKSIZE_IRQ;

	pr_debug("IRQ_STACK_START=0x%x\n", (uint32_t)IRQ_STACK_START);
	/*cpu0_set_irq_stack(IRQ_STACK_START);*/

	return arch_interrupt_init();
}

int interrupt_exit(void)
{
	return arch_interrupt_exit();
}

/* enable IRQ interrupts */
void enable_interrupts(void)
{
	unsigned long temp;
	__asm__ __volatile__("mrs %0, cpsr\n"
			     "bic %0, %0, #0x80\n"
			     "msr cpsr_c, %0"
			     : "=r"(temp)
			     :
			     : "memory");
}

/* get  interrupts state */
int interrupts_is_open(void)
{
	unsigned long temp = 0;
	__asm__ __volatile__("mrs %0, cpsr\n" : "=r"(temp) : : "memory");
	return ((temp & 0x80) == 0) ? 1 : 0;
}

/*
 * disable IRQ/FIQ interrupts
 * returns true if interrupts had been enabled before we disabled them
 */
int disable_interrupts(void)
{
	unsigned long old, temp;
	__asm__ __volatile__("mrs %0, cpsr\n"
			     "orr %1, %0, #0xc0\n"
			     "msr cpsr_c, %1"
			     : "=r"(old), "=r"(temp)
			     :
			     : "memory");
	return (old & 0x80) == 0;
}

void bad_mode(void)
{
	asm volatile("b .");
}