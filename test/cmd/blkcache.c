// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the blkcache command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <blk.h>
#include <blkmap.h>
#include <command.h>
#include <dm.h>
#include <malloc.h>
#include <test/cmd.h>
#include <test/ut.h>

/* Block size of a blkmap device */
#define BLKSZ		512

/* Blocks in the device the tests read from */
#define DEV_BLOCKS	8

/* Blocks read at a time, which is more than one entry can hold */
#define LONG_BLOCKS	2

/*
 * The tests which read from a device carry UTF_DM, even though the device is
 * created by the test rather than read from the devicetree. Without it the
 * test runs against whatever tree the last test left behind, where
 * of_live_active() no longer matches the ofnode the root device was bound
 * with, and probing a device whose parent is that root reads the wrong
 * devicetree
 */
#define DEV_FLAGS	(UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA | UTF_SCAN_FDT)

/* Test the statistics report and the limits the cache starts out with */
static int cmd_test_blkcache_base(struct unit_test_state *uts)
{
	/* every test starts with an empty cache and the default limits */
	ut_assertok(run_command("blkcache show", 0));
	ut_assert_nextline("hits: 0");
	ut_assert_nextline("misses: 0");
	ut_assert_nextline("entries: 0");
	ut_assert_nextline("max blocks/entry: %d", BLKCACHE_DEF_BLOCKS);
	ut_assert_nextline("max cache entries: %d", BLKCACHE_DEF_ENTRIES);
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_blkcache_base, UTF_CONSOLE);

/* Test the arguments the command refuses */
static int cmd_test_blkcache_usage(struct unit_test_state *uts)
{
	/* the command does nothing without a sub-command */
	ut_asserteq(1, run_command("blkcache", 0));
	ut_assert_nextline("blkcache - block cache diagnostics and control");
	ut_assert_nextline_empty();

	/* the rest of the message is there only where the long help is */
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_nextline("blkcache show - show and reset statistics");
		/*
		 * The line for configure is too long to write out as one
		 * string here, so check only that it starts as expected
		 */
		ut_assert_nextlinen("blkcache configure <blocks> <entries> -");
		ut_assert_nextline_empty();
	}
	ut_assert_console_end();

	/* a sub-command which does not exist shows the usage message */
	ut_asserteq(1, run_command("blkcache bogus", 0));
	ut_assert_nextline("blkcache - block cache diagnostics and control");
	console_record_reset();

	/* configure needs both of its arguments */
	ut_asserteq(1, run_command("blkcache configure 8", 0));
	ut_assert_nextline("blkcache - block cache diagnostics and control");
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_blkcache_usage, UTF_CONSOLE);

/**
 * struct cache_dev - A memory-backed block device for the tests
 *
 * @dev: blkmap device
 * @blk: Block device the blkmap device provides, which is what reads go to
 * @mem: Memory holding the contents of the device
 * @buf: Buffer which a read delivers its blocks into
 */
struct cache_dev {
	struct udevice *dev;
	struct udevice *blk;
	void *mem;
	void *buf;
};

/**
 * make_dev() - Create a block device to read from
 *
 * The device is backed by memory, so a read costs nothing and always
 * delivers the same blocks. Creating it already reads from it through
 * the cache, so the cache is reset afterwards to give the test a clean start.
 *
 * @uts: Test state
 * @cd: Device to fill in
 * Return: 0 if OK, other value on error
 */
static int make_dev(struct unit_test_state *uts, struct cache_dev *cd)
{
	cd->mem = calloc(1, DEV_BLOCKS * BLKSZ);
	ut_assertnonnull(cd->mem);
	cd->buf = calloc(1, LONG_BLOCKS * BLKSZ);
	ut_assertnonnull(cd->buf);

	ut_assertok(blkmap_create("blkcache-test", &cd->dev));
	ut_assertok(blkmap_map_mem(cd->dev, 0, DEV_BLOCKS, cd->mem));
	ut_assertok(blk_get_from_parent(cd->dev, &cd->blk));
	blkcache_reset();

	return 0;
}

/**
 * free_dev() - Drop a device made by make_dev()
 *
 * The cache may still hold entries for the device, but the test framework
 * empties the cache around every test, so they are not dropped here.
 *
 * @uts: Test state
 * @cd: Device to drop
 * Return: 0 if OK, other value on error
 */
