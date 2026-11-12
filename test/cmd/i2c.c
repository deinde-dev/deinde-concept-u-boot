// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the i2c command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <mapmem.h>
#include <test/cmd.h>
#include <test/ut.h>

/* the EEPROM which sandbox has on bus 0, and an address with nothing on it */
#define EEPROM		"2c"
#define NO_CHIP		"50"

/* memory for the read and write sub-commands */
#define MEM_ADDR	(CONFIG_SYS_LOAD_ADDR + 0x2000)
#define MEM_SIZE	0x10

/*
 * Bytes of the EEPROM which these tests use. The devicetree gives the
 * bootcount two bytes at 0x0a and a MAC address six at 0x18, so keep below
 * both
 */
#define SCRATCH		8

/* the command which dumps them, and what it shows before anything is put in */
#define MD_SCRATCH	"i2c md " EEPROM " 0 8"
#define MD_EMPTY	"0000: 00 00 00 00 00 00 00 00    ........"

/* select bus 0, which every sub-command apart from 'bus' needs */
static int select_bus(struct unit_test_state *uts)
{
	ut_assertok(run_command("i2c dev 0", 0));
	ut_assert_nextline("Setting bus to 0");
	ut_assert_console_end();

	return 0;
}

/*
 * Put the scratch bytes to zero and the chip back to its devicetree flags, so
 * that a test which looks at the contents sees the same thing however many
 * tests have run before it. The '-s' form writes the lot in one transaction,
 * with no write delay, but it clears the write-address flag as it goes
 */
static int clear_chip(struct unit_test_state *uts)
{
	u8 *buf;

	buf = map_sysmem(MEM_ADDR, MEM_SIZE);
	memset(buf, '\0', SCRATCH);
	unmap_sysmem(buf);

	ut_assertok(run_commandf("i2c write %lx %s 0 %x -s", (ulong)MEM_ADDR,
				 EEPROM, SCRATCH));
	ut_assertok(run_commandf("i2c flags %s 0", EEPROM));
	ut_assert_console_end();

	return 0;
}

/* Test showing and selecting the bus */
static int cmd_test_i2c_base(struct unit_test_state *uts)
{
	ut_assertok(select_bus(uts));

	ut_assertok(run_command("i2c dev", 0));
	ut_assert_nextline("Current bus is 0");
	ut_assert_console_end();

	/* the bus, with the chips the devicetree gives it */
	ut_assertok(run_command("i2c bus 0", 0));
	ut_assert_nextline("Bus 0:\ti2c@0  (active 0)");
	ut_assert_nextline("   2c: eeprom@2c, offset len 1, flags 0");
	ut_assert_nextline("   43: rtc@43, offset len 1, flags 0");

	/* the chips after this one differ between the sandbox variants */
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_i2c_base, UTF_CONSOLE);

/* Test showing and setting the bus speed */
static int cmd_test_i2c_speed(struct unit_test_state *uts)
{
	ut_assertok(select_bus(uts));

	ut_assertok(run_command("i2c speed", 0));
	ut_assert_nextline("Current bus speed=100000");
	ut_assert_console_end();

	ut_assertok(run_command("i2c speed 400000", 0));
	ut_assert_nextline("Setting bus speed to 400000 Hz");
	ut_assert_console_end();

	ut_assertok(run_command("i2c speed", 0));
	ut_assert_nextline("Current bus speed=400000");
	ut_assert_console_end();

	/* put it back, since the bus outlives the test */
	ut_assertok(run_command("i2c speed 100000", 0));
	ut_assert_nextline("Setting bus speed to 100000 Hz");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_i2c_speed, UTF_CONSOLE);

/* Test probing for chips on the bus */
static int cmd_test_i2c_probe(struct unit_test_state *uts)
{
	ut_assertok(select_bus(uts));

	/* an address which has a chip */
	ut_assertok(run_commandf("i2c probe %s", EEPROM));
	ut_assert_nextline("Valid chip addresses: 2C");
	ut_assert_console_end();

	/* and one which has not, which gives 1 since nothing is found */
	ut_asserteq(1, run_commandf("i2c probe %s", NO_CHIP));

	/*
	 * The emulator logs the chip it has no emulation for, on the same line,
	 * so check the start of it and throw the rest away
	 */
	ut_assert_nextlinen("Valid chip addresses:");
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_i2c_probe, UTF_CONSOLE);

/* Test reading the chip with 'i2c md' */
static int cmd_test_i2c_md(struct unit_test_state *uts)
{
	char *const args[] = {"i2c", "md", EEPROM, "0", "8"};
	int repeatable;

	ut_assertok(select_bus(uts));
	ut_assertok(clear_chip(uts));

	ut_assertok(run_command(MD_SCRATCH, 0));
	ut_assert_nextline(MD_EMPTY);
	ut_assert_console_end();

	/* the count is remembered, so this shows the same eight bytes */
	ut_assertok(run_commandf("i2c md %s 0", EEPROM));
	ut_assert_nextline(MD_EMPTY);
	ut_assert_console_end();

	/*
	 * A repeat carries on from where the last one stopped. run_command()
	 * drops the repeat flag when hush is in use, so go through
	 * cmd_process()
	 */
	ut_assertok(cmd_process(CMD_FLAG_REPEAT, ARRAY_SIZE(args), args,
				&repeatable, NULL));
	ut_assert_nextline("0008: 00 00 00 00 00 00 00 00    ........");
	ut_assert_console_end();

	/* an offset of more than three bytes is refused */
	ut_asserteq(1, run_commandf("i2c md %s 0.4 4", EEPROM));
	ut_assert_nextline("i2c - I2C sub-system");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_skip_to_linen("i2c bus [n]");
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_i2c_md, UTF_CONSOLE);

/* Test writing a value to the chip with 'i2c mw' */
static int cmd_test_i2c_mw(struct unit_test_state *uts)
{
	ut_assertok(select_bus(uts));
	ut_assertok(clear_chip(uts));

	/* a single byte by default */
	ut_assertok(run_commandf("i2c mw %s 0 55", EEPROM));
	ut_assertok(run_command(MD_SCRATCH, 0));
	ut_assert_nextline("0000: 55 00 00 00 00 00 00 00    U.......");
	ut_assert_console_end();

	/* and a count fills that many, at increasing offsets */
	ut_assertok(run_commandf("i2c mw %s 2 ab 3", EEPROM));
	ut_assertok(run_command(MD_SCRATCH, 0));
	ut_assert_nextline("0000: 55 00 ab ab ab 00 00 00    U.......");
	ut_assert_console_end();

	ut_assertok(clear_chip(uts));
	ut_assertok(run_command(MD_SCRATCH, 0));
	ut_assert_nextline(MD_EMPTY);
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_i2c_mw, UTF_CONSOLE);

/* Test moving bytes between memory and the chip */
static int cmd_test_i2c_rw(struct unit_test_state *uts)
{
	u8 *buf;

	ut_assertok(select_bus(uts));
	ut_assertok(clear_chip(uts));

	buf = map_sysmem(MEM_ADDR, MEM_SIZE);
	memset(buf, '\0', MEM_SIZE);
	buf[0] = 0x12;
	buf[1] = 0x34;

	/* write two bytes from memory to offset 4 of the chip */
	ut_assertok(run_commandf("i2c write %lx %s 4 2", (ulong)MEM_ADDR,
				 EEPROM));
	ut_assertok(run_command(MD_SCRATCH, 0));
	ut_assert_nextline("0000: 00 00 00 00 12 34 00 00    .....4..");
	ut_assert_console_end();

	/* each byte went out with its own offset, which sets that chip flag */
	ut_assertok(run_commandf("i2c flags %s", EEPROM));
	ut_assert_nextline("4");
	ut_assert_console_end();

	/* read them back to a different place in memory */
	ut_assertok(run_commandf("i2c read %s 4 2 %lx", EEPROM,
				 (ulong)MEM_ADDR + 8));
	ut_assert_console_end();
	ut_asserteq(0x12, buf[8]);
	ut_asserteq(0x34, buf[9]);

	/* -s writes the whole length in one transaction, with no offset byte */
	buf[0] = 0x56;
	buf[1] = 0x78;
	ut_assertok(run_commandf("i2c write %lx %s 6 2 -s", (ulong)MEM_ADDR,
				 EEPROM));
	ut_assertok(run_command(MD_SCRATCH, 0));
	ut_assert_nextline("0000: 00 00 00 00 12 34 56 78    .....4Vx");
	ut_assert_console_end();

	ut_assertok(run_commandf("i2c flags %s", EEPROM));
	ut_assert_nextline("0");
	ut_assert_console_end();

	unmap_sysmem(buf);
	ut_assertok(clear_chip(uts));

	return 0;
}
CMD_TEST(cmd_test_i2c_rw, UTF_CONSOLE);

/* Test the checksum of a region of the chip */
static int cmd_test_i2c_crc(struct unit_test_state *uts)
{
	ut_assertok(select_bus(uts));
	ut_assertok(clear_chip(uts));

	ut_assertok(run_commandf("i2c crc32 %s 0 %x", EEPROM, SCRATCH));
	ut_assert_nextline("CRC32 for 00000000 ... 00000007 ==> 6522df69");
	ut_assert_console_end();

	/* a different value gives a different checksum */
	ut_assertok(run_commandf("i2c mw %s 0 55", EEPROM));
	ut_assertok(run_commandf("i2c crc32 %s 0 %x", EEPROM, SCRATCH));
	ut_assert_nextline("CRC32 for 00000000 ... 00000007 ==> 78e422cb");
	ut_assert_console_end();

	ut_assertok(clear_chip(uts));

	return 0;
}
CMD_TEST(cmd_test_i2c_crc, UTF_CONSOLE);

/* Test showing and setting the chip's flags and offset length */
static int cmd_test_i2c_chip(struct unit_test_state *uts)
{
	ut_assertok(select_bus(uts));

	/* the EEPROM takes a one-byte offset */
	ut_assertok(run_commandf("i2c olen %s", EEPROM));
	ut_assert_nextline("1");
	ut_assert_console_end();

	ut_assertok(run_commandf("i2c olen %s 2", EEPROM));
	ut_assertok(run_commandf("i2c olen %s", EEPROM));
	ut_assert_nextline("2");
	ut_assert_console_end();

	ut_assertok(run_commandf("i2c olen %s 1", EEPROM));
	ut_assert_console_end();

	/* flag 1 asks for 10-bit addressing */
	ut_assertok(run_commandf("i2c flags %s", EEPROM));
	ut_assert_nextline("0");
	ut_assert_console_end();

	ut_assertok(run_commandf("i2c flags %s 1", EEPROM));
	ut_assertok(run_commandf("i2c flags %s", EEPROM));
	ut_assert_nextline("1");
	ut_assert_console_end();

	ut_assertok(run_commandf("i2c flags %s 0", EEPROM));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_i2c_chip, UTF_CONSOLE);

/* Test modifying the chip a location at a time */
static int cmd_test_i2c_mm(struct unit_test_state *uts)
{
	ut_assertok(select_bus(uts));

	/*
	 * Write two values, skip the third with an empty line, then finish
	 * with something which is not a hex value
	 */
	ut_assertok(clear_chip(uts));

	console_in_puts("11\n22\n\nq\n");
	ut_assertok(run_commandf("i2c mm %s 0", EEPROM));
	ut_assert_nextline("00000000: 00 ? 11");
	ut_assert_nextline("00000001: 00 ? 22");
	ut_assert_nextlinen("00000002: 00 ? ");
	ut_assert_nextline("00000003: 00 ? q");
	ut_assert_console_end();

	ut_assertok(run_command(MD_SCRATCH, 0));
	ut_assert_nextline("0000: 11 22 00 00 00 00 00 00    .\"......");
	ut_assert_console_end();

	/* 'i2c nm' stays at the same location */
	console_in_puts("33\n44\nq\n");
	ut_assertok(run_commandf("i2c nm %s 4", EEPROM));
	ut_assert_nextline("00000004: 00 ? 33");
	ut_assert_nextline("00000004: 33 ? 44");
	ut_assert_nextline("00000004: 44 ? q");
	ut_assert_console_end();

	ut_assertok(run_command(MD_SCRATCH, 0));
	ut_assert_nextline("0000: 11 22 00 00 44 00 00 00    .\"..D...");
	ut_assert_console_end();

	ut_assertok(clear_chip(uts));

	return 0;
}
CMD_TEST(cmd_test_i2c_mm, UTF_CONSOLE);

