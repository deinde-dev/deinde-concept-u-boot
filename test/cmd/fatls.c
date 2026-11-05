// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the fatls command
 *
 * These are manual tests, since they need a filesystem image. The Python side
 * makes the image and passes its path in; see test/py/tests/test_fatls.py
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <test/cmd.h>
#include <test/test.h>
#include <test/ut.h>

#define ARG_IMAGE	0	/* fs_image: path to the filesystem image */

/* enough for a listing line, which holds a name of up to 255 characters */
#define MAX_LINE	280

/**
 * check_entries() - Check a listing holds exactly the expected entries
 *
 * A directory holds its entries in whatever order it was written in, which is
 * neither alphabetical nor something mkfs promises, so the lines are matched
 * without regard to the order they arrive in.
 *
 * @uts: Test state
 * @exp: Expected lines, each an entry of the listing
 * @count: Number of expected lines
 * Return: 0 if OK, 1 on failure
 */
static int check_entries(struct unit_test_state *uts, const char *const *exp,
			 int count)
{
	bool seen[8];
	char line[MAX_LINE];
	int i;

	ut_assert(count <= (int)ARRAY_SIZE(seen));
	memset(seen, '\0', sizeof(seen));

	for (i = 0; i < count; i++) {
		int which;

		ut_assert(console_record_readline(line, sizeof(line)) >= 0);
		for (which = 0; which < count; which++) {
			if (!seen[which] && !strcmp(exp[which], line))
				break;
		}
		ut_assertf(which < count, "unexpected line '%s'", line);
		seen[which] = true;
	}

	return 0;
}

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

/* Test that fatls lists the root directory and counts what it found */
static int cmd_test_fatls_base_norun(struct unit_test_state *uts)
{
	static const char *const root[] = {
		"            docs/",
		"            empty/",
		"       14   hello.txt",
	};

	ut_assertok(bind_image(uts));

	ut_assertok(run_command("fatls host 0", 0));
	ut_assertok(check_entries(uts, root, ARRAY_SIZE(root)));
	ut_assertok(check_summary(uts, 1, 2));

	/* the root is the default, so naming it makes no difference */
	ut_assertok(run_command("fatls host 0 /", 0));
	ut_assertok(check_entries(uts, root, ARRAY_SIZE(root)));
	ut_assertok(check_summary(uts, 1, 2));

	return 0;
}
CMD_TEST_ARGS(cmd_test_fatls_base_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that a sub-directory can be listed, with its . and .. entries */
static int cmd_test_fatls_subdir_norun(struct unit_test_state *uts)
{
	static const char *const docs[] = {
		"            ./",
		"            ../",
		"        5   note.txt",
	};
	static const char *const empty[] = {
		"            ./",
		"            ../",
	};

	ut_assertok(bind_image(uts));

	ut_assertok(run_command("fatls host 0 docs", 0));
	ut_assertok(check_entries(uts, docs, ARRAY_SIZE(docs)));
	ut_assertok(check_summary(uts, 1, 2));

	/* a leading slash makes no difference, since the path is absolute */
	ut_assertok(run_command("fatls host 0 /docs", 0));
	ut_assertok(check_entries(uts, docs, ARRAY_SIZE(docs)));
	ut_assertok(check_summary(uts, 1, 2));

	/* a directory with nothing in it still holds . and .. */
	ut_assertok(run_command("fatls host 0 empty", 0));
	ut_assertok(check_entries(uts, empty, ARRAY_SIZE(empty)));
	ut_assertok(check_summary(uts, 0, 2));

	return 0;
}
CMD_TEST_ARGS(cmd_test_fatls_subdir_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that a path which is not a directory fails without printing anything */
static int cmd_test_fatls_missing_norun(struct unit_test_state *uts)
{
	ut_assertok(bind_image(uts));

	/* the return value is all there is to tell this from an empty listing */
	ut_asserteq(1, run_command("fatls host 0 nodir", 0));
	ut_assert_console_end();

	/* a file is not a directory, so it cannot be listed either */
	ut_asserteq(1, run_command("fatls host 0 hello.txt", 0));
	ut_assert_console_end();

	return 0;
}
CMD_TEST_ARGS(cmd_test_fatls_missing_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that fatls complains about a device which is not there */
static int cmd_test_fatls_baddev_norun(struct unit_test_state *uts)
{
	ut_assertok(bind_image(uts));

	ut_asserteq(1, run_command("fatls host 1", 0));
	ut_assert_nextline("** Bad device specification host 1 **");
	ut_assert_nextline("Couldn't find partition host 1");
	ut_assert_console_end();

	return 0;
}
CMD_TEST_ARGS(cmd_test_fatls_baddev_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that fatls shows its usage message when it cannot go on */
static int cmd_test_fatls_usage_norun(struct unit_test_state *uts)
{
	ut_assertok(bind_image(uts));

	/* the interface is the one argument which has no default */
	ut_asserteq(1, run_command("fatls", 0));
	ut_assert_nextline("fatls - list files in a directory (default /)");
	ut_assert_nextline("%s", "");
	ut_assert_nextline("Usage:");
	ut_assert_nextline("fatls <interface> [<dev[:part]>] [directory]");
	ut_assert_nextline("    - list files from 'dev' on 'interface' in a 'directory'");
	ut_assert_console_end();

	/* and there is nothing to give beyond the directory */
	ut_asserteq(1, run_command("fatls host 0 docs extra", 0));
	ut_assert_skip_to_line("    - list files from 'dev' on 'interface' in a 'directory'");
	ut_assert_console_end();

	return 0;
}
CMD_TEST_ARGS(cmd_test_fatls_usage_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });
