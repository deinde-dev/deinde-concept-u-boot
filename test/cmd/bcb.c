// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the bcb command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <bcb.h>
#include <blkmap.h>
#include <command.h>
#include <dm.h>
#include <env.h>
#include <errno.h>
#include <malloc.h>
#include <test/cmd.h>
#include <test/ut.h>

/* Block size of a blkmap device */
#define BLKSZ		512

/* Blocks in the device the tests create, enough for the GPT and the BCB */
#define BLOCKS		0x100

/* Where the 'misc' partition sits in the device, in bytes */
#define MISC_START	0x8000
#define MISC_SIZE	0x10000

/* Partition UUID, which the GPT needs and nothing here looks at */
#define MISC_UUID	"4ab5f3c8-0000-4000-8000-000000000001"

/*
 * Every test here carries UTF_DM, as the blkmap tests do, since the device it
 * creates must be bound in the devicetree the test is running against
 */

/**
 * struct bcb_dev - A memory-backed block device holding a BCB
 *
 * @dev: blkmap device
 * @mem: Memory holding the contents of the device
 * @seq: Device number, which is what the command takes as its <dev> argument
 */
struct bcb_dev {
	struct udevice *dev;
	void *mem;
	int seq;
};

/**
 * have_tools() - Check for the commands the tests need to make a partition
 *
 * The BCB lives in a partition, which the test writes with 'gpt write' since
 * nothing else can create one.
 *
 * Return: true if a device with a 'misc' partition can be made
 */
static bool have_tools(void)
{
	return IS_ENABLED(CONFIG_CMD_BLKMAP) && IS_ENABLED(CONFIG_CMD_GPT) &&
		IS_ENABLED(CONFIG_EFI_PARTITION) &&
		IS_ENABLED(CONFIG_PARTITION_UUIDS);
}

/**
 * make_dev() - Create a block device with an empty 'misc' partition
 *
 * The memory starts out zeroed, so the BCB in the partition reads as empty
 * strings until a test stores something there.
 *
 * @uts: Test state
 * @bd: Device to fill in
 * Return: 0 if OK, other value on error
 */
static int make_dev(struct unit_test_state *uts, struct bcb_dev *bd)
{
	bd->mem = calloc(1, BLOCKS * BLKSZ);
	ut_assertnonnull(bd->mem);

	ut_assertok(blkmap_create("bcb", &bd->dev));
	ut_assertok(blkmap_map_mem(bd->dev, 0, BLOCKS, bd->mem));
	bd->seq = dev_seq(bd->dev);

	ut_assertok(run_commandf(
		"gpt write blkmap %d name=misc,start=%#x,size=%#x,uuid=%s",
		bd->seq, MISC_START, MISC_SIZE, MISC_UUID));
	ut_assert_nextline("Writing GPT: success!");
	ut_assert_console_end();

	return 0;
}

/**
 * free_dev() - Drop a device made by make_dev()
 *
 * The command keeps a pointer to the device's descriptor, so that is dropped
 * as well; the device itself goes away with the test's devicetree.
 *
 * @uts: Test state
 * @bd: Device to drop
 * Return: 0 if OK, other value on error
 */
static int free_dev(struct unit_test_state *uts, struct bcb_dev *bd)
{
	bcb_reset();
	ut_assertok(blkmap_destroy(bd->dev));
	free(bd->mem);

	return 0;
}

/**
 * check_usage() - Check the usage message the command shows
 *
 * @uts: Test state
 * Return: 0 if OK, other value on error
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline(
		"bcb - Load/set/clear/test/dump/store Android BCB fields");
	ut_assert_nextline_empty();
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_skip_to_line(
			"              during 'bcb set' and used as separator by upper layers");
		ut_assert_nextline_empty();
	}
	ut_assert_console_end();

	return 0;
}

/* Test setting a field, storing it and reading it back */
static int cmd_test_bcb_base(struct unit_test_state *uts)
{
	struct bcb_dev bd;

	if (!have_tools())
		return -EAGAIN;
	ut_assertok(make_dev(uts, &bd));

	ut_assertok(run_commandf("bcb load blkmap %d misc", bd.seq));
	ut_assert_console_end();

	ut_assertok(run_command("bcb set command boot-recovery", 0));
	ut_assertok(run_command("bcb dump command", 0));
	ut_assert_nextline(
		"00000000: 62 6f 6f 74 2d 72 65 63 6f 76 65 72 79 00 00 00  boot-recovery...");
	ut_assert_nextline(
		"00000010: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................");
	ut_assert_console_end();

	/* the copy in memory is all that has changed so far */
	ut_assertok(run_command("bcb test command = boot-recovery", 0));
	ut_asserteq(1, run_command("bcb test command = bootonce-bootloader", 0));
	ut_assertok(run_command("bcb test command ~ recovery", 0));
	ut_assert_console_end();

	ut_assertok(run_command("bcb store", 0));
	ut_assert_console_end();

	/* reading it back from the device gives the same value */
	ut_assertok(run_command("bcb clear", 0));
	ut_asserteq(1, run_command("bcb test command = boot-recovery", 0));
	ut_assertok(run_commandf("bcb load blkmap %d misc", bd.seq));
	ut_assertok(run_command("bcb test command = boot-recovery", 0));
	ut_assert_console_end();

	ut_assertok(free_dev(uts, &bd));

	return 0;
}
CMD_TEST(cmd_test_bcb_base, UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA |
	 UTF_SCAN_FDT);

