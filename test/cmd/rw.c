// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Tests for read and write commands
 */

#include <command.h>
#include <mapmem.h>
#include <part.h>
#include <test/cmd.h>
#include <test/ut.h>

/* Block where partition 1 starts, leaving room for the GPT */
#define DATA_START	48

/* Size of partition 1 in blocks */
#define DATA_BLOCKS	4

/* Block where partition 2 starts */
#define LOG_START	52

/* Size of partition 2 in blocks */
#define LOG_BLOCKS	10

/* Blocks in the sandbox mmc 2 device, which is 1MB */
#define DEV_BLOCKS	2048

static int setup_partitions(struct unit_test_state *uts, struct blk_desc **mmc_dev_desc)
{
	char str_disk_guid[UUID_STR_LEN + 1];
	struct disk_partition parts[2] = {
		{
			.start = DATA_START,
			.size = DATA_BLOCKS,
			.name = "data",
		},
		{
			.start = LOG_START,
			.size = LOG_BLOCKS,
			.name = "log",
		},
	};

	ut_asserteq(2, blk_get_device_by_str("mmc", "2", mmc_dev_desc));
	if (IS_ENABLED(CONFIG_RANDOM_UUID)) {
		gen_rand_uuid_str(parts[0].uuid, UUID_STR_FORMAT_STD);
		gen_rand_uuid_str(parts[1].uuid, UUID_STR_FORMAT_STD);
		gen_rand_uuid_str(str_disk_guid, UUID_STR_FORMAT_STD);
	}
	ut_assertok(gpt_restore(*mmc_dev_desc, str_disk_guid, parts,
				ARRAY_SIZE(parts)));
	return 0;
}

/* Fill the write buffer with pseudo-random data, clear the read buffer. */
static void init_buffers(char *rb, char *wb, size_t size, unsigned seed)
{
	memset(rb, 0, size);
	while (size--) {
		*wb++ = seed;
		seed *= 43;
		seed += 17 + size/4;
	}
}

