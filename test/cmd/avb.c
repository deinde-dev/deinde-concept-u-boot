// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the avb command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <avb_verify.h>
#include <command.h>
#include <dm.h>
#include <errno.h>
#include <mapmem.h>
#include <asm/global_data.h>
#include <test/cmd.h>
#include <test/ut.h>

DECLARE_GLOBAL_DATA_PTR;

/* Where the 'boot' partition sits on the device, in bytes */
#define BOOT_START	0x8000
#define BOOT_SIZE	0x10000

/* Partition UUID, which the GPT needs and 'avb get_uuid' reports */
#define BOOT_UUID	"4ab5f3c8-0000-4000-8000-000000000001"

/*
 * Addresses the data is written to and read back from. A board with no
 * default load address leaves CONFIG_SYS_LOAD_ADDR at 0, so the tests which
 * use these check them against the start of memory.
 */
#define SRC_ADDR	((ulong)CONFIG_SYS_LOAD_ADDR + 0x3000)
#define DEST_ADDR	((ulong)CONFIG_SYS_LOAD_ADDR + 0x4000)

/* Bytes written to the partition and read back from it */
#define DATA_SIZE	8
static const u8 data[DATA_SIZE] = {
	0xef, 0xbe, 0xad, 0xde, 0xef, 0xbe, 0xad, 0xde };

/* The message which every sub-command but init shows before an init */
#define NOT_INIT	"AVB is not initialized, please run 'avb init <id>'"

/*
 * The tests carry UTF_DM so that the MMC device the command uses, and the
 * emulated OP-TEE which holds the rollback indexes and the persistent values,
 * belong to the test. Both are then in a known state and whatever a test
 * writes to them goes away with the test
 */

/**
 * have_tools() - Check for the commands the tests need to make a partition
 *
 * The sub-commands which read a partition need one on the first MMC device,
 * which the test writes with 'gpt write' since nothing else can create one.
 *
 * Return: true if a device with a 'boot' partition can be made
 */
static bool have_tools(void)
{
	return IS_ENABLED(CONFIG_CMD_GPT) && IS_ENABLED(CONFIG_CMD_MMC) &&
		IS_ENABLED(CONFIG_EFI_PARTITION) &&
		IS_ENABLED(CONFIG_PARTITION_UUIDS);
}

/**
 * make_dev() - Write a partition table with a 'boot' partition to mmc 0
 *
 * The partition is empty, so it reads as zeroes until a test writes to it.
 * The command looks the device up by number, which needs it probed, and the
 * gpt command does that on the way.
 *
 * @uts: Test state
 * Return: 0 if OK, other value on error
 */
static int make_dev(struct unit_test_state *uts)
{
	ut_assertok(run_commandf(
		"gpt write mmc 0 name=boot,start=%#x,size=%#x,uuid=%s",
		BOOT_START, BOOT_SIZE, BOOT_UUID));
	ut_assert_nextline("Writing GPT: success!");
	ut_assert_console_end();

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
		"avb - Provides commands for testing Android Verified Boot 2.0 functionality");
	ut_assert_nextline_empty();
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_skip_to_line(
			"    [slot_suffix] - _a, _b, etc (if vbmeta partition is slotted)");
		ut_assert_nextline_empty();
	}
	ut_assert_console_end();

	return 0;
}

/* Test the lock state and the rollback indexes */
static int cmd_test_avb_base(struct unit_test_state *uts)
{
	ut_assertok(run_command("avb init 0", 0));
	ut_assert_console_end();

	/* sandbox reports itself unlocked, as a board without OP-TEE does */
	ut_assertok(run_command("avb is_unlocked", 0));
	ut_assert_nextline("Unlocked = 1");
	ut_assert_console_end();

	ut_assertok(run_command("avb read_rb 1", 0));
	ut_assert_nextline("Rollback index: 0");
	ut_assert_console_end();

	ut_assertok(run_command("avb write_rb 1 5", 0));
	ut_assert_console_end();

	/*
	 * The index is only stored where OP-TEE provides the secure storage;
	 * without it the write does nothing and the location still reads as 0
	 */
	ut_assertok(run_command("avb read_rb 1", 0));
	if (IS_ENABLED(CONFIG_OPTEE_TA_AVB))
		ut_assert_nextline("Rollback index: 5");
	else
		ut_assert_nextline("Rollback index: 0");
	ut_assert_console_end();

	avb_cmd_reset();

	return 0;
}
CMD_TEST(cmd_test_avb_base, UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA |
	 UTF_SCAN_FDT);

