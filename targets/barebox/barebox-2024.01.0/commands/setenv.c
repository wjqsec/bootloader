// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: © 2007 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix

#include <common.h>
#include <command.h>
#include <errno.h>
#include <environment.h>
#include <complete.h>

static int do_setenv(int argc, char *argv[])
{
	char *val;
	int ret;

	if (argc < 2)
		return COMMAND_ERROR_USAGE;

	val = parse_assignment(argv[1]);
	if (val)
		argv[2] = val;

	if (argv[2])
		ret = setenv(argv[1], argv[2]);
	else
		ret = unsetenv(argv[1]);

	return ret ? COMMAND_ERROR : COMMAND_SUCCESS;
}

BAREBOX_CMD_HELP_START(setenv)
BAREBOX_CMD_HELP_TEXT("Set environment variable NAME to VALUE.")
BAREBOX_CMD_HELP_TEXT("If VALUE is ommitted, then the variable is deleted.")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(setenv)
	.cmd		= do_setenv,
	BAREBOX_CMD_DESC("set environment variable")
	BAREBOX_CMD_OPTS("NAME [VALUE]")
	BAREBOX_CMD_GROUP(CMD_GRP_ENV)
	BAREBOX_CMD_COMPLETE(env_param_noeval_complete)
	BAREBOX_CMD_HELP(cmd_setenv_help)
BAREBOX_CMD_END
