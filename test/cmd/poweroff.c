// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the poweroff command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <sysreset.h>
#include <asm/state.h>
#include <test/cmd.h>
#include <test/ut.h>

/**
 * refuse_poweroff() - Stop the sandbox state from switching the power off
 *
 * The command exits the sandbox process when it succeeds, which would end the
 * test run, so each type sysreset_walk() tries from SYSRESET_POWER_OFF
 * onwards is refused. The old settings go into @old so the test can put them
 * back.
 *
 * @state: Sandbox state
 * @old: Receives the setting of each type the function changes
 */
static void refuse_poweroff(struct sandbox_state *state,
			    bool old[SYSRESET_COUNT])
{
	for (int type = SYSRESET_POWER_OFF; type < SYSRESET_COUNT; type++) {
		old[type] = state->sysreset_allowed[type];
		state->sysreset_allowed[type] = false;
	}
}

/**
 * allow_poweroff() - Put the sandbox state back as it was found
 *
 * @state: Sandbox state
 * @old: Setting of each type refuse_poweroff() changed
 */
static void allow_poweroff(struct sandbox_state *state,
			   const bool old[SYSRESET_COUNT])
{
	for (int type = SYSRESET_POWER_OFF; type < SYSRESET_COUNT; type++)
		state->sysreset_allowed[type] = old[type];
}

/**
 * check_usage() - Check the usage message the command shows
 *
 * @uts: Test state
 * Return: 0 if OK, other value on error
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("poweroff - Perform POWEROFF of the device");
	ut_assert_nextline_empty();

	/* the rest of the message is there only where the long help is */
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");

		/* the command has no long help, so this is just its name */
		ut_assert_nextline("poweroff ");
	}
	ut_assert_console_end();

	return 0;
}

/* Test that the command asks the sysreset uclass to switch the power off */
static int cmd_test_poweroff_base(struct unit_test_state *uts)
{
	struct sandbox_state *state = state_get_current();
	enum sysreset_t last = state->last_sysreset;
	bool old[SYSRESET_COUNT];

	refuse_poweroff(state, old);

	/* skip the 100ms the command waits for its message to be sent */
	ut_set_skip_delays(uts, true);

	/*
	 * With no device willing to cut the power the command returns, which
	 * it does not do on a board which switches off
	 */
	ut_asserteq(1, run_command("poweroff", 0));
	ut_assert_nextline("poweroff ...");
	ut_assert_console_end();

	/* the state records the type the command asked for */
	ut_asserteq(SYSRESET_POWER_OFF, state->last_sysreset);

	state->last_sysreset = last;
	allow_poweroff(state, old);

	return 0;
}
CMD_TEST(cmd_test_poweroff_base, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test that the command takes no arguments */
static int cmd_test_poweroff_usage(struct unit_test_state *uts)
{
	struct sandbox_state *state = state_get_current();
	bool old[SYSRESET_COUNT];

	refuse_poweroff(state, old);

	/* an argument is refused, so the board stays on */
	ut_asserteq(1, run_command("poweroff extra", 0));
	ut_assertok(check_usage(uts));

	/* as is an option, since the command has none */
	ut_asserteq(1, run_command("poweroff -x", 0));
	ut_assertok(check_usage(uts));

	allow_poweroff(state, old);

	return 0;
}
CMD_TEST(cmd_test_poweroff_usage, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);
