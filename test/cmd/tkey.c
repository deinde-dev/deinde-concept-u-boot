// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for tkey command
 *
 * Copyright (C) 2025 Canonical Ltd
 */

#include <console.h>
#include <dm.h>
#include <tkey.h>
#include <dm/test.h>
#include <test/cmd.h>
#include <test/ut.h>

/* What the emulated device returns for its public key */
#define PUBKEY \
	"505152535455565758595a5b5c5d5e5f505152535455565758595a5b5c5d5e5f"

/* The disk key derived from that public key */
#define DISK_KEY \
	"e9b0599268ff8b083ef80dbd04be207ce9a19a60a888ccb3fe93710a0a70a34e"

/* The verification hash of that disk key */
#define VERIFY_HASH \
	"8583a08d6c534e84ae81a8518071c16a8030893df05fecb84e514438591ba5ed"

/* A hash of the right length which the device never produces */
#define OTHER_HASH \
	"1111111111111111111111111111111111111111111111111111111111111111"

/* The wrapping key derived from the UDI and the password 'testpass' */
#define WRAP_KEY \
	"f91450f0396768885aeaee7f0cc3305de25f6e50db79e7978a83c08896fcbf0d"

/* A secret one byte longer than the device accepts */
#define LONG_USS	"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"

/* What 'tkey loadapp' shows before it reports the outcome */
#define LOADING		"Loading signer app (%ld bytes)"

/* Test 'tkey' command help output */
static int cmd_test_tkey_help(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("tkey", 0));
	ut_assert_nextlinen("tkey - Tillitis TKey security token operations");
	ut_assert_nextline_empty();
	ut_assert_nextlinen("Usage:");
	ut_assert_nextlinen("tkey connect");
	ut_assert_skip_to_linen("tkey wrapkey");

	return 0;
}
CMD_TEST(cmd_test_tkey_help, UTF_DM | UTF_SCAN_FDT | UTF_CONSOLE);

/* Test 'tkey' subcommands with emulator */
static int cmd_test_tkey_base(struct unit_test_state *uts)
{
	/* Use tkey-emul for predictable test results */
	ut_assertok(run_command("tkey connect tkey-emul", 0));
	ut_assert_nextline("Connected to TKey device");

	/* Test info command */
	ut_assertok(run_command("tkey info", 0));
	ut_assert_nextline("Name0: tk1  Name1: mkdf Version: 4");
	ut_assert_nextline("UDI: a0a1a2a3a4a5a6a7");

	/* Test fwmode command - should be in firmware mode initially */
	ut_assertok(run_command("tkey fwmode", 0));
	ut_assert_nextline("firmware mode");

	/* Test signer command */
	ut_assertok(run_command("tkey signer", 0));
	ut_assert_nextlinen("signer binary: ");

	/* Test wrapkey command */
	ut_assertok(run_command("tkey wrapkey testpass", 0));
	ut_assert_nextline("Wrapping Key: " WRAP_KEY);

	/* Test getkey command */
	ut_assertok(run_command("tkey getkey testuss", 0));
	ut_assert_nextline("Public Key: " PUBKEY);
	ut_assert_nextline("Disk Key: " DISK_KEY);
	ut_assert_nextline("Verification Hash: " VERIFY_HASH);

	/* After getkey, device should be in app mode */
	ut_assertok(run_command("tkey fwmode", 0));
	ut_assert_nextline("app mode");

	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_tkey_base, UTF_DM | UTF_SCAN_FDT | UTF_CONSOLE);

/* Test the arguments which the command accepts */
static int cmd_test_tkey_args(struct unit_test_state *uts)
{
	/* 'tkey info' takes nothing, so a stray argument is refused */
	ut_asserteq(1, run_command("tkey info extra", 0));
	ut_assert_nextlinen("tkey - Tillitis TKey security token operations");
	ut_assert_skip_to_linen("tkey wrapkey");
	ut_assert_console_end();

	/* A sub-command which does not exist is refused in the same way */
	ut_asserteq(1, run_command("tkey bogus", 0));
	ut_assert_nextlinen("tkey - Tillitis TKey security token operations");
	ut_assert_skip_to_linen("tkey wrapkey");
	ut_assert_console_end();

	/* 'tkey getkey' takes a secret and at most a hash */
	ut_asserteq(1, run_command("tkey getkey a b c", 0));
	ut_assert_nextlinen("tkey - Tillitis TKey security token operations");
	ut_assert_skip_to_linen("tkey wrapkey");
	ut_assert_console_end();

	ut_asserteq(1, run_command("tkey getkey", 0));
	ut_assert_nextlinen("tkey - Tillitis TKey security token operations");
	ut_assert_skip_to_linen("tkey wrapkey");
	ut_assert_console_end();

	/* 'tkey wrapkey' needs its password */
	ut_asserteq(1, run_command("tkey wrapkey", 0));
	ut_assert_nextlinen("tkey - Tillitis TKey security token operations");
	ut_assert_skip_to_linen("tkey wrapkey");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_tkey_args, UTF_CONSOLE);