/* Test the fields the command knows and the values they take */
static int cmd_test_bcb_fields(struct unit_test_state *uts)
{
	struct bcb_dev bd;

	if (!have_tools())
		return -EAGAIN;
	ut_assertok(make_dev(uts, &bd));

	ut_assertok(run_commandf("bcb load blkmap %d 1", bd.seq));
	ut_assert_console_end();

	/* each field has its own offset in the block */
	ut_assertok(run_command("bcb set status fine", 0));
	ut_assertok(run_command("bcb set stage 1/2", 0));
	ut_assertok(run_command("bcb dump stage", 0));
	ut_assert_nextline(
		"00000340: 31 2f 32 00 00 00 00 00 00 00 00 00 00 00 00 00  1/2.............");
	ut_assert_nextline(
		"00000350: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................");
	ut_assert_console_end();

	/* a colon in the value becomes a line feed */
	ut_assertok(run_command("bcb set recovery recovery:--wipe_data", 0));
	ut_assertok(run_command("bcb test recovery ~ --wipe_data", 0));
	ut_assertok(run_command("bcb dump recovery", 0));
	ut_assert_nextline(
		"00000040: 72 65 63 6f 76 65 72 79 0a 2d 2d 77 69 70 65 5f  recovery.--wipe_");
	ut_assert_skip_to_line(
		"00000330: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................");
	ut_assert_console_end();

	/* clearing one field leaves the others alone */
	ut_assertok(run_command("bcb clear stage", 0));
	ut_asserteq(1, run_command("bcb test stage = 1/2", 0));
	ut_assertok(run_command("bcb test status = fine", 0));
	ut_assert_console_end();

	/* a field which does not exist, and a value too long for one */
	ut_asserteq(1, run_command("bcb set nosuch value", 0));
	ut_assert_nextline("Error: Unknown bcb field 'nosuch'");
	ut_asserteq(1, run_command("bcb dump nosuch", 0));
	ut_assert_nextline("Error: Unknown bcb field 'nosuch'");
	ut_asserteq(1, run_command("bcb clear nosuch", 0));
	ut_assert_nextline("Error: Unknown bcb field 'nosuch'");
	ut_asserteq(1, run_command(
		"bcb set command 0123456789012345678901234567890123", 0));
	ut_assert_nextline(
		"Error: sizeof('0123456789012345678901234567890123') = 34 >= 32 = sizeof(bcb.command)");
	ut_assert_console_end();

	/* 'test' takes only the two operators */
	ut_asserteq(1, run_command("bcb test status ! fine", 0));
	ut_assert_nextline("Error: Unknown operator '!'");
	ut_assert_console_end();

	ut_assertok(free_dev(uts, &bd));

	return 0;
}
CMD_TEST(cmd_test_bcb_fields, UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA |
	 UTF_SCAN_FDT);

/* Test the devices and partitions the load sub-command accepts */
static int cmd_test_bcb_load(struct unit_test_state *uts)
{
	struct bcb_dev bd;

	if (!have_tools())
		return -EAGAIN;
	ut_assertok(make_dev(uts, &bd));

	/* the interface may be left out, in which case mmc is used */
	ut_asserteq(1, run_command("bcb load 99 1", 0));
	ut_assert_nextline("Error: mmc 99: read failed (-19)");
	ut_assert_console_end();

	ut_asserteq(1, run_commandf("bcb load blkmap %d nosuch", bd.seq));
	ut_assert_nextlinen("Error: blkmap ");
	ut_assert_console_end();

	ut_asserteq(1, run_command("bcb load blkmap x 1", 0));
	ut_assert_nextline("Error: Device id 'x' not a number");
	ut_assert_console_end();

	ut_assertok(free_dev(uts, &bd));

	return 0;
}
CMD_TEST(cmd_test_bcb_load, UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA |
	 UTF_SCAN_FDT);

