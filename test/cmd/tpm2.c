// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the tpm2 command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <dm.h>
#include <mapmem.h>
#include <tpm-v2.h>
#include <test/cmd.h>
#include <test/ut.h>

/*
 * These tests do not use UTF_DM, for the reason test/cmd/tpm.c gives: do_tpm()
 * reads the currently selected TPM, a static, before it dispatches a
 * sub-command, so the dangling pointer which a driver-model test leaves behind
 * cannot be recovered from. The commands run against the tree U-Boot has bound
 * for itself, which stays where it is.
 */
#define TPM2_TEST_FLAGS	UTF_CONSOLE

/* Memory used by these tests, and a second buffer for reading a PCR back */
#define TPM2_ADDR	(CONFIG_SYS_LOAD_ADDR + 0x2000)
#define TPM2_ADDR2	(CONFIG_SYS_LOAD_ADDR + 0x2100)

/* a PCR which no other test measures into */
#define TPM2_PCR	10

/* and an index which the device does not have */
#define TPM2_BAD_PCR	99

/* a password one character longer than a digest */
#define TPM2_LONG_PW	"0123456789012345678901234567890123"

/*
 * Name of the function which logs the dictionary-attack parameters,
 * right-aligned in CONFIG_LOGF_FUNC_PAD (20) characters as the log emits it;
 * this one is longer than the padding
 */
#define DAM_FUNC	"do_tpm_dam_parameters"

/* the last line of the help text, which ends the usage message */
#define TPM2_USAGE_LAST	"    <password>: optional password"

/* the line before it, which is unique in the help text */
#define TPM2_USAGE_OFF \
	"        * off - Clear all available PCRs associated with the specified"

/* 32 zero bytes, as the command shows them */
#define ZERO_LINE	" 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"

/**
 * select_tpm2() - Select the emulated TPMv2.x device and bring it up
 *
 * Sandbox has the TPMv2.x device first, so it is the default choice, but an
 * earlier test may have selected the TPMv1.x one. The device can be opened
 * only once in a boot, so each test autostarts it rather than assuming that it
 * is or is not open already.
 *
 * @uts: Test state
 */
static int select_tpm2(struct unit_test_state *uts)
{
	ut_assertok(run_command("tpm2 device 0", 0));
	ut_assertok(run_command("tpm2 autostart", 0));
	ut_assert_console_end();

	return 0;
}

/**
 * check_log() - Check the next console line, which the command logs
 *
 * The log format shows the function name only when CONFIG_LOGF_FUNC is
 * enabled. That is true of sandbox but of none of the other sandbox variants,
 * so the expected line differs between them.
 *
 * @uts: Test state
 * @msg: Message which is logged
 */
static int check_log(struct unit_test_state *uts, const char *msg)
{
	if (IS_ENABLED(CONFIG_LOGF_FUNC))
		ut_assert_nextline("%s() %s", DAM_FUNC, msg);
	else
		ut_assert_nextline("%s", msg);

	return 0;
}

/* Test 'tpm2 device', 'tpm2 info' and the sub-commands which open the device */
static int cmd_test_tpm2_base(struct unit_test_state *uts)
{
	ut_assertok(run_command("tpm2 device", 0));
	ut_assert_nextline("device 0: Sandbox TPM2.x");
	ut_assert_nextline("device 1: sandbox TPM");
	ut_assert_console_end();

	ut_assertok(select_tpm2(uts));

	ut_assertok(run_command("tpm2 info", 0));
	ut_assert_nextline("Sandbox TPM2.x");
	ut_assert_console_end();

	ut_assertok(run_command("tpm2 state", 0));
	ut_assert_nextline("init_done=1");
	ut_assert_console_end();

	/* the sub-commands which the autostart has already done say nothing */
	ut_assertok(run_command("tpm2 startup TPM2_SU_CLEAR", 0));
	ut_assertok(run_command("tpm2 self_test full", 0));
	ut_assertok(run_command("tpm2 self_test continue", 0));
	ut_assert_console_end();

	/* the device is open, so opening it again is refused */
	ut_asserteq(1, run_command("tpm2 init", 0));
	ut_assert_nextline("Error: -16");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_tpm2_base, TPM2_TEST_FLAGS);

/* Test that the sub-commands come from the device, not from the name typed */
static int cmd_test_tpm2_shared(struct unit_test_state *uts)
{
	/* without the TPMv1.x stack there is no 'tpm' command to try */
	if (!IS_ENABLED(CONFIG_CMD_TPM_V1))
		return -EAGAIN;

	ut_assertok(select_tpm2(uts));

	/* 'state' belongs to the TPMv2.x set, which 'tpm' offers here */
	ut_assertok(run_command("tpm state", 0));
	ut_assert_nextline("init_done=1");
	ut_assert_console_end();

	ut_assertok(run_command("tpm info", 0));
	ut_assert_nextline("Sandbox TPM2.x");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_tpm2_shared, TPM2_TEST_FLAGS);

/* Test reading and extending a PCR */
static int cmd_test_tpm2_pcr(struct unit_test_state *uts)
{
	u8 *buf, *buf2;

	ut_assertok(select_tpm2(uts));

	buf = map_sysmem(TPM2_ADDR, TPM2_DIGEST_LEN);
	memset(buf, '\0', TPM2_DIGEST_LEN);

	ut_assertok(run_commandf("tpm2 pcr_read %d %#lx", TPM2_PCR,
				 (ulong)TPM2_ADDR));
	ut_assert_nextline("PCR #%d sha256 32 byte content (0 known updates):",
			   TPM2_PCR);
	ut_assert_nextline(ZERO_LINE);
	ut_assert_nextline(ZERO_LINE);
	ut_assert_console_end();

	/* the digest is shown, not the register it is added to */
	ut_assertok(run_commandf("tpm2 pcr_extend %d %#lx", TPM2_PCR,
				 (ulong)TPM2_ADDR));
	ut_assert_nextline("PCR #%d extended with 32 byte sha256 digest",
			   TPM2_PCR);
	ut_assert_nextline(ZERO_LINE);
	ut_assert_nextline(ZERO_LINE);
	ut_assert_console_end();

	/*
	 * Read into other memory, so the digest is left alone. Extending a
	 * zeroed register with a zeroed digest gives a known hash, and the
	 * count of updates follows it
	 */
	ut_assertok(run_commandf("tpm2 pcr_read %d %#lx", TPM2_PCR,
				 (ulong)TPM2_ADDR2));
	ut_assert_nextline("PCR #%d sha256 32 byte content (1 known updates):",
			   TPM2_PCR);
	ut_assert_nextline(" f5 a5 fd 42 d1 6a 20 30 27 98 ef 6e d3 09 97 9b");
	ut_assert_nextline(" 43 00 3d 23 20 d9 f0 e8 ea 98 31 a9 27 59 fb 4b");
	ut_assert_console_end();

	/* the read leaves the contents in memory as well as showing them */
	buf2 = map_sysmem(TPM2_ADDR2, TPM2_DIGEST_LEN);
	ut_asserteq(0xf5, buf2[0]);
	ut_asserteq(0x4b, buf2[TPM2_DIGEST_LEN - 1]);
	unmap_sysmem(buf2);
	unmap_sysmem(buf);

	/* leave the TPM as it was found, since the registers outlive the test */
	ut_assertok(run_command("tpm2 clear TPM2_RH_PLATFORM", 0));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_tpm2_pcr, TPM2_TEST_FLAGS);

