// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2026 Free Mobile - Vincent Jardin
 *
 * Unit tests for the PMBus 1.x framework, the generic
 * PMBus regulator and the pmbus CLI command.
 */

#include <console.h>
#include <dm.h>
#include <i2c.h>
#include <pmbus.h>
#include <dm/test.h>
#include <test/test.h>
#include <test/ut.h>

/* The line pmbus dev prints when a chip is selected */
#define PMBUS_ACTIVE_LINE \
	"pmbus: active i2c0:0x70  rail=\"sandbox-pmbus-vout\"  " \
	"MFR_ID=\"SANDBOX\"  MODEL=\"PMBUS-EMUL\"  vendor=(generic)"

/* The line every sub-command needing a chip prints when there is none */
#define PMBUS_NO_ACTIVE_LINE \
	"pmbus: no active device. Use 'pmbus dev <bus>:<addr>' first."

/* The line pmbus dev prints for something it cannot resolve to a chip */
#define PMBUS_NO_CHIP_LINE(what) \
	"pmbus: '" what "' is neither <bus>:<addr> nor a known regulator-name (-19)"

/* The line pmbus dev prints for an address which no chip answers */
#define PMBUS_UNKNOWN_LINE \
	"pmbus: active i2c0:0x71  MFR_ID=\"?\"  MODEL=\"?\"  " \
	"vendor=(generic)  [no driver_info]"

/* The line pmbus list prints for the bound chip */
#define PMBUS_LIST_LINE \
	"  i2c0:0x70  rail=\"sandbox-pmbus-vout\"  node=pmbus@70  " \
	"driver=pmbus_generic_regulator"

/* Select the emulated chip and check the resulting banner line */
static int pmbus_select(struct unit_test_state *uts)
{
	ut_assertok(run_command("pmbus dev 0:70", 0));
	ut_assert_nextline(PMBUS_ACTIVE_LINE);
	return 0;
}

/* The chip is reachable via UCLASS_REGULATOR (compatible = "pmbus") */
static int dm_test_pmbus_bind(struct unit_test_state *uts)
{
	struct udevice *dev;

	ut_assertok(uclass_get_device_by_name(UCLASS_REGULATOR, "pmbus@70",
					      &dev));
	ut_asserteq_str("pmbus_generic_regulator", dev->driver->name);
	ut_asserteq(UCLASS_I2C, device_get_uclass_id(dev_get_parent(dev)));

	return 0;
}

DM_TEST(dm_test_pmbus_bind, UTF_SCAN_FDT);

/* pmbus dev by <bus>:<addr> and by regulator-name select the chip */
static int dm_test_pmbus_dev(struct unit_test_state *uts)
{
	ut_assertok(run_command("pmbus dev 0:70", 0));
	ut_assert_nextline(PMBUS_ACTIVE_LINE);
	ut_assert_console_end();

	/* Selecting by DT regulator-name resolves to the same chip */
	ut_assertok(run_command("pmbus dev sandbox-pmbus-vout", 0));
	ut_assert_nextline(PMBUS_ACTIVE_LINE);
	ut_assert_console_end();

	/* pmbus dev with no argument reprints the active chip */
	ut_assertok(run_command("pmbus dev", 0));
	ut_assert_nextline(PMBUS_ACTIVE_LINE);
	ut_assert_console_end();

	return 0;
}

DM_TEST(dm_test_pmbus_dev, UTF_SCAN_FDT | UTF_CONSOLE);

/* pmbus list enumerates the bound chip among the UCLASS_REGULATOR devices */
static int dm_test_pmbus_list(struct unit_test_state *uts)
{
	ut_assertok(run_command("pmbus list", 0));
	ut_assert_skip_to_line(PMBUS_LIST_LINE);

	return 0;
}

DM_TEST(dm_test_pmbus_list, UTF_SCAN_FDT | UTF_CONSOLE);

