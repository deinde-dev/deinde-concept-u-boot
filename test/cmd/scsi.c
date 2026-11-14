// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the scsi command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <blk.h>
#include <command.h>
#include <console.h>
#include <mapmem.h>
#include <test/cmd.h>
#include <test/ut.h>

/* number of blocks in scsi.img, the file behind the emulated drive */
#define SCSI_BLOCKS	4096

/* a block well inside the drive, clear of its partition table */
#define SCSI_BLOCK	0x100

/* how the emulated drive identifies itself, after its device number */
#define SCSI_DRIVE	"(0:0) Vendor: SANDBOX Prod.: FAKE DISK Rev: 1.0"

/* the start of the last line of the help text, which ends the usage message */
#define SCSI_USAGE_LAST	"scsi erase blk# cnt - erase"

/*
 * The lines a transfer prints, the block number being in decimal, so 256 for
 * SCSI_BLOCK and 4096 for SCSI_BLOCKS, just past the end of the drive. A
 * transfer which the driver refuses outright shows the error code in place of
 * the number of blocks moved.
 */
#define READ_OK \
	"scsi read: device 0 block # 256, count 1 ... 1 blocks read: OK"
#define READ_ERR \
	"scsi read: device 0 block # 4096, count 1 ... -5 blocks read: ERROR"
#define WRITE_OK \
	"scsi write: device 0 block # 256, count 1 ... 1 blocks written: OK"
#define WRITE_ERR \
	"scsi write: device 0 block # 4096, count 1 ... -5 blocks written: ERROR"
#define ERASE_OK \
	"scsi erase: device 0 block # 256, count 1 ... 1 blocks erased: OK"
#define ERASE_ERR \
	"scsi erase: device 0 block # 4096, count 1 ... 0 blocks erased: ERROR"

/**
 * scan_bus() - Scan the SCSI bus and check the emulated drive is ready
 *
 * The sandbox drive is backed by scsi.img, which test.py creates. Without that
 * file the drive has no size, so the test fails rather than testing nothing.
 *
 * @uts: Test state
 * Return: 0 if the drive is ready, 1 on failure
 */
static int scan_bus(struct unit_test_state *uts)
{
	struct blk_desc *desc;

	ut_assertok(run_command("scsi scan", 0));
	console_record_reset();

	desc = blk_get_devnum_by_uclass_id(UCLASS_SCSI, 0);
	ut_assertnonnull(desc);
	ut_assertf(desc->lba == SCSI_BLOCKS,
		   "drive has %lu blocks, not %d: is scsi.img missing?",
		   (ulong)desc->lba, SCSI_BLOCKS);

	return 0;
}

/**
 * check_usage() - Check the usage message, which a bad command line produces
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("scsi - SCSI sub-system");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_skip_to_linen(SCSI_USAGE_LAST);
	ut_assert_console_end();

	return 0;
}

/* Test the sub-commands which report on the drives */
static int cmd_test_scsi_base(struct unit_test_state *uts)
{
	ut_assertok(scan_bus(uts));

	/* a scan lists what it finds, indenting each drive by two spaces */
	ut_assertok(run_command("scsi scan", 0));
	ut_assert_nextline("scanning bus for devices...");
	ut_assert_nextline("  Device 0: " SCSI_DRIVE);
	ut_assert_nextline("            Type: Hard Disk");
	ut_assert_nextline("            Capacity: 2.0 MB = 0.0 GB (4096 x 512)");
	ut_assert_console_end();

	/* the listing afterwards shows the same drive, without the indent */
	ut_assertok(run_command("scsi info", 0));
	ut_assert_nextline("Device 0: " SCSI_DRIVE);
	ut_assert_skip_to_linen("            Capacity:");
	ut_assert_console_end();

	/* the current device is that one, and says so by its own name */
	ut_assertok(run_command("scsi device", 0));
	ut_assert_nextline_empty();
	ut_assert_nextline("scsi device 0: " SCSI_DRIVE);
	ut_assert_skip_to_linen("            Capacity:");
	ut_assert_console_end();

	/* selecting it reports the drive and then says that it is current */
	ut_assertok(run_command("scsi device 0", 0));
	ut_assert_nextline_empty();
	ut_assert_nextline("Device 0: " SCSI_DRIVE);
	ut_assert_skip_to_line("... is now current device");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_scsi_base, UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA |
	 UTF_SCAN_FDT);

/* Test the sub-commands when the device asked for is not there */
static int cmd_test_scsi_missing(struct unit_test_state *uts)
{
	ut_assertok(scan_bus(uts));

	/* there is one drive, so a high device number finds nothing */
	ut_asserteq(1, run_command("scsi device 99", 0));
	ut_assert_nextline_empty();
	ut_assert_nextline("Device 99: unknown device");
	ut_assert_console_end();

	ut_asserteq(1, run_command("scsi part 99", 0));
	ut_assert_nextline_empty();
	ut_assert_nextline("scsi device 99 not available");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_scsi_missing, UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA |
	 UTF_SCAN_FDT);

