// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the virtio command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <blk.h>
#include <command.h>
#include <console.h>
#include <dm.h>
#include <mapmem.h>
#include <test/cmd.h>
#include <test/ut.h>

/* the last line of the help text, which ends the usage message */
#define VIRTIO_USAGE_LAST	"     `blk#' from memory address `addr'"

/* the two fixed lines at the top of the 'virtio list' table */
#define LIST_TITLE	"Name                  Type            Driver"
#define LIST_RULE	"--------------------  --------------  ---------------"

/* the emulated device before and after it is probed */
#define LIST_EMUL_OFF	"mmio                   0: (unknown)   (none)"
#define LIST_EMUL_ON	"mmio                   2: blk         virtio-blk#3"

/* vendor ID the MMIO emulator reports, from drivers/virtio/sandbox_emul.c */
#define EMUL_VENDOR	0xf003

/* block size of the emulated disk */
#define BLKSZ		512

/*
 * The tests build a driver-model tree of their own, since they probe virtio
 * devices and the block-device numbers handed out must start from zero
 */
#define VIRTIO_FLAGS	(UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT)

/**
 * probe_blk() - Probe the emulated virtio device and its block device
 *
 * Sandbox provides a block-device emulator behind the 'mmio' transport, which
 * is the only virtio device here that completes a request. The others use a
 * stub transport whose notify op does nothing, so these tests cannot use
 * 'virtio scan': probing the children of those devices never returns.
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int probe_blk(struct unit_test_state *uts)
{
	struct udevice *bus, *blk;

	ut_assertok(uclass_get_device_by_name(UCLASS_VIRTIO, "mmio", &bus));
	ut_assertok(blk_get_device(UCLASS_VIRTIO, 0, &blk));
	console_record_reset();

	return 0;
}

/**
 * check_usage() - Check the usage message, which a bad command line shows
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("virtio - virtio block devices sub-system");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_skip_to_line(VIRTIO_USAGE_LAST);
	ut_assert_console_end();

	return 0;
}

/* Test the listing of virtio devices, both unprobed and probed */
static int cmd_test_virtio_base(struct unit_test_state *uts)
{
	/* nothing is probed yet, so no device knows its own type */
	ut_assertok(run_command("virtio list", 0));
	ut_assert_nextline(LIST_TITLE);
	ut_assert_nextline(LIST_RULE);
	ut_assert_nextline("sandbox_virtio1        0: (unknown)   (none)");
	ut_assert_nextline("sandbox_virtio2        0: (unknown)   (none)");
	ut_assert_nextline("sandbox-virtio-blk     0: (unknown)   (none)");
	ut_assert_nextline(LIST_EMUL_OFF);
	ut_assert_nextline("sandbox-virtio-fs      0: (unknown)   (none)");
	ut_assert_console_end();

	/* the emulated device shows its type and the driver bound to it */
	ut_assertok(probe_blk(uts));
	ut_assertok(run_command("virtio list", 0));
	ut_assert_skip_to_line(LIST_EMUL_ON);
	ut_assert_nextline("sandbox-virtio-fs      0: (unknown)   (none)");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_virtio_base, VIRTIO_FLAGS);

/* Test the sub-commands which report on the block devices */
static int cmd_test_virtio_info(struct unit_test_state *uts)
{
	ut_assertok(probe_blk(uts));

	/* the listing names the disk the emulator provides */
	ut_assertok(run_command("virtio info", 0));
	ut_assert_nextline("Device 0: %08x VirtIO Block Device", EMUL_VENDOR);
	ut_assert_nextline("            Type: Hard Disk");
	ut_assert_nextlinen("            Capacity: 1.0 MB");
	ut_assert_console_end();

	/* the current device is that one, and says so by its own name */
	ut_assertok(run_command("virtio device", 0));
	ut_assert_nextline_empty();
	ut_assert_nextline("virtio device 0: %08x VirtIO Block Device",
			   EMUL_VENDOR);
	ut_assert_skip_to_linen("            Capacity: 1.0 MB");
	ut_assert_console_end();

	/* the emulated disk holds no partition table */
	ut_assertok(run_command("virtio part", 0));
	ut_assert_nextline_empty();
	ut_assert_nextline("no virtio partition table available");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_virtio_info, VIRTIO_FLAGS);

/* Test the sub-commands when the device asked for is not there */
static int cmd_test_virtio_missing(struct unit_test_state *uts)
{
	ut_assertok(probe_blk(uts));

	/* there is one disk, so the second does not exist */
	ut_asserteq(1, run_command("virtio dev 1", 0));
	ut_assert_nextline_empty();
	ut_assert_nextline("Device 1: unknown device");
	ut_assert_console_end();

	ut_asserteq(1, run_command("virtio part 1", 0));
	ut_assert_nextline_empty();
	ut_assert_nextline("virtio device 1 not available");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_virtio_missing, VIRTIO_FLAGS);

/* Test moving data to and from the emulated disk */
static int cmd_test_virtio_rw(struct unit_test_state *uts)
{
	ulong addr = CONFIG_SYS_LOAD_ADDR + 0x1000;
	u8 *buf;
	int i;

	ut_assertok(probe_blk(uts));

	buf = map_sysmem(addr, BLKSZ);
	for (i = 0; i < BLKSZ; i++)
		buf[i] = i;

	/* block 5 is well inside the 1MB disk the emulator provides */
	ut_assertok(run_commandf("virtio write %lx 5 1", addr));
	ut_assert_nextline_empty();
	ut_assert_nextlinen("virtio write: device 0 block # 5, count 1 ...");
	ut_assert_console_end();

	memset(buf, '\0', BLKSZ);
	ut_assertok(run_commandf("virtio read %lx 5 1", addr));
	ut_assert_nextline_empty();
	ut_assert_nextlinen("virtio read: device 0 block # 5, count 1 ...");
	ut_assert_console_end();

	/* what came back is what went out */
	for (i = 0; i < BLKSZ; i++)
		ut_asserteq(i & 0xff, buf[i]);

	unmap_sysmem(buf);

	return 0;
}
CMD_TEST(cmd_test_virtio_rw, VIRTIO_FLAGS);

/* Test the command lines which the command refuses */
static int cmd_test_virtio_usage(struct unit_test_state *uts)
{
	/* a sub-command is needed */
	ut_asserteq(1, run_command("virtio", 0));
	ut_assertok(check_usage(uts));

	/* and it must be one the command knows */
	ut_asserteq(1, run_command("virtio bogus", 0));
	ut_assertok(check_usage(uts));

	/* the command takes no options, so the framework refuses one */
	ut_asserteq(1, run_command("virtio -x", 0));
	ut_assertok(check_usage(uts));

	/* neither scan nor list takes an argument */
	ut_asserteq(1, run_command("virtio scan now", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("virtio list now", 0));
	ut_assertok(check_usage(uts));

	/* a transfer needs an address, a block number and a count */
	ut_asserteq(1, run_command("virtio read 1000 0", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("virtio write 1000 0", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_virtio_usage, UTF_CONSOLE);
