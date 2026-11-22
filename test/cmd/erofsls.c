// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the erofsls command
 *
 * These are manual tests, since they need a filesystem image. The Python side
 * makes the image and passes its path in; see test/py/tests/test_erofsls.py
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <test/cmd.h>
#include <test/test.h>
#include <test/ut.h>

#define ARG_IMAGE	0	/* fs_image: path to the filesystem image */

/* the last line of the usage message, which several tests wait for */
#define USAGE_LAST \
	"    - list files from 'dev' on 'interface' in 'directory'"

/**
 * check_summary() - Check the counts which end a listing
 *
 * @uts: Test state
 * @nfiles: Expected number of files
 * @ndirs: Expected number of directories
 * Return: 0 if OK, 1 on failure
 */
static int check_summary(struct unit_test_state *uts, int nfiles, int ndirs)
{
	ut_assert_nextline_empty();
	ut_assert_nextline("%d file(s), %d dir(s)", nfiles, ndirs);
	ut_assert_nextline_empty();
	ut_assert_console_end();

	return 0;
}

/**
 * bind_image() - Attach the image which the Python side made
 *
 * @uts: Test state
 * Return: 0 if OK, other value on error
 */
static int bind_image(struct unit_test_state *uts)
{
	const char *fs_image = ut_str(ARG_IMAGE);

	ut_assertnonnull(fs_image);
	ut_assertok(run_commandf("host bind 0 %s", fs_image));
	console_record_reset();

	return 0;
}

/**
 * check_root() - Check a listing of the root directory
 *
 * EROFS holds the entries of a directory sorted by name and keeps the . and
 * .. entries, so the order of the lines is fixed. A symbolic link is counted
 * as a file, and a compressed file shows the size it has once expanded.
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_root(struct unit_test_state *uts)
{
	ut_assert_nextline("            ./");
	ut_assert_nextline("            ../");
	ut_assert_nextline("     8192   comp.dat");
	ut_assert_nextline("            docs/");
	ut_assert_nextline("            empty/");
	ut_assert_nextline("       14   hello.txt");
	ut_assert_nextline("    <SYM>   sym");
	ut_assert_nextline("    <SYM>   symfile");
	ut_assertok(check_summary(uts, 4, 4));

	return 0;
}

/**
 * check_docs() - Check a listing of the docs directory
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_docs(struct unit_test_state *uts)
{
	ut_assert_nextline("            ./");
	ut_assert_nextline("            ../");
	ut_assert_nextline("        5   note.txt");
	ut_assertok(check_summary(uts, 1, 2));

	return 0;
}

/* Test that erofsls lists the root directory and counts what it found */
static int cmd_test_erofsls_base_norun(struct unit_test_state *uts)
{
	ut_assertok(bind_image(uts));

	ut_assertok(run_command("erofsls host 0", 0));
	ut_assertok(check_root(uts));

	/* the root is the default, so naming it makes no difference */
	ut_assertok(run_command("erofsls host 0 /", 0));
	ut_assertok(check_root(uts));

	return 0;
}
CMD_TEST_ARGS(cmd_test_erofsls_base_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that a sub-directory can be listed, through its name or a symlink */
static int cmd_test_erofsls_subdir_norun(struct unit_test_state *uts)
{
	ut_assertok(bind_image(uts));

	ut_assertok(run_command("erofsls host 0 docs", 0));
	ut_assertok(check_docs(uts));

	/* a leading slash makes no difference, since the path is absolute */
	ut_assertok(run_command("erofsls host 0 /docs", 0));
	ut_assertok(check_docs(uts));

	/* sym points at docs, so it lists the same directory */
	ut_assertok(run_command("erofsls host 0 sym", 0));
	ut_assertok(check_docs(uts));

	return 0;
}
CMD_TEST_ARGS(cmd_test_erofsls_subdir_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that an empty directory still has its . and .. entries */
static int cmd_test_erofsls_empty_norun(struct unit_test_state *uts)
{
	ut_assertok(bind_image(uts));

	ut_assertok(run_command("erofsls host 0 empty", 0));
	ut_assert_nextline("            ./");
	ut_assert_nextline("            ../");
	ut_assertok(check_summary(uts, 0, 2));

	return 0;
}
CMD_TEST_ARGS(cmd_test_erofsls_empty_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that a path which is not a directory is reported by the return value */
static int cmd_test_erofsls_missing_norun(struct unit_test_state *uts)
{
	ut_assertok(bind_image(uts));

	/* the EROFS driver prints nothing at all for any of these */
	ut_asserteq(1, run_command("erofsls host 0 nodir", 0));
	ut_assert_console_end();

	/* a file is not a directory, so it cannot be listed */
	ut_asserteq(1, run_command("erofsls host 0 hello.txt", 0));
	ut_assert_console_end();

	/* nor is a symbolic link which points at a file */
	ut_asserteq(1, run_command("erofsls host 0 symfile", 0));
	ut_assert_console_end();

	/* a path leading through a file stops at it */
	ut_asserteq(1, run_command("erofsls host 0 hello.txt/nodir", 0));
	ut_assert_console_end();

	/* and so does one leading through a symbolic link */
	ut_asserteq(1, run_command("erofsls host 0 sym/note.txt", 0));
	ut_assert_console_end();

	return 0;
}
CMD_TEST_ARGS(cmd_test_erofsls_missing_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that erofsls complains about a device which is not there */
static int cmd_test_erofsls_baddev_norun(struct unit_test_state *uts)
{
	ut_assertok(bind_image(uts));

	ut_asserteq(1, run_command("erofsls host 1", 0));
	ut_assert_nextline("** Bad device specification host 1 **");
	ut_assert_nextline("Couldn't find partition host 1");
	ut_assert_console_end();

	/* the device number is needed, even though the help text allows none */
	ut_asserteq(1, run_command("erofsls host", 0));
	ut_assert_nextline("** No device specified **");
	ut_assert_nextline("Couldn't find partition host <NULL>");
	ut_assert_console_end();

	return 0;
}
CMD_TEST_ARGS(cmd_test_erofsls_baddev_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that erofsls shows its usage message when it cannot go on */
static int cmd_test_erofsls_usage_norun(struct unit_test_state *uts)
{
	ut_assertok(bind_image(uts));

	/* the interface is the one argument which has no default */
	ut_asserteq(1, run_command("erofsls", 0));
	ut_assert_nextline(
		"erofsls - List files in directory. Default: root (/).");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("erofsls <interface> [<dev[:part]>] [directory]");
	ut_assert_nextline(USAGE_LAST);

	/* the help text ends with a newline, so a blank line follows it */
	ut_assert_nextline_empty();
	ut_assert_console_end();

	/* and there is nothing to give beyond the directory */
	ut_asserteq(1, run_command("erofsls host 0 docs extra", 0));
	ut_assert_skip_to_line(USAGE_LAST);
	ut_assert_nextline_empty();
	ut_assert_console_end();

	return 0;
}
CMD_TEST_ARGS(cmd_test_erofsls_usage_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });
