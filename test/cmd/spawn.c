// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Tests for spawn and wait commands
 *
 * Copyright 2025, Linaro Ltd.
 */

#include <command.h>
#include <console.h>
#include <env.h>
#include <test/cmd.h>
#include <test/test.h>
#include <test/ut.h>

/**
 * clear_jobs() - Finish any job an earlier test has left behind
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int clear_jobs(struct unit_test_state *uts)
{
	ut_assertok(run_command("wait", 0));
	console_record_reset();

	return 0;
}

/* Test two jobs, their identifiers and their return values */
static int cmd_test_spawn_base(struct unit_test_state *uts)
{
	ut_assertok(run_command("wait; spawn sleep 2; setenv j ${job_id}; "
				"spawn setenv spawned true; "
				"setenv jj ${job_id}; wait; "
				"echo ${j} ${jj} ${spawned}", 0));
	console_record_readline(uts->actual_str, sizeof(uts->actual_str));
	ut_asserteq_ptr(uts->actual_str,
			strstr(uts->actual_str, "1 2 true"));

	ut_assertok(run_command("spawn true; wait; setenv t $?; spawn false; "
				"wait; setenv f $?; wait; echo $t $f $?", 0));
	console_record_readline(uts->actual_str, sizeof(uts->actual_str));
	ut_asserteq_ptr(uts->actual_str,
			strstr(uts->actual_str, "0 1 0"));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_spawn_base, UTF_CONSOLE);

/* Test that the arguments reach the command rather than spawn */
static int cmd_test_spawn_args(struct unit_test_state *uts)
{
	ut_assertok(clear_jobs(uts));

	/* an option after the command name belongs to that command */
	ut_assertok(run_command("spawn echo -n hi", 0));
	ut_assertok(run_command("wait; echo there", 0));
	ut_assert_nextline("hithere");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_spawn_args, UTF_CONSOLE);

/* Test that a job runs only once the shell gives up the processor */
static int cmd_test_spawn_background(struct unit_test_state *uts)
{
	ut_assertok(clear_jobs(uts));

	/* the job is created but has not run */
	ut_assertok(run_command("spawn echo hello", 0));
	ut_assert_console_end();

	/* the wait is where it runs */
	ut_assertok(run_command("wait", 0));
	ut_assert_nextline("hello");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_spawn_background, UTF_CONSOLE);

/* Test how job identifiers are handed out and reused */
static int cmd_test_spawn_jobs(struct unit_test_state *uts)
{
	ut_assertok(clear_jobs(uts));

	/* each running job takes the next identifier */
	ut_assertok(run_command("spawn sleep 0.1", 0));
	ut_asserteq_str("1", env_get("job_id"));
	ut_assertok(run_command("spawn sleep 0.1", 0));
	ut_asserteq_str("2", env_get("job_id"));

	/* a job can be waited for on its own */
	ut_assertok(run_command("wait 2", 0));
	ut_assertok(run_command("wait 1", 0));

	/* an identifier is free again once its job has finished */
	ut_assertok(run_command("spawn true", 0));
	ut_asserteq_str("1", env_get("job_id"));
	ut_assertok(run_command("wait", 0));

	/* and an identifier with no job returns at once */
	ut_assertok(run_command("wait 2", 0));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_spawn_jobs, UTF_CONSOLE);

/* Test that two jobs make progress together */
static int cmd_test_spawn_parallel(struct unit_test_state *uts)
{
	ut_assertok(clear_jobs(uts));

	/* the second job sleeps for longer, so the order is not in doubt */
	ut_assertok(env_set("ut_job_a", "echo a1; sleep 0.1; echo a2"));
	ut_assertok(env_set("ut_job_b", "echo b1; sleep 0.3; echo b2"));

	ut_assertok(run_command("spawn run ut_job_a", 0));
	ut_assertok(run_command("spawn run ut_job_b", 0));
	ut_assertok(run_command("wait", 0));

	/* each job yields in its sleep, so the two take turns */
	ut_assert_nextline("a1");
	ut_assert_nextline("b1");
	ut_assert_nextline("a2");
	ut_assert_nextline("b2");
	ut_assert_console_end();

	ut_assertok(env_set("ut_job_a", NULL));
	ut_assertok(env_set("ut_job_b", NULL));

	return 0;
}
CMD_TEST(cmd_test_spawn_parallel, UTF_CONSOLE);

/**
 * check_spawn_usage() - Check that spawn's usage message is the only output
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_spawn_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("spawn - run a command in the background");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("spawn command [args...]");
	ut_assert_nextline(
		"    - Run the command as a background job and set the job_id");
	ut_assert_nextline(
		"      environment variable to the identifier of the new job");
	ut_assert_console_end();

	return 0;
}

/**
 * check_wait_usage() - Check that wait's usage message is the only output
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_wait_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("wait - wait for one or more jobs to complete");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("wait [job_id ...]");
	ut_assert_nextline(
		"    - Wait until all specified jobs have exited and return the");
	ut_assert_nextline(
		"      exit status of the last job waited for. When no job_id is");
	ut_assert_nextline("      given, wait for all the background jobs.");
	ut_assert_console_end();

	return 0;
}

/* Test the usage messages */
static int cmd_test_spawn_usage(struct unit_test_state *uts)
{
	ut_assertok(clear_jobs(uts));

	/* spawn needs a command to run */
	ut_asserteq(1, run_command("spawn", 0));
	ut_assertok(check_spawn_usage(uts));

	/* job identifiers start at 1 */
	ut_asserteq(1, run_command("wait 0", 0));
	ut_assertok(check_wait_usage(uts));

	/* and stop at the number of jobs */
	ut_asserteq(1, run_commandf("wait %d", CONFIG_CMD_SPAWN_NUM_JOBS + 1));
	ut_assertok(check_wait_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_spawn_usage, UTF_CONSOLE);
