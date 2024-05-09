// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: © 2008-2010 Jean-Christophe PLAGNIOL-VILLARD <plagnioj@jcrosoft.com>

#include <common.h>
#include <command.h>
#include <complete.h>
#include <password.h>

static int do_login(int argc, char *argv[])
{
	login();

	return 0;
}

BAREBOX_CMD_HELP_START(login)
BAREBOX_CMD_HELP_TEXT("Asks for a password from the console before script execution continues.")
BAREBOX_CMD_HELP_TEXT("The password can be set with the 'passwd' command.")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(login)
	.cmd		= do_login,
	BAREBOX_CMD_DESC("ask for a password")
	BAREBOX_CMD_GROUP(CMD_GRP_CONSOLE)
	BAREBOX_CMD_HELP(cmd_login_help)
	BAREBOX_CMD_COMPLETE(empty_complete)
BAREBOX_CMD_END
