// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Tests for hash command
 *
 * Copyright 2024, Heinrich Schuchardt <heinrich.schuchardt@canoncal.com>
 */

#include <command.h>
#include <console.h>
#include <env.h>
#include <errno.h>
#include <mapmem.h>
#include <linux/string.h>
#include <asm/global_data.h>
#include <test/cmd.h>
#include <test/test.h>
#include <test/ut.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * Address of the data the tests hash. A board with no default load address
 * leaves CONFIG_SYS_LOAD_ADDR at 0 and qemu-arm-sbsa has no memory below 4GB,
 * so each test which uses these checks them against the start of memory.
 */
#define HASH_ADDR	((ulong)CONFIG_SYS_LOAD_ADDR + 0x7000)

/* Address the digest is stored at by the '*address' form */
#define HASH_DEST	((ulong)CONFIG_SYS_LOAD_ADDR + 0x8000)

/* Number of bytes hashed, all of them 'a' */
#define HASH_SIZE	4

/* MD5 of those four bytes, as a string and as the bytes which get stored */
static const char md5_sum[] = "74b87337454200d4d33f80c4663dc5e5";
static const u8 md5_bytes[] = {
	0x74, 0xb8, 0x73, 0x37, 0x45, 0x42, 0x00, 0xd4,
	0xd3, 0x3f, 0x80, 0xc4, 0x66, 0x3d, 0xc5, 0xe5
};

/* MD5 of no bytes at all, which is what a count of zero gives */
static const char md5_empty[] = "d41d8cd98f00b204e9800998ecf8427e";

/* Write the data which the digests above are taken over */
static void setup_data(void)
{
	void *buf;

	buf = map_sysmem(HASH_ADDR, HASH_SIZE);
	memset(buf, 'a', HASH_SIZE);
	unmap_sysmem(buf);
}

/* Test computing a digest, which is all the command does on its own */
static int cmd_test_hash_base(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_MD5) || gd->ram_base > HASH_ADDR)
		return -EAGAIN;
	setup_data();

	ut_assertok(run_commandf("hash md5 %lx %x", HASH_ADDR, HASH_SIZE));
	ut_assert_nextline("md5 for %08lx ... %08lx ==> %s", HASH_ADDR,
			   HASH_ADDR + HASH_SIZE - 1, md5_sum);
	ut_assert_console_end();

	/* the algorithm name is matched in lower case */
	ut_assertok(run_commandf("hash MD5 %lx %x", HASH_ADDR, HASH_SIZE));
	ut_assert_nextline("md5 for %08lx ... %08lx ==> %s", HASH_ADDR,
			   HASH_ADDR + HASH_SIZE - 1, md5_sum);
	ut_assert_console_end();

	/*
	 * a count of zero hashes nothing, so the last address printed is
	 * below the first
	 */
	ut_assertok(run_commandf("hash md5 %lx 0", HASH_ADDR));
	ut_assert_nextline("md5 for %08lx ... %08lx ==> %s", HASH_ADDR,
			   HASH_ADDR - 1, md5_empty);
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_hash_base, UTF_CONSOLE);

/* Test saving the digest to an environment variable and to memory */
static int cmd_test_hash_save(struct unit_test_state *uts)
{
	void *buf;

	if (!IS_ENABLED(CONFIG_MD5) || gd->ram_base > HASH_ADDR)
		return -EAGAIN;
	setup_data();

	/* a destination without a '*' is an environment variable */
	ut_assertok(run_commandf("hash md5 %lx %x hashvar", HASH_ADDR,
				 HASH_SIZE));
	ut_assert_nextline("md5 for %08lx ... %08lx ==> %s", HASH_ADDR,
			   HASH_ADDR + HASH_SIZE - 1, md5_sum);
	ut_asserteq_str(md5_sum, env_get("hashvar"));
	ut_assert_console_end();

	/* with a '*' it is an address, and the raw digest bytes go there */
	buf = map_sysmem(HASH_DEST, sizeof(md5_bytes));
	memset(buf, '\0', sizeof(md5_bytes));
	ut_assertok(run_commandf("hash md5 %lx %x *%lx", HASH_ADDR, HASH_SIZE,
				 HASH_DEST));
	ut_assert_nextline("md5 for %08lx ... %08lx ==> %s", HASH_ADDR,
			   HASH_ADDR + HASH_SIZE - 1, md5_sum);
	ut_asserteq_mem(md5_bytes, buf, sizeof(md5_bytes));
	unmap_sysmem(buf);
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_hash_save, UTF_CONSOLE);