/* Test that a rollback index cannot be lowered or written out of range */
static int cmd_test_avb_rollback(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_OPTEE_TA_AVB))
		return -EAGAIN;

	ut_assertok(run_command("avb init 0", 0));
	ut_assertok(run_command("avb write_rb 1 5", 0));
	ut_assert_console_end();

	/* an index may be raised but never lowered */
	ut_asserteq(1, run_command("avb write_rb 1 3", 0));
	ut_assert_nextline("Failed to write rollback index id = 1, err = 2");
	ut_assert_console_end();

	/* there are only a few locations, so a high one does not exist */
	ut_asserteq(1, run_command("avb write_rb 9999 1", 0));
	ut_assert_nextline(
		"Failed to write rollback index id = 39321, err = 5");
	ut_asserteq(1, run_command("avb read_rb 9999", 0));
	ut_assert_nextline(
		"Failed to read rollback index id = 39321, err = 5");
	ut_assert_console_end();

	avb_cmd_reset();

	return 0;
}
CMD_TEST(cmd_test_avb_rollback, UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA |
	 UTF_SCAN_FDT);

/* Test reading a partition UUID and copying bytes to and from a partition */
static int cmd_test_avb_part(struct unit_test_state *uts)
{
	void *buf;

	if (!have_tools() || gd->ram_base > SRC_ADDR)
		return -EAGAIN;
	ut_assertok(make_dev(uts));

	ut_assertok(run_command("avb init 0", 0));
	ut_assertok(run_command("avb get_uuid boot", 0));
	ut_assert_nextline("'boot' UUID: " BOOT_UUID);
	ut_assert_console_end();

	buf = map_sysmem(SRC_ADDR, DATA_SIZE);
	memcpy(buf, data, DATA_SIZE);
	unmap_sysmem(buf);

	ut_assertok(run_commandf("avb write_part boot 0 %x %lx", DATA_SIZE,
				 SRC_ADDR));
	ut_assert_nextline("Wrote 8 bytes");
	ut_assert_console_end();

	/* the bytes come back from the partition, at a different address */
	buf = map_sysmem(DEST_ADDR, DATA_SIZE);
	memset(buf, '\0', DATA_SIZE);
	unmap_sysmem(buf);

	ut_assertok(run_commandf("avb read_part boot 0 %x %lx", DATA_SIZE,
				 DEST_ADDR));
	ut_assert_nextline("Read 8 bytes");
	ut_assert_console_end();

	buf = map_sysmem(DEST_ADDR, DATA_SIZE);
	ut_asserteq_mem(data, buf, DATA_SIZE);
	unmap_sysmem(buf);

	/* read_part_hex shows the same bytes rather than storing them */
	ut_assertok(run_commandf("avb read_part_hex boot 0 %x", DATA_SIZE));
	ut_assert_nextline("Requested 8, read 8 bytes");
	ut_assert_nextline("Data: EFBEADDEEFBEADDE");
	ut_assert_console_end();

	avb_cmd_reset();

	return 0;
}
CMD_TEST(cmd_test_avb_part, UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA |
	 UTF_SCAN_FDT);

/* Test that each sub-command refuses a partition which is not there */
static int cmd_test_avb_nopart(struct unit_test_state *uts)
{
	if (!have_tools() || gd->ram_base > SRC_ADDR)
		return -EAGAIN;
	ut_assertok(make_dev(uts));

	ut_assertok(run_command("avb init 0", 0));
	ut_asserteq(1, run_command("avb get_uuid nosuch", 0));
	ut_assert_nextline("get_partition: can't find partition 'nosuch'");
	ut_assert_nextline("Failed to read partition 'nosuch' UUID, err = 3");
	ut_assert_console_end();

	ut_asserteq(1, run_commandf("avb read_part nosuch 0 %x %lx", DATA_SIZE,
				    SRC_ADDR));
	ut_assert_nextline("get_partition: can't find partition 'nosuch'");
	ut_assert_nextline("Failed to read from partition 'nosuch', err = 3");
	ut_assert_console_end();

	ut_asserteq(1, run_commandf("avb write_part nosuch 0 %x %lx",
				    DATA_SIZE, SRC_ADDR));
	ut_assert_nextline("get_partition: can't find partition 'nosuch'");
	ut_assert_nextline("Failed to write in partition 'nosuch', err = 3");
	ut_assert_console_end();

	avb_cmd_reset();

	return 0;
}
CMD_TEST(cmd_test_avb_nopart, UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA |
	 UTF_SCAN_FDT);

