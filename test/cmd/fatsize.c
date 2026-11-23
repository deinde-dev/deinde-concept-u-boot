// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the fatsize command
 *
 * These are manual tests, since they need a filesystem image. The Python side
 * makes the image and passes its path in; see test/py/tests/test_fatsize.py
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <env.h>
#include <test/cmd.h>
#include <test/test.h>
#include <test/ut.h>

#define ARG_IMAGE	0	/* fs_image: path to the filesystem image */

/* value put in the filesize variable, to show when it is left alone */
#define SENTINEL	"sentinel"

/* enough for the hex size which the command writes, or a sentinel */
#define MAX_SIZE	32

/**
 * bind_image() - Attach the image which the Python side made
 *
 * The filesize variable is set to a sentinel, so that a test can tell a size
 * the command worked out from one an earlier command left behind. Its own
 * value is kept in @saved for put_filesize() to restore, since the tests which
 * follow in a single run read it
 *
 * @uts: Test state
 * @saved: Buffer to hold the value of the filesize variable
 * @size: Size of that buffer
 * Return: 0 if OK, other value on error
 */
static int bind_image(struct unit_test_state *uts, char *saved, int size)
{
	const char *fs_image = ut_str(ARG_IMAGE);
	const char *orig = env_get("filesize");

	*saved = '\0';
	if (orig)
		strlcpy(saved, orig, size);

	ut_assertnonnull(fs_image);
	ut_assertok(run_commandf("host bind 0 %s", fs_image));
	ut_assertok(env_set("filesize", SENTINEL));
	console_record_reset();

	return 0;
}

/**
 * put_filesize() - Put the filesize variable back as it was
 *
 * @uts: Test state
 * @saved: Value the variable held, empty if it was not set
 * Return: 0 if OK, other value on error
 */
static int put_filesize(struct unit_test_state *uts, const char *saved)
{
	ut_assertok(env_set("filesize", *saved ? saved : NULL));

	return 0;
}

/**
 * check_size() - Check the size the command worked out
 *
 * @uts: Test state
 * @expect: Expected value of the filesize variable, in hex
 * Return: 0 if OK, 1 on failure
 */
static int check_size(struct unit_test_state *uts, const char *expect)
{
	/* the command says nothing at all when it finds the file */
	ut_assert_console_end();
	ut_asserteq_str(expect, env_get("filesize"));

	return 0;
}

/* Test that fatsize puts a file's size in the filesize variable, in hex */
static int cmd_test_fatsize_base_norun(struct unit_test_state *uts)
{
	char saved[MAX_SIZE];

	ut_assertok(bind_image(uts, saved, sizeof(saved)));

	/* hello.txt holds 14 bytes, so the variable reads 'e' */
	ut_assertok(run_command("fatsize host 0 hello.txt", 0));
	ut_assertok(check_size(uts, "e"));

	/* a filename is a path within the filesystem */
	ut_assertok(run_command("fatsize host 0 docs/note.txt", 0));
	ut_assertok(check_size(uts, "5"));

	/* the image has no partition table, so the whole device is used */
	ut_assertok(run_command("fatsize host 0:0 hello.txt", 0));
	ut_assertok(check_size(uts, "e"));

	ut_assertok(put_filesize(uts, saved));

	return 0;
}
CMD_TEST_ARGS(cmd_test_fatsize_base_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that a directory is reported as zero bytes rather than as an error */
static int cmd_test_fatsize_dir_norun(struct unit_test_state *uts)
{
	char saved[MAX_SIZE];

	ut_assertok(bind_image(uts, saved, sizeof(saved)));

	ut_assertok(run_command("fatsize host 0 docs", 0));
	ut_assertok(check_size(uts, "0"));

	ut_assertok(put_filesize(uts, saved));

	return 0;
}
CMD_TEST_ARGS(cmd_test_fatsize_dir_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that a missing file fails and leaves the filesize variable alone */
static int cmd_test_fatsize_missing_norun(struct unit_test_state *uts)
{
	char saved[MAX_SIZE];

	ut_assertok(bind_image(uts, saved, sizeof(saved)));

	ut_asserteq(1, run_command("fatsize host 0 missing.txt", 0));
	ut_assert_console_end();

	/* a stale value from an earlier command is still there */
	ut_asserteq_str(SENTINEL, env_get("filesize"));

	ut_assertok(put_filesize(uts, saved));

	return 0;
}
CMD_TEST_ARGS(cmd_test_fatsize_missing_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that fatsize complains about a device which is not there */
static int cmd_test_fatsize_baddev_norun(struct unit_test_state *uts)
{
	char saved[MAX_SIZE];

	ut_assertok(bind_image(uts, saved, sizeof(saved)));

	ut_asserteq(1, run_command("fatsize host 1 hello.txt", 0));
	ut_assert_nextline("** Bad device specification host 1 **");
	ut_assert_nextline("Couldn't find partition host 1");
	ut_assert_console_end();

	ut_assertok(put_filesize(uts, saved));

	return 0;
}
CMD_TEST_ARGS(cmd_test_fatsize_baddev_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* the last line of the usage message, which several tests wait for */
#define USAGE_LAST	"      and determine its size."

/* Test that fatsize shows its usage message when it cannot go on */
static int cmd_test_fatsize_usage_norun(struct unit_test_state *uts)
{
	char saved[MAX_SIZE];

	ut_assertok(bind_image(uts, saved, sizeof(saved)));

	/* none of the three arguments has a default */
	ut_asserteq(1, run_command("fatsize host 0", 0));
	ut_assert_nextline("fatsize - determine a file's size");
	ut_assert_nextline("%s", "");
	ut_assert_nextline("Usage:");
	ut_assert_nextline("fatsize <interface> <dev[:part]> <filename>");
	ut_assert_nextline(
		"    - Find file 'filename' from 'dev' on 'interface'");
	ut_assert_nextline(USAGE_LAST);
	ut_assert_console_end();

	/* and there is nothing to give beyond the filename */
	ut_asserteq(1, run_command("fatsize host 0 hello.txt extra", 0));
	ut_assert_skip_to_line(USAGE_LAST);
	ut_assert_console_end();

	ut_assertok(put_filesize(uts, saved));

	return 0;
}
CMD_TEST_ARGS(cmd_test_fatsize_usage_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });
