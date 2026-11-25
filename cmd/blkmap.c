// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2023 Addiva Elektronik
 * Author: Tobias Waldekranz <tobias@waldekranz.com>
 */

#include <blk.h>
#include <blkmap.h>
#include <command.h>
#include <env.h>
#include <getopt.h>
#include <malloc.h>
#include <dm/device.h>

static int blkmap_curr_dev;

struct map_ctx {
	struct udevice *dev;
	lbaint_t blknr, blkcnt;
};

typedef int (*map_parser_fn)(struct map_ctx *ctx, int argc, char *const argv[]);

struct map_handler {
	const char *name;
	map_parser_fn fn;
};

static int do_blkmap_map_linear(struct map_ctx *ctx, int argc,
				char *const argv[])
{
	struct udevice *ldev;
	int err, ldevnum;
	lbaint_t lblknr;

	if (argc < 4)
		return CMD_RET_USAGE;

	ldevnum = dectoul(argv[2], NULL);
	lblknr = dectoul(argv[3], NULL);

	err = blk_get_devnum_by_uclass_idname(argv[1], ldevnum, &ldev);
	if (err)
		return cmd_fail(err, "Found no device matching \"%s %d\"\n",
				argv[1], ldevnum);

	err = blkmap_map_linear(ctx->dev, ctx->blknr, ctx->blkcnt,
				ldev, lblknr);
	if (err)
		return cmd_fail(err,
				"Unable to map \"%s %d\" at block 0x" LBAF ": %d\n",
				argv[1], ldevnum, ctx->blknr, err);

	printf("Block 0x" LBAF "+0x" LBAF " mapped to block 0x" LBAF " of \"%s %d\"\n",
	       ctx->blknr, ctx->blkcnt, lblknr, argv[1], ldevnum);
	return CMD_RET_SUCCESS;
}

static int do_blkmap_map_mem(struct map_ctx *ctx, int argc, char *const argv[])
{
	phys_addr_t addr;
	int err;
	bool preserve = false;

	if (argc < 2)
		return CMD_RET_USAGE;

	addr = hextoul(argv[1], NULL);

	if (argc == 3 && !strcmp(argv[2], "preserve"))
		preserve = true;

	err = blkmap_map_pmem(ctx->dev, ctx->blknr, ctx->blkcnt, addr,
			      preserve);
	if (err)
		return cmd_fail(err, "Unable to map %#llx at block 0x" LBAF ": %d\n",
				(unsigned long long)addr, ctx->blknr, err);

	printf("Block 0x" LBAF "+0x" LBAF " mapped to %#llx\n",
	       ctx->blknr, ctx->blkcnt, (unsigned long long)addr);
	return CMD_RET_SUCCESS;
}

static struct map_handler map_handlers[] = {
	{ .name = "linear", .fn = do_blkmap_map_linear },
	{ .name = "mem", .fn = do_blkmap_map_mem },

	{ .name = NULL }
};

static int do_blkmap_map(struct getopt_state *gs)
{
	int argc = gs->argc;
	char *const *argv = gs->argv;
	struct map_handler *handler;
	struct map_ctx ctx;

	if (argc < 5)
		return CMD_RET_USAGE;

	ctx.dev = blkmap_from_label(argv[1]);
	if (!ctx.dev)
		return cmd_fail(0,
				"\"%s\" is not the name of any known blkmap\n",
				argv[1]);

	ctx.blknr = hextoul(argv[2], NULL);
	ctx.blkcnt = hextoul(argv[3], NULL);
	argc -= 4;
	argv += 4;

	for (handler = map_handlers; handler->name; handler++) {
		if (!strcmp(handler->name, argv[0]))
			return handler->fn(&ctx, argc, argv);
	}

	cmd_err("Unknown map type \"%s\"\n", argv[0]);

	return CMD_RET_USAGE;
}

static int do_blkmap_create(struct getopt_state *gs)
{
	const char *label;
	int err;

	if (gs->argc != 2)
		return CMD_RET_USAGE;

	label = gs->argv[1];

	err = blkmap_create(label, NULL);
	if (err)
		return cmd_fail(err, "Unable to create \"%s\": %d\n", label,
				err);

	printf("Created \"%s\"\n", label);
	return CMD_RET_SUCCESS;
}

static int do_blkmap_destroy(struct getopt_state *gs)
{
	struct udevice *dev;
	const char *label;
	int err;

	if (gs->argc != 2)
		return CMD_RET_USAGE;

	label = gs->argv[1];

	dev = blkmap_from_label(label);
	if (!dev)
		return cmd_fail(0,
				"\"%s\" is not the name of any known blkmap\n",
				label);

	err = blkmap_destroy(dev);
	if (err)
		return cmd_fail(err, "Unable to destroy \"%s\": %d\n", label,
				err);

	printf("Destroyed \"%s\"\n", label);
	return CMD_RET_SUCCESS;
}

static int do_blkmap_get(struct getopt_state *gs)
{
	int argc = gs->argc;
	char *const *argv = gs->argv;
	struct udevice *dev;
	const char *label;
	int err;

	if (argc < 3)
		return CMD_RET_USAGE;

	label = argv[1];

	dev = blkmap_from_label(label);
	if (!dev)
		return cmd_fail(0,
				"\"%s\" is not the name of any known blkmap\n",
				label);

	if (!strcmp(argv[2], "dev")) {
		if (argc == 3) {
			printf("%d\n", dev_seq(dev));
		} else {
			err = env_set_hex(argv[3], dev_seq(dev));
			if (err)
				return CMD_RET_FAILURE;
		}
	} else {
		return CMD_RET_USAGE;
	}

	return CMD_RET_SUCCESS;
}

static int do_blkmap_common(struct getopt_state *gs)
{
	return blk_common_subcmd(gs->argc, gs->argv, UCLASS_BLKMAP,
				 &blkmap_curr_dev);
}

U_BOOT_CMD_WITH_SUBCMDS(
	blkmap, "Composeable virtual block devices",
	"info - list configured devices\n"
	"blkmap part - list available partitions on current blkmap device\n"
	"blkmap dev [<dev>] - show or set current blkmap device\n"
	"blkmap read <addr> <blk#> <cnt>\n"
	"blkmap write <addr> <blk#> <cnt>\n"
	"blkmap get <label> dev [<var>] - store device number in variable\n"
	"blkmap create <label> - create device\n"
	"blkmap destroy <label> - destroy device\n"
	"blkmap map <label> <blk#> <cnt> linear <interface> <dev> <blk#> - device mapping\n"
	"blkmap map <label> <blk#> <cnt> mem <addr> [preserve] - memory mapping\n",
	U_BOOT_CMD_MKENT_NOOPTS(info, 2, 1, do_blkmap_common, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(part, 2, 1, do_blkmap_common, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(dev, 4, 1, do_blkmap_common, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(read, 5, 1, do_blkmap_common, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(write, 5, 1, do_blkmap_common, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(get, 5, 1, do_blkmap_get, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(create, 2, 1, do_blkmap_create, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(destroy, 2, 1, do_blkmap_destroy, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(map, 32, 1, do_blkmap_map, "", ""));