/* Test reading capabilities and setting the dictionary-attack parameters */
static int cmd_test_tpm2_capability(struct unit_test_state *uts)
{
	ut_assertok(select_tpm2(uts));

	/* property 0x20e is the lockout counter, zero on a clear TPM */
	ut_assertok(run_commandf("tpm2 get_capability 0x6 0x20e %#lx 1",
				 (ulong)TPM2_ADDR));
	ut_assert_nextline("Capabilities read from TPM:");
	ut_assert_nextline("Property 0x0000020e: 0x00000000");
	ut_assert_console_end();

	ut_assertok(run_command("tpm2 dam_parameters 3 10 0", 0));
	ut_assertok(check_log(uts, "Changing dictionary attack parameters:"));
	ut_assertok(check_log(uts, "- maxTries: 3"));
	ut_assertok(check_log(uts, "- recoveryTime: 10"));
	ut_assertok(check_log(uts, "- lockoutRecovery: 0"));
	ut_assert_console_end();

	/* the three parameters read back as they were set */
	ut_assertok(run_commandf("tpm2 get_capability 0x6 0x20f %#lx 3",
				 (ulong)TPM2_ADDR));
	ut_assert_nextline("Capabilities read from TPM:");
	ut_assert_nextline("Property 0x0000020f: 0x00000003");
	ut_assert_nextline("Property 0x00000210: 0x0000000a");
	ut_assert_nextline("Property 0x00000211: 0x00000000");
	ut_assert_console_end();

	/* the parameters outlive the test, so put them back */
	ut_assertok(run_command("tpm2 clear TPM2_RH_PLATFORM", 0));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_tpm2_capability, TPM2_TEST_FLAGS);

/* Test setting the password of a hierarchy */
static int cmd_test_tpm2_auth(struct unit_test_state *uts)
{
	ut_assertok(select_tpm2(uts));

	ut_assertok(run_command("tpm2 change_auth TPM2_RH_LOCKOUT unicorn", 0));
	ut_assert_console_end();

	/* the hierarchy now wants its password */
	ut_asserteq(1, run_command("tpm2 clear TPM2_RH_LOCKOUT", 0));
	ut_assert_nextline("Authentication failed: wrong password.");
	ut_assert_nextline("Error: 162");
	ut_assert_console_end();

	ut_assertok(run_command("tpm2 clear TPM2_RH_LOCKOUT unicorn", 0));
	ut_assert_console_end();

	/* clearing the hierarchy has dropped the password again */
	ut_assertok(run_command("tpm2 clear TPM2_RH_LOCKOUT", 0));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_tpm2_auth, TPM2_TEST_FLAGS);

/* Test the error paths */
static int cmd_test_tpm2_missing(struct unit_test_state *uts)
{
	/* a device which is not there */
	ut_asserteq(1, run_command("tpm2 device 5", 0));
	ut_assert_nextline("Couldn't set TPM 5 (rc = 1)");
	ut_assert_console_end();

	ut_assertok(select_tpm2(uts));

	ut_asserteq(1, run_command("tpm2 startup FOO", 0));
	ut_assert_nextline("Couldn't recognize mode string: FOO");
	ut_assert_console_end();

	ut_asserteq(1, run_command("tpm2 self_test bogus", 0));
	ut_assert_nextline("Couldn't recognize test mode: bogus");
	ut_assert_console_end();

	/* a PCR which the device does not have, which says nothing */
	ut_asserteq(1, run_commandf("tpm2 pcr_read %d %#lx", TPM2_BAD_PCR,
				    (ulong)TPM2_ADDR));
	ut_assert_console_end();

	/* nor does a password longer than a digest */
	ut_asserteq(1, run_command("tpm2 clear TPM2_RH_LOCKOUT "
				   TPM2_LONG_PW, 0));
	ut_assert_console_end();

	/* a sub-command missing an argument shows the usage message */
	ut_asserteq(1, run_commandf("tpm2 pcr_extend %d", TPM2_PCR));
	ut_assert_skip_to_line(TPM2_USAGE_OFF);
	ut_assert_nextline("                algorithm (bank)");
	ut_assert_nextline(TPM2_USAGE_LAST);
	ut_assert_nextline_empty();
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_tpm2_missing, TPM2_TEST_FLAGS);
