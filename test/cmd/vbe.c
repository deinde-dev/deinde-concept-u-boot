// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the vbe command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <test/cmd.h>
#include <test/ut.h>

/* The heading and the rule which 'vbe list' shows */
#define HEAD_LINE	\
	"  #  Sel  Device           Driver          Description"
#define RULE_LINE	\
	"---  ---  --------------   --------------  -----------"

/* The two VBE bootmeths in the sandbox devicetree */
#define LIST_OS		\
	"  2       vbe              vbe_abrec_os    VBE A/B/recovery for OS"
#define LIST_SIMPLE	\
	"  3       firmware0        vbe_simple      VBE simple"

/* The second of them, selected */
#define LIST_SIMPLE_SEL	\
	"  3  *    firmware0        vbe_simple      VBE simple"

/* The sequence number of the vbe-simple bootmeth, from the '#' column */
#define SIMPLE_SEQ	3

/* A sequence number which no bootmeth has */
#define BAD_SEQ		7

/* Lines of the usage message which only the long help shows */
#define USAGE_LIST	"vbe list   - list VBE bootmeths"
#define USAGE_SELECT	"vbe select - select a VBE bootmeth by sequence or name"
#define USAGE_INFO	"vbe info   - show information about a VBE bootmeth"
#define USAGE_STATE	"vbe state  - show VBE state"
#define USAGE_RECOVERY	"vbe recovery [on|off] - show or change whether to " \
			"boot recovery firmware"
#define USAGE_TRY	"vbe try [on|off] - show or change whether to try the " \
			"B firmware"

/**
 * check_usage() - Check the usage message which every error path shows
 *
 * @uts: Test state
 * Return: 0 if OK, other value on error
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("vbe - Verified Boot for Embedded");
	ut_assert_nextline_empty();

	/* the rest of the message is there only where the long help is */
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_nextline(USAGE_LIST);
		ut_assert_nextline(USAGE_SELECT);
		ut_assert_nextline(USAGE_INFO);
		ut_assert_nextline(USAGE_STATE);
		ut_assert_nextline(USAGE_RECOVERY);
		ut_assert_nextline(USAGE_TRY);
	}
	ut_assert_console_end();

	return 0;
}

/* Test 'vbe list' and selecting a bootmeth by name */
static int cmd_test_vbe_base(struct unit_test_state *uts)
{
	/* nothing is selected to start with */
	ut_assertok(run_command("vbe list", 0));
	ut_assert_nextline(HEAD_LINE);
	ut_assert_nextline(RULE_LINE);
	ut_assert_nextline(LIST_OS);
	ut_assert_nextline(LIST_SIMPLE);
	ut_assert_nextline(RULE_LINE);
	ut_assert_console_end();

	/* selecting by name marks that bootmeth in the 'Sel' column */
	ut_assertok(run_command("vbe select firmware0", 0));
	ut_assert_console_end();

	ut_assertok(run_command("vbe list", 0));
	ut_assert_skip_to_line(LIST_SIMPLE_SEL);
	ut_assert_nextline(RULE_LINE);
	ut_assert_console_end();

	/* with no argument the selection is cleared */
	ut_assertok(run_command("vbe select", 0));
	ut_assert_console_end();

	ut_assertok(run_command("vbe list", 0));
	ut_assert_skip_to_line(LIST_SIMPLE);
	ut_assert_nextline(RULE_LINE);
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_vbe_base, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test selecting a VBE bootmeth by sequence number, and names which fail */
static int cmd_test_vbe_select(struct unit_test_state *uts)
{
	/* the sequence number from the '#' column picks the same bootmeth */
	ut_assertok(run_commandf("vbe select %d", SIMPLE_SEQ));
	ut_assert_console_end();

	ut_assertok(run_command("vbe list", 0));
	ut_assert_skip_to_line(LIST_SIMPLE_SEL);
	ut_assert_nextline(RULE_LINE);
	ut_assert_console_end();

	/* a name which no bootmeth has leaves the selection alone */
	ut_asserteq(1, run_command("vbe select nosuch", 0));
	ut_assert_nextline("Cannot probe VBE bootmeth 'nosuch' (err=-19)");
	ut_assert_console_end();

	/* as does a sequence number which no bootmeth has */
	ut_asserteq(1, run_commandf("vbe select %d", BAD_SEQ));
	ut_assert_nextline("Cannot find '%d' (err=-19)", BAD_SEQ);
	ut_assert_console_end();

	ut_assertok(run_command("vbe list", 0));
	ut_assert_skip_to_line(LIST_SIMPLE_SEL);
	ut_assert_nextline(RULE_LINE);
	ut_assert_console_end();

	ut_assertok(run_command("vbe select", 0));

	return 0;
}
CMD_TEST(cmd_test_vbe_select, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test that 'vbe info' needs a bootmeth to report on */
static int cmd_test_vbe_info(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("vbe info", 0));
	ut_assert_nextline("No VBE bootmeth selected");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_vbe_info, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test 'vbe state' where no phase of U-Boot used VBE */
static int cmd_test_vbe_state(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("vbe state", 0));
	ut_assert_nextline("No VBE state");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_vbe_state, UTF_CONSOLE);

/*
 * Test 'vbe recovery' and 'vbe try' where no bootmeth has the firmware they
 * deal with. The bootmeths in the sandbox devicetree are vbe-simple and the
 * A/B/recovery method for the OS, neither of which is fwupd,vbe-abrec.
 */
static int cmd_test_vbe_abrec(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("vbe recovery", 0));
	if (IS_ENABLED(CONFIG_BOOTMETH_VBE_ABREC))
		ut_assert_nextline("No VBE bootmeth has recovery firmware");
	else
		ut_assert_nextline("Recovery is not supported");
	ut_assert_console_end();

	ut_asserteq(1, run_command("vbe try", 0));
	if (IS_ENABLED(CONFIG_BOOTMETH_VBE_ABREC))
		ut_assert_nextline("No VBE bootmeth has B firmware");
	else
		ut_assert_nextline("Trying B is not supported");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_vbe_abrec, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test the arguments the command refuses */
static int cmd_test_vbe_args(struct unit_test_state *uts)
{
	/* the command does nothing without a sub-command */
	ut_asserteq(1, run_command("vbe", 0));
	ut_assertok(check_usage(uts));

	/* nor with one which does not exist */
	ut_asserteq(1, run_command("vbe bogus", 0));
	ut_assertok(check_usage(uts));

	/* 'vbe list' takes no argument */
	ut_asserteq(1, run_command("vbe list extra", 0));
	ut_assertok(check_usage(uts));

	/* the sub-commands which take an argument take only one */
	ut_asserteq(1, run_command("vbe select a b", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("vbe recovery on off", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("vbe try on off", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_vbe_args, UTF_CONSOLE);

/* Test the options and the stray arguments which no sub-command accepts */
static int cmd_test_vbe_opt(struct unit_test_state *uts)
{
	static const char * const cmds[] = {
		"vbe list -x",
		"vbe select -x",
		"vbe state -x",
		"vbe info -x",
		"vbe recovery -x",
		"vbe try -x",
		"vbe state extra",
		"vbe info extra",
	};
	int i;

	/* the command framework only looks for an option with this on */
	if (!IS_ENABLED(CONFIG_CMD_ARG_CHECK))
		return -EAGAIN;

	for (i = 0; i < ARRAY_SIZE(cmds); i++) {
		ut_asserteq(1, run_command(cmds[i], 0));
		ut_assertok(check_usage(uts));
	}

	return 0;
}
CMD_TEST(cmd_test_vbe_opt, UTF_CONSOLE);
