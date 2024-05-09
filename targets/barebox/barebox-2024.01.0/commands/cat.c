// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: © 2007 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix

/**
 * @file
 * @brief Concatenate files to stdout command
 */

#include <common.h>
#include <command.h>
#include <fs.h>
#include <fcntl.h>
#include <linux/ctype.h>
#include <errno.h>
#include <xfuncs.h>
#include <malloc.h>

#define BUFSIZE 1024

/**
 * @param[in] argc Argument count from command line
 * @param[in] argv List of input arguments
 */
static int do_cat(int argc, char *argv[])
{
	int ret;
	int fd, i;
	char *buf;
	int err = 0;
	int args = 1;

	if (argc < 2) {
		perror("cat");
		return 1;
	}

	buf = xmalloc(BUFSIZE);

	while (args < argc) {
		fd = open(argv[args], O_RDONLY);
		if (fd < 0) {
			err = 1;
			printf("could not open %s: %m\n", argv[args]);
			goto out;
		}

		while((ret = read(fd, buf, BUFSIZE)) > 0) {
			for(i = 0; i < ret; i++) {
				if (isprint(buf[i]) || buf[i] == '\n' || buf[i] == '\t')
					putchar(buf[i]);
				else
					putchar('.');
			}
			if(ctrlc()) {
				err = 1;
				close(fd);
				goto out;
			}
		}
		close(fd);
		args++;
	}

out:
	free(buf);

	return err;
}

BAREBOX_CMD_HELP_START(cat)
BAREBOX_CMD_HELP_TEXT("Currently only printable characters and NL, TAB are printed.")
BAREBOX_CMD_HELP_END

BAREBOX_CMD_START(cat)
	.cmd		= do_cat,
	BAREBOX_CMD_DESC("concatenate file(s) to stdout")
	BAREBOX_CMD_OPTS("FILE...")
	BAREBOX_CMD_GROUP(CMD_GRP_FILE)
	BAREBOX_CMD_HELP(cmd_cat_help)
BAREBOX_CMD_END
