// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the crosec command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <cros_ec.h>
#include <dm.h>
#include <mapmem.h>
#include <asm/global_data.h>
#include <asm/test.h>
#include <test/cmd.h>
#include <test/ut.h>

DECLARE_GLOBAL_DATA_PTR;

/* Where the image written to the EC's flash is built */
#define IMAGE_ADDR	((ulong)CONFIG_SYS_LOAD_ADDR + 0x5000)

/* Size of that image, which is as small as the emulator accepts */
#define IMAGE_SIZE	2

/*
 * Last byte of an EC image, which the emulator looks for to work out how much
 * of a region holds an image
 */
#define IMAGE_END	0xea

/* sha256 of the image the flash test writes */
#define IMAGE_DIGEST \
	"d021c38903eefed7e83e17fe8b952f0102a1109da26f83d16f90c2bb219114f4"

/* The value cros_ec_hello() sends and what the EC adds to it */
#define HELLO_IN	0x12345678
#define HELLO_ADD	0x01020304

/* The RW region of the emulated EC's flash */
#define RW_OFFSET	0x10000
#define RW_SIZE		0x10000

/*
 * Every test carries UTF_DM so that the emulated EC, and the flash and host
 * events it holds, belong to the test. Whatever a test writes to the EC then
 * goes away with it, rather than being seen by a later test
 */

