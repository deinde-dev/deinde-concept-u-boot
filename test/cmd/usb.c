// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the usb command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <asm/state.h>
#include <test/cmd.h>
#include <test/ut.h>

/* where a block read lands */
#define USB_ADDR	(CONFIG_SYS_LOAD_ADDR + 0x2000)

/* a block well past the end of any of the emulated sticks */
#define USB_PAST_END	0x100000

/* the last line of the usage message */
#define USB_USAGE_END	"    from memory address `addr'"

/* lines too long to sit inside an assertion */
#define USB_SCAN_LINE	"       scanning usb for storage devices... 4 Storage Device(s) found"
#define USB_TEST_SET	"Test mode successfully set. Use 'usb start' to return to normal operation."
#define USB_READ_FAIL	"usb read: device 0 block # %d, count 1 ... 0 blocks read: ERROR"

/*
 * The tests reset the bus rather than starting it, since usb_started is a
 * global which an earlier test may have left set, and a 'usb start' then does
 * nothing at all. They stop it again at the end, so the next test finds the
 * bus as this one did.
 *
 * Which emulated stick is found first varies from run to run, so neither the
 * bus addresses nor the storage numbering is fixed, and the sticks are
 * different sizes. Only the number of devices found and what a sub-command
 * prints about the device it is given are checked here, never which stick
 * that is.
 */

/* whether delays were being honoured before the bus was reset */
static bool old_skip_delays;

/* Reset the bus and check the output of the scan */
static int reset_usb(struct unit_test_state *uts)
{
	/* the delays in the scan cost two seconds and show nothing */
	old_skip_delays = state_get_skip_delays();
	state_set_skip_delays(true);

	ut_assertok(run_command("usb reset", 0));
	ut_assert_nextline("resetting USB...");

	/*
	 * the scan probes the sticks, one of which is a single block long, so
	 * the filesystem probe has something to complain about
	 */
	ut_assert_skip_to_line("Bus usb@1: 6 USB Device(s) found");
	ut_assert_nextline(USB_SCAN_LINE);
	ut_assert_console_end();

	return 0;
}

/* Stop the bus again and put the delays back as they were */
static int stop_usb(struct unit_test_state *uts)
{
	ut_assertok(run_command("usb stop", 0));
	ut_assert_nextline("stopping USB..");
	ut_assert_console_end();
	state_set_skip_delays(old_skip_delays);

	return 0;
}