/* Test the three forms the -v argument can take */
static int cmd_test_hash_verify(struct unit_test_state *uts)
{
	void *buf;

	if (!IS_ENABLED(CONFIG_MD5) || gd->ram_base > HASH_ADDR)
		return -EAGAIN;
	setup_data();

	if (!IS_ENABLED(CONFIG_HASH_VERIFY)) {
		/* without the option getopt() refuses it */
		ut_asserteq(1, run_commandf("hash -v md5 %lx %x %s", HASH_ADDR,
					    HASH_SIZE, md5_sum));
		ut_assert_nextline("hash - compute hash message digest");

		/* the rest is the usage message, which has its own test */
		console_record_reset();

		return 0;
	}

	/* an immediate digest of the right length is used as it stands */
	ut_assertok(run_commandf("hash -v md5 %lx %x %s", HASH_ADDR, HASH_SIZE,
				 md5_sum));
	ut_assert_console_end();

	/* anything else of the wrong length is an environment variable */
	ut_assertok(env_set("hashvar", md5_sum));
	ut_assertok(run_commandf("hash -v md5 %lx %x hashvar", HASH_ADDR,
				 HASH_SIZE));
	ut_assert_console_end();

	/* a '*' makes it an address holding the digest */
	buf = map_sysmem(HASH_DEST, sizeof(md5_bytes));
	memcpy(buf, md5_bytes, sizeof(md5_bytes));
	unmap_sysmem(buf);
	ut_assertok(run_commandf("hash -v md5 %lx %x *%lx", HASH_ADDR,
				 HASH_SIZE, HASH_DEST));
	ut_assert_console_end();

	/* a digest which does not match shows both of them */
	ut_asserteq(1, run_commandf("hash -v md5 %lx %x %s", HASH_ADDR,
				    HASH_SIZE, md5_empty));
	ut_assert_nextline("md5 for %08lx ... %08lx ==> %s != %s ** ERROR **",
			   HASH_ADDR, HASH_ADDR + HASH_SIZE - 1, md5_sum,
			   md5_empty);
	ut_assert_console_end();

	/* a variable holding something which is not a digest is rejected */
	ut_assertok(env_set("hashvar", "nonsense"));
	ut_asserteq(1, run_commandf("hash -v md5 %lx %x hashvar", HASH_ADDR,
				    HASH_SIZE));
	ut_assert_nextline("Expected 32 hex digits in env var");
	ut_assert_nextline("ERROR: hashvar does not contain a valid md5 sum");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_hash_verify, UTF_CONSOLE);

/* Test the error paths, each of which shows the usage message */
static int cmd_test_hash_err(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_MD5) || gd->ram_base > HASH_ADDR)
		return -EAGAIN;

	/* the algorithm, the address and the count are all required */
	ut_asserteq(1, run_commandf("hash md5 %lx", HASH_ADDR));
	ut_assert_nextline("hash - compute hash message digest");
	console_record_reset();

	/* an algorithm the build does not have is named in the error */
	ut_asserteq(1, run_commandf("hash sha999 %lx %x", HASH_ADDR,
				    HASH_SIZE));
	ut_assert_nextline("Unknown hash algorithm 'sha999'");
	ut_assert_nextline("hash - compute hash message digest");
	console_record_reset();

	/* -v is the only option there is */
	ut_asserteq(1, run_commandf("hash -z md5 %lx %x", HASH_ADDR,
				    HASH_SIZE));
	ut_assert_nextline("hash - compute hash message digest");
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_hash_err, UTF_CONSOLE);

