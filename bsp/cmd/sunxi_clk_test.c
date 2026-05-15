// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2013 Xilinx, Inc.
 */
#include <common.h>
#include <command.h>
#include <clk.h>
#if defined(CONFIG_DM) && defined(CONFIG_CLK)
#include <dm.h>
#include <dm/device.h>
#include <dm/root.h>
#include <dm/device-internal.h>
#include <linux/clk-provider.h>
#endif

#if defined(CONFIG_DM) && defined(CONFIG_CLK)
static void show_clks(struct udevice *dev, int depth, int last_flag)
{
	int i, is_last;
	struct udevice *child;
	struct clk *clkp, *parent;
	u32 rate;

	clkp = dev_get_clk_ptr(dev);
	if (clkp) {
		parent = clk_get_parent(clkp);
		if (!IS_ERR(parent) && depth == -1)
			return;
		depth++;
		rate = clk_get_rate(clkp);

		printf(" %-12u  %8d        ", rate, clkp->enable_count);

		for (i = depth; i >= 0; i--) {
			is_last = (last_flag >> i) & 1;
			if (i) {
				if (is_last)
					printf("    ");
				else
					printf("|   ");
			} else {
				if (is_last)
					printf("`-- ");
				else
					printf("|-- ");
			}
		}

		printf("%s\n", dev->name);
	}

	device_foreach_child_probe(child, dev) {
		if (device_get_uclass_id(child) != UCLASS_CLK)
			continue;
		if (child == dev)
			continue;
		is_last = list_is_last(&child->sibling_node, &dev->child_head);
		show_clks(child, depth, (last_flag << 1) | is_last);
	}
}

int __weak soc_clk_dump(void)
{
	struct udevice *dev;

	printf(" Rate               Usecnt      Name\n");
	printf("------------------------------------------\n");

	uclass_foreach_dev_probe(UCLASS_CLK, dev)
		show_clks(dev, -1, 0);

	return 0;
}
#else
int __weak soc_clk_dump(void)
{
	puts("Not implemented\n");
	return 1;
}
#endif

static int do_clk_dump(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	int ret;

	ret = soc_clk_dump();
	if (ret < 0) {
		printf("Clock dump error %d\n", ret);
		ret = CMD_RET_FAILURE;
	}

	return ret;
}

#if CONFIG_IS_ENABLED(DM) && CONFIG_IS_ENABLED(CLK)
static int do_clk_setfreq(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	struct clk *clk = NULL;
	s32 freq;
	struct udevice *dev;

	if (argc != 3)
		return CMD_RET_USAGE;

	freq = dectoul(argv[2], NULL);

	if (!uclass_get_device_by_name(UCLASS_CLK, argv[1], &dev))
		clk = dev_get_clk_ptr(dev);

	if (!clk) {
		printf("clock '%s' not found.\n", argv[1]);
		return CMD_RET_FAILURE;
	}

	freq = clk_set_rate(clk, freq);
	if (freq < 0) {
		printf("set_rate failed: %d\n", freq);
		return CMD_RET_FAILURE;
	}

	printf("set_rate returns %u\n", freq);
	return 0;
}

static int do_clk_setparent(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	struct clk *clk = NULL;
	struct clk *p_clk = NULL;
	s32 freq;
	int ret;
	struct udevice *dev;
	struct udevice *p_dev;

	if (argc != 3)
		return CMD_RET_USAGE;

	freq = dectoul(argv[2], NULL);

	if (!uclass_get_device_by_name(UCLASS_CLK, argv[1], &dev))
		clk = dev_get_clk_ptr(dev);

	if (!clk) {
		printf("clock '%s' not found.\n", argv[1]);
		return CMD_RET_FAILURE;
	}

	if (!uclass_get_device_by_name(UCLASS_CLK, argv[2], &p_dev))
		p_clk = dev_get_clk_ptr(p_dev);

	if (!p_clk) {
		printf("clock '%s' not found.\n", argv[1]);
		return CMD_RET_FAILURE;
	}

	ret = clk_set_parent(clk, p_clk);
	if (ret < 0) {
		printf("set_parent failed\n");
		return CMD_RET_FAILURE;
	}

	printf("set_rate returns %u\n", ret);
	return 0;
}

static int do_clk_enable(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	struct clk *clk = NULL;
	s32 freq;
	int ret;
	struct udevice *dev;

	if (argc != 2)
		return CMD_RET_USAGE;

	freq = dectoul(argv[2], NULL);

	if (!uclass_get_device_by_name(UCLASS_CLK, argv[1], &dev))
		clk = dev_get_clk_ptr(dev);

	if (!clk) {
		printf("clock '%s' not found.\n", argv[1]);
		return CMD_RET_FAILURE;
	}

	ret = clk_enable(clk);
	if (ret < 0) {
		printf("enable clk failed\n");
		return CMD_RET_FAILURE;
	}

	printf("set_enable returns %u\n", ret);
	return 0;
}
#endif

static struct cmd_tbl cmd_clk_sub[] = {
	U_BOOT_CMD_MKENT(dump, 1, 1, do_clk_dump, "", ""),
#if CONFIG_IS_ENABLED(DM) && CONFIG_IS_ENABLED(CLK)
	U_BOOT_CMD_MKENT(setfreq, 3, 1, do_clk_setfreq, "", ""),
	U_BOOT_CMD_MKENT(setparent, 3, 1, do_clk_setparent, "", ""),
	U_BOOT_CMD_MKENT(enable, 3, 1, do_clk_enable, "", ""),
#endif
};

static int do_clk(struct cmd_tbl *cmdtp, int flag, int argc,
		  char *const argv[])
{
	struct cmd_tbl *c;

	if (argc < 2)
		return CMD_RET_USAGE;

	/* Strip off leading 'clk' command argument */
	argc--;
	argv++;

	c = find_cmd_tbl(argv[0], &cmd_clk_sub[0], ARRAY_SIZE(cmd_clk_sub));

	if (c)
		return c->cmd(cmdtp, flag, argc, argv);
	else
		return CMD_RET_USAGE;
}

#ifdef CONFIG_SYS_LONGHELP
static char clk_help_text[] =
	"dump - Print clock frequencies\n"
	"sunxi_clk setfreq [clk] [freq] - Set clock frequency\n"
	"sunxi_clk setparent [clk-name] [parent-name] - Set clock parent\n"
	"sunxi_clk enable [clk-name] - Enable clock\n";
#endif

U_BOOT_CMD(sunxi_clk, 5, 1, do_clk, "CLK sub-system", clk_help_text);
