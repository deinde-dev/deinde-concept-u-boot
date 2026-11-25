// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the blkmap command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <blk.h>
#include <blkmap.h>
#include <command.h>
#include <dm.h>
#include <env.h>
#include <malloc.h>
#include <mapmem.h>
#include <test/cmd.h>
#include <test/ut.h>

/* Block size of a blkmap device */
#define BLKSZ		512

/* Blocks in the memory region which the tests map */
#define BLOCKS		8

/* Environment variable which the 'get' sub-command writes */
#define DEVNUM_VAR	"blkmap_devnum"

/*
 * Every test here carries UTF_DM, since it creates block devices of its own;
 * see the note in test/cmd/clone.c
 */

/**
 * make_mem() - Allocate memory for a mem-backed blkmap device
 *
 * The memory starts out zeroed, so a test can tell blocks the command has
 * written from untouched ones.
 *
 * @uts: Test state
 * @blocks: Number of blocks to allocate
 * @memp: Returns a pointer to the memory, which the caller frees
 * @addrp: Returns the address to give the command
 * Return: 0 if OK, 1 on failure
 */
static int make_mem(struct unit_test_state *uts, uint blocks, void **memp,
		    ulong *addrp)
{
	void *mem;

	mem = calloc(blocks, BLKSZ);
	ut_assertnonnull(mem);
	*memp = mem;
	*addrp = (ulong)map_to_sysmem(mem);

	return 0;
}

/**
 * save_env() - Take a copy of an environment variable
 *
 * @uts: Test state
 * @name: Name of the variable to save
 * @savedp: Returns a copy of the value, or NULL if it is unset
 * Return: 0 if OK, 1 on failure
 */
static int save_env(struct unit_test_state *uts, const char *name,
		    char **savedp)
{
	const char *val = env_get(name);

	*savedp = NULL;
	if (val) {
		*savedp = strdup(val);
		ut_assertnonnull(*savedp);
	}

	return 0;
}

/* Test creating a device, mapping memory into it and destroying it again */
static int cmd_test_blkmap_base(struct unit_test_state *uts)
{
	struct udevice *dev;
	char *saved;
	void *mem;
	ulong addr;

	ut_assertok(save_env(uts, DEVNUM_VAR, &saved));

	/* nothing is listed until a device exists */
	ut_assertok(run_command("blkmap info", 0));
	ut_assert_console_end();

	ut_assertok(run_command("blkmap create ramdisk", 0));
	ut_assert_nextline("Created \"ramdisk\"");
	ut_assert_console_end();

	ut_assertnonnull(dev = blkmap_from_label("ramdisk"));

	ut_assertok(make_mem(uts, BLOCKS, &mem, &addr));
	ut_assertok(run_commandf("blkmap map ramdisk 0 %x mem 0x%lx", BLOCKS,
				 addr));
	ut_assert_nextline("Block 0x0+0x8 mapped to 0x%lx", addr);
	ut_assert_console_end();

	/* the device is as large as the region mapped into it */
	ut_assertok(run_command("blkmap info", 0));
	ut_assert_nextline("Device %d: Vendor: U-Boot Rev: 1.0 Prod: blkmap",
			   dev_seq(dev));
	ut_assert_nextline("            Type: Hard Disk");
	ut_assert_nextline("            Capacity: 0.0 MB = 0.0 GB (8 x 512)");
	ut_assert_console_end();

	/* with no variable named, the device number is printed */
	ut_assertok(run_command("blkmap get ramdisk dev", 0));
	ut_assert_nextline("%d", dev_seq(dev));
	ut_assert_console_end();

	ut_assertok(run_commandf("blkmap get ramdisk dev %s", DEVNUM_VAR));
	ut_asserteq(dev_seq(dev), env_get_hex(DEVNUM_VAR, 0xdeadbeef));
	ut_assert_console_end();

	ut_assertok(run_command("blkmap destroy ramdisk", 0));
	ut_assert_nextline("Destroyed \"ramdisk\"");
	ut_assert_console_end();

	ut_assertok(run_command("blkmap info", 0));
	ut_assert_console_end();

	free(mem);
	ut_assertok(env_set(DEVNUM_VAR, saved));
	free(saved);

	return 0;
}
CMD_TEST(cmd_test_blkmap_base,
	 UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA | UTF_SCAN_FDT);