/* Test the error paths */
static int cmd_test_i2c_err(struct unit_test_state *uts)
{
	ut_assertok(select_bus(uts));

	/* a bus which does not exist */
	ut_asserteq(1, run_command("i2c bus 3", 0));
	ut_assert_nextline("Invalid bus 3: err=-19");
	ut_assert_console_end();

	/* selecting one leaves the current bus as it was */
	ut_asserteq(1, run_command("i2c dev 3", 0));
	ut_assert_nextline("Setting bus to 3");
	ut_assert_nextline("Failure changing bus number (-19)");
	ut_assert_console_end();

	ut_assertok(run_command("i2c dev", 0));
	ut_assert_nextline("Current bus is 0");
	ut_assert_console_end();

	/* a chip which does not answer; the emulator logs it first */
	ut_asserteq(1, run_commandf("i2c md %s 0 4", NO_CHIP));
	ut_assert_skip_to_line("Error reading the chip: -2");
	ut_assert_console_end();

	/* an EEPROM is not a monitor */
	if (IS_ENABLED(CONFIG_I2C_EDID)) {
		ut_asserteq(1, run_commandf("i2c edid %s", EEPROM));
		ut_assert_nextline("Content isn't valid EDID.");
		ut_assert_console_end();
	}

	/* and sandbox's driver cannot recover the bus */
	ut_asserteq(1, run_command("i2c reset", 0));
	ut_assert_nextline("Error: Not supported by the driver");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_i2c_err, UTF_CONSOLE);

/* Test the command with a bad command line */
static int cmd_test_i2c_usage(struct unit_test_state *uts)
{
	ut_assertok(select_bus(uts));

	/* a sub-command is required */
	ut_asserteq(1, run_command("i2c", 0));
	ut_assert_nextline("i2c - I2C sub-system");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_skip_to_linen("i2c bus [n]");
	console_record_reset();

	/* as is one which exists */
	ut_asserteq(1, run_command("i2c frobnicate", 0));
	ut_assert_nextline("i2c - I2C sub-system");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_skip_to_linen("i2c bus [n]");
	console_record_reset();

	/* 'md' needs a chip and an address */
	ut_asserteq(1, run_commandf("i2c md %s", EEPROM));
	ut_assert_nextline("i2c - I2C sub-system");
	ut_assert_skip_to_linen("i2c bus [n]");
	console_record_reset();

	/* 'mw' needs a value as well */
	ut_asserteq(1, run_commandf("i2c mw %s 0", EEPROM));
	ut_assert_nextline("i2c - I2C sub-system");
	ut_assert_skip_to_linen("i2c bus [n]");
	console_record_reset();

	/* 'read' needs both a length and a memory address */
	ut_asserteq(1, run_commandf("i2c read %s 0 4", EEPROM));
	ut_assert_nextline("i2c - I2C sub-system");
	ut_assert_skip_to_linen("i2c bus [n]");
	console_record_reset();

	/* 'loop' needs a chip and an address */
	ut_asserteq(1, run_commandf("i2c loop %s", EEPROM));
	ut_assert_nextline("i2c - I2C sub-system");
	ut_assert_skip_to_linen("i2c bus [n]");
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_i2c_usage, UTF_CONSOLE);