/* Test reading blocks from the drive into memory */
static int cmd_test_scsi_read(struct unit_test_state *uts)
{
	ulong addr = CONFIG_SYS_LOAD_ADDR + 0x3000;
	u8 *buf;

	ut_assertok(scan_bus(uts));

	buf = map_sysmem(addr, 512);
	memset(buf, 0xff, 512);

	ut_assertok(run_commandf("scsi read %lx %x 1", addr, SCSI_BLOCK));
	ut_assert_nextline_empty();
	ut_assert_nextline(READ_OK);
	ut_assert_console_end();

	/* that part of the image is empty, so the memory is now clear */
	ut_asserteq(0, buf[0]);
	ut_asserteq(0, buf[511]);

	/* a read which runs off the end of the drive gets an error back */
	ut_asserteq(1, run_commandf("scsi read %lx %x 1", addr, SCSI_BLOCKS));
	ut_assert_nextline_empty();
	ut_assert_nextline(READ_ERR);
	ut_assert_console_end();

	unmap_sysmem(buf);

	return 0;
}
CMD_TEST(cmd_test_scsi_read, UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA |
	 UTF_SCAN_FDT);

/* Test writing blocks to the drive and erasing them again */
static int cmd_test_scsi_write(struct unit_test_state *uts)
{
	ulong src = CONFIG_SYS_LOAD_ADDR + 0x3000;
	ulong dst = CONFIG_SYS_LOAD_ADDR + 0x4000;
	u8 *sbuf, *dbuf;

	ut_assertok(scan_bus(uts));

	sbuf = map_sysmem(src, 512);
	dbuf = map_sysmem(dst, 512);
	memset(sbuf, 0xa5, 512);
	memset(dbuf, '\0', 512);

	/* what is written reads back */
	ut_assertok(run_commandf("scsi write %lx %x 1", src, SCSI_BLOCK));
	ut_assert_nextline_empty();
	ut_assert_nextline(WRITE_OK);
	ut_assert_console_end();

	ut_assertok(run_commandf("scsi read %lx %x 1", dst, SCSI_BLOCK));
	ut_assert_nextline_empty();
	ut_assert_nextline(READ_OK);
	ut_assert_console_end();
	ut_asserteq_mem(sbuf, dbuf, 512);

	/* an erased block reads back as zeroes, leaving scsi.img as it was */
	ut_assertok(run_commandf("scsi erase %x 1", SCSI_BLOCK));
	ut_assert_nextline_empty();
	ut_assert_nextline(ERASE_OK);
	ut_assert_console_end();

	ut_assertok(run_commandf("scsi read %lx %x 1", dst, SCSI_BLOCK));
	ut_assert_nextline_empty();
	ut_assert_nextline(READ_OK);
	ut_assert_console_end();
	ut_asserteq(0, dbuf[0]);
	ut_asserteq(0, dbuf[511]);

	/* neither may run off the end of the drive */
	ut_asserteq(1, run_commandf("scsi write %lx %x 1", src, SCSI_BLOCKS));
	ut_assert_nextline_empty();
	ut_assert_nextline(WRITE_ERR);
	ut_assert_console_end();

	ut_asserteq(1, run_commandf("scsi erase %x 1", SCSI_BLOCKS));
	ut_assert_nextline_empty();
	ut_assert_nextline(ERASE_ERR);
	ut_assert_console_end();

	unmap_sysmem(dbuf);
	unmap_sysmem(sbuf);

	return 0;
}
CMD_TEST(cmd_test_scsi_write, UTF_CONSOLE | UTF_DM | UTF_SCAN_PDATA |
	 UTF_SCAN_FDT);

/* Test the command lines which the command refuses */
static int cmd_test_scsi_usage(struct unit_test_state *uts)
{
	/* a sub-command is needed */
	ut_asserteq(1, run_command("scsi", 0));
	ut_assertok(check_usage(uts));

	/* and it must be one the command knows */
	ut_asserteq(1, run_command("scsi bogus", 0));
	ut_assertok(check_usage(uts));

	/* scan takes no argument */
	ut_asserteq(1, run_command("scsi scan now", 0));
	ut_assertok(check_usage(uts));

	/* a transfer needs an address, a block number and a count */
	ut_asserteq(1, run_command("scsi read 1000 0", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("scsi write 1000 0", 0));
	ut_assertok(check_usage(uts));

	/* an option is not a sub-command */
	ut_asserteq(1, run_command("scsi -x", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_scsi_usage, UTF_CONSOLE);