/* Test that every sub-command but load needs a BCB in memory */
static int cmd_test_bcb_notloaded(struct unit_test_state *uts)
{
	/* an earlier test may have left a block loaded */
	bcb_reset();

	ut_asserteq(1, run_command("bcb set command boot-recovery", 0));
	ut_assert_nextline("Error: Please, load BCB first!");
	ut_asserteq(1, run_command("bcb clear", 0));
	ut_assert_nextline("Error: Please, load BCB first!");
	ut_asserteq(1, run_command("bcb test command = x", 0));
	ut_assert_nextline("Error: Please, load BCB first!");
	ut_asserteq(1, run_command("bcb dump command", 0));
	ut_assert_nextline("Error: Please, load BCB first!");
	ut_asserteq(1, run_command("bcb store", 0));
	ut_assert_nextline("Error: Please, load BCB first!");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_bcb_notloaded, UTF_CONSOLE);

/* Test choosing an A/B slot and reporting the metadata */
static int cmd_test_bcb_ab(struct unit_test_state *uts)
{
	struct bcb_dev bd;

	if (!have_tools() || !IS_ENABLED(CONFIG_ANDROID_AB))
		return -EAGAIN;
	ut_assertok(make_dev(uts, &bd));

	/*
	 * The metadata is empty, so the first call writes fresh metadata and
	 * takes the first slot. The log lines which explain that are skipped,
	 * since the log format depends on the build
	 */
	ut_assertok(run_commandf("bcb ab_select slot blkmap %d#misc", bd.seq));
	ut_assert_skip_to_line("ANDROID: Booting slot: a");
	ut_assert_console_end();
	ut_asserteq_str("a", env_get("slot"));

	/* the attempt is registered, so the next call moves to the other slot */
	ut_assertok(run_commandf("bcb ab_select slot blkmap %d#misc", bd.seq));
	ut_assert_skip_to_line("ANDROID: Booting slot: b");
	ut_assert_console_end();
	ut_asserteq_str("b", env_get("slot"));

	/*
	 * Both slots now have six tries left, so the first is chosen again.
	 * With --no-dec it keeps its six, as the dump below shows
	 */
	ut_assertok(run_commandf("bcb ab_select slot blkmap %d#misc --no-dec",
				 bd.seq));
	ut_assert_skip_to_line("ANDROID: Booting slot: a");
	ut_assert_console_end();

	ut_assertok(run_commandf("bcb ab_dump blkmap %d#misc", bd.seq));
	ut_assert_nextline("Bootloader Control:       [misc]");
	ut_assert_nextline("Active Slot:              _a");
	ut_assert_nextline("Magic Number:             0x42414342");
	ut_assert_nextline("Version:                  1");
	ut_assert_nextline("Number of Slots:          2");
	ut_assert_nextline("Recovery Tries Remaining: 0");
	ut_assert_nextline("CRC:                      0xb28aedd5 (Valid)");
	ut_assert_nextline_empty();
	ut_assert_nextline("Slot[0] Metadata:");
	ut_assert_nextline("\t- Priority:         15");
	ut_assert_nextline("\t- Tries Remaining:  6");
	ut_assert_nextline("\t- Successful Boot:  0");
	ut_assert_nextline("\t- Verity Corrupted: 0");
	ut_assert_nextline_empty();
	ut_assert_nextline("Slot[1] Metadata:");
	ut_assert_nextline("\t- Priority:         15");
	ut_assert_nextline("\t- Tries Remaining:  6");
	ut_assert_nextline("\t- Successful Boot:  0");
	ut_assert_nextline("\t- Verity Corrupted: 0");
	ut_assert_console_end();

	ut_assertok(free_dev(uts, &bd));

	return 0;
}
CMD_TEST(cmd_test_bcb_ab, UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA |
	 UTF_SCAN_FDT);

/* Test that the command refuses a sub-command it does not have */
static int cmd_test_bcb_usage(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("bcb", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("bcb frobnicate", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("bcb load 0", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("bcb set command", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("bcb test command =", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_bcb_usage, UTF_CONSOLE);
