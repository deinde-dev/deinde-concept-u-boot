// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Tests for acpi command
 */

#include <mapmem.h>
#include <acpi/acpi_table.h>
#include <asm/global_data.h>
#include <dm/acpi.h>
#include <linux/bitops.h>
#include <test/cmd.h>
#include <test/ut.h>

DECLARE_GLOBAL_DATA_PTR;

#define HAVE_RSDP BIT(0)
#define HAVE_XSDT BIT(1)
#define HAVE_FADT BIT(2)
#define HAVE_ALL (HAVE_RSDP | HAVE_XSDT | HAVE_FADT)

/* Header shown by 'acpi list' */
#define LIST_HDR	"Name              Base   Size  Detail"
#define LIST_DASHES	"----  ----------------  -----  ----------------------------"

/* Header shown by 'acpi items' */
#define ITEMS_HDR	"Seq  Type       Base   Size  Device/Writer"
#define ITEMS_DASHES	"---  -----  --------   ----  -------------"

/**
 * is_table_line() - Check whether a line of 'acpi list' describes a table
 *
 * The signature is four characters followed by two spaces, so this tells a
 * table apart from anything else the command may print, such as the warning
 * about a FADT which is not hardware-reduced.
 *
 * @line: Line to check
 * Return: true if the line starts with a table signature
 */
static bool is_table_line(const char *line)
{
	int i;

	for (i = 0; i < ACPI_NAME_LEN; i++) {
		if (line[i] == ' ' || !line[i])
			return false;
	}

	return line[ACPI_NAME_LEN] == ' ' && line[ACPI_NAME_LEN + 1] == ' ';
}

/**
 * cmd_test_acpi_base() - test the acpi command
 */
static int cmd_test_acpi_base(struct unit_test_state *uts)
{
	unsigned int actual = 0;
	int ret;

	/*
	 * Check that some mandatory ACPI tables exist:
	 * - RSDP
	 * - RSDT or XSDT
	 * - FADT
	 */
	ut_assertok(run_commandf("acpi list"));
	ut_assert_nextline(LIST_HDR);
	ut_assert_nextline(LIST_DASHES);
	for (;;) {
		ret = console_record_readline(uts->actual_str, sizeof(uts->actual_str));
		if (ret == -ENOENT) {
			ut_asserteq(HAVE_ALL, actual);

			return 0;
		}
		if (ret < 0)
			ut_asserteq(0, ret);

		if (!strncmp("RSDP", uts->actual_str, 4))
			actual |= HAVE_RSDP;
		else if (!strncmp("RSDT", uts->actual_str, 4))
			actual |= HAVE_XSDT;
		else if (!strncmp("XSDT", uts->actual_str, 4))
			actual |= HAVE_XSDT;
		else if (!strncmp("FACP", uts->actual_str, 4))
			actual |= HAVE_FADT;
	}
}
CMD_TEST(cmd_test_acpi_base, UTF_CONSOLE);

/* Test that 'acpi list -c' reports the checksum of each table */
static int cmd_test_acpi_list_chksum(struct unit_test_state *uts)
{
	int count = 0;
	int ret;

	ut_assertok(run_command("acpi list -c", 0));
	ut_assert_nextline(LIST_HDR);
	ut_assert_nextline(LIST_DASHES);
	for (;;) {
		ret = console_record_readline(uts->actual_str,
					      sizeof(uts->actual_str));
		if (ret == -ENOENT)
			break;
		ut_assert(ret >= 0);

		if (!is_table_line(uts->actual_str))
			continue;

		/* FACS has no header fields, so it shows no checksum */
		if (!strncmp("FACS", uts->actual_str, ACPI_NAME_LEN))
			continue;

		ut_assertf(!!strstr(uts->actual_str, "  OK"),
			   "no good checksum in '%s'", uts->actual_str);
		count++;
	}

	/* the RSDP, the RSDT or XSDT and the FADT are always present */
	ut_assert(count >= 3);

	return 0;
}
CMD_TEST(cmd_test_acpi_list_chksum, UTF_CONSOLE);