/* pmbus info decodes identification + the detected driver_info */
static int dm_test_pmbus_info(struct unit_test_state *uts)
{
	ut_assertok(pmbus_select(uts));

	ut_assertok(run_command("pmbus info", 0));
	ut_assert_nextline("pmbus device i2c0:0x70");
	ut_assert_nextline("  regulator-name: \"sandbox-pmbus-vout\"");
	ut_assert_nextline("  MFR_ID        : \"SANDBOX\"");
	ut_assert_nextline("  MFR_MODEL     : \"PMBUS-EMUL\"");
	ut_assert_nextline("  MFR_REVISION  : \"1.0\"  raw=0x312e30");
	ut_assert_nextline("  PMBUS_REVISION: 0x33 (PMBus 1.3)");
	ut_assert_nextline("  vendor        : (none)");
	ut_assert_nextline("  driver_info   : pages=1");
	ut_assert_nextline("    [VOLTAGE_IN  ] format=LINEAR");
	ut_assert_nextline("    [VOLTAGE_OUT ] format=LINEAR");
	ut_assert_nextline("    [CURRENT_IN  ] format=LINEAR");
	ut_assert_nextline("    [CURRENT_OUT ] format=LINEAR");
	ut_assert_nextline("    [POWER       ] format=LINEAR");
	ut_assert_nextline("    [TEMPERATURE ] format=LINEAR");
	ut_assert_console_end();

	return 0;
}

DM_TEST(dm_test_pmbus_info, UTF_SCAN_FDT | UTF_CONSOLE);

/*
 * pmbus telemetry decodes the implemented sensors and prints
 * "(not supported)" for the commands the emulator NAKs (READ_IIN,
 * READ_POUT). LINEAR11 is used for VIN/IOUT/TEMP, LINEAR16 (with the
 * VOUT_MODE 2^-8 exponent) for VOUT.
 */
static int dm_test_pmbus_telemetry(struct unit_test_state *uts)
{
	ut_assertok(pmbus_select(uts));

	ut_assertok(run_command("pmbus telemetry", 0));
	ut_assert_nextline("pmbus telemetry @ i2c0:0x70");
	ut_assert_nextline("  VIN  : raw=0x0abc  1400.000V");
	ut_assert_nextline("  VOUT : raw=0x0200  2.000V");
	ut_assert_nextline("  IIN  : (not supported)");
	ut_assert_nextline("  IOUT : raw=0x0123  291.000A");
	ut_assert_nextline("  POUT : (not supported)");
	ut_assert_nextline("  TEMP : raw=0x0019  25.000C");
	ut_assert_console_end();

	return 0;
}

DM_TEST(dm_test_pmbus_telemetry, UTF_SCAN_FDT | UTF_CONSOLE);

/* pmbus status decodes every STATUS_* register; the emulator is clean */
static int dm_test_pmbus_status(struct unit_test_state *uts)
{
	ut_assertok(pmbus_select(uts));

	ut_assertok(run_command("pmbus status", 0));
	ut_assert_nextline("pmbus status @ i2c0:0x70");
	ut_assert_nextline("  STATUS_WORD    (79h) = 0x0000  [clean]");
	ut_assert_nextline("  STATUS_VOUT    (7Ah) = 0x00  [clean]");
	ut_assert_nextline("  STATUS_IOUT    (7Bh) = 0x00  [clean]");
	ut_assert_nextline("  STATUS_INPUT   (7Ch) = 0x00  [clean]");
	ut_assert_nextline("  STATUS_TEMP    (7Dh) = 0x00  [clean]");
	ut_assert_nextline("  STATUS_CML     (7Eh) = 0x00  [clean]");
	ut_assert_console_end();

	return 0;
}

DM_TEST(dm_test_pmbus_status, UTF_SCAN_FDT | UTF_CONSOLE);

/* pmbus read/pmbus write raw register access (byte, word, string) */
static int dm_test_pmbus_read_write(struct unit_test_state *uts)
{
	ut_assertok(pmbus_select(uts));

	/* Symbolic and numeric register names both resolve */
	ut_assertok(run_command("pmbus read VOUT_MODE", 0));
	ut_assert_nextline("  20h  VOUT_MODE       b=0x18");
	ut_assertok(run_command("pmbus read 8b w", 0));
	ut_assert_nextline("  8bh  READ_VOUT       w=0x0200");
	ut_assertok(run_command("pmbus read MFR_ID s", 0));
	ut_assert_nextline("  99h  MFR_ID          s=\"SANDBOX\"");

	/* A word write is observable on the next read-back */
	ut_assertok(run_command("pmbus write VOUT_COMMAND 123 w", 0));
	ut_assert_nextline("pmbus: wrote 0x123 to 21h (VOUT_COMMAND)");
	ut_assertok(run_command("pmbus read VOUT_COMMAND w", 0));
	ut_assert_nextline("  21h  VOUT_COMMAND    w=0x0123");
	ut_assert_console_end();

	return 0;
}

