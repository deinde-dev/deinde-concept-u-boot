// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the md5sum command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <env.h>
#include <errno.h>
#include <mapmem.h>
#include <linux/string.h>
#include <asm/global_data.h>
#include <test/cmd.h>
#include <test/ut.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * Address of the data the tests hash. A board with no default load address
 * leaves CONFIG_SYS_LOAD_ADDR at 0 and qemu-arm-sbsa has no memory below 4GB,
 * so each test checks these against the start of memory.
 */
#define MD5_ADDR	((ulong)CONFIG_SYS_LOAD_ADDR + 0x7000)

/* Address the digest is stored at by the '*address' form */
#define MD5_DEST	((ulong)CONFIG_SYS_LOAD_ADDR + 0x8000)

/* Number of bytes hashed, all of them 'a' */
#define MD5_SIZE	4

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

	buf = map_sysmem(MD5_ADDR, MD5_SIZE);
	memset(buf, 'a', MD5_SIZE);
	unmap_sysmem(buf);
}

/* Test computing a digest, which names the algorithm and not the command */
static int cmd_test_md5sum_base(struct unit_test_state *uts)
{
	if (gd->ram_base > MD5_ADDR)
		return -EAGAIN;
	setup_data();

	ut_assertok(run_commandf("md5sum %lx %x", MD5_ADDR, MD5_SIZE));
	ut_assert_nextline("md5 for %08lx ... %08lx ==> %s", MD5_ADDR,
			   MD5_ADDR + MD5_SIZE - 1, md5_sum);
	ut_assert_console_end();

	/*
	 * a count of zero hashes nothing, so the last address printed is
	 * below the first
	 */
	ut_assertok(run_commandf("md5sum %lx 0", MD5_ADDR));
	ut_assert_nextline("md5 for %08lx ... %08lx ==> %s", MD5_ADDR,
			   MD5_ADDR - 1, md5_empty);
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_md5sum_base, UTF_CONSOLE);

/* Test saving the digest to an environment variable and to memory */
static int cmd_test_md5sum_save(struct unit_test_state *uts)
{
	void *buf;

	if (gd->ram_base > MD5_ADDR)
		return -EAGAIN;
	setup_data();

	/* a destination without a '*' is an environment variable */
	ut_assertok(run_commandf("md5sum %lx %x md5var", MD5_ADDR, MD5_SIZE));
	ut_assert_nextline("md5 for %08lx ... %08lx ==> %s", MD5_ADDR,
			   MD5_ADDR + MD5_SIZE - 1, md5_sum);
	ut_asserteq_str(md5_sum, env_get("md5var"));
	ut_assert_console_end();

	/* with a '*' it is an address, and the raw digest bytes go there */
	buf = map_sysmem(MD5_DEST, sizeof(md5_bytes));
	memset(buf, '\0', sizeof(md5_bytes));
	ut_assertok(run_commandf("md5sum %lx %x *%lx", MD5_ADDR, MD5_SIZE,
				 MD5_DEST));
	ut_assert_nextline("md5 for %08lx ... %08lx ==> %s", MD5_ADDR,
			   MD5_ADDR + MD5_SIZE - 1, md5_sum);
	ut_asserteq_mem(md5_bytes, buf, sizeof(md5_bytes));
	unmap_sysmem(buf);
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_md5sum_save, UTF_CONSOLE);

/* Test the three forms the -v argument can take */
static int cmd_test_md5sum_verify(struct unit_test_state *uts)
{
	void *buf;

	if (gd->ram_base > MD5_ADDR)
		return -EAGAIN;
	setup_data();

	if (!IS_ENABLED(CONFIG_MD5SUM_VERIFY)) {
		/*
		 * without the option the command has no room for a fourth
		 * argument
		 */
		ut_asserteq(1, run_commandf("md5sum -v %lx %x %s", MD5_ADDR,
					    MD5_SIZE, md5_sum));
		ut_assert_nextline("md5sum - compute MD5 message digest");

		/* the rest is the usage message, which has its own test */
		console_record_reset();

		return 0;
	}

	/* an immediate digest of the right length is used as it stands */
	ut_assertok(run_commandf("md5sum -v %lx %x %s", MD5_ADDR, MD5_SIZE,
				 md5_sum));
	ut_assert_console_end();

	/* anything else of the wrong length is an environment variable */
	ut_assertok(env_set("md5var", md5_sum));
	ut_assertok(run_commandf("md5sum -v %lx %x md5var", MD5_ADDR,
				 MD5_SIZE));
	ut_assert_console_end();

	/* a '*' makes it an address holding the digest */
	buf = map_sysmem(MD5_DEST, sizeof(md5_bytes));
	memcpy(buf, md5_bytes, sizeof(md5_bytes));
	unmap_sysmem(buf);
	ut_assertok(run_commandf("md5sum -v %lx %x *%lx", MD5_ADDR, MD5_SIZE,
				 MD5_DEST));
	ut_assert_console_end();

	/* a digest which does not match shows both of them */
	ut_asserteq(1, run_commandf("md5sum -v %lx %x %s", MD5_ADDR, MD5_SIZE,
				    md5_empty));
	ut_assert_nextline("md5 for %08lx ... %08lx ==> %s != %s ** ERROR **",
			   MD5_ADDR, MD5_ADDR + MD5_SIZE - 1, md5_sum,
			   md5_empty);
	ut_assert_console_end();

	/* a variable whose value is the wrong length is rejected */
	ut_assertok(env_set("md5var", "zz"));
	ut_asserteq(1, run_commandf("md5sum -v %lx %x md5var", MD5_ADDR,
				    MD5_SIZE));
	ut_assert_nextline("Expected 32 hex digits in env var");
	ut_assert_nextline("ERROR: md5var does not contain a valid md5 sum");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_md5sum_verify, UTF_CONSOLE);

/* Test the usage message, which shows the -v form only where it exists */
static int cmd_test_md5sum_usage(struct unit_test_state *uts)
{
	/* the address and the count are both required; neither is read */
	ut_asserteq(1, run_commandf("md5sum %lx", MD5_ADDR));
	ut_assert_nextline("md5sum - compute MD5 message digest");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("md5sum address count [[*]sum]");
	ut_assert_nextline("    - compute MD5 message digest [save to sum]");
	if (IS_ENABLED(CONFIG_MD5SUM_VERIFY)) {
		ut_assert_nextline("md5sum -v address count [*]sum");
		ut_assert_nextline("    - verify md5sum of memory area");
	}
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_md5sum_usage, UTF_CONSOLE);
