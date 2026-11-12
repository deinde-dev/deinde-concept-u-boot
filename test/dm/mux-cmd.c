// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Texas Instruments Inc.
 * Pratyush Yadav <p.yadav@ti.com>
 */
#include <dm.h>
#include <mux.h>
#include <mux-internal.h>
#include <dt-bindings/mux/mux.h>
#include <asm/test.h>
#include <dm/test.h>
#include <test/ut.h>
#include <console.h>
#include <rand.h>
#include <time.h>

#define BUF_SIZE		256

/* Test 'mux list' */
static int dm_test_cmd_mux_list(struct unit_test_state *uts)
{
	char str[BUF_SIZE], *tok;
	struct udevice *dev;
	struct mux_chip *chip;
	struct mux_control *mux;
	int i;
	unsigned long val;

	ut_assertok(uclass_get_device_by_name(UCLASS_MUX, "a-mux-controller",
					      &dev));
	chip = dev_get_uclass_priv(dev);
	ut_assertnonnull(chip);

	run_command("mux list", 0);
	ut_assert_nextline("a-mux-controller:");

	/*
	 * Check the table header to make sure we are not out of sync with the
	 * code in the command. If we are, catch it early.
	 */
	console_record_readline(str, BUF_SIZE);
	tok = strtok(str, " ");
	ut_asserteq_str("ID", tok);

	tok = strtok(NULL, " ");
	ut_asserteq_str("Selected", tok);

	tok = strtok(NULL, " ");
	ut_asserteq_str("Current", tok);
	tok = strtok(NULL, " ");
	ut_asserteq_str("State", tok);

	tok = strtok(NULL, " ");
	ut_asserteq_str("Idle", tok);
	tok = strtok(NULL, " ");
	ut_asserteq_str("State", tok);

	tok = strtok(NULL, " ");
	ut_asserteq_str("Num", tok);
	tok = strtok(NULL, " ");
	ut_asserteq_str("States", tok);

	for (i = 0; i < chip->controllers; i++) {
		mux = &chip->mux[i];

		console_record_readline(str, BUF_SIZE);

		/*
		 * Check if the ID printed matches with the ID of the chip we
		 * have.
		 */
		tok = strtok(str, " ");
		ut_assertok(strict_strtoul(tok, 10, &val));
		ut_asserteq(i, val);

		/* Check if mux selection state matches. */
		tok = strtok(NULL, " ");
		if (mux->in_use) {
			ut_asserteq_str("yes", tok);
		} else {
			ut_asserteq_str("no", tok);
		}

		/* Check if the current state matches. */
		tok = strtok(NULL, " ");
		if (mux->cached_state == MUX_IDLE_AS_IS) {
			ut_asserteq_str("unknown", tok);
		} else {
			ut_assertok(strict_strtoul(tok, 16, &val));
			ut_asserteq(mux->cached_state, val);
		}

		/* Check if the idle state matches */
		tok = strtok(NULL, " ");
		if (mux->idle_state == MUX_IDLE_AS_IS) {
			ut_asserteq_str("as-is", tok);
		} else {
			ut_assertok(strict_strtoul(tok, 16, &val));
			ut_asserteq(mux->idle_state, val);
		}

		/* Check if the number of states matches */
		tok = strtok(NULL, " ");
		ut_assertok(strict_strtoul(tok, 16, &val));
		ut_asserteq(mux->states, val);
	}

	return 0;
}
DM_TEST(dm_test_cmd_mux_list, UTF_SCAN_PDATA | UTF_SCAN_FDT | UTF_CONSOLE);

static int dm_test_cmd_mux_select(struct unit_test_state *uts)
{
	struct udevice *dev;
	struct mux_chip *chip;
	struct mux_control *mux;
	char cmd[BUF_SIZE];
	unsigned int i, state;

	ut_assertok(uclass_get_device_by_name(UCLASS_MUX, "a-mux-controller",
					      &dev));
	chip = dev_get_uclass_priv(dev);
	ut_assertnonnull(chip);

	srand(get_ticks() + rand());
	for (i = 0; i < chip->controllers; i++) {
		mux = &chip->mux[i];

		state = rand() % mux->states;

		snprintf(cmd, BUF_SIZE, "mux select a-mux-controller %x %x", i,
			 state);
		run_command(cmd, 0);
		ut_asserteq(!!mux->in_use, true);
		ut_asserteq(state, mux->cached_state);

		ut_assertok(mux_control_deselect(mux));
	}

	return 0;
}
DM_TEST(dm_test_cmd_mux_select, UTF_SCAN_PDATA | UTF_SCAN_FDT);