DM_TEST(dm_test_pmbus_read_write, UTF_SCAN_FDT | UTF_CONSOLE);

/* pmbus vout reads back VOUT via the active driver_info decoder */
static int dm_test_pmbus_vout(struct unit_test_state *uts)
{
	ut_assertok(pmbus_select(uts));

	ut_assertok(run_command("pmbus vout", 0));
	ut_assert_nextline("pmbus VOUT @ i2c0:0x70  raw=0x0200  2.000V");
	ut_assert_console_end();

	return 0;
}

DM_TEST(dm_test_pmbus_vout, UTF_SCAN_FDT | UTF_CONSOLE);

/* pmbus dump walks every standard register; spot-check one line */
static int dm_test_pmbus_dump(struct unit_test_state *uts)
{
	ut_assertok(pmbus_select(uts));

	ut_assertok(run_command("pmbus dump", 0));
	ut_assert_nextline("pmbus dump @ i2c0:0x70 (registers known to <pmbus.h>)");
	ut_assert_skip_to_line("  20h  VOUT_MODE       b=0x18");

	return 0;
}

DM_TEST(dm_test_pmbus_dump, UTF_SCAN_FDT | UTF_CONSOLE);

/* pmbus clear [faults] issues CLEAR_FAULTS (03h) */
static int dm_test_pmbus_clear(struct unit_test_state *uts)
{
	ut_assertok(pmbus_select(uts));

	ut_assertok(run_command("pmbus clear faults", 0));
	ut_assert_nextline("pmbus: CLEAR_FAULTS (03h) issued (RAM sticky STATUS_* cleared)");
	ut_assert_console_end();

	return 0;
}

DM_TEST(dm_test_pmbus_clear, UTF_SCAN_FDT | UTF_CONSOLE);

/* pmbus scan finds the emulated chip by its MFR_ID block read */
static int dm_test_pmbus_scan(struct unit_test_state *uts)
{
	ut_assertok(run_command("pmbus scan 0", 0));
	ut_assert_skip_to_line("  i2c0:0x70  MFR_ID=\"SANDBOX\"");

	return 0;
}

DM_TEST(dm_test_pmbus_scan, UTF_SCAN_FDT | UTF_CONSOLE);

/* pmbus help lists vendor extensions; none are registered here */
static int dm_test_pmbus_help(struct unit_test_state *uts)
{
	ut_assertok(run_command("pmbus help", 0));
	ut_assert_nextline("pmbus: no vendor extensions registered.");
	ut_assert_skip_to_line("       board hook (boot snapshot) and re run 'pmbus help'.");

	return 0;
}

DM_TEST(dm_test_pmbus_help, UTF_SCAN_FDT | UTF_CONSOLE);

/* Check that the usage message is shown and drop the rest of it */
static int pmbus_check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("pmbus - PMBus 1.x device interrogation and control");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	console_record_reset();

	return 0;
}

