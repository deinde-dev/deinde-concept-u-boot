// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the help command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <test/cmd.h>
#include <test/ut.h>

/* message help shows for a name which does not select a command */
#define UNKNOWN_MSG	"Unknown command '%s' - try 'help' without " \
			"arguments for list of all known commands"

/* first line of the help for the version command */
#define VERSION_HELP	"version - print monitor, compiler and linker version"

/* line the listing gives the version command */
#define VERSION_LINE	"version   - print monitor, compiler and linker version"

/**
 * check_version_help() - Check the long help shown for the version command
 *
 * Every board with a command line has this command, since cmd/version.c is
 * built whenever CONFIG_CMDLINE is enabled. Its help text is empty, so the
 * last line is the command name alone and carries a trailing space
 *
 * @uts: Test state
 * Return: 0 if OK, -ve on error
 */
static int check_version_help(struct unit_test_state *uts)
{
	ut_assert_nextline(VERSION_HELP);
	ut_assert_nextline_empty();
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_nextline("version ");
	}

	return 0;
}

/* Test 'help' with no arguments, which lists every command */
static int cmd_test_help_base(struct unit_test_state *uts)
{
	ut_assertok(run_command("help", 0));

	/*
	 * The listing is sorted by name and holds whichever commands the board
	 * has, so skip to the three which are always there: the '?' alias and
	 * help itself come from cmd/help.c and version from cmd/version.c.
	 * Skipping to each in turn checks the order as well as the text
	 */
	ut_assert_skip_to_line("?         - alias for 'help'");
	ut_assert_skip_to_line("help      - print command description/usage");
	ut_assert_skip_to_line(VERSION_LINE);

	return 0;
}
CMD_TEST(cmd_test_help_base, UTF_CONSOLE);

/* Test 'help' naming a command, which shows that command's long help */
static int cmd_test_help_cmd(struct unit_test_state *uts)
{
	ut_asserteq(CMD_RET_FAILURE, run_command("help help", 0));

	ut_assert_nextline("help - print command description/usage");
	ut_assert_nextline_empty();
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");

		/* the help text starts with a newline, so this line ends here */
		ut_assert_nextline("help ");
		ut_assert_nextline("\t- print brief description of all commands");
		ut_assert_nextline("help command ...");
		ut_assert_nextline("\t- print detailed usage of 'command'");
	}
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_help_cmd, UTF_CONSOLE);

/* Test 'help' naming a command whose entry has no long help */
static int cmd_test_help_nolong(struct unit_test_state *uts)
{
	/* the 'true' command, which has no help text, lives in cmd/test.c */
	if (!IS_ENABLED(CONFIG_HUSH_PARSER))
		return -EAGAIN;

	ut_asserteq(CMD_RET_FAILURE, run_command("help true", 0));

	ut_assert_nextline("true - do nothing, successfully");
	ut_assert_nextline_empty();
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_nextline("true - No additional help available.");
	}
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_help_nolong, UTF_CONSOLE);

/* Test that 'help' accepts an abbreviation, but not an ambiguous one */
static int cmd_test_help_abbrev(struct unit_test_state *uts)
{
	ut_asserteq(CMD_RET_FAILURE, run_command("help vers", 0));
	ut_assertok(check_version_help(uts));
	ut_assert_console_end();

	/* 'help' and 'history' both start with 'h', so this names neither */
	if (!IS_ENABLED(CONFIG_CMD_HISTORY))
		return 0;

	ut_asserteq(CMD_RET_FAILURE, run_command("help h", 0));

	ut_assert_nextline(UNKNOWN_MSG, "h");
	ut_assert_nextline_empty();
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_help_abbrev, UTF_CONSOLE);

/* Test 'help' naming something which is not a command */
static int cmd_test_help_unknown(struct unit_test_state *uts)
{
	ut_asserteq(CMD_RET_FAILURE, run_command("help nosuchcmd", 0));

	ut_assert_nextline(UNKNOWN_MSG, "nosuchcmd");
	ut_assert_nextline_empty();
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_help_unknown, UTF_CONSOLE);

/*
 * Test 'help' with several names, each of which is looked up in turn, the
 * failures setting the return value without stopping the rest
 */
static int cmd_test_help_many(struct unit_test_state *uts)
{
	ut_asserteq(CMD_RET_FAILURE, run_command("help nosuchcmd version", 0));

	ut_assert_nextline(UNKNOWN_MSG, "nosuchcmd");
	ut_assert_nextline_empty();
	ut_assertok(check_version_help(uts));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_help_many, UTF_CONSOLE);

/* Test that '?' is an alias for 'help' */
static int cmd_test_help_alias(struct unit_test_state *uts)
{
	ut_asserteq(CMD_RET_FAILURE, run_command("help ?", 0));

	ut_assert_nextline("? - alias for 'help'");
	ut_assert_nextline_empty();
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");

		/* the entry's help text is empty, so only the name is shown */
		ut_assert_nextline("? ");
	}
	ut_assert_console_end();

	/* the alias runs the same code, so it lists every command too */
	ut_assertok(run_command("?", 0));
	ut_assert_skip_to_line("?         - alias for 'help'");

	return 0;
}
CMD_TEST(cmd_test_help_alias, UTF_CONSOLE);

/* Test that Ctrl-C stops the listing */
static int cmd_test_help_ctrlc(struct unit_test_state *uts)
{
	int prev;

	/*
	 * The check comes before the first line is shown, so queue a Ctrl-C
	 * and expect no output at all. Sandbox turns off Ctrl-C checking, so
	 * enable it for the duration of the command
	 */
	console_in_puts("\x03");
	prev = disable_ctrlc(0);
	ut_asserteq(CMD_RET_FAILURE, run_command("help", 0));
	disable_ctrlc(prev);

	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_help_ctrlc, UTF_CONSOLE);

/* Test 'help' with an option, which it does not have */
static int cmd_test_help_opt(struct unit_test_state *uts)
{
	ut_asserteq(CMD_RET_FAILURE, run_command("help -x", 0));

	/*
	 * the entry takes no options, so cmd_invoke() refuses this one before
	 * help runs and the usage message is shown; cmd_test_help_cmd() checks
	 * the rest of that message
	 */
	ut_assert_nextline("help - print command description/usage");
	ut_assert_nextline_empty();
	if (IS_ENABLED(CONFIG_SYS_LONGHELP))
		ut_assert_skip_to_line("\t- print detailed usage of 'command'");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_help_opt, UTF_CONSOLE);
