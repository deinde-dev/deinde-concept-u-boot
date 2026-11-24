/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#include <command.h>
#include <getopt.h>
#include <mapmem.h>
#include <part.h>
#include <vsprintf.h>

static int do_rw(struct getopt_state *gs)
{
	struct blk_desc *dev_desc = NULL;
	struct disk_partition part_info;
	char *const *argv = gs->argv;
	ulong offset, limit;
	uint blk, cnt, res;
	void *ptr;
	int part;

	if (gs->argc != 6)
		return CMD_RET_USAGE;

	part = part_get_info_by_dev_and_name_or_num(argv[1], argv[2],
						    &dev_desc, &part_info, 1);
	if (part < 0)
		return 1;

	ptr = map_sysmem(hextoul(argv[3], NULL), 0);
	blk = hextoul(argv[4], NULL);
	cnt = hextoul(argv[5], NULL);

	if (part > 0) {
		offset = part_info.start;
		limit = part_info.size;
	} else {
		/* Largest address not available in struct blk_desc. */
		offset = 0;
		limit = ~0;
	}

	if (blk > limit || cnt > limit - blk) {
		printf("%s out of range\n", argv[0]);
		unmap_sysmem(ptr);
		return 1;
	}

	/*
	 * Both commands share this function, so the name the user typed is
	 * what says which one is running
	 */
	if (IS_ENABLED(CONFIG_CMD_WRITE) && argv[0][0] == 'w')
		res = blk_dwrite(dev_desc, offset + blk, cnt, ptr);
	else
		res = blk_dread(dev_desc, offset + blk, cnt, ptr);
	unmap_sysmem(ptr);

	if (res != cnt) {
		printf("%s error\n", argv[0]);
		return 1;
	}

	return 0;
}

#ifdef CONFIG_CMD_READ
U_BOOT_CMD_NOOPTS(
	read,	6,	0,	do_rw,
	"Load binary data from a partition",
	"<interface> <dev[:part|#partname]> addr blk# cnt"
);
#endif

#ifdef CONFIG_CMD_WRITE
U_BOOT_CMD_NOOPTS(
	write,	6,	0,	do_rw,
	"Store binary data to a partition",
	"<interface> <dev[:part|#partname]> addr blk# cnt"
);
#endif