/* Every sub-command which works on a chip needs one to be selected first */
static int dm_test_pmbus_no_active(struct unit_test_state *uts)
{
	pmbus_clear_active();

	/* pmbus dev says there is none and succeeds */
	ut_assertok(run_command("pmbus dev", 0));
	ut_assert_nextline("pmbus: no active device");
	ut_assert_console_end();

	/* the sub-commands which read the chip fail */
	ut_asserteq(1, run_command("pmbus info", 0));
	ut_assert_nextline(PMBUS_NO_ACTIVE_LINE);
	ut_asserteq(1, run_command("pmbus telemetry", 0));
	ut_assert_nextline(PMBUS_NO_ACTIVE_LINE);
	ut_asserteq(1, run_command("pmbus status", 0));
	ut_assert_nextline(PMBUS_NO_ACTIVE_LINE);
	ut_asserteq(1, run_command("pmbus dump", 0));
	ut_assert_nextline(PMBUS_NO_ACTIVE_LINE);
	ut_asserteq(1, run_command("pmbus read VOUT_MODE", 0));
	ut_assert_nextline(PMBUS_NO_ACTIVE_LINE);
	ut_asserteq(1, run_command("pmbus write VOUT_COMMAND 200 w", 0));
	ut_assert_nextline(PMBUS_NO_ACTIVE_LINE);
	ut_asserteq(1, run_command("pmbus clear faults", 0));
	ut_assert_nextline(PMBUS_NO_ACTIVE_LINE);
	ut_asserteq(1, run_command("pmbus vout", 0));
	ut_assert_nextline(PMBUS_NO_ACTIVE_LINE);
	ut_assert_console_end();

	/* the ones which do not need a chip still work */
	ut_assertok(run_command("pmbus list", 0));
	ut_assert_skip_to_line(PMBUS_LIST_LINE);
	console_record_reset();

	return 0;
}
DM_TEST(dm_test_pmbus_no_active, UTF_SCAN_FDT | UTF_CONSOLE);

/* pmbus dev reports a name it cannot resolve and a bus which is not there */
static int dm_test_pmbus_dev_missing(struct unit_test_state *uts)
{
	/* a regulator-name which no device has */
	ut_asserteq(1, run_command("pmbus dev nosuchrail", 0));
	ut_assert_nextline(PMBUS_NO_CHIP_LINE("nosuchrail"));
	ut_assert_console_end();

	/* an address outside the 7-bit range is not <bus>:<addr> either */
	ut_asserteq(1, run_command("pmbus dev 0:80", 0));
	ut_assert_nextline(PMBUS_NO_CHIP_LINE("0:80"));
	ut_assert_console_end();

	/* the form is right here, but there is no i2c9 */
	ut_asserteq(1, run_command("pmbus dev 9:70", 0));
	ut_assert_nextline("pmbus: cannot select i2c9:0x70 (-19)");
	ut_assert_console_end();

	/*
	 * An address with nothing at it is selected all the same, since the
	 * bus does not probe it. The failure shows up on the first read.
	 * Sandbox's I2C emulation logs a line of its own for each such
	 * address, so skip to the ones the command prints.
	 */
	ut_assertok(run_command("pmbus dev 0:71", 0));
	ut_assert_skip_to_line(PMBUS_UNKNOWN_LINE);
	ut_asserteq(1, run_command("pmbus read VOUT_MODE", 0));
	ut_assert_skip_to_line("pmbus: read byte 0x20 failed (-2)");
	console_record_reset();

	pmbus_clear_active();

	return 0;
}
DM_TEST(dm_test_pmbus_dev_missing, UTF_SCAN_FDT | UTF_CONSOLE);

/* A register is accepted by name whatever its case, and by code in hex */
static int dm_test_pmbus_read_forms(struct unit_test_state *uts)
{
	ut_assertok(pmbus_select(uts));

	/* the name is matched without regard to case */
	ut_assertok(run_command("pmbus read vout_mode", 0));
	ut_assert_nextline("  20h  VOUT_MODE       b=0x18");

	/* the code may carry a 0x prefix */
	ut_assertok(run_command("pmbus read 0x20", 0));
	ut_assert_nextline("  20h  VOUT_MODE       b=0x18");

	/* the format defaults to a byte, which gives the low half of a word */
	ut_assertok(run_command("pmbus read READ_VOUT", 0));
	ut_assert_nextline("  8bh  READ_VOUT       b=0x00");
	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_pmbus_read_forms, UTF_SCAN_FDT | UTF_CONSOLE);