static int cmd_test_hash_md5(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_MD5)) {
		ut_assert(run_command("hash md5 $loadaddr 0", 0));

		return 0;
	}

	ut_assertok(run_command("hash md5 $loadaddr 0", 0));
	console_record_readline(uts->actual_str, sizeof(uts->actual_str));
	ut_asserteq_ptr(uts->actual_str,
			strstr(uts->actual_str, "md5 for "));
	ut_assert(strstr(uts->actual_str,
			 "d41d8cd98f00b204e9800998ecf8427e"));
	ut_assert_console_end();

	ut_assertok(run_command("hash md5 $loadaddr 0 foo; echo $foo", 0));
	console_record_readline(uts->actual_str, sizeof(uts->actual_str));
	ut_asserteq_ptr(uts->actual_str,
			strstr(uts->actual_str, "md5 for "));
	ut_assert(strstr(uts->actual_str,
			 "d41d8cd98f00b204e9800998ecf8427e"));
	ut_assertok(ut_check_console_line(uts,
					  "d41d8cd98f00b204e9800998ecf8427e"));

	if (!IS_ENABLED(CONFIG_HASH_VERIFY)) {
		ut_assert(run_command("hash -v md5 $loadaddr 0 foo", 0));
		ut_assertok(ut_check_console_line(
				uts, "hash - compute hash message digest"));

		return 0;
	}

	ut_assertok(run_command("hash -v md5 $loadaddr 0 foo", 0));
	ut_assert_console_end();

	env_set("foo", "ffffffffffffffffffffffffffffffff");
	ut_assert(run_command("hash -v md5 $loadaddr 0 foo", 0));
	console_record_readline(uts->actual_str, sizeof(uts->actual_str));
	ut_assert(strstr(uts->actual_str, "!="));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_hash_md5, UTF_CONSOLE);

static int cmd_test_hash_sha256(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_SHA256)) {
		ut_assert(run_command("hash sha256 $loadaddr 0", 0));

		return 0;
	}

	ut_assertok(run_command("hash sha256 $loadaddr 0", 0));
	console_record_readline(uts->actual_str, sizeof(uts->actual_str));
	ut_asserteq_ptr(uts->actual_str,
			strstr(uts->actual_str, "sha256 for "));
	ut_assert(strstr(uts->actual_str,
			 "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
	ut_assert_console_end();

	ut_assertok(run_command("hash sha256 $loadaddr 0 foo; echo $foo", 0));
	console_record_readline(uts->actual_str, sizeof(uts->actual_str));
	ut_asserteq_ptr(uts->actual_str,
			strstr(uts->actual_str, "sha256 for "));
	ut_assert(strstr(uts->actual_str,
			 "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
	ut_assertok(ut_check_console_line(
			uts, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));

	if (!IS_ENABLED(CONFIG_HASH_VERIFY)) {
		ut_assert(run_command("hash -v sha256 $loadaddr 0 foo", 0));
		ut_assertok(ut_check_console_line(
				uts, "hash - compute hash message digest"));

		return 0;
	}

	ut_assertok(run_command("hash -v sha256 $loadaddr 0 foo", 0));
	ut_assert_console_end();

	env_set("foo", "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
	ut_assert(run_command("hash -v sha256 $loadaddr 0 foo", 0));
	console_record_readline(uts->actual_str, sizeof(uts->actual_str));
	ut_assert(strstr(uts->actual_str, "!="));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_hash_sha256, UTF_CONSOLE);

static int cmd_test_hash_sm3_256(struct unit_test_state *uts)
{
	const char *sum = "1ab21d8355cfa17f8e61194831e81a8f22bec8c728fefb747ed035eb5082aa2b";

	if (!IS_ENABLED(CONFIG_SM3)) {
		ut_assert(run_command("hash sm3_256 $loadaddr 0", 0));

		return 0;
	}

	ut_assertok(run_command("hash sm3_256 $loadaddr 0", 0));
	console_record_readline(uts->actual_str, sizeof(uts->actual_str));
	ut_asserteq_ptr(uts->actual_str,
			strstr(uts->actual_str, "sm3_256 for "));
	ut_assert(strstr(uts->actual_str, sum));
	ut_assert_console_end();

	ut_assertok(run_command("hash sm3_256 $loadaddr 0 foo; echo $foo", 0));
	console_record_readline(uts->actual_str, sizeof(uts->actual_str));
	ut_asserteq_ptr(uts->actual_str,
			strstr(uts->actual_str, "sm3_256 for "));
	ut_assert(strstr(uts->actual_str, sum));
	ut_assertok(ut_check_console_line(uts, sum));

	if (!IS_ENABLED(CONFIG_HASH_VERIFY)) {
		ut_assert(run_command("hash -v sm3_256 $loadaddr 0 foo", 0));
		ut_assertok(ut_check_console_line(uts,
						  "hash - compute hash message digest"));

		return 0;
	}

	ut_assertok(run_command("hash -v sm3_256 $loadaddr 0 foo", 0));
	ut_assert_console_end();

	env_set("foo",
		"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
	ut_assert(run_command("hash -v sm3_256 $loadaddr 0 foo", 0));
	console_record_readline(uts->actual_str, sizeof(uts->actual_str));
	ut_assert(strstr(uts->actual_str, "!="));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_hash_sm3_256, UTF_CONSOLE);
