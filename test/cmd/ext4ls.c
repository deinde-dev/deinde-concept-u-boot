// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the ext4ls command
 *
 * These are manual tests, since they need a filesystem image. The Python side
 * makes the image and passes its path in; see test/py/tests/test_ext4ls.py
 *
 * Sandbox builds the CONFIG_FS_EXT4L driver, which lists the entries without
 * the counts that the original ext4 driver ends a listing with.
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

/* the last line of the usage message, which several tests wait for */
#define USAGE_LAST \
	"    - list files from 'dev' on 'interface' in a 'directory'"

/**
 * check_entries() - Check a listing holds exactly the expected entries
 *
 * An ext4 directory holds its entries in an order which depends on how mkfs
 * wrote it, so the lines are matched without regard to the order they arrive
 * in.
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

/* Test that ext4ls lists the root directory */
static int cmd_test_ext4ls_base_norun(struct unit_test_state *uts)
{
	static const char *const root[] = {
		"            ./",
		"            ../",
		"            docs/",
		"            empty/",
		"            lost+found/",
		"       14   hello.txt",
	};

	ut_assertok(bind_image(uts));

	ut_assertok(run_command("ext4ls host 0", 0));
	ut_assertok(check_entries(uts, root, ARRAY_SIZE(root)));

	/* the root is the default, so naming it makes no difference */
	ut_assertok(run_command("ext4ls host 0 /", 0));
	ut_assertok(check_entries(uts, root, ARRAY_SIZE(root)));

	return 0;
}
CMD_TEST_ARGS(cmd_test_ext4ls_base_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that a sub-directory can be listed, with its . and .. entries */
static int cmd_test_ext4ls_subdir_norun(struct unit_test_state *uts)
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

	ut_assertok(run_command("ext4ls host 0 docs", 0));
	ut_assertok(check_entries(uts, docs, ARRAY_SIZE(docs)));

	/* a leading slash makes no difference, since the path is absolute */
	ut_assertok(run_command("ext4ls host 0 /docs", 0));
	ut_assertok(check_entries(uts, docs, ARRAY_SIZE(docs)));

	/* a directory with nothing in it still holds . and .. */
	ut_assertok(run_command("ext4ls host 0 /empty", 0));
	ut_assertok(check_entries(uts, empty, ARRAY_SIZE(empty)));

	return 0;
}
CMD_TEST_ARGS(cmd_test_ext4ls_subdir_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that a path which is not a directory fails without printing anything */
static int cmd_test_ext4ls_missing_norun(struct unit_test_state *uts)
{
	ut_assertok(bind_image(uts));

	/* the return value is all there is to tell this from an empty listing */
	ut_asserteq(1, run_command("ext4ls host 0 /nodir", 0));
	ut_assert_console_end();

	/* a file is not a directory, so it cannot be listed either */
	ut_asserteq(1, run_command("ext4ls host 0 hello.txt", 0));
	ut_assert_console_end();

	return 0;
}
CMD_TEST_ARGS(cmd_test_ext4ls_missing_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that ext4ls complains about a device it cannot use */
static int cmd_test_ext4ls_baddev_norun(struct unit_test_state *uts)
{
	ut_assertok(bind_image(uts));

	ut_asserteq(1, run_command("ext4ls host 1", 0));
	ut_assert_nextline("** Bad device specification host 1 **");
	ut_assert_nextline("Couldn't find partition host 1");
	ut_assert_console_end();

	/* the device number is needed, even though the help text allows none */
	ut_asserteq(1, run_command("ext4ls host", 0));
	ut_assert_nextline("** No device specified **");
	ut_assert_nextline("Couldn't find partition host <NULL>");
	ut_assert_console_end();

	return 0;
}
CMD_TEST_ARGS(cmd_test_ext4ls_baddev_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that ext4ls shows its usage message when it cannot go on */
static int cmd_test_ext4ls_usage_norun(struct unit_test_state *uts)
{
	ut_assertok(bind_image(uts));

	/* the interface is the one argument which has no default */
	ut_asserteq(1, run_command("ext4ls", 0));
	ut_assert_nextline("ext4ls - list files in a directory (default /)");
	ut_assert_nextline("%s", "");
	ut_assert_nextline("Usage:");
	ut_assert_nextline("ext4ls <interface> <dev[:part]> [directory]");
	ut_assert_nextline(USAGE_LAST);
	ut_assert_console_end();

	/* and there is nothing to give beyond the directory */
	ut_asserteq(1, run_command("ext4ls host 0 docs extra", 0));
	ut_assert_skip_to_line(USAGE_LAST);
	ut_assert_console_end();

	return 0;
}
CMD_TEST_ARGS(cmd_test_ext4ls_usage_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that ext4ls refuses an option, since it has none */
static int cmd_test_ext4ls_badopt_norun(struct unit_test_state *uts)
{
	ut_assertok(bind_image(uts));

	ut_asserteq(1, run_command("ext4ls -l host 0", 0));
	ut_assert_skip_to_line(USAGE_LAST);
	ut_assert_console_end();

	/* a directory which looks like an option is still a directory */
	ut_asserteq(1, run_command("ext4ls host 0 -l", 0));
	ut_assert_console_end();

	return 0;
}
CMD_TEST_ARGS(cmd_test_ext4ls_badopt_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });
