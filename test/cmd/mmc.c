// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the mmc command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <mapmem.h>
#include <mmc.h>
#include <part.h>
#include <test/cmd.h>
#include <test/ut.h>

/* Emulated device which the tests write to, 1MB and not removable */
#define SCRATCH_DEV	2

/* Block of the scratch device used for the transfers, clear of the GPT */
#define TEST_BLK	64

/* Number of blocks transferred */
#define TEST_CNT	2

/* Block where partition 1 starts, leaving room for the GPT */
#define DATA_START	48

/* Size of partition 1 in blocks */
#define DATA_BLOCKS	4

/* Size of partition 2 in blocks */
#define LOG_BLOCKS	10

/**
 * select_mmc() - Make an emulated MMC device the current one
 *
 * Most sub-commands work on the current device, which an earlier test may
 * have changed
 *
 * @uts: Test state
 * @dev: Device number to select
 * Return: 0 if OK, other value on error
 */
static int select_mmc(struct unit_test_state *uts, int dev)
{
	ut_assertok(run_commandf("mmc dev %d", dev));
	console_record_reset();

	return 0;
}

/**
 * check_usage() - Check the usage message the command shows
 *
 * The message lists every sub-command, so only its first lines are checked
 *
 * @uts: Test state
 * Return: 0 if OK, other value on error
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("mmc - MMC sub system");
	ut_assert_nextline_empty();
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_nextline("mmc info - display info of the current MMC device");
	}
	console_record_reset();

	return 0;
}

/**
 * setup_partitions() - Write a partition table to the scratch device
 *
 * @uts: Test state
 * @descp: Returns the block descriptor of the device
 * Return: 0 if OK, other value on error
 */
static int setup_partitions(struct unit_test_state *uts,
			    struct blk_desc **descp)
{
	char str_disk_guid[UUID_STR_LEN + 1];
	struct disk_partition parts[2] = {
		{
			.start = DATA_START,
			.size = DATA_BLOCKS,
			.name = "data",
		},
		{
			.start = DATA_START + DATA_BLOCKS,
			.size = LOG_BLOCKS,
			.name = "log",
		},
	};

	ut_asserteq(SCRATCH_DEV, blk_get_device_by_str("mmc", "2", descp));
	if (IS_ENABLED(CONFIG_RANDOM_UUID)) {
		gen_rand_uuid_str(parts[0].uuid, UUID_STR_FORMAT_STD);
		gen_rand_uuid_str(parts[1].uuid, UUID_STR_FORMAT_STD);
		gen_rand_uuid_str(str_disk_guid, UUID_STR_FORMAT_STD);
	}
	ut_assertok(gpt_restore(*descp, str_disk_guid, parts,
				ARRAY_SIZE(parts)));

	return 0;
}

/* Test selecting a device and the report for it */
static int cmd_test_mmc_base(struct unit_test_state *uts)
{
	ut_assertok(run_command("mmc dev 0", 0));
	ut_assert_nextline("switch to partitions #0, OK");
	ut_assert_nextline("mmc0 is current device");
	ut_assert_console_end();

	/* with no device number it re-initialises the current device */
	ut_assertok(run_command("mmc dev", 0));
	ut_assert_nextline("switch to partitions #0, OK");
	ut_assert_nextline("mmc0 is current device");
	ut_assert_console_end();

	/* 'info' reports on the current device */
	ut_assertok(run_command("mmc info", 0));
	ut_assert_nextline("Device: mmc0");
	ut_assert_skip_to_line("Capacity: 1 MiB");
	console_record_reset();

	/* 'rescan' says nothing at all when it succeeds */
	ut_assertok(run_command("mmc rescan", 0));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_mmc_base, UTF_CONSOLE);

/* Test the list of devices */
static int cmd_test_mmc_list(struct unit_test_state *uts)
{
	ut_assertok(select_mmc(uts, 0));

	/*
	 * The devices are listed in the order they were bound and one of the
	 * emulated cards has a backing file which may be missing, so look for
	 * the line for the first card rather than reading the whole list
	 */
	ut_assertok(run_command("mmc list", 0));
	ut_assert_skip_to_line("mmc0: 0 (SD)");
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_mmc_list, UTF_CONSOLE);