/**
 * check_usage() - Check the usage message the command shows
 *
 * @uts: Test state
 * Return: 0 if OK, other value on error
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("crosec - CROS-EC utility command");
	ut_assert_nextline_empty();
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_skip_to_line(
			"crosec version             Read CROS-EC version");
	}
	ut_assert_console_end();

	return 0;
}

/* Test the sub-commands which report what the EC is */
static int cmd_test_crosec_base(struct unit_test_state *uts)
{
	ut_assertok(run_command("crosec id", 0));
	ut_assert_nextline("sandbox_ro");

	ut_assertok(run_command("crosec curimage", 0));
	ut_assert_nextline("1");

	ut_assertok(run_command("crosec sku", 0));
	ut_assert_nextline("1234");

	ut_assertok(run_command("crosec test", 0));
	ut_assert_nextline("Test passed");

	/* re-probing the EC leaves it answering as before */
	ut_assertok(run_command("crosec init", 0));
	ut_assertok(run_command("crosec id", 0));
	ut_assert_nextline("sandbox_ro");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_crosec_base, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test 'crosec version', which the emulator answers only in part */
static int cmd_test_crosec_version(struct unit_test_state *uts)
{
	/* the emulator has no build information, so the command fails */
	ut_asserteq(1, run_command("crosec version", 0));
	ut_assert_nextline("RO version:    sandbox_ro");
	ut_assert_nextline("RW version:    sandbox_rw");
	ut_assert_nextline("Firmware copy: RO");
	ut_assert_skip_to_line("Error: CROS-EC command failed (error -1)");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_crosec_version, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test the host events, which the lid switch sets */
static int cmd_test_crosec_events(struct unit_test_state *uts)
{
	struct udevice *dev;

	ut_assertok(uclass_first_device_err(UCLASS_CROS_EC, &dev));

	ut_assertok(run_command("crosec events", 0));
	ut_assert_nextline("00000000");

	/* opening the lid sets an event, which the command names */
	sandbox_cros_ec_set_test_flags(dev, CROSECT_LID_OPEN);
	ut_assertok(run_command("crosec events", 0));
	ut_assert_nextline("00000002");
	ut_assert_nextline("lid_open");

	ut_assertok(run_command("crosec switches", 0));
	ut_assert_nextline("lid open");

	/* clearing that event leaves the mask empty */
	ut_assertok(run_command("crosec clrevents 2", 0));
	ut_assertok(run_command("crosec events", 0));
	ut_assert_nextline("00000000");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_crosec_events, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test the sub-commands which work on the EC's flash */
static int cmd_test_crosec_flash(struct unit_test_state *uts)
{
	u8 *buf;

	if (gd->ram_base > IMAGE_ADDR)
		return -EAGAIN;

	ut_assertok(run_command("crosec regioninfo ro", 0));
	ut_assert_nextline("EC flash region 0: offset=0x0, size=0xf000");
	ut_assert_nextline("Region: RO");
	ut_assert_nextline("Offset: 0");
	ut_assert_nextline("Size:   f000");

	ut_assertok(run_commandf("crosec regioninfo rw"));
	ut_assert_nextlinen("EC flash region 1: ");
	ut_assert_nextline("Region: RW");
	ut_assert_nextline("Offset: %x", RW_OFFSET);
	ut_assert_nextline("Size:   %x", RW_SIZE);

	ut_assertok(run_command("crosec erase rw", 0));
	console_record_reset();

	/* write a tiny image to the region, so that it can be hashed */
	buf = map_sysmem(IMAGE_ADDR, IMAGE_SIZE);
	buf[0] = 0;
	buf[IMAGE_SIZE - 1] = IMAGE_END;
	unmap_sysmem(buf);
	ut_assertok(run_commandf("crosec write rw %lx %x", IMAGE_ADDR,
				 IMAGE_SIZE));
	console_record_reset();

	ut_assertok(run_command("crosec hash", 0));
	ut_assert_nextline("type:    SHA-256");
	ut_assert_nextline("offset:  0x%08x", RW_OFFSET);
	ut_assert_nextline("size:    0x%08x", IMAGE_SIZE);
	ut_assert_nextline("digest:  " IMAGE_DIGEST);
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_crosec_flash, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test the sub-commands which the emulated EC does not implement */
static int cmd_test_crosec_missing(struct unit_test_state *uts)
{
	/* the emulator has no keyboard matrix */
	ut_asserteq(1, run_command("crosec info", 0));
	ut_assert_nextline("   ** Unknown EC command 0x61");

	/* nor any flash geometry */
	ut_asserteq(1, run_command("crosec flashinfo", 0));
	ut_assert_nextline("   ** Unknown EC command 0x10");
	ut_assert_nextline("Error: CROS-EC command failed (error -1)");

	/* nor a way of reading its flash back */
	ut_asserteq(1, run_commandf("crosec read rw %lx %x", IMAGE_ADDR,
				    IMAGE_SIZE));
	ut_assert_skip_to_line("Error: CROS-EC command failed (error -1)");

	/*
	 * nor any LDOs. This checks that the error is reported rather than
	 * being returned from the handler, where -1 means CMD_RET_USAGE
	 */
	ut_asserteq(1, run_command("crosec ldo 1", 0));
	ut_assert_skip_to_line("Error: CROS-EC command failed (error -1)");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_crosec_missing, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test that a broken handshake is reported as a failure */
static int cmd_test_crosec_test(struct unit_test_state *uts)
{
	struct udevice *dev;

	ut_assertok(uclass_first_device_err(UCLASS_CROS_EC, &dev));

	/* the EC then answers one more than the value the handshake wants */
	sandbox_cros_ec_set_test_flags(dev, CROSECT_BREAK_HELLO);
	ut_asserteq(1, run_command("crosec test", 0));
	ut_assert_nextline("Received invalid handshake %x",
			   HELLO_IN + HELLO_ADD + 1);
	ut_assert_nextline("Received invalid handshake %x", HELLO_IN);
	ut_assert_nextline("Test failed with error -%d", ENOTSYNC);
	ut_assert_console_end();

	sandbox_cros_ec_set_test_flags(dev, 0);

	return 0;
}
CMD_TEST(cmd_test_crosec_test, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test the arguments the command refuses */
static int cmd_test_crosec_usage(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("crosec", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("crosec frobnicate", 0));
	ut_assertok(check_usage(uts));

	/* a region is needed by these three */
	ut_asserteq(1, run_command("crosec regioninfo", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("crosec erase", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("crosec reboot", 0));
	ut_assertok(check_usage(uts));

	/* an address is needed as well as a region */
	ut_asserteq(1, run_command("crosec write rw", 0));
	ut_assertok(check_usage(uts));

	/* and an LDO index must be a number */
	ut_asserteq(1, run_command("crosec ldo", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("crosec ldo x", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_crosec_usage, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);
