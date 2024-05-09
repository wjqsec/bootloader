// SPDX-License-Identifier: GPL-2.0-only

#include <common.h>
#include <command.h>
#include <getopt.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <environment.h>
#include <malloc.h>

static int do_clk_enable(int argc, char *argv[])
{
	struct clk *clk;

	if (argc != 2)
		return COMMAND_ERROR_USAGE;

	clk = clk_lookup(argv[1]);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	return clk_enable(clk);
}

BAREBOX_CMD_START(clk_enable)
	.cmd		= do_clk_enable,
	BAREBOX_CMD_DESC("enable a clock")
	BAREBOX_CMD_OPTS("CLK")
	BAREBOX_CMD_GROUP(CMD_GRP_HWMANIP)
	BAREBOX_CMD_COMPLETE(clk_name_complete)
BAREBOX_CMD_END

static int do_clk_disable(int argc, char *argv[])
{
	struct clk *clk;

	if (argc != 2)
		return COMMAND_ERROR_USAGE;

	clk = clk_lookup(argv[1]);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	clk_disable(clk);

	return COMMAND_SUCCESS;
}

BAREBOX_CMD_START(clk_disable)
	.cmd		= do_clk_disable,
	BAREBOX_CMD_DESC("disable a clock")
	BAREBOX_CMD_OPTS("CLK")
	BAREBOX_CMD_GROUP(CMD_GRP_HWMANIP)
	BAREBOX_CMD_COMPLETE(clk_name_complete)
BAREBOX_CMD_END

static int do_clk_set_rate(int argc, char *argv[])
{
	unsigned long rate;

	if (argc != 3)
		return COMMAND_ERROR_USAGE;

	rate = simple_strtoul(argv[2], NULL, 0);

	return clk_name_set_rate(argv[1], rate);
}

BAREBOX_CMD_HELP_START(clk_set_rate)
BAREBOX_CMD_HELP_TEXT("Set clock CLK to RATE")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(clk_set_rate)
	.cmd		= do_clk_set_rate,
	BAREBOX_CMD_DESC("set a clocks rate")
	BAREBOX_CMD_OPTS("CLK HZ")
	BAREBOX_CMD_GROUP(CMD_GRP_HWMANIP)
	BAREBOX_CMD_HELP(cmd_clk_set_rate_help)
	BAREBOX_CMD_COMPLETE(clk_name_complete)
BAREBOX_CMD_END

static int do_clk_round_rate(int argc, char *argv[])
{
	struct clk *clk;
	unsigned long rate;

	if (argc != 3)
		return COMMAND_ERROR_USAGE;

	clk = clk_lookup(argv[1]);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	rate = simple_strtoul(argv[2], NULL, 0);

	printf("%ld\n", clk_round_rate(clk, rate));

	return 0;
}

BAREBOX_CMD_HELP_START(clk_round_rate)
BAREBOX_CMD_HELP_TEXT("Show clock CLK actual rate if set to HZ")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(clk_round_rate)
	.cmd		= do_clk_round_rate,
	BAREBOX_CMD_DESC("show a resulting clocks rate")
	BAREBOX_CMD_OPTS("CLK HZ")
	BAREBOX_CMD_GROUP(CMD_GRP_HWMANIP)
	BAREBOX_CMD_HELP(cmd_clk_round_rate_help)
	BAREBOX_CMD_COMPLETE(clk_name_complete)
BAREBOX_CMD_END

static int do_clk_get_rate(int argc, char *argv[])
{
	int opt;
	struct clk *clk;
	unsigned long rate;
	const char *variable_name = NULL;

	while ((opt = getopt(argc, argv, "s:")) > 0) {
		switch (opt) {
		case 's':
			variable_name = optarg;
			break;
		default:
			return COMMAND_ERROR_USAGE;
		}
	}

	if (optind == argc) {
		eprintf("No clock name given\n");
		return COMMAND_ERROR_USAGE;
	}

	clk = clk_lookup(argv[optind]);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	rate = clk_get_rate(clk);

	if (variable_name)
		pr_setenv(variable_name, "%lu", rate);
	else
		printf("%lu\n", rate);

	return COMMAND_SUCCESS;
}

BAREBOX_CMD_HELP_START(clk_get_rate)
BAREBOX_CMD_HELP_TEXT("Show clock CLK rate")
BAREBOX_CMD_HELP_OPT("-s VARNAME",  "set variable VARNAME instead of showing information")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(clk_get_rate)
	.cmd		= do_clk_get_rate,
	BAREBOX_CMD_DESC("get a clocks rate")
	BAREBOX_CMD_OPTS("[-s VARNAME] CLK")
	BAREBOX_CMD_GROUP(CMD_GRP_HWMANIP)
	BAREBOX_CMD_HELP(cmd_clk_get_rate_help)
	BAREBOX_CMD_COMPLETE(clk_name_complete)
BAREBOX_CMD_END

static int do_clk_dump(int argc, char *argv[])
{
	int opt, verbose = 0;
	struct clk *clk;

	while ((opt = getopt(argc, argv, "v")) > 0) {
		switch(opt) {
		case 'v':
			verbose = 1;
			break;
		default:
			return -EINVAL;

		}
	}

	if (optind == argc) {
		clk_dump(verbose);
		return COMMAND_SUCCESS;
	}

	clk = clk_lookup(argv[optind]);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	clk_dump_one(clk, verbose);

	return COMMAND_SUCCESS;
}

BAREBOX_CMD_HELP_START(clk_dump)
BAREBOX_CMD_HELP_TEXT("Options:")
BAREBOX_CMD_HELP_OPT ("-v",  "verbose")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(clk_dump)
	.cmd		= do_clk_dump,
	BAREBOX_CMD_DESC("show information about registered clocks")
	BAREBOX_CMD_OPTS("[-v] [clkname]")
	BAREBOX_CMD_GROUP(CMD_GRP_INFO)
	BAREBOX_CMD_HELP(cmd_clk_dump_help)
	BAREBOX_CMD_COMPLETE(clk_name_complete)
BAREBOX_CMD_END

static int do_clk_set_parent(int argc, char *argv[])
{
	if (argc != 3)
		return COMMAND_ERROR_USAGE;

	return clk_name_set_parent(argv[1], argv[2]);
}

BAREBOX_CMD_START(clk_set_parent)
	.cmd		= do_clk_set_parent,
	BAREBOX_CMD_DESC("set parent of a clock")
	BAREBOX_CMD_OPTS("CLK PARENT")
	BAREBOX_CMD_GROUP(CMD_GRP_HWMANIP)
	BAREBOX_CMD_COMPLETE(clk_name_complete)
BAREBOX_CMD_END
