/* SPDX-License-Identifier: GPL-2.0 */
/*
 * (C) Copyright 2024 allwinnertech  <huangrongcun@allwinnertech.com>
 */
#include <linux/linkage.h>

#include <asm/opcodes-sec.h>
#include <asm/opcodes-virt.h>


ENTRY(__sunxi_smc_call)
	stmfd	sp!, {r4-r12, lr}	@ save reg state
	.arch_extension sec
	smc #0
	ldmfd	sp!, {r4-r12, pc}	@ restore saved regs and return
ENDPROC(__sunxi_smc_call)
