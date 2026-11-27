// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the scp03 command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <test/cmd.h>
#include <test/ut.h>

/* check the usage message, which every argument error shows */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("scp03 - Secure Channel Protocol 03 control");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline(
		"scp03 provides a command to enable SCP03 and provision the SCP03 keys");
	ut_assert_nextline(" enable    - enable SCP03 on the TEE");
	ut_assert_nextline(" provision - provision SCP03 on the TEE");
	ut_assert_nextline_empty();
	ut_assert_console_end();

	return 0;
}

/* Test 'scp03 enable' and 'scp03 provision' */
static int cmd_test_scp03_base(struct unit_test_state *uts)
{
	ut_assertok(run_command("scp03 enable", 0));
	ut_assert_nextline("SCP03 is enabled");
	ut_assert_console_end();

	ut_assertok(run_command("scp03 provision", 0));
	ut_assert_nextline("SCP03 is provisioned");
	ut_assert_console_end();

	/*
	 * The TEE holds the channel open, so asking again succeeds and says
	 * the same thing
	 */
	ut_assertok(run_command("scp03 enable", 0));
	ut_assert_nextline("SCP03 is enabled");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_scp03_base, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test what the command reports when the TEE refuses */
static int cmd_test_scp03_no_tee(struct unit_test_state *uts)
{
	struct udevice *dev;

	/* drop the emulated TEE, so that the command can find none */
	ut_assertok(uclass_first_device_err(UCLASS_TEE, &dev));
	ut_assertok(device_remove(dev, DM_REMOVE_NORMAL));
	ut_assertok(device_unbind(dev));
	ut_asserteq(-ENODEV, uclass_first_device_err(UCLASS_TEE, &dev));

	/*
	 * Without CONFIG_CMD_ERR_MSG the messages are compiled out and
	 * cmd_process() reports the command and the error number instead
	 */
	ut_asserteq(1, run_command("scp03 enable", 0));
	if (IS_ENABLED(CONFIG_CMD_ERR_MSG))
		ut_assert_nextline("TEE failed to enable SCP03");
	else
		ut_assert_nextline("scp03: failed, err %d", -ENODEV);
	ut_assert_console_end();

	ut_asserteq(1, run_command("scp03 provision", 0));
	if (IS_ENABLED(CONFIG_CMD_ERR_MSG))
		ut_assert_nextline("TEE failed to provision SCP03 keys");
	else
		ut_assert_nextline("scp03: failed, err %d", -ENODEV);
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_scp03_no_tee, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test the arguments which show the usage message */
static int cmd_test_scp03_usage(struct unit_test_state *uts)
{
	/* no sub-command */
	ut_asserteq(1, run_command("scp03", 0));
	ut_assertok(check_usage(uts));

	/* a sub-command which does not exist */
	ut_asserteq(1, run_command("scp03 wibble", 0));
	ut_assertok(check_usage(uts));

	/* an argument for a sub-command which takes none */
	ut_asserteq(1, run_command("scp03 enable extra", 0));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("scp03 provision extra", 0));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_scp03_usage, UTF_CONSOLE);