static int free_dev(struct unit_test_state *uts, struct cache_dev *cd)
{
	ut_assertok(blkmap_destroy(cd->dev));
	free(cd->buf);
	free(cd->mem);

	return 0;
}

/* Test that the command counts the hits and misses of real reads */
static int cmd_test_blkcache_stats(struct unit_test_state *uts)
{
	struct cache_dev cd;

	ut_assertok(make_dev(uts, &cd));

	/* the first read of a block finds nothing, so it misses and is kept */
	ut_asserteq(1, blk_read(cd.blk, 0, 1, cd.buf));
	ut_assertok(run_command("blkcache show", 0));
	ut_assert_nextline("hits: 0");
	ut_assert_nextline("misses: 1");
	ut_assert_nextline("entries: 1");
	ut_assert_nextline("max blocks/entry: %d", BLKCACHE_DEF_BLOCKS);
	ut_assert_nextline("max cache entries: %d", BLKCACHE_DEF_ENTRIES);
	ut_assert_console_end();

	/*
	 * Reading the same block again finds it in the cache. The report above
	 * reset the counts, so these cover that read alone
	 */
	ut_asserteq(1, blk_read(cd.blk, 0, 1, cd.buf));
	ut_assertok(run_command("blkcache show", 0));
	ut_assert_nextline("hits: 1");
	ut_assert_nextline("misses: 0");
	ut_assert_nextline("entries: 1");
	ut_assert_nextline("max blocks/entry: %d", BLKCACHE_DEF_BLOCKS);
	ut_assert_nextline("max cache entries: %d", BLKCACHE_DEF_ENTRIES);
	ut_assert_console_end();

	ut_assertok(free_dev(uts, &cd));

	return 0;
}
CMD_TEST(cmd_test_blkcache_stats, DEV_FLAGS);

/* Test that the limits configure sets decide what is cached */
static int cmd_test_blkcache_limits(struct unit_test_state *uts)
{
	struct cache_dev cd;

	ut_assertok(make_dev(uts, &cd));

	/* with no entries allowed nothing is kept, so every read misses */
	ut_assertok(run_commandf("blkcache configure %d 0",
				 BLKCACHE_DEF_BLOCKS));
	ut_assert_nextline("changed to max of 0 entries of %d blocks each",
			   BLKCACHE_DEF_BLOCKS);
	ut_assert_console_end();

	ut_asserteq(1, blk_read(cd.blk, 0, 1, cd.buf));
	ut_asserteq(1, blk_read(cd.blk, 0, 1, cd.buf));
	ut_assertok(run_command("blkcache show", 0));
	ut_assert_nextline("hits: 0");
	ut_assert_nextline("misses: 2");
	ut_assert_nextline("entries: 0");
	ut_assert_nextline("max blocks/entry: %d", BLKCACHE_DEF_BLOCKS);
	ut_assert_nextline("max cache entries: 0");
	ut_assert_console_end();

	/* a read longer than an entry can hold is not kept either */
	ut_assertok(run_commandf("blkcache configure 1 %d",
				 BLKCACHE_DEF_ENTRIES));
	ut_assert_nextline("changed to max of %d entries of 1 blocks each",
			   BLKCACHE_DEF_ENTRIES);
	ut_assert_console_end();

	ut_asserteq(LONG_BLOCKS, blk_read(cd.blk, 0, LONG_BLOCKS, cd.buf));
	ut_asserteq(LONG_BLOCKS, blk_read(cd.blk, 0, LONG_BLOCKS, cd.buf));
	ut_assertok(run_command("blkcache show", 0));
	ut_assert_nextline("hits: 0");
	ut_assert_nextline("misses: 2");
	ut_assert_nextline("entries: 0");
	ut_assert_nextline("max blocks/entry: 1");
	ut_assert_nextline("max cache entries: %d", BLKCACHE_DEF_ENTRIES);
	ut_assert_console_end();

	ut_assertok(free_dev(uts, &cd));

	return 0;
}
CMD_TEST(cmd_test_blkcache_limits, DEV_FLAGS);
