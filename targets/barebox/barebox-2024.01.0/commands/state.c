// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: © 2012 Jan Luebbe <j.luebbe@pengutronix.de>

#include <common.h>
#include <getopt.h>
#include <command.h>
#include <state.h>

static int do_state(int argc, char *argv[])
{
	int opt, ret = 0;
	struct state *state = NULL;
	int do_save = 0, do_load = 0;
	const char *statename = "state";
	int no_auth = 0;

	while ((opt = getopt(argc, argv, "sln")) > 0) {
		switch (opt) {
		case 's':
			do_save = 1;
			break;
		case 'l':
			do_load = 1;
			break;
		case 'n':
			no_auth = 1;
			break;
		default:
			return COMMAND_ERROR_USAGE;
		}
	}

	if (!do_save && !do_load) {
		state_info();
		return 0;
	}

	if (optind < argc)
		statename = argv[optind];

	state = state_by_name(statename);
	if (!state) {
		printf("cannot find state %s\n", statename);
		return -ENOENT;
	}

	if (do_load) {
		if (no_auth)
			ret = state_load_no_auth(state);
		else
			ret = state_load(state);
	} else if (do_save) {
		ret = state_save(state);
	}

	if (ret == -ENOMEDIUM)
		ret = 0;

	return ret;
}

BAREBOX_CMD_HELP_START(state)
BAREBOX_CMD_HELP_TEXT("Usage: state [OPTIONS] [STATENAME]")
BAREBOX_CMD_HELP_TEXT("")
BAREBOX_CMD_HELP_TEXT("options:")
BAREBOX_CMD_HELP_OPT ("-s", "save state")
BAREBOX_CMD_HELP_OPT ("-l", "load state")
BAREBOX_CMD_HELP_OPT ("-n", "no authentication")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(state)
	.cmd		= do_state,
	BAREBOX_CMD_DESC("load or save state information")
	BAREBOX_CMD_OPTS("[-sln] [STATENAME]")
	BAREBOX_CMD_GROUP(CMD_GRP_MISC)
	BAREBOX_CMD_HELP(cmd_state_help)
BAREBOX_CMD_END