/* Test the partition list, with and without a partition table */
static int cmd_test_mmc_part(struct unit_test_state *uts)
{
	struct blk_desc *desc;

	ut_assertok(select_mmc(uts, 0));

	/*
	 * The emulated card is blank, so the command reports the type it read
	 * and still succeeds
	 */
	ut_assertok(run_command("mmc part", 0));
	ut_assert_nextline("## Unknown partition table type 0");
	ut_assert_console_end();

	ut_assertok(setup_partitions(uts, &desc));
	ut_assertok(select_mmc(uts, SCRATCH_DEV));

	ut_assertok(run_command("mmc part", 0));
	ut_assert_nextline_empty();
	ut_assert_nextline("Partition Map for mmc device 2  --   Partition Type: EFI");
	ut_assert_nextline_empty();
	ut_assert_nextline("Part\tStart LBA\tEnd LBA\t\tName");
	ut_assert_nextline("\tAttributes");
	ut_assert_nextline("\tType GUID");
	ut_assert_nextline("\tPartition GUID");
	ut_assert_nextline("  1\t0x%08x\t0x%08x\t\"data\"", DATA_START,
			   DATA_START + DATA_BLOCKS - 1);
	ut_assert_skip_to_line("  2\t0x%08x\t0x%08x\t\"log\"",
			       DATA_START + DATA_BLOCKS,
			       DATA_START + DATA_BLOCKS + LOG_BLOCKS - 1);
	console_record_reset();

	ut_assertok(select_mmc(uts, 0));

	return 0;
}
CMD_TEST(cmd_test_mmc_part, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test writing blocks to a device, reading them back and erasing them */
static int cmd_test_mmc_rw(struct unit_test_state *uts)
{
	char wbuf[TEST_CNT * 512], rbuf[TEST_CNT * 512];
	ulong wa, ra;
	uint i;

	/* the write and erase sub-commands come with CONFIG_MMC_WRITE */
	if (!IS_ENABLED(CONFIG_MMC_WRITE))
		return -EAGAIN;

	ut_assertok(select_mmc(uts, SCRATCH_DEV));

	for (i = 0; i < sizeof(wbuf); i++)
		wbuf[i] = i + 1;
	memset(rbuf, '\0', sizeof(rbuf));
	wa = map_to_sysmem(wbuf);
	ra = map_to_sysmem(rbuf);

	ut_assertok(run_commandf("mmc write 0x%lx %x %x", wa, TEST_BLK,
				 TEST_CNT));
	ut_assert_nextline("MMC write: dev # 2, block # 64, count 2 ... 2 blocks written: OK");
	ut_assert_console_end();

	ut_assertok(run_commandf("mmc read 0x%lx %x %x", ra, TEST_BLK,
				 TEST_CNT));
	ut_assert_nextline("MMC read: dev # 2, block # 64, count 2 ... 2 blocks read: OK");
	ut_assert_console_end();
	ut_assertok(memcmp(wbuf, rbuf, sizeof(wbuf)));

	/* erasing the blocks leaves them reading as zero */
	ut_assertok(run_commandf("mmc erase %x %x", TEST_BLK, TEST_CNT));
	ut_assert_nextline("MMC erase: dev # 2, block # 64, count 2 ... 2 blocks erased: OK");
	ut_assert_console_end();

	ut_assertok(run_commandf("mmc read 0x%lx %x %x", ra, TEST_BLK,
				 TEST_CNT));
	console_record_reset();
	for (i = 0; i < sizeof(rbuf); i++)
		ut_asserteq(0, rbuf[i]);

	ut_assertok(select_mmc(uts, 0));

	return 0;
}
CMD_TEST(cmd_test_mmc_rw, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test the arguments the command refuses */
static int cmd_test_mmc_usage(struct unit_test_state *uts)
{
	ut_assertok(select_mmc(uts, 0));

	/* a sub-command is required */
	ut_asserteq(1, run_command("mmc", 0));
	ut_assertok(check_usage(uts));

	/* a sub-command the command does not have */
	ut_asserteq(1, run_command("mmc bogus", 0));
	ut_assertok(check_usage(uts));

	/* more arguments than the sub-command takes */
	ut_asserteq(1, run_command("mmc info extra", 0));
	ut_assertok(check_usage(uts));

	/* the command has no options, so one is refused as well */
	ut_asserteq(1, run_command("mmc -x", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_mmc_usage, UTF_CONSOLE);

/* Test the device and partition numbers which 'mmc dev' refuses */
static int cmd_test_mmc_dev_bad(struct unit_test_state *uts)
{
	ut_assertok(select_mmc(uts, 0));

	/* a device which does not exist */
	ut_asserteq(1, run_command("mmc dev 9", 0));
	ut_assert_nextline("no mmc device at slot 9");
	ut_assert_console_end();

	/* a device number which is not a number */
	ut_asserteq(1, run_command("mmc dev x", 0));
	ut_assert_nextline("Invalid device number 'x'");
	ut_assertok(check_usage(uts));

	/* a partition number which is not a number */
	ut_asserteq(1, run_command("mmc dev 0 x", 0));
	ut_assert_nextline("Invalid part number 'x'");
	ut_assertok(check_usage(uts));

	/* a partition number the hardware cannot hold */
	ut_asserteq(1, run_commandf("mmc dev 0 %d", PART_ACCESS_MASK + 1));
	ut_assert_nextline("#part_num shouldn't be larger than %d",
			   PART_ACCESS_MASK);
	ut_assert_console_end();

	ut_assertok(select_mmc(uts, 0));

	return 0;
}
CMD_TEST(cmd_test_mmc_dev_bad, UTF_CONSOLE);

/* Test the sub-commands which work only on an eMMC device */
static int cmd_test_mmc_emmc_only(struct unit_test_state *uts)
{
	ut_assertok(select_mmc(uts, 0));

	/* the emulated card is an SD card, so the boot partitions are absent */
	ut_asserteq(1, run_command("mmc wp", 0));
	ut_assert_nextline("It is not an eMMC device");
	ut_assert_console_end();

	ut_asserteq(1, run_command("mmc wp 0", 0));
	ut_assert_nextline("It is not an eMMC device");
	ut_assert_console_end();

	if (IS_ENABLED(CONFIG_MMC_HW_PARTITIONING)) {
		ut_asserteq(1, run_command("mmc hwpartition check", 0));
		ut_assert_nextline("SD doesn't support partitioning");
		ut_assert_console_end();
	}

	return 0;
}
CMD_TEST(cmd_test_mmc_emmc_only, UTF_CONSOLE);

/* Test setting the DSR register value */
static int cmd_test_mmc_setdsr(struct unit_test_state *uts)
{
	ut_assertok(select_mmc(uts, 0));

	ut_assertok(run_command("mmc setdsr 1234", 0));
	ut_assert_nextline("set dsr OK, force rescan");
	ut_assert_console_end();

	/* the value is required */
	ut_asserteq(1, run_command("mmc setdsr", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_mmc_setdsr, UTF_CONSOLE);

/* Test that the command reports the repeatability of its sub-command */
static int cmd_test_mmc_repeat(struct unit_test_state *uts)
{
	char *const list_args[] = { "mmc", "list", NULL };
	char *const dev_args[] = { "mmc", "dev", NULL };
	int repeatable;

	ut_assertok(select_mmc(uts, 0));

	/* 'list' may be repeated by pressing Enter */
	repeatable = -1;
	ut_assertok(cmd_process(0, 2, list_args, &repeatable, NULL));
	ut_asserteq(1, repeatable);
	console_record_reset();

	/*
	 * 'dev' may not, and the command is marked repeatable itself, so
	 * without the dispatcher narrowing it every sub-command would look
	 * repeatable
	 */
	repeatable = -1;
	ut_assertok(cmd_process(0, 2, dev_args, &repeatable, NULL));
	ut_asserteq(0, repeatable);
	console_record_reset();

	/* a repeat of it is refused before the sub-command runs */
	ut_assertok(cmd_process(CMD_FLAG_REPEAT, 2, dev_args, &repeatable,
				NULL));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_mmc_repeat, UTF_CONSOLE);

/* Test the speed mode, which is an argument only where it can be set */
static int cmd_test_mmc_speed_mode(struct unit_test_state *uts)
{
	ut_assertok(select_mmc(uts, 0));

	if (IS_ENABLED(CONFIG_MMC_SPEED_MODE_SET)) {
		ut_assertok(run_command("mmc rescan 0", 0));
		ut_assert_console_end();
	} else {
		ut_asserteq(1, run_command("mmc rescan 0", 0));
		ut_assertok(check_usage(uts));
	}

	return 0;
}
CMD_TEST(cmd_test_mmc_speed_mode, UTF_CONSOLE);
