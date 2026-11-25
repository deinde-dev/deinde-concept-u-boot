// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the env and printenv commands
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <env.h>
#include <errno.h>
#include <test/cmd.h>
#include <test/ut.h>

/* the first line of the env command's usage message */
#define ENV_USAGE	"env - environment handling commands"

/* the last line of it, so a test can skip over the sub-command list */
#define ENV_USAGE_END	"env set [-f] name [arg ...]"

/* Check the usage message, which is the same whatever is at fault */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline(ENV_USAGE);
	ut_assert_skip_to_line(ENV_USAGE_END);
	ut_assert_nextline_empty();
	ut_assert_console_end();

	return 0;
}

/* Test 'env print' and 'printenv' showing named variables */
static int cmd_test_env_base(struct unit_test_state *uts)
{
	ut_assertok(run_command("setenv ut_env wilma", 0));
	ut_assertok(run_command("setenv ut_env2 betty", 0));

	ut_assertok(run_command("env print ut_env", 0));
	ut_assert_nextline("ut_env=wilma");
	ut_assert_console_end();

	/* printenv is another name for the same command */
	ut_assertok(run_command("printenv ut_env", 0));
	ut_assert_nextline("ut_env=wilma");
	ut_assert_console_end();

	/* each name given is shown, in the order asked for */
	ut_assertok(run_command("printenv ut_env2 ut_env", 0));
	ut_assert_nextline("ut_env2=betty");
	ut_assert_nextline("ut_env=wilma");
	ut_assert_console_end();

	ut_assertok(run_command("setenv ut_env", 0));
	ut_assertok(run_command("setenv ut_env2", 0));

	return 0;
}
CMD_TEST(cmd_test_env_base, UTF_CONSOLE);

/* Test that 'printenv' lists the whole environment, with -a for hidden names */
static int cmd_test_env_print_all(struct unit_test_state *uts)
{
	/* a name starting with a dot sorts before any other */
	ut_assertok(run_command("setenv .ut_env wilma", 0));

	/* without -a such a variable is left out of the listing */
	ut_assertok(run_command("printenv", 0));
	ut_assert_nextline_regex("^[^.]");
	ut_assert_skip_to_linen("Environment size: ");
	ut_assert_console_end();

	/* with -a it is shown, and so comes first */
	ut_assertok(run_command("printenv -a", 0));
	ut_assert_nextline(".ut_env=wilma");
	ut_assert_skip_to_linen("Environment size: ");
	ut_assert_console_end();

	ut_assertok(run_command("setenv .ut_env", 0));

	return 0;
}
CMD_TEST(cmd_test_env_print_all, UTF_CONSOLE);

/* Test that 'printenv' counts the names it cannot find */
static int cmd_test_env_print_missing(struct unit_test_state *uts)
{
	char *const args[] = { "printenv", "ut_none", "ut_none2", NULL };
	int repeatable = 0;

	ut_asserteq(1, run_command("printenv ut_none", 0));
	ut_assert_nextline("## Error: \"ut_none\" not defined");
	ut_assert_console_end();

	/* every name is looked up, so both are reported */
	ut_asserteq(1, run_command("env print ut_none ut_none2", 0));
	ut_assert_nextline("## Error: \"ut_none\" not defined");
	ut_assert_nextline("## Error: \"ut_none2\" not defined");
	ut_assert_console_end();

	/* the value returned is the number of names which are not found */
	ut_asserteq(2, cmd_process(0, 3, args, &repeatable, NULL));
	ut_assert_nextline("## Error: \"ut_none\" not defined");
	ut_assert_nextline("## Error: \"ut_none2\" not defined");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_env_print_missing, UTF_CONSOLE);

/* Test 'env exists' reporting whether a variable is set */
static int cmd_test_env_exists(struct unit_test_state *uts)
{
	ut_assertok(run_command("setenv ut_env wilma", 0));
	ut_assertok(run_command("env exists ut_env", 0));
	ut_assert_console_end();

	/* a variable which is not set gives a failure and no message */
	ut_asserteq(1, run_command("env exists ut_none", 0));
	ut_assert_console_end();

	/* the name is required */
	ut_asserteq(1, run_command("env exists", 0));
	ut_assertok(check_usage(uts));

	ut_assertok(run_command("setenv ut_env", 0));

	return 0;
}
CMD_TEST(cmd_test_env_exists, UTF_CONSOLE);

/* Test 'env default' resetting a variable to its default value */
static int cmd_test_env_default(struct unit_test_state *uts)
{
	ut_assertok(run_command("setenv ut_env wilma", 0));

	/* a variable which has no default value is deleted */
	ut_assertok(run_command("env default ut_env", 0));
	ut_assert_nextline(
		"WARNING: 'ut_env' not in imported env, deleting it!");
	ut_assert_console_end();
	ut_assertnull(env_get("ut_env"));

	/* with -k such a variable is kept instead */
	ut_assertok(run_command("setenv ut_env wilma", 0));
	ut_assertok(run_command("env default -k -f ut_env", 0));
	ut_assert_console_end();
	ut_asserteq_str("wilma", env_get("ut_env"));

	/* an unknown option shows the usage message */
	ut_asserteq(1, run_command("env default -x ut_env", 0));
	ut_assertok(check_usage(uts));

	/* a variable name or -a is required */
	ut_asserteq(1, run_command("env default", 0));
	ut_assertok(check_usage(uts));

	ut_assertok(run_command("setenv ut_env", 0));

	return 0;
}
CMD_TEST(cmd_test_env_default, UTF_CONSOLE);

