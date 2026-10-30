// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the crc32 command
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
 * Address of the data the tests checksum. A board with no default load
 * address leaves CONFIG_SYS_LOAD_ADDR at 0 and qemu-arm-sbsa has no memory
 * below 4GB, so each test checks these against the start of memory.
 */
#define CRC_ADDR	((ulong)CONFIG_SYS_LOAD_ADDR + 0x7000)

/* Address the checksum is stored at by the third argument */
#define CRC_DEST	((ulong)CONFIG_SYS_LOAD_ADDR + 0x8000)

/* Number of bytes checksummed, all of them 'a' */
#define CRC_SIZE	4

/*
 * common/hash.c leaves the algorithm table out where crc32 is the only digest
 * the board wants, and the cut-down code it uses instead prints 'CRC32 for'
 * and stores the checksum as a long. This is multi_hash() from that file;
 * malta, malta64 and qemu-arm-sbsa are the test boards which build the small
 * version, so the tests which look at the output skip there.
 */
#define MULTI_HASH	(IS_ENABLED(CONFIG_SHA256) || \
			 IS_ENABLED(CONFIG_CMD_SHA1SUM) || \
			 IS_ENABLED(CONFIG_CRC32_VERIFY) || \
			 IS_ENABLED(CONFIG_CMD_HASH) || \
			 IS_ENABLED(CONFIG_SHA384) || \
			 IS_ENABLED(CONFIG_SHA512) || \
			 IS_ENABLED(CONFIG_CMD_MD5SUM) || \
			 IS_ENABLED(CONFIG_SM3))

/* CRC-32 of those four bytes, as a string and as the bytes which get stored */
static const char crc_sum[] = "ad98e545";
static const u8 crc_bytes[] = { 0xad, 0x98, 0xe5, 0x45 };

/* Write the data which the checksum above is taken over */
static void setup_data(void)
{
	void *buf;

	buf = map_sysmem(CRC_ADDR, CRC_SIZE);
	memset(buf, 'a', CRC_SIZE);
	unmap_sysmem(buf);
}

/* Test computing a checksum, which is all the command does on its own */
static int cmd_test_crc32_base(struct unit_test_state *uts)
{
	if (!MULTI_HASH || gd->ram_base > CRC_ADDR)
		return -EAGAIN;
	setup_data();

	ut_assertok(run_commandf("crc32 %lx %x", CRC_ADDR, CRC_SIZE));
	ut_assert_nextline("crc32 for %08lx ... %08lx ==> %s", CRC_ADDR,
			   CRC_ADDR + CRC_SIZE - 1, crc_sum);
	ut_assert_console_end();

	/*
	 * a count of zero checksums nothing, so the result is 0 and the last
	 * address printed is below the first
	 */
	ut_assertok(run_commandf("crc32 %lx 0", CRC_ADDR));
	ut_assert_nextline("crc32 for %08lx ... %08lx ==> 00000000", CRC_ADDR,
			   CRC_ADDR - 1);
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_crc32_base, UTF_CONSOLE);

/* Test storing the checksum, which the command only does to an address */
static int cmd_test_crc32_store(struct unit_test_state *uts)
{
	void *buf;

	if (!MULTI_HASH || gd->ram_base > CRC_ADDR)
		return -EAGAIN;
	setup_data();

	buf = map_sysmem(CRC_DEST, sizeof(crc_bytes));
	memset(buf, '\0', sizeof(crc_bytes));
	ut_assertok(run_commandf("crc32 %lx %x %lx", CRC_ADDR, CRC_SIZE,
				 CRC_DEST));
	ut_assert_nextline("crc32 for %08lx ... %08lx ==> %s", CRC_ADDR,
			   CRC_ADDR + CRC_SIZE - 1, crc_sum);

	/* the four bytes go in with the most significant one first */
	ut_asserteq_mem(crc_bytes, buf, sizeof(crc_bytes));
	unmap_sysmem(buf);
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_crc32_store, UTF_CONSOLE);

/* Test the three forms the -v argument can take */
static int cmd_test_crc32_verify(struct unit_test_state *uts)
{
	void *buf;

	if (!MULTI_HASH || gd->ram_base > CRC_ADDR)
		return -EAGAIN;
	setup_data();

	if (!IS_ENABLED(CONFIG_CRC32_VERIFY)) {
		/*
		 * without the option the command has no room for a fourth
		 * argument
		 */
		ut_asserteq(1, run_commandf("crc32 -v %lx %x %s", CRC_ADDR,
					    CRC_SIZE, crc_sum));
		ut_assert_nextline("crc32 - checksum calculation");

		/* the rest is the usage message, which has its own test */
		console_record_reset();

		return 0;
	}

	/* an immediate checksum of the right length is used as it stands */
	ut_assertok(run_commandf("crc32 -v %lx %x %s", CRC_ADDR, CRC_SIZE,
				 crc_sum));
	ut_assert_console_end();

	/* anything else of the wrong length is an environment variable */
	ut_assertok(env_set("crcvar", crc_sum));
	ut_assertok(run_commandf("crc32 -v %lx %x crcvar", CRC_ADDR,
				 CRC_SIZE));
	ut_assert_console_end();

	/* a '*' makes it an address holding the checksum */
	buf = map_sysmem(CRC_DEST, sizeof(crc_bytes));
	memcpy(buf, crc_bytes, sizeof(crc_bytes));
	unmap_sysmem(buf);
	ut_assertok(run_commandf("crc32 -v %lx %x *%lx", CRC_ADDR, CRC_SIZE,
				 CRC_DEST));
	ut_assert_console_end();

	/* a checksum which does not match shows both of them */
	ut_asserteq(1, run_commandf("crc32 -v %lx %x deadbeef", CRC_ADDR,
				    CRC_SIZE));
	ut_assert_nextline("crc32 for %08lx ... %08lx ==> %s != deadbeef ** ERROR **",
			   CRC_ADDR, CRC_ADDR + CRC_SIZE - 1, crc_sum);
	ut_assert_console_end();

	/* a variable whose value is the wrong length is rejected */
	ut_assertok(env_set("crcvar", "zz"));
	ut_asserteq(1, run_commandf("crc32 -v %lx %x crcvar", CRC_ADDR,
				    CRC_SIZE));
	ut_assert_nextline("Expected 8 hex digits in env var");
	ut_assert_nextline("ERROR: crcvar does not contain a valid crc32 sum");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_crc32_verify, UTF_CONSOLE);

/* Test the usage message, which shows both forms of the command */
static int cmd_test_crc32_usage(struct unit_test_state *uts)
{
	/* the address and the count are both required; neither is read */
	ut_asserteq(1, run_commandf("crc32 %lx", CRC_ADDR));
	ut_assert_nextline("crc32 - checksum calculation");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("crc32 address count [addr]");
	ut_assert_nextline("    - compute CRC32 checksum [save at addr]");
	if (IS_ENABLED(CONFIG_CRC32_VERIFY)) {
		ut_assert_nextline("crc32 -v address count crc");
		ut_assert_nextline("    - verify crc of memory area");
	}
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_crc32_usage, UTF_CONSOLE);