/* pmbus vout <uV> encodes the voltage for the format VOUT_MODE selects */
static int dm_test_pmbus_vout_set(struct unit_test_state *uts)
{
	ut_assertok(pmbus_select(uts));

	/* VOUT_MODE 0x18 asks for LINEAR16 with an exponent of -8 */
	ut_assertok(run_command("pmbus vout 1500000", 0));
	ut_assert_nextline("pmbus: VOUT_COMMAND <- 0x0180 (LINEAR16, target 1500000 uV)");
	ut_assertok(run_command("pmbus read VOUT_COMMAND w", 0));
	ut_assert_nextline("  21h  VOUT_COMMAND    w=0x0180");

	/* put the chip back as it was */
	ut_assertok(run_command("pmbus write VOUT_COMMAND 200 w", 0));
	ut_assert_nextline("pmbus: wrote 0x200 to 21h (VOUT_COMMAND)");
	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_pmbus_vout_set, UTF_SCAN_FDT | UTF_CONSOLE);

/* pmbus scan reports a bus which does not exist */
static int dm_test_pmbus_scan_missing(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("pmbus scan 9", 0));
	ut_assert_nextline("pmbus: i2c9 not available");
	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_pmbus_scan_missing, UTF_SCAN_FDT | UTF_CONSOLE);

/* Each way of getting the command line wrong shows the usage message */
static int dm_test_pmbus_usage(struct unit_test_state *uts)
{
	ut_assertok(pmbus_select(uts));

	/* a sub-command is required */
	ut_asserteq(1, run_command("pmbus", 0));
	ut_assertok(pmbus_check_usage(uts));

	/* as is one which exists */
	ut_asserteq(1, run_command("pmbus frobnicate", 0));
	ut_assert_nextline("pmbus: unknown subcommand 'frobnicate'");
	ut_assertok(pmbus_check_usage(uts));

	/* read needs a register, and takes at most a format after it */
	ut_asserteq(1, run_command("pmbus read", 0));
	ut_assertok(pmbus_check_usage(uts));
	ut_asserteq(1, run_command("pmbus read VOUT_MODE b extra", 0));
	ut_assertok(pmbus_check_usage(uts));
	ut_asserteq(1, run_command("pmbus read zz", 0));
	ut_assert_nextline("pmbus: invalid register 'zz'");
	ut_assertok(pmbus_check_usage(uts));
	ut_asserteq(1, run_command("pmbus read VOUT_MODE x", 0));
	ut_assert_nextline("pmbus: unknown format 'x' (expected b, w, or s)");
	ut_assertok(pmbus_check_usage(uts));

	/* write needs a value too, and has no string form */
	ut_asserteq(1, run_command("pmbus write VOUT_COMMAND", 0));
	ut_assertok(pmbus_check_usage(uts));
	ut_asserteq(1, run_command("pmbus write VOUT_COMMAND zz", 0));
	ut_assert_nextline("pmbus: invalid value 'zz'");
	ut_assertok(pmbus_check_usage(uts));
	ut_asserteq(1, run_command("pmbus write VOUT_COMMAND 200 s", 0));
	ut_assert_nextline("pmbus: unknown format 's' (expected b or w)");
	ut_assertok(pmbus_check_usage(uts));

	/* the value must fit the format it is written with */
	ut_asserteq(1, run_command("pmbus write VOUT_COMMAND 100 b", 0));
	ut_assert_nextline("pmbus: byte value out of range");
	ut_assertok(pmbus_check_usage(uts));
	ut_asserteq(1, run_command("pmbus write VOUT_COMMAND 10000 w", 0));
	ut_assert_nextline("pmbus: word value out of range");
	ut_assertok(pmbus_check_usage(uts));

	/* clear takes the word 'faults' or nothing at all */
	ut_asserteq(1, run_command("pmbus clear everything", 0));
	ut_assert_nextline("pmbus: unknown clear subcommand 'everything' (expected 'faults')");
	ut_assertok(pmbus_check_usage(uts));

	/* vout takes microvolts in decimal */
	ut_asserteq(1, run_command("pmbus vout zz", 0));
	ut_assert_nextline("pmbus: invalid microvolt value 'zz'");
	ut_assertok(pmbus_check_usage(uts));

	/* scan takes a bus number in decimal */
	ut_asserteq(1, run_command("pmbus scan zz", 0));
	ut_assert_nextline("pmbus: invalid bus seq 'zz'");
	ut_assertok(pmbus_check_usage(uts));

	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_pmbus_usage, UTF_SCAN_FDT | UTF_CONSOLE);