/* Test 'env delete' removing variables */
static int cmd_test_env_delete(struct unit_test_state *uts)
{
	ut_assertok(run_command("setenv ut_env wilma", 0));
	ut_assertok(run_command("setenv ut_env2 betty", 0));

	/* more than one name can be given */
	ut_assertok(run_command("env delete ut_env ut_env2", 0));
	ut_assert_console_end();
	ut_assertnull(env_get("ut_env"));
	ut_assertnull(env_get("ut_env2"));

	/* -f deletes a variable whatever its access flags say */
	ut_assertok(run_command("setenv ut_env wilma", 0));
	ut_assertok(run_command("env delete -f ut_env", 0));
	ut_assert_console_end();
	ut_assertnull(env_get("ut_env"));

	/* an unknown option shows the usage message */
	ut_asserteq(1, run_command("env delete -x ut_env", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_env_delete, UTF_CONSOLE);

/* Test 'env info' showing and evaluating the state of the environment */
static int cmd_test_env_info(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_CMD_NVEDIT_INFO))
		return -EAGAIN;

	ut_assertok(run_command("env info", 0));
	ut_assert_nextlinen("env_valid = ");
	ut_assert_nextlinen("env_ready = ");
	ut_assert_nextlinen("env_use_default = ");
	ut_assert_console_end();

	/* -d asks whether the default environment is in use */
	ut_assertok(run_command("env info -d", 0));
	ut_assert_nextline("Default environment is used");
	ut_assert_console_end();

	/* -q leaves out the report, so only the return value says anything */
	ut_assertok(run_command("env info -d -q", 0));
	ut_assert_console_end();

	/*
	 * the results are combined with AND and sandbox cannot persist its
	 * environment, so asking for both fails
	 */
	ut_asserteq(1, run_command("env info -d -p -q", 0));
	ut_assert_console_end();

	/* an unknown option shows the usage message */
	ut_asserteq(1, run_command("env info -x", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_env_info, UTF_CONSOLE);

/* Test 'env indirect' copying the value of one variable to another */
static int cmd_test_env_indirect(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_CMD_NVEDIT_INDIRECT))
		return -EAGAIN;

	ut_assertok(run_command("setenv ut_env wilma", 0));

	ut_assertok(run_command("env indirect ut_env2 ut_env", 0));
	ut_assert_console_end();
	ut_asserteq_str("wilma", env_get("ut_env2"));

	/* the third argument is used when <from> is not set */
	ut_assertok(run_command("env indirect ut_env2 ut_none betty", 0));
	ut_assert_console_end();
	ut_asserteq_str("betty", env_get("ut_env2"));

	/* without it an unset <from> is an error */
	ut_asserteq(1, run_command("env indirect ut_env2 ut_none", 0));
	ut_assert_nextline(
		"## env indirect: Environment variable for <from> (ut_none) does not exist.");
	ut_assert_console_end();

	/* both names are required */
	ut_asserteq(1, run_command("env indirect ut_env2", 0));
	ut_assertok(check_usage(uts));

	ut_assertok(run_command("setenv ut_env", 0));
	ut_assertok(run_command("setenv ut_env2", 0));

	return 0;
}
CMD_TEST(cmd_test_env_indirect, UTF_CONSOLE);

/* Test 'env callbacks' and 'env flags' listing the attributes of variables */
static int cmd_test_env_lists(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_CMD_ENV_CALLBACK) &&
	    !IS_ENABLED(CONFIG_CMD_ENV_FLAGS))
		return -EAGAIN;

	if (IS_ENABLED(CONFIG_CMD_ENV_CALLBACK)) {
		ut_assertok(run_command("env callbacks", 0));
		ut_assert_nextline("Available callbacks:");
		ut_assert_nextline("\tCallback Name");
		ut_assert_nextline("\t-------------");
		ut_assert_skip_to_line("Static callback bindings:");
		ut_assert_skip_to_line("Active callback bindings:");
		console_record_reset();
	}

	if (IS_ENABLED(CONFIG_CMD_ENV_FLAGS)) {
		ut_assertok(run_command("env flags", 0));
		ut_assert_nextline(
			"Available variable type flags (position 0):");
		ut_assert_nextline("\tFlag\tVariable Type Name");
		ut_assert_nextline("\t----\t------------------");
		ut_assert_nextline("\ts   -\tstring");
		ut_assert_skip_to_line("Static flags:");
		ut_assert_skip_to_line("Active flags:");
		console_record_reset();
	}

	return 0;
}
CMD_TEST(cmd_test_env_lists, UTF_CONSOLE);

/* Test that env refuses a sub-command it does not have */
static int cmd_test_env_subcmd(struct unit_test_state *uts)
{
	/* a sub-command is required */
	ut_asserteq(1, run_command("env", 0));
	ut_assertok(check_usage(uts));

	/* so is a sub-command which exists */
	ut_asserteq(1, run_command("env nosuch", 0));
	ut_assertok(check_usage(uts));

	/* the command itself takes no options */
	ut_asserteq(1, run_command("env -x print", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_env_subcmd, UTF_CONSOLE);
