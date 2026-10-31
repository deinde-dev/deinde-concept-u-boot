// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2011
 * Joe Hershberger, National Instruments, joe.hershberger@ni.com
 *
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

#include <command.h>
#include <env.h>
#include <getopt.h>
#include <image.h>
#include <hash.h>
#include <mapmem.h>
#include <u-boot/md5.h>
#include <asm/io.h>

static int do_md5sum(struct getopt_state *gs)
{
	int flags = HASH_FLAG_ENV;
	int opt;

	while (IS_ENABLED(CONFIG_MD5SUM_VERIFY) &&
	       (opt = getopt(gs, "+v")) > 0) {
		switch (opt) {
		case 'v':
			flags |= HASH_FLAG_VERIFY;
			break;
		default:
			return CMD_RET_USAGE;
		}
	}

	/* Need at least: address count */
	if (gs->argc - gs->index < 2)
		return CMD_RET_USAGE;

	return hash_command("md5", flags, gs->argc - gs->index,
			    &gs->argv[gs->index]);
}

#if IS_ENABLED(CONFIG_MD5SUM_VERIFY)
U_BOOT_CMD_GETOPT(
	md5sum,	5,	1,	do_md5sum,
	"compute MD5 message digest",
	"address count [[*]sum]\n"
		"    - compute MD5 message digest [save to sum]\n"
	"md5sum -v address count [*]sum\n"
		"    - verify md5sum of memory area"
);
#else
U_BOOT_CMD_NOOPTS(
	md5sum,	4,	1,	do_md5sum,
	"compute MD5 message digest",
	"address count [[*]sum]\n"
		"    - compute MD5 message digest [save to sum]"
);
#endif /* IS_ENABLED(CONFIG_MD5SUM_VERIFY) */