/* Test the persistent values which OP-TEE stores */
static int cmd_test_avb_pvalue(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_OPTEE_TA_AVB))
		return -EAGAIN;

	ut_assertok(run_command("avb init 0", 0));

	/* the terminator is stored as well, so 'green' is six bytes */
	ut_assertok(run_command("avb write_pvalue bootstate green", 0));
	ut_assert_nextline("Wrote 6 bytes");
	ut_assertok(run_command("avb read_pvalue bootstate 6", 0));
	ut_assert_nextline("Read 6 bytes, value = green");
	ut_assert_console_end();

	/* a value which was never written does not exist */
	ut_asserteq(1, run_command("avb read_pvalue nosuch 6", 0));
	ut_assert_nextline("Failed to read persistent value, err = 5");
	ut_assert_console_end();

	avb_cmd_reset();

	return 0;
}
CMD_TEST(cmd_test_avb_pvalue, UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA |
	 UTF_SCAN_FDT);

/* Test that verification reports a vbmeta structure it cannot use */
static int cmd_test_avb_verify(struct unit_test_state *uts)
{
	if (!have_tools())
		return -EAGAIN;
	ut_assertok(make_dev(uts));

	/*
	 * The partition holds no image, so libavb finds no footer. Its own
	 * messages are skipped, since they name the line they come from
	 */
	ut_assertok(run_command("avb init 0", 0));
	ut_asserteq(1, run_command("avb verify", 0));
	ut_assert_nextline("## Android Verified Boot 2.0 version 1.3.0");
	ut_assert_skip_to_line(
		"Verification failed, reason: Metadata is invalid or inconsistent");
	ut_assert_console_end();

	avb_cmd_reset();

	return 0;
}
CMD_TEST(cmd_test_avb_verify, UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA |
	 UTF_SCAN_FDT);

/* Test that every sub-command but init needs the operations an init creates */
static int cmd_test_avb_noinit(struct unit_test_state *uts)
{
	/* an earlier test may have left the operations in place */
	avb_cmd_reset();

	ut_asserteq(1, run_command("avb is_unlocked", 0));
	ut_assert_nextline(NOT_INIT);
	ut_asserteq(1, run_command("avb read_rb 1", 0));
	ut_assert_nextline(NOT_INIT);
	ut_asserteq(1, run_command("avb write_rb 1 5", 0));
	ut_assert_nextline(NOT_INIT);
	ut_asserteq(1, run_command("avb get_uuid boot", 0));
	ut_assert_nextline(NOT_INIT);
	ut_asserteq(1, run_command("avb read_part boot 0 8 1000", 0));
	ut_assert_nextline(NOT_INIT);
	ut_asserteq(1, run_command("avb read_part_hex boot 0 8", 0));
	ut_assert_nextline(NOT_INIT);
	ut_asserteq(1, run_command("avb write_part boot 0 8 1000", 0));
	ut_assert_nextline(NOT_INIT);
	ut_asserteq(1, run_command("avb verify", 0));
	ut_assert_nextline(NOT_INIT);
	if (IS_ENABLED(CONFIG_OPTEE_TA_AVB)) {
		ut_asserteq(1, run_command("avb read_pvalue bootstate 6", 0));
		ut_assert_nextline(NOT_INIT);
		ut_asserteq(1, run_command("avb write_pvalue bootstate x", 0));
		ut_assert_nextline(NOT_INIT);
	}
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_avb_noinit, UTF_CONSOLE);

/* Test the arguments the command refuses */
static int cmd_test_avb_usage(struct unit_test_state *uts)
{
	/*
	 * A sub-command looks for the operations before it looks at its
	 * arguments, so an init must run first for the usage message to be
	 * what a wrong argument produces
	 */
	ut_assertok(run_command("avb init 0", 0));
	ut_assert_console_end();

	ut_asserteq(1, run_command("avb", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("avb frobnicate", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("avb init", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("avb init 0 1", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("avb read_rb", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("avb is_unlocked x", 0));
	ut_assertok(check_usage(uts));

	/* none of the sub-commands takes an option */
	ut_asserteq(1, run_command("avb -x", 0));
	ut_assertok(check_usage(uts));

	avb_cmd_reset();

	return 0;
}
CMD_TEST(cmd_test_avb_usage, UTF_CONSOLE);
