// SPDX-License-Identifier: GPL-2.0
/*
 * VFS commands - 'fs mount', 'fs umount', 'fs ls'
 *
 * Provides a new 'fs' command with subcommands for the virtual filesystem
 * layer, co-existing with the legacy filesystem commands in cmd/fs.c.
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <dm.h>
#include <fs.h>
#include <getopt.h>
#include <vfs.h>
#include <dm/uclass.h>

static int fs_mount_handler(int argc, char *const argv[])
{
	struct udevice *vfs, *fsdev, *dir, *mnt;
	const char *subpath;
	int ret;

	vfs = vfs_root();
	if (!vfs)
		return -ENXIO;

	if (argc < 2) {
		vfs_print_mounts();
		return 0;
	}

	if (argc < 3)
		return -EINVAL;

	/* check whether something is mounted there already */
	ret = vfs_find_mount(vfs, argv[2], &mnt, &subpath);
	if (!ret && mnt && !*subpath)
		return -EBUSY;

	ret = uclass_get_device_by_name(UCLASS_FS, argv[1], &fsdev);
	if (ret)
		return ret;

	ret = vfs_resolve(vfs, argv[2], &dir);
	if (ret)
		return ret;

	return vfs_mount(vfs, dir, fsdev);
}

static int do_fs_mount(struct getopt_state *gs)
{
	int ret;

	if (gs->argc == 2)
		return CMD_RET_USAGE;

	ret = fs_mount_handler(gs->argc, gs->argv);
	if (ret)
		return cmd_fail(ret, "fs mount failed: %dE\n", ret);

	return CMD_RET_SUCCESS;
}

static int fs_umount_handler(const char *path)
{
	struct udevice *vfs;

	vfs = vfs_root();
	if (!vfs)
		return -ENXIO;

	return vfs_umount_path(vfs, path);
}

static int do_fs_umount(struct getopt_state *gs)
{
	const char *path;
	bool all = false;
	int opt, ret;

	while ((opt = getopt(gs, "+a")) > 0) {
		switch (opt) {
		case 'a':
			all = true;
			break;
		default:
			return CMD_RET_USAGE;
		}
	}

	if (all) {
		ret = vfs_umount_all();
		if (ret)
			return cmd_fail(ret, "fs umount -a failed: %dE\n", ret);

		return CMD_RET_SUCCESS;
	}

	path = getopt_pop(gs);
	if (!path)
		return CMD_RET_USAGE;

	ret = fs_umount_handler(path);
	if (ret)
		return cmd_fail(ret, "fs umount failed: %dE\n", ret);

	return CMD_RET_SUCCESS;
}

static int do_fs_ls(struct getopt_state *gs)
{
	const char *path = gs->argc >= 2 ? gs->argv[1] : "/";
	int ret;

	ret = vfs_ls(path);
	if (ret)
		return cmd_fail(ret, "fs ls failed: %dE\n", ret);

	return CMD_RET_SUCCESS;
}

U_BOOT_LONGHELP(fs,
	"mount [<dev> <mountpoint>]  - list or create mounts\n"
	"fs umount <mountpoint> | -a    - unmount one or all filesystems\n"
	"fs ls [<path>]                 - list a directory (default /)\n"
	"fs cp <source> <dest>          - copy a file");

U_BOOT_CMD_WITH_SUBCMDS(fs, "Filesystem operations", fs_help_text,
	U_BOOT_CMD_MKENT_NOOPTS(mount, 3, 1, do_fs_mount, "", ""),
	U_BOOT_CMD_MKENT_GETOPT(umount, 2, 1, do_fs_umount, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(ls, 2, 1, do_fs_ls, "", ""),
	U_BOOT_CMD_MKENT_NOOPTS(cp, 3, 0, do_cp, "", ""));
