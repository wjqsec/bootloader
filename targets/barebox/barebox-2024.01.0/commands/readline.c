// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: © 2000-2003 Wolfgang Denk <wd@denx.de>, DENX Software Engineering

#include <common.h>
#include <command.h>
#include <malloc.h>
#include <slice.h>
#include <xfuncs.h>
#include <environment.h>

static int do_readline(int argc, char *argv[])
{
	char *buf;

	if (argc < 3)
		return COMMAND_ERROR_USAGE;

	buf = xzalloc(CONFIG_CBSIZE);

	command_slice_release();

	if (readline(argv[1], buf, CONFIG_CBSIZE - 1) < 0) {
		command_slice_acquire();
		free(buf);
		return COMMAND_ERROR;
	}

	command_slice_acquire();

	setenv(argv[2], buf);
	free(buf);

	return COMMAND_SUCCESS;
}

BAREBOX_CMD_HELP_START(readline)
BAREBOX_CMD_HELP_TEXT("First it displays the PROMPT, then it reads a line of user input into")
BAREBOX_CMD_HELP_TEXT("variable VAR.")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(readline)
	.cmd		= do_readline,
	BAREBOX_CMD_DESC("prompt for user input")
	BAREBOX_CMD_OPTS("PROMPT VAR")
	BAREBOX_CMD_GROUP(CMD_GRP_CONSOLE)
	BAREBOX_CMD_HELP(cmd_readline_help)
BAREBOX_CMD_END
