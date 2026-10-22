// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the shim command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <efi_loader.h>
#include <efi_variable.h>
#include <shim.h>
#include <test/cmd.h>
#include <test/ut.h>

/* the last line of the help text, which ends the usage message */
#define SHIM_USAGE_LAST \
	"shim debug [[-n] <0/1>]  - Enable / disable debug verbose mode"

/**
 * init_efi() - Start the EFI subsystem, keeping its output out of the way
 *
 * The command starts EFI if it is not up yet, which on sandbox warns that
 * there is nowhere to keep the variables. Do that here, before the console
 * is recorded, so the warning does not land in the middle of a test.
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int init_efi(struct unit_test_state *uts)
{
	ut_asserteq(EFI_SUCCESS, efi_init_obj_list());
	console_record_reset_enable();

	return 0;
}

/**
 * clear_verbose() - Delete shim's variable, so a test starts without it
 *
 * UEFI does not allow the attributes of an existing variable to be changed,
 * so a test which writes the variable must start from nothing.
 */
static void clear_verbose(void)
{
	efi_set_variable_int(SHIM_VERBOSE_VAR_NAME, &efi_shim_lock, 0, 0, NULL,
			     false);
}

/**
 * check_usage() - Check the usage message, which a bad command line shows
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("shim - Shim utilities");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline(SHIM_USAGE_LAST);
	ut_assert_console_end();

	return 0;
}

/* Test reading and writing shim's verbose-mode variable */
static int cmd_test_shim_base(struct unit_test_state *uts)
{
	ut_assertok(init_efi(uts));
	clear_verbose();

	/* with no variable at all the mode reads as disabled */
	ut_assertok(run_command("shim debug", 0));
	ut_assert_nextline("0");
	ut_assert_console_end();

	ut_assertok(run_command("shim debug 1", 0));
	ut_assert_console_end();

	ut_assertok(run_command("shim debug", 0));
	ut_assert_nextline("1");
	ut_assert_console_end();

	/* any value other than zero enables the mode */
	ut_assertok(run_command("shim debug 2", 0));
	ut_assertok(run_command("shim debug", 0));
	ut_assert_nextline("1");
	ut_assert_console_end();

	ut_assertok(run_command("shim debug 0", 0));
	ut_assertok(run_command("shim debug", 0));
	ut_assert_nextline("0");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_shim_base, UTF_CONSOLE);

/* Test that -n keeps the setting across a reset */
static int cmd_test_shim_persist(struct unit_test_state *uts)
{
	efi_uintn_t size;
	u32 value, attr;

	ut_assertok(init_efi(uts));
	clear_verbose();

	ut_assertok(run_command("shim debug -n 1", 0));
	ut_assert_console_end();

	/* the variable is written with the attribute which survives a reset */
	size = sizeof(value);
	ut_asserteq(EFI_SUCCESS,
		    efi_get_variable_int(SHIM_VERBOSE_VAR_NAME, &efi_shim_lock,
					 &attr, &size, &value, NULL));
	ut_asserteq(EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE,
		    attr);
	ut_asserteq(sizeof(value), size);
	ut_asserteq(1, value);

	ut_assertok(run_command("shim debug", 0));
	ut_assert_nextline("1");
	ut_assert_console_end();

	/*
	 * UEFI does not allow the attributes of a variable to change, so
	 * writing the setting again without -n fails
	 */
	ut_asserteq(1, run_command("shim debug 0", 0));
	ut_assert_nextline("Failed to write variable (err=%lx)",
			   EFI_INVALID_PARAMETER);
	ut_assert_console_end();

	/* leave nothing behind, as this variable outlives a reset */
	clear_verbose();

	return 0;
}
CMD_TEST(cmd_test_shim_persist, UTF_CONSOLE);

/* Test the command lines which the command refuses */
static int cmd_test_shim_usage(struct unit_test_state *uts)
{
	ut_assertok(init_efi(uts));

	/* a sub-command is needed */
	ut_asserteq(1, run_command("shim", 0));
	ut_assertok(check_usage(uts));

	/* and it must be one the command knows */
	ut_asserteq(1, run_command("shim bogus", 0));
	ut_assertok(check_usage(uts));

	/* -n says how to write the value, so it cannot appear on its own */
	ut_asserteq(1, run_command("shim debug -n", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_shim_usage, UTF_CONSOLE);
