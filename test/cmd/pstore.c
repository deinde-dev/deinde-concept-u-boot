// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the pstore command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <mapmem.h>
#include <memalign.h>
#include <test/cmd.h>
#include <test/ut.h>

/* Signature which Linux writes at the start of a zone, 'DBGC' */
#define PSTORE_SIG	0x43474244

/* Size of each zone the tests set up */
#define ZONE_SIZE	0x1000

/* The memory holds one dump zone and the console zone */
#define MEM_SIZE	(2 * ZONE_SIZE)

/* First line of a dump record, which holds the time of the crash */
#define DUMP_HDR	"====1.0-D\n"

/*
 * Header at the start of a zone, as struct persistent_ram_buffer in
 * cmd/pstore.c. The command declares it privately, so the tests declare it
 * again
 */
struct test_zone_hdr {
	u32 sig;
	u32 start;
	u32 size;
	u8 data[];
};

/**
 * write_record() - Write a record into a zone
 *
 * The rest of the zone is cleared, so that the command stops printing at the
 * end of @text
 *
 * @zone: Start of the zone
 * @sig: Signature to write, 0 for none
 * @text: Text of the record
 */
static void write_record(void *zone, u32 sig, const char *text)
{
	struct test_zone_hdr *hdr = zone;

	memset(zone, '\0', ZONE_SIZE);
	hdr->sig = sig;
	hdr->start = 0;
	hdr->size = strlen(text);
	memcpy(hdr->data, text, hdr->size);
}

/**
 * setup_pstore() - Point the command at memory holding two records
 *
 * The zones are laid out as Linux lays them out: one dump zone, then the
 * console zone, with no FTrace or user space zone. The memory comes from the
 * heap, since a fixed address is memory on sandbox alone
 *
 * @uts: Test state
 * @bufp: Returns the memory, which the caller frees
 * Return: 0 if OK, other value on error
 */
static int setup_pstore(struct unit_test_state *uts, void **bufp)
{
	void *buf;

	buf = malloc_cache_aligned(MEM_SIZE);
	ut_assertnonnull(buf);

	write_record(buf, PSTORE_SIG, DUMP_HDR "dump text\n");
	write_record(buf + ZONE_SIZE, PSTORE_SIG, "console text\n");

	ut_assertok(run_commandf("pstore set 0x%lx 0x%x 0x%x 0x%x 0 0",
				 (ulong)map_to_sysmem(buf), MEM_SIZE, ZONE_SIZE,
				 ZONE_SIZE));
	ut_assert_console_end();
	*bufp = buf;

	return 0;
}

/**
 * restore_pstore() - Put the configuration back to the build-time defaults
 *
 * The command keeps its configuration in statics, so a test which changes it
 * must put it back for the tests which follow, including the Python tests of
 * test_pstore.py
 *
 * @uts: Test state
 * Return: 0 if OK, other value on error
 */
static int restore_pstore(struct unit_test_state *uts)
{
	ut_assertok(run_commandf("pstore set 0x%lx 0x%lx 0x%x 0x%x 0x%x 0x%x %d",
				 (ulong)CONFIG_CMD_PSTORE_MEM_ADDR,
				 (ulong)CONFIG_CMD_PSTORE_MEM_SIZE,
				 CONFIG_CMD_PSTORE_RECORD_SIZE,
				 CONFIG_CMD_PSTORE_CONSOLE_SIZE,
				 CONFIG_CMD_PSTORE_FTRACE_SIZE,
				 CONFIG_CMD_PSTORE_PMSG_SIZE,
				 CONFIG_CMD_PSTORE_ECC_SIZE));
	console_record_reset();

	return 0;
}

/**
 * check_usage() - Check the usage message the command shows
 *
 * The message describes every sub-command, so only its first lines and the
 * start of the 'display' description are checked
 *
 * @uts: Test state
 * Return: 0 if OK, other value on error
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("pstore - Manage Linux Persistent Storage");
	ut_assert_nextline_empty();
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_skip_to_line("pstore display [record-type] [nb]");
	}
	console_record_reset();

	return 0;
}

/* Test that the command displays the records it finds */
static int cmd_test_pstore_base(struct unit_test_state *uts)
{
	void *buf;

	ut_assertok(setup_pstore(uts, &buf));

	ut_assertok(run_command("pstore display", 0));
	ut_assert_nextline("**** Dump");
	ut_assert_nextline("dump text");
	ut_assert_nextline("**** Console");
	ut_assert_nextline("console text");
	ut_assert_console_end();

	free(buf);
	ut_assertok(restore_pstore(uts));

	return 0;
}
CMD_TEST(cmd_test_pstore_base, UTF_CONSOLE);

/* Test that a record type selects which records are displayed */
static int cmd_test_pstore_type(struct unit_test_state *uts)
{
	void *buf;

	ut_assertok(setup_pstore(uts, &buf));

	ut_assertok(run_command("pstore display dump", 0));
	ut_assert_nextline("**** Dump");
	ut_assert_nextline("dump text");
	ut_assert_console_end();

	ut_assertok(run_command("pstore display console", 0));
	ut_assert_nextline("**** Console");
	ut_assert_nextline("console text");
	ut_assert_console_end();

	/* the FTrace and user space zones have no room for a record */
	ut_assertok(run_command("pstore display ftrace", 0));
	ut_assert_console_end();

	ut_assertok(run_command("pstore display user", 0));
	ut_assert_console_end();

	/* a dump zone can be chosen by number, and there is only zone 0 */
	ut_assertok(run_command("pstore display dump 0", 0));
	ut_assert_nextline("**** Dump");
	ut_assert_nextline("dump text");
	ut_assert_console_end();

	ut_assertok(run_command("pstore display dump 1", 0));
	ut_assert_console_end();

	free(buf);
	ut_assertok(restore_pstore(uts));

	return 0;
}
CMD_TEST(cmd_test_pstore_type, UTF_CONSOLE);

/* Test what the command makes of the header line of a dump record */
static int cmd_test_pstore_dump_header(struct unit_test_state *uts)
{
	void *buf;

	ut_assertok(setup_pstore(uts, &buf));

	/* a record without the kernel header is not a dump record */
	write_record(buf, PSTORE_SIG, "no header\n");
	ut_assertok(run_command("pstore display dump", 0));
	ut_assert_console_end();

	/* a compressed record is reported rather than displayed */
	write_record(buf, PSTORE_SIG, "====1.0-C\ncompressed data\n");
	ut_assertok(run_command("pstore display dump", 0));
	ut_assert_nextline("Compressed buffer, display not available");
	ut_assert_console_end();

	free(buf);
	ut_assertok(restore_pstore(uts));

	return 0;
}
CMD_TEST(cmd_test_pstore_dump_header, UTF_CONSOLE);

/* Test the check which pstore set makes on the sizes */
static int cmd_test_pstore_set(struct unit_test_state *uts)
{
	void *buf;

	ut_assertok(setup_pstore(uts, &buf));

	/*
	 * The zones must fit in the memory. The command says that they do not
	 * and forgets the configuration
	 */
	ut_assertok(run_commandf("pstore set 0x%lx 0x%x 0x%x 0x%x 0 0",
				 (ulong)map_to_sysmem(buf), ZONE_SIZE, ZONE_SIZE,
				 ZONE_SIZE));
	ut_assert_nextline("pstore <len> should be larger than the sum of all records sizes");
	ut_assert_console_end();

	/* with no memory to read there is nothing to display */
	ut_asserteq(1, run_command("pstore display", 0));
	ut_assert_nextline("Please set PStore configuration");
	ut_assertok(check_usage(uts));

	free(buf);
	ut_assertok(restore_pstore(uts));

	return 0;
}
CMD_TEST(cmd_test_pstore_set, UTF_CONSOLE);

/* Test the command lines which show the usage message */
static int cmd_test_pstore_usage(struct unit_test_state *uts)
{
	/* no sub-command at all */
	ut_asserteq(1, run_command("pstore", 0));
	ut_assertok(check_usage(uts));

	/* a sub-command which does not exist */
	ut_asserteq(1, run_command("pstore bogus", 0));
	ut_assertok(check_usage(uts));

	/* 'set' needs both an address and a length */
	ut_asserteq(1, run_command("pstore set 1000", 0));
	ut_assertok(check_usage(uts));

	/* 'display' takes one of four record types */
	ut_asserteq(1, run_command("pstore display bogus", 0));
	ut_assertok(check_usage(uts));

	/* 'save' needs an interface, a device and a directory */
	ut_asserteq(1, run_command("pstore save hostfs -", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_pstore_usage, UTF_CONSOLE);