/* Test showing and setting the address of the ACPI tables */
static int cmd_test_acpi_set(struct unit_test_state *uts)
{
	ulong start = gd_acpi_start();

	ut_assertok(run_command("acpi set", 0));
	ut_assert_nextline("ACPI pointer: %lx", start);
	ut_assert_console_end();

	ut_assertok(run_commandf("acpi set %lx", start + 0x10));
	ut_assert_nextline("Setting ACPI pointer to %lx", start + 0x10);
	ut_asserteq(start + 0x10, gd_acpi_start());
	ut_assert_console_end();

	ut_assertok(run_command("acpi set", 0));
	ut_assert_nextline("ACPI pointer: %lx", start + 0x10);
	ut_assert_console_end();

	/* put it back, since the other tests need to find the tables */
	gd_set_acpi_start(start);

	return 0;
}
CMD_TEST(cmd_test_acpi_set, UTF_CONSOLE);

/* Test listing the pieces of ACPI data which devices and writers produced */
static int cmd_test_acpi_items(struct unit_test_state *uts)
{
	/*
	 * The list is built as U-Boot writes the tables, so it is empty where
	 * the tables come from QEMU instead
	 */
	if (!IS_ENABLED(CONFIG_ACPIGEN) || IS_ENABLED(CONFIG_QFW_ACPI))
		return -EAGAIN;

	ut_assertok(run_command("acpi items", 0));
	ut_assert_nextline(ITEMS_HDR);
	ut_assert_nextline(ITEMS_DASHES);

	/* the first item holds the RSDP and the tables which point to it */
	ut_assert_nextlinen("  0  other");
	console_record_reset();

	/* -d adds a dump of each item, starting with the first */
	ut_assertok(run_command("acpi items -d", 0));
	ut_assert_nextline(ITEMS_HDR);
	ut_assert_nextline(ITEMS_DASHES);
	ut_assert_nextlinen("  0  other");
	ut_assert_nextlinen("00000000: ");
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_acpi_items, UTF_CONSOLE);

/* Test dumping a single table */
static int cmd_test_acpi_dump(struct unit_test_state *uts)
{
	struct acpi_table_header *hdr;

	/* the FADT is mandatory, so it is always there to dump */
	hdr = acpi_find_table("FACP");
	ut_assertnonnull(hdr);

	/* the name is not case-sensitive */
	ut_assertok(run_command("acpi dump facp", 0));
	ut_assert_nextline("FACP @ %16lx", (ulong)nomap_to_sysmem(hdr));
	ut_assert_nextlines_are_dump(hdr->length);
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_acpi_dump, UTF_CONSOLE);

/* Test the options and arguments which the command refuses */
static int cmd_test_acpi_usage(struct unit_test_state *uts)
{
	ulong start = gd_acpi_start();

	/* an option which the command does not have */
	ut_asserteq(1, run_command("acpi list -x", 0));
	ut_assert_nextlinen("acpi - ");
	console_record_reset();

	if (!IS_ENABLED(CONFIG_CMD_ARG_CHECK))
		return 0;

	/* an argument which list has no use for */
	ut_asserteq(1, run_command("acpi list extra", 0));
	ut_assert_nextlinen("acpi - ");
	console_record_reset();

	/* set takes no options, so it does not read one as an address */
	ut_asserteq(1, run_command("acpi set -x", 0));
	ut_assert_nextlinen("acpi - ");
	ut_asserteq(start, gd_acpi_start());
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_acpi_usage, UTF_CONSOLE);

/* Test the errors which 'acpi dump' reports */
static int cmd_test_acpi_dump_err(struct unit_test_state *uts)
{
	/* a signature is four characters */
	ut_asserteq(1, run_command("acpi dump abc", 0));
	ut_assert_nextline("Table name 'abc' must be four characters");
	ut_assert_console_end();

	/* a table which is not present */
	ut_asserteq(1, run_command("acpi dump zzzz", 0));
	ut_assert_nextline("Table 'ZZZZ' not found");
	ut_assert_console_end();

	/* the name is needed */
	ut_asserteq(1, run_command("acpi dump", 0));
	ut_assert_nextline("acpi - ACPI tables");
	ut_assert_nextline_empty();

	/* the rest of the message is there only where the long help is */
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_nextline(
			"acpi list [-c] - list ACPI tables [check checksums]");
		ut_assert_nextline(
			"acpi items [-d]   - List/dump each piece of ACPI data from devices");
		ut_assert_nextline(
			"acpi set [<addr>] - Set or show address of ACPI tables");
		ut_assert_nextline("acpi dump <name>  - Dump ACPI table");
	}
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_acpi_dump_err, UTF_CONSOLE);