/* Test selecting a device and reading and writing its blocks */
static int cmd_test_blkmap_rw(struct unit_test_state *uts)
{
	struct udevice *dev;
	void *mem, *buf;
	ulong addr, bufaddr;
	u8 *data;
	uint i;

	ut_assertok(run_command("blkmap create disk", 0));
	ut_assert_nextline("Created \"disk\"");
	ut_assertnonnull(dev = blkmap_from_label("disk"));

	ut_assertok(make_mem(uts, BLOCKS, &mem, &addr));
	ut_assertok(make_mem(uts, BLOCKS, &buf, &bufaddr));
	ut_assertok(run_commandf("blkmap map disk 0 %x mem 0x%lx", BLOCKS,
				 addr));
	console_record_reset();

	/* give every byte of the device a different value */
	data = mem;
	for (i = 0; i < BLOCKS * BLKSZ; i++)
		data[i] = i;

	ut_assertok(run_commandf("blkmap dev %d", dev_seq(dev)));
	ut_assert_nextline_empty();
	ut_assert_nextline("Device %d: Vendor: U-Boot Rev: 1.0 Prod: blkmap",
			   dev_seq(dev));
	ut_assert_nextline("            Type: Hard Disk");
	ut_assert_nextline("            Capacity: 0.0 MB = 0.0 GB (8 x 512)");
	ut_assert_nextline("... is now current device");
	ut_assert_console_end();

	/* a memory-backed device has no partition table */
	ut_assertok(run_command("blkmap part", 0));
	ut_assert_nextline_empty();
	ut_assert_nextline("no blkmap partition table available");
	ut_assert_console_end();

	/* read the first two blocks into the second buffer */
	ut_assertok(run_commandf("blkmap read 0x%lx 0 2", bufaddr));
	ut_assert_nextline_empty();
	ut_assert_nextline(
		"blkmap read: device %d block # 0, count 2 ... 2 blocks read: OK",
		dev_seq(dev));
	ut_assert_console_end();
	ut_asserteq_mem(mem, buf, 2 * BLKSZ);

	/* write them back to the end of the device */
	ut_assertok(run_commandf("blkmap write 0x%lx 6 2", bufaddr));
	ut_assert_nextline_empty();
	ut_assert_nextline(
		"blkmap write: device %d block # 6, count 2 ... 2 blocks written: OK",
		dev_seq(dev));
	ut_assert_console_end();
	ut_asserteq_mem(mem, mem + 6 * BLKSZ, 2 * BLKSZ);

	ut_assertok(run_command("blkmap destroy disk", 0));
	console_record_reset();

	free(buf);
	free(mem);

	return 0;
}
CMD_TEST(cmd_test_blkmap_rw,
	 UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA | UTF_SCAN_FDT);

/* Test mapping part of one blkmap device into another */
static int cmd_test_blkmap_linear(struct unit_test_state *uts)
{
	struct udevice *dev, *lin;
	void *mem, *buf;
	ulong addr, bufaddr;
	u8 *data;
	uint i;

	ut_assertok(run_command("blkmap create disk", 0));
	ut_assertnonnull(dev = blkmap_from_label("disk"));
	ut_assertok(make_mem(uts, BLOCKS, &mem, &addr));
	ut_assertok(run_commandf("blkmap map disk 0 %x mem 0x%lx", BLOCKS,
				 addr));

	/* give every byte of the device a different value */
	data = mem;
	for (i = 0; i < BLOCKS * BLKSZ; i++)
		data[i] = i;

	ut_assertok(run_command("blkmap create window", 0));
	ut_assertnonnull(lin = blkmap_from_label("window"));
	console_record_reset();

	/* the window shows the last four blocks of the first device */
	ut_assertok(run_commandf("blkmap map window 0 4 linear blkmap %d 4",
				 dev_seq(dev)));
	ut_assert_nextline(
		"Block 0x0+0x4 mapped to block 0x4 of \"blkmap %d\"",
		dev_seq(dev));
	ut_assert_console_end();

	ut_assertok(make_mem(uts, BLOCKS, &buf, &bufaddr));
	ut_assertok(run_commandf("blkmap dev %d", dev_seq(lin)));
	console_record_reset();

	ut_assertok(run_commandf("blkmap read 0x%lx 0 4", bufaddr));
	ut_assert_nextline_empty();
	ut_assert_nextline(
		"blkmap read: device %d block # 0, count 4 ... 4 blocks read: OK",
		dev_seq(lin));
	ut_assert_console_end();
	ut_asserteq_mem(mem + 4 * BLKSZ, buf, 4 * BLKSZ);

	ut_assertok(run_command("blkmap destroy window", 0));
	ut_assertok(run_command("blkmap destroy disk", 0));
	console_record_reset();

	free(buf);
	free(mem);

	return 0;
}
CMD_TEST(cmd_test_blkmap_linear,
	 UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA | UTF_SCAN_FDT);