/* Test starting and stopping the bus */
static int cmd_test_usb_base(struct unit_test_state *uts)
{
	/* nothing works until the bus is enumerated */
	ut_asserteq(1, run_command("usb tree", 0));
	ut_assert_nextline("USB is stopped. Please issue 'usb start' first.");
	ut_assert_console_end();

	ut_assertok(reset_usb(uts));

	/* the hub is at address 1, with the five devices behind it */
	ut_assertok(run_command("usb tree", 0));
	ut_assert_nextline("USB device tree:");
	ut_assert_nextline("  1  Hub (480 Mb/s, 100mA)");
	ut_assert_nextline("  |  sandbox hub 2345");

	/* the order the devices come in is not fixed, so skip the rest */
	console_record_reset();

	ut_assertok(stop_usb(uts));

	ut_asserteq(1, run_command("usb tree", 0));
	ut_assert_nextline("USB is stopped. Please issue 'usb start' first.");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_usb_base, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test showing the descriptors of a device */
static int cmd_test_usb_info(struct unit_test_state *uts)
{
	ut_assertok(reset_usb(uts));

	/* device 0 is the hub, which 'usb tree' shows at address 1 */
	ut_assertok(run_command("usb info 0", 0));
	ut_assert_nextline("config for device 0");
	ut_assert_nextline("1: Hub,  USB Revision 2.0");
	ut_assert_nextline(" - sandbox hub 2345");
	ut_assert_nextline(" - Class: Hub");
	ut_assert_nextline(" - PacketSize: 0  Configurations: 1");
	ut_assert_nextline(" - Vendor: 0x1234  Product 0x5678 Version 0.0");
	ut_assert_nextline("   Configuration: 0");
	ut_assert_nextline("   - Interfaces: 1 Bus Powered 100mA");
	ut_assert_nextline("     Interface: 0");
	ut_assert_nextline("     - Alternate Setting 0, Endpoints: 1");
	ut_assert_nextline("     - Class Hub");
	ut_assert_nextline(
		"     - Endpoint 1 In Interrupt MaxPacket 1024 Interval 0ms");
	ut_assert_nextline_empty();
	ut_assert_console_end();

	/* a device which is not there is reported, but still counts as success */
	ut_assertok(run_command("usb info 99", 0));
	ut_assert_nextline("config for device 99");
	ut_assert_nextline("*** No device available ***");
	ut_assert_console_end();

	ut_assertok(stop_usb(uts));

	return 0;
}
CMD_TEST(cmd_test_usb_info, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test the USB 2.0 test modes */
static int cmd_test_usb_testmode(struct unit_test_state *uts)
{
	ut_assertok(reset_usb(uts));

	/* device 1 is not a hub, so its own upstream port is the only one */
	ut_assertok(run_command("usb test 1 0 J", 0));
	ut_assert_nextline("Setting Test_J mode on upstream facing port...");
	ut_assert_nextline(USB_TEST_SET);
	ut_assert_console_end();

	/* the mode is read from its first letter, so an unknown one is refused */
	ut_asserteq(1, run_command("usb test 1 0 X", 0));
	ut_assert_nextline("Unrecognized test mode: X");
	ut_assert_nextline(
		"Available modes: J, K, S[E0_NAK], P[acket], F[orce_Enable]");
	ut_assert_console_end();

	ut_asserteq(1, run_command("usb test 1 9 J", 0));
	ut_assert_nextline("Device is no hub or does not have 9 ports.");
	ut_assert_console_end();

	ut_asserteq(1, run_command("usb test 99 0 J", 0));
	ut_assert_nextline("Device 99 does not exist.");
	ut_assert_console_end();

	/* all three arguments are needed */
	ut_asserteq(1, run_command("usb test 1 0", 0));
	ut_assert_nextline("usb - USB sub-system");
	ut_assert_skip_to_line(USB_USAGE_END);
	ut_assert_console_end();

	ut_assertok(stop_usb(uts));

	return 0;
}
CMD_TEST(cmd_test_usb_testmode, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test the storage sub-commands, which come from the shared block handler */
static int cmd_test_usb_storage(struct unit_test_state *uts)
{
	ut_assertok(reset_usb(uts));

	/* all four sticks report the same strings, with different sizes */
	ut_assertok(run_command("usb storage", 0));
	ut_assert_nextline("  Device 0: Vendor: sandbox Rev: 1.0 Prod: flash");
	ut_assert_nextline("            Type: Hard Disk");
	ut_assert_skip_to_line(
		"  Device 3: Vendor: sandbox Rev: 1.0 Prod: flash");
	ut_assert_nextline("            Type: Hard Disk");
	console_record_reset();

	/* the scan leaves the first device current */
	ut_assertok(run_command("usb dev", 0));
	ut_assert_nextline_empty();
	ut_assert_nextline("usb device 0: Vendor: sandbox Rev: 1.0 Prod: flash");
	ut_assert_nextline("            Type: Hard Disk");
	console_record_reset();

	ut_assertok(run_command("usb dev 2", 0));
	ut_assert_skip_to_line("... is now current device");
	ut_assert_console_end();

	ut_asserteq(1, run_command("usb dev 99", 0));
	ut_assert_nextline_empty();
	ut_assert_nextline("Device 99: unknown device");
	ut_assert_console_end();

	ut_assertok(stop_usb(uts));

	return 0;
}
CMD_TEST(cmd_test_usb_storage, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test reading blocks from a storage device */
static int cmd_test_usb_read(struct unit_test_state *uts)
{
	ut_assertok(reset_usb(uts));

	/* every stick has at least one block */
	ut_assertok(run_commandf("usb read %lx 0 1", (ulong)USB_ADDR));
	ut_assert_nextline_empty();
	ut_assert_nextline(
		"usb read: device 0 block # 0, count 1 ... 1 blocks read: OK");
	ut_assert_console_end();

	/* a block past the end of the device reads nothing */
	ut_asserteq(1, run_commandf("usb read %lx %x 1", (ulong)USB_ADDR,
				    USB_PAST_END));
	ut_assert_nextline_empty();
	ut_assert_nextline(USB_READ_FAIL, USB_PAST_END);
	ut_assert_console_end();

	ut_assertok(stop_usb(uts));

	return 0;
}
CMD_TEST(cmd_test_usb_read, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test the arguments which the command does not understand */
static int cmd_test_usb_usage(struct unit_test_state *uts)
{
	/* the command needs a sub-command */
	ut_asserteq(1, run_command("usb", 0));
	ut_assert_nextline("usb - USB sub-system");
	ut_assert_skip_to_line(USB_USAGE_END);
	ut_assert_console_end();

	ut_assertok(reset_usb(uts));

	/* an unknown sub-command reaches the shared block handler */
	ut_asserteq(1, run_command("usb bogus", 0));
	ut_assert_nextline("usb - USB sub-system");
	ut_assert_skip_to_line(USB_USAGE_END);
	ut_assert_console_end();

	ut_assertok(stop_usb(uts));

	return 0;
}
CMD_TEST(cmd_test_usb_usage, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);