static int dm_test_cmd_mux_deselect(struct unit_test_state *uts)
{
	struct udevice *dev;
	struct mux_chip *chip;
	struct mux_control *mux;
	char cmd[BUF_SIZE];
	unsigned int i, state;

	ut_assertok(uclass_get_device_by_name(UCLASS_MUX, "a-mux-controller",
					      &dev));
	chip = dev_get_uclass_priv(dev);
	ut_assertnonnull(chip);

	srand(get_ticks() + rand());
	for (i = 0; i < chip->controllers; i++) {
		mux = &chip->mux[i];

		state = rand() % mux->states;
		ut_assertok(mux_control_select(mux, state));

		snprintf(cmd, BUF_SIZE, "mux deselect a-mux-controller %d", i);
		run_command(cmd, 0);
		ut_asserteq(!!mux->in_use, false);
	}

	return 0;
}
DM_TEST(dm_test_cmd_mux_deselect, UTF_SCAN_PDATA | UTF_SCAN_FDT);

/* Test the errors which 'mux select' reports */
static int dm_test_cmd_mux_select_err(struct unit_test_state *uts)
{
	struct udevice *dev;
	struct mux_chip *chip;

	ut_assertok(uclass_get_device_by_name(UCLASS_MUX, "a-mux-controller",
					      &dev));
	chip = dev_get_uclass_priv(dev);
	ut_assertnonnull(chip);

	/* a chip which does not exist */
	ut_asserteq(1, run_command("mux select no-such-mux 0 1", 0));
	ut_assert_nextline("Failed to find the specified mux");

	/* a controller the chip does not have */
	ut_asserteq(1, run_commandf("mux select a-mux-controller %d 1",
				    chip->controllers));
	ut_assert_nextline("Failed to find the specified mux");

	/* a controller number which is not a number */
	ut_asserteq(1, run_command("mux select a-mux-controller x 1", 0));
	ut_assert_nextline("Failed to find the specified mux");

	/* a state which is not a hex number */
	ut_asserteq(1, run_command("mux select a-mux-controller 1 zz", 0));
	ut_assert_nextline("Invalid state");

	/* a controller which something already holds */
	ut_assertok(run_command("mux select a-mux-controller 1 5", 0));
	ut_asserteq(1, run_command("mux select a-mux-controller 1 5", 0));
	ut_assert_nextline("Failed to select requested state");
	ut_assertok(mux_control_deselect(&chip->mux[1]));

	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_cmd_mux_select_err,
	UTF_SCAN_PDATA | UTF_SCAN_FDT | UTF_CONSOLE);

/* Test the errors which 'mux deselect' reports */
static int dm_test_cmd_mux_deselect_err(struct unit_test_state *uts)
{
	struct udevice *dev;
	struct mux_chip *chip;

	ut_assertok(uclass_get_device_by_name(UCLASS_MUX, "a-mux-controller",
					      &dev));
	chip = dev_get_uclass_priv(dev);
	ut_assertnonnull(chip);

	/* a chip which does not exist */
	ut_asserteq(1, run_command("mux deselect no-such-mux 0", 0));
	ut_assert_nextline("Failed to find the specified mux");

	/* a controller the chip does not have */
	ut_asserteq(1, run_commandf("mux deselect a-mux-controller %d",
				    chip->controllers));
	ut_assert_nextline("Failed to find the specified mux");

	/* releasing a controller which nothing holds is not an error */
	ut_assertok(run_command("mux deselect a-mux-controller 1", 0));

	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_cmd_mux_deselect_err,
	UTF_SCAN_PDATA | UTF_SCAN_FDT | UTF_CONSOLE);

/* Test the mux command showing its usage message */
static int dm_test_cmd_mux_usage(struct unit_test_state *uts)
{
	static const char * const cmds[] = {
		"mux",				/* no sub-command */
		"mux bogus",			/* an unknown sub-command */
		"mux list extra",		/* too many arguments */
		"mux select a-mux-controller 1",	/* too few */
		"mux deselect a-mux-controller",	/* too few */
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(cmds); i++) {
		ut_asserteq(1, run_command(cmds[i], 0));
		ut_assert_nextline("mux - List, select, and deselect muxes");
		ut_assert_nextline_empty();

		/*
		 * The rest of the message is the long help, which
		 * CONFIG_SYS_LONGHELP can leave out
		 */
		console_record_reset();
	}

	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_cmd_mux_usage, UTF_SCAN_PDATA | UTF_SCAN_FDT | UTF_CONSOLE);