/* Test reading blocks of a partition into memory */
static int cmd_test_read_base(struct unit_test_state *uts)
{
	struct blk_desc *dev_desc;
	char wbuf[1024], rbuf[1024];
	ulong ra;

	ut_assertok(setup_partitions(uts, &dev_desc));
	init_buffers(rbuf, wbuf, sizeof(rbuf), __LINE__);
	ra = map_to_sysmem(rbuf);

	/* put the data on the device without using the write command */
	ut_asserteq(2, blk_dwrite(dev_desc, DATA_START, 2, wbuf));

	/* the command says nothing at all when it succeeds */
	ut_assertok(run_commandf("read mmc 2:1 0x%lx 0 2", ra));
	ut_assert_console_end();
	ut_assertok(memcmp(wbuf, rbuf, sizeof(wbuf)));

	/* the block number counts from the start of the partition */
	init_buffers(rbuf, wbuf, sizeof(rbuf), __LINE__);
	ut_asserteq(1, blk_dwrite(dev_desc, DATA_START + 1, 1, wbuf));
	ut_assertok(run_commandf("read mmc 2:1 0x%lx 1 1", ra));
	ut_assertok(memcmp(wbuf, rbuf, 512));

	return 0;
}
CMD_TEST(cmd_test_read_base, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test writing blocks from memory to a partition */
static int cmd_test_write_base(struct unit_test_state *uts)
{
	struct blk_desc *dev_desc;
	char wbuf[1024], rbuf[1024];
	ulong wa;

	if (!IS_ENABLED(CONFIG_CMD_WRITE))
		return -EAGAIN;

	ut_assertok(setup_partitions(uts, &dev_desc));
	init_buffers(rbuf, wbuf, sizeof(rbuf), __LINE__);
	wa = map_to_sysmem(wbuf);

	ut_assertok(run_commandf("write mmc 2:1 0x%lx 0 2", wa));
	ut_assert_console_end();

	/* check the blocks landed at the start of the partition */
	ut_asserteq(2, blk_dread(dev_desc, DATA_START, 2, rbuf));
	ut_assertok(memcmp(wbuf, rbuf, sizeof(wbuf)));

	/* the block number counts from the start of the partition */
	init_buffers(rbuf, wbuf, sizeof(rbuf), __LINE__);
	ut_assertok(run_commandf("write mmc 2:1 0x%lx 2 1", wa));
	ut_asserteq(1, blk_dread(dev_desc, DATA_START + 2, 1, rbuf));
	ut_assertok(memcmp(wbuf, rbuf, 512));

	return 0;
}
CMD_TEST(cmd_test_write_base, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/*
 * Test the ways of naming the device and partition, which both commands take
 * in the same form: a partition number, a partition name, or the whole device
 */
static int cmd_test_rw_specs(struct unit_test_state *uts)
{
	struct blk_desc *dev_desc;
	char wbuf[1024], rbuf[1024];
	ulong wa, ra;

#define INIT_BUFFERS() init_buffers(rbuf, wbuf, sizeof(rbuf), __LINE__)

	if (!IS_ENABLED(CONFIG_CMD_WRITE))
		return -EAGAIN;

	ut_assertok(setup_partitions(uts, &dev_desc));

	wa = map_to_sysmem(wbuf);
	ra = map_to_sysmem(rbuf);

	/* Simple test, write to/read from same partition. */
	INIT_BUFFERS();
	ut_assertok(run_commandf("write mmc 2:1 0x%lx 0 2", wa));
	ut_assertok(run_commandf("read  mmc 2:1 0x%lx 0 2", ra));
	ut_assertok(memcmp(wbuf, rbuf, sizeof(wbuf)));

	/* Use name for write, number for read. */
	INIT_BUFFERS();
	ut_assertok(run_commandf("write mmc 2#log 0x%lx 0 2", wa));
	ut_assertok(run_commandf("read  mmc 2:2   0x%lx 0 2", ra));
	ut_assertok(memcmp(wbuf, rbuf, sizeof(wbuf)));

	/* Use full device for write, name for read. */
	INIT_BUFFERS();
	ut_assertok(run_commandf("write mmc 2:0    0x%lx 0x30 2", wa));
	ut_assertok(run_commandf("read  mmc 2#data 0x%lx    0 2", ra));
	ut_assertok(memcmp(wbuf, rbuf, sizeof(wbuf)));

	/* Use name for write, full device for read */
	INIT_BUFFERS();
	ut_assertok(run_commandf("write mmc 2#log 0x%lx    1 2", wa));
	ut_assertok(run_commandf("read  mmc 2:0   0x%lx 0x35 2", ra));
	ut_assertok(memcmp(wbuf, rbuf, sizeof(wbuf)));

	/*
	 * Leaving the partition out altogether means partition 1, so this
	 * reads back what the first case above wrote there
	 */
	INIT_BUFFERS();
	ut_assertok(run_commandf("write mmc 2:1 0x%lx 0 2", wa));
	ut_assertok(run_commandf("read  mmc 2   0x%lx 0 2", ra));
	ut_assertok(memcmp(wbuf, rbuf, sizeof(wbuf)));

	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_rw_specs, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test the arguments and the blocks which the read command refuses */
static int cmd_test_read_bad(struct unit_test_state *uts)
{
	struct blk_desc *dev_desc;
	char rbuf[1024];
	ulong ra;

	ut_assertok(setup_partitions(uts, &dev_desc));
	ra = map_to_sysmem(rbuf);

	/* all five arguments are required */
	ut_asserteq(1, run_commandf("read mmc 2:1 0x%lx 0", ra));
	ut_assert_nextline("read - Load binary data from a partition");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("read <interface> <dev[:part|#partname]> addr blk# cnt");
	ut_assert_console_end();

	/*
	 * The command has no options, so one is refused before it runs. The
	 * old code read it as the interface name instead
	 */
	ut_asserteq(1, run_commandf("read -x mmc 2:1 0x%lx 0", ra));
	ut_assert_nextline("read - Load binary data from a partition");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("read <interface> <dev[:part|#partname]> addr blk# cnt");
	ut_assert_console_end();

	/* a device which does not exist */
	ut_asserteq(1, run_commandf("read mmc 9 0x%lx 0 1", ra));
	ut_assert_nextline("** Bad device specification mmc 9 **");
	ut_assert_nextline("Couldn't find partition mmc 9");
	ut_assert_console_end();

	/* a partition which does not exist */
	ut_asserteq(1, run_commandf("read mmc 2:9 0x%lx 0 1", ra));
	ut_assert_nextline("** Invalid partition 9 **");
	ut_assert_nextline("Couldn't find partition mmc 2:9");
	ut_assert_console_end();

	/* a request reaching past the end of the partition */
	ut_asserteq(1, run_commandf("read mmc 2#data 0x%lx 3 2", ra));
	ut_assert_nextline("read out of range");
	ut_assert_console_end();

	/*
	 * The whole device has no size in struct blk_desc, so a request past
	 * its end reaches the device and fails there. The driver reports the
	 * bad block number with log_err(), whose line carries the function
	 * name only where CONFIG_LOGF_FUNC is enabled
	 */
	ut_asserteq(1, run_commandf("read mmc 2:0 0x%lx 0x%x 2", ra,
				    DEV_BLOCKS - 1));
	ut_assert_skip_to_line("read error");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_read_bad, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test the arguments and the blocks which the write command refuses */
static int cmd_test_write_bad(struct unit_test_state *uts)
{
	struct blk_desc *dev_desc;
	char wbuf[1024];
	ulong wa;

	if (!IS_ENABLED(CONFIG_CMD_WRITE))
		return -EAGAIN;

	ut_assertok(setup_partitions(uts, &dev_desc));
	wa = map_to_sysmem(wbuf);

	/* all five arguments are required */
	ut_asserteq(1, run_commandf("write mmc 2:1 0x%lx 0", wa));
	ut_assert_nextline("write - Store binary data to a partition");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("write <interface> <dev[:part|#partname]> addr blk# cnt");
	ut_assert_console_end();

	/* an option is refused before the command runs */
	ut_asserteq(1, run_commandf("write -x mmc 2:1 0x%lx 0", wa));
	ut_assert_nextline("write - Store binary data to a partition");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("write <interface> <dev[:part|#partname]> addr blk# cnt");
	ut_assert_console_end();

	/* a request reaching past the end of the partition */
	ut_asserteq(1, run_commandf("write mmc 2#log 0x%lx 9 2", wa));
	ut_assert_nextline("write out of range");
	ut_assert_console_end();

	/* a request past the end of the whole device fails in the device */
	ut_asserteq(1, run_commandf("write mmc 2:0 0x%lx 0x%x 2", wa,
				    DEV_BLOCKS - 1));
	ut_assert_skip_to_line("write error");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_write_bad, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);