/* Test which device 'tkey connect' selects */
static int cmd_test_tkey_connect(struct unit_test_state *uts)
{
	ut_assertok(run_command("tkey connect tkey-emul", 0));
	ut_assert_nextline("Connected to TKey device");
	ut_assert_console_end();

	/* A name which no device has */
	ut_asserteq(1, run_command("tkey connect nosuch", 0));
	ut_assert_nextlinen("Failed to find TKey device 'nosuch' (err -19");
	ut_assert_console_end();

	/* The device selected before the failure is still the current one */
	ut_assertok(run_command("tkey fwmode", 0));
	ut_assert_nextline("firmware mode");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_tkey_connect, UTF_DM | UTF_SCAN_FDT | UTF_CONSOLE);

/* Test 'tkey loadapp' and what app mode allows */
static int cmd_test_tkey_loadapp(struct unit_test_state *uts)
{
	ut_assertok(run_command("tkey connect tkey-emul", 0));
	ut_assert_nextline("Connected to TKey device");

	ut_assertok(run_command("tkey loadapp mysecret", 0));
	ut_assert_nextline(LOADING " with USS...done", (long)TKEY_SIGNER_SIZE);

	ut_assertok(run_command("tkey fwmode", 0));
	ut_assert_nextline("app mode");

	/* The UDI is out of reach in app mode, so info and wrapkey fail */
	ut_asserteq(1, run_command("tkey info", 0));
	ut_assert_nextline("Name0: tk1  Name1: sign Version: 4");
	ut_assert_nextlinen("Failed to get UDI (err -5");

	ut_asserteq(1, run_command("tkey wrapkey testpass", 0));
	ut_assert_nextlinen("Cannot derive wrapping key (err -5");

	/* An app can only be loaded in firmware mode */
	ut_asserteq(1, run_command("tkey loadapp", 0));
	ut_assert_nextline(LOADING "...Invalid mode - replug device?",
			   (long)TKEY_SIGNER_SIZE);
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_tkey_loadapp, UTF_DM | UTF_SCAN_FDT | UTF_CONSOLE);

/* Test that a sub-command refuses an option */
static int cmd_test_tkey_opt(struct unit_test_state *uts)
{
	static const char * const cmds[] = {
		"tkey connect -x",
		"tkey getkey -x",
		"tkey loadapp -x",
		"tkey wrapkey -x",
	};
	int i;

	/* The command framework only looks for an option with this on */
	if (!IS_ENABLED(CONFIG_CMD_ARG_CHECK))
		return -EAGAIN;

	for (i = 0; i < ARRAY_SIZE(cmds); i++) {
		ut_asserteq(1, run_command(cmds[i], 0));
		ut_assert_nextlinen("tkey - ");
		ut_assert_skip_to_linen("tkey wrapkey");
		ut_assert_console_end();
	}

	return 0;
}
CMD_TEST(cmd_test_tkey_opt, UTF_CONSOLE);

/* Test the secret and the verification hash which 'tkey getkey' takes */
static int cmd_test_tkey_uss(struct unit_test_state *uts)
{
	ut_assertok(run_command("tkey connect tkey-emul", 0));
	ut_assert_nextline("Connected to TKey device");

	/* Both sub-commands taking a secret check its length */
	ut_asserteq(1, run_command("tkey getkey " LONG_USS, 0));
	ut_assert_nextline("USS too long (max 32 bytes, got 33)");
	ut_assert_console_end();

	ut_asserteq(1, run_command("tkey loadapp " LONG_USS, 0));
	ut_assert_nextline("USS too long (max 32 bytes, got 33)");
	ut_assert_console_end();

	/* A hash of the wrong length shows the usage message */
	ut_asserteq(1, run_command("tkey getkey testuss abcd", 0));
	ut_assert_nextline("Verification hash must be 64 hex chars");
	ut_assert_nextlinen("tkey - Tillitis TKey security token operations");
	ut_assert_skip_to_linen("tkey wrapkey");
	ut_assert_console_end();

	/* The hash the device produces confirms the secret */
	ut_assertok(run_command("tkey getkey testuss " VERIFY_HASH, 0));
	ut_assert_nextline("Public Key: " PUBKEY);
	ut_assert_nextline("Disk Key: " DISK_KEY);
	ut_assert_nextline_empty();
	ut_assert_nextline("password correct");
	ut_assert_console_end();

	/* Any other hash reports both the expected and the derived one */
	ut_asserteq(1, run_command("tkey getkey testuss " OTHER_HASH, 0));
	ut_assert_nextline("Public Key: " PUBKEY);
	ut_assert_nextline("Disk Key: " DISK_KEY);
	ut_assert_nextline_empty();
	ut_assert_nextline("wrong password");
	ut_assert_nextline("Expected: " OTHER_HASH);
	ut_assert_nextline("Got: " VERIFY_HASH);
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_tkey_uss, UTF_DM | UTF_SCAN_FDT | UTF_CONSOLE);