/* Test what the command reports when it cannot do what it is asked */
static int cmd_test_blkmap_errors(struct unit_test_state *uts)
{
	struct udevice *dev;
	void *mem;
	ulong addr;

	ut_assertok(run_command("blkmap create disk", 0));
	ut_assertnonnull(dev = blkmap_from_label("disk"));
	console_record_reset();

	/* a label can only name one device */
	ut_asserteq(1, run_command("blkmap create disk", 0));
	ut_assert_nextline("Unable to create \"disk\": -16");
	ut_assert_console_end();

	/* the other sub-commands all want a label which exists */
	ut_asserteq(1, run_command("blkmap destroy other", 0));
	ut_assert_nextline("\"other\" is not the name of any known blkmap");
	ut_assert_console_end();

	ut_asserteq(1, run_command("blkmap get other dev", 0));
	ut_assert_nextline("\"other\" is not the name of any known blkmap");
	ut_assert_console_end();

	ut_asserteq(1, run_command("blkmap map other 0 1 mem 0x1000", 0));
	ut_assert_nextline("\"other\" is not the name of any known blkmap");
	ut_assert_console_end();

	/* a mapping has to be of a type the command knows */
	ut_asserteq(1, run_command("blkmap map disk 0 1 other 0x1000", 0));
	ut_assert_nextline("Unknown map type \"other\"");
	ut_assert_nextline("blkmap - Composeable virtual block devices");

	/* the rest is the usage text, which this test has nothing to say about */
	console_record_reset();

	/* a linear mapping has to name a device which exists */
	ut_asserteq(1, run_command("blkmap map disk 0 1 linear blkmap 99 0", 0));
	ut_assert_nextline("Found no device matching \"blkmap 99\"");
	ut_assert_console_end();

	/* two mappings cannot cover the same block */
	ut_assertok(make_mem(uts, BLOCKS, &mem, &addr));
	ut_assertok(run_commandf("blkmap map disk 0 %x mem 0x%lx", BLOCKS,
				 addr));
	console_record_reset();

	ut_asserteq(1, run_commandf("blkmap map disk 4 1 mem 0x%lx", addr));
	ut_assert_nextline("Unable to map 0x%lx at block 0x4: -16", addr);
	ut_assert_console_end();

	ut_assertok(run_command("blkmap destroy disk", 0));
	console_record_reset();

	free(mem);

	return 0;
}
CMD_TEST(cmd_test_blkmap_errors,
	 UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA | UTF_SCAN_FDT);

/**
 * expect_usage() - Run a command which shows the usage message
 *
 * @uts: Test state
 * @cmd: Command to run
 * Return: 0 if OK, 1 on failure
 */
static int expect_usage(struct unit_test_state *uts, const char *cmd)
{
	ut_asserteq(1, run_command(cmd, 0));
	ut_assert_nextline("blkmap - Composeable virtual block devices");

	/* the rest is the usage text, which this test has nothing to say about */
	console_record_reset();

	return 0;
}

/* Test the arguments which show the usage message */
static int cmd_test_blkmap_usage(struct unit_test_state *uts)
{
	ut_assertok(run_command("blkmap create disk", 0));
	console_record_reset();

	/* a sub-command is required, and must be one of those on offer */
	ut_assertok(expect_usage(uts, "blkmap"));
	ut_assertok(expect_usage(uts, "blkmap other"));

	/* 'create' and 'destroy' take a label and nothing else */
	ut_assertok(expect_usage(uts, "blkmap create"));
	ut_assertok(expect_usage(uts, "blkmap create disk other"));
	ut_assertok(expect_usage(uts, "blkmap destroy"));

	/* 'get' knows only how to look up a device number */
	ut_assertok(expect_usage(uts, "blkmap get disk"));
	ut_assertok(expect_usage(uts, "blkmap get disk other"));

	/* 'map' wants a label, a block number, a count and a type */
	ut_assertok(expect_usage(uts, "blkmap map disk 0 1"));
	ut_assertok(expect_usage(uts, "blkmap map disk 0 1 mem"));
	ut_assertok(expect_usage(uts, "blkmap map disk 0 1 linear blkmap"));

	/* the sub-commands which blk_common_cmd() handles want their own */
	ut_assertok(expect_usage(uts, "blkmap info other"));
	ut_assertok(expect_usage(uts, "blkmap read 0x1000 0"));

	/* no sub-command takes an option, so each refuses one */
	ut_assertok(expect_usage(uts, "blkmap create -x"));
	ut_assertok(expect_usage(uts, "blkmap destroy -x"));
	ut_assertok(expect_usage(uts, "blkmap get -x dev"));
	ut_assertok(expect_usage(uts, "blkmap map -x 0 1 mem 0x1000"));
	ut_assertok(expect_usage(uts, "blkmap dev -1"));

	ut_assertok(run_command("blkmap destroy disk", 0));
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_blkmap_usage,
	 UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA | UTF_SCAN_FDT);
