// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the ext4write command
 *
 * These are manual tests, since they need a filesystem image. The Python side
 * makes one image per test, since each test writes to it, and passes its path
 * in; see test/py/tests/test_ext4write.py
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <env.h>
#include <mapmem.h>
#include <test/cmd.h>
#include <test/test.h>
#include <test/ut.h>

#define ARG_IMAGE	0	/* fs_image: path to the filesystem image */

/* where the tests build the data to write, away from anything else */
#define SRC_ADDR	(CONFIG_SYS_LOAD_ADDR + 0x1000)

/* where they read it back to, so the two can be compared */
#define LOAD_ADDR	(CONFIG_SYS_LOAD_ADDR + 0x2000)

/* size of the file the tests write, and the byte they fill it with */
#define FILE_SIZE	0x40
#define FILL_BYTE	0x5a

/* size of docs/old.txt, which the image already holds */
#define OLD_SIZE	9

/*
 * A write more than one journal transaction can hold on the 8MB filesystem
 * the tests use, so the write has to restart its journal handle part-way
 */
#define LARGE_SIZE	0x140000

/* a write larger than the whole filesystem, which runs out of space */
#define FULL_SIZE	0x800000

/* where the tests of large writes build their data and read it back */
#define BIG_SRC_ADDR	(CONFIG_SYS_LOAD_ADDR + 0x100000)
#define BIG_LOAD_ADDR	(CONFIG_SYS_LOAD_ADDR + 0x1000000)

/*
 * The name of the function which logs a failed write, right-aligned in
 * CONFIG_LOGF_FUNC_PAD (20) characters as the log emits it
 */
#define FUNC_WRITE	"            fs_write"

/**
 * check_log() - Check the next console line, which the fs layer logs
 *
 * The log format shows the function name only when CONFIG_LOGF_FUNC is
 * enabled. That is true of sandbox but of none of the other sandbox
 * variants, so the expected line differs between them.
 *
 * @uts: Test state
 * @msg: Rest of the line
 * Return: 0 if OK, 1 on failure
 */
static int check_log(struct unit_test_state *uts, const char *msg)
{
	if (IS_ENABLED(CONFIG_LOGF_FUNC))
		ut_assert_nextline("%s() %s", FUNC_WRITE, msg);
	else
		ut_assert_nextline("%s", msg);

	return 0;
}

/* the two lines of the usage message which are too long to write inline */
#define USAGE_ARGS \
	"ext4write <interface> <dev[:part]> <addr> <filename> <bytes> [<offset>]"
#define USAGE_HELP \
	"    - write 'bytes' bytes from the address 'addr' in RAM to file"

/**
 * check_usage() - Check the console holds the ext4write usage message
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("ext4write - write a file into an ext4 filesystem");
	ut_assert_nextline("%s", "");
	ut_assert_nextline("Usage:");
	ut_assert_nextline(USAGE_ARGS);
	ut_assert_nextline(USAGE_HELP);
	ut_assert_nextline("      'filename' on 'dev' on 'interface'");
	ut_assert_console_end();

	return 0;
}

/**
 * bind_image() - Attach the image which the Python side made
 *
 * @uts: Test state
 * Return: 0 if OK, other value on error
 */
static int bind_image(struct unit_test_state *uts)
{
	const char *fs_image = ut_str(ARG_IMAGE);

	ut_assertnonnull(fs_image);
	ut_assertok(run_commandf("host bind 0 %s", fs_image));
	console_record_reset();

	return 0;
}

/**
 * fill_mem() - Put a known pattern in memory for the command to write out
 *
 * @addr: Address to fill
 * @byte: Byte to fill it with
 * @size: Number of bytes
 * Return: pointer to the filled memory, which the caller unmaps
 */
static u8 *fill_mem(ulong addr, int byte, uint size)
{
	u8 *buf = map_sysmem(addr, size);

	memset(buf, byte, size);

	return buf;
}

/**
 * fill_pattern() - Put a pattern which differs between blocks in memory
 *
 * A single repeated byte cannot show a block written to the wrong place, so
 * mix the block number into each byte.
 *
 * @addr: Address to fill
 * @size: Number of bytes
 * Return: pointer to the filled memory, which the caller unmaps
 */
static u8 *fill_pattern(ulong addr, uint size)
{
	u8 *buf = map_sysmem(addr, size);
	uint i;

	for (i = 0; i < size; i++)
		buf[i] = i * 7 + (i >> 12);

	return buf;
}

/* Test that ext4write creates a file and reports what it wrote */
static int cmd_test_ext4write_base_norun(struct unit_test_state *uts)
{
	u8 *src, *dst;

	ut_assertok(bind_image(uts));
	src = fill_mem(SRC_ADDR, FILL_BYTE, FILE_SIZE);

	ut_assertok(run_commandf("ext4write host 0 %lx data.bin %x",
				 (ulong)SRC_ADDR, FILE_SIZE));
	ut_assert_nextlinen("64 bytes written in ");
	ut_assert_console_end();

	/* the file is there and is as long as the data written */
	ut_assertok(run_command("ext4size host 0 data.bin", 0));
	ut_assert_console_end();
	ut_asserteq(FILE_SIZE, env_get_hex("filesize", 0));

	/* and it holds what was in memory */
	dst = fill_mem(LOAD_ADDR, 0, FILE_SIZE);
	ut_assertok(run_commandf("ext4load host 0 %lx data.bin",
				 (ulong)LOAD_ADDR));
	ut_assert_nextlinen("64 bytes read in ");
	ut_assert_console_end();
	ut_asserteq_mem(src, dst, FILE_SIZE);

	unmap_sysmem(dst);
	unmap_sysmem(src);

	return 0;
}
CMD_TEST_ARGS(cmd_test_ext4write_base_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that writing at an offset leaves the rest of the file alone */
static int cmd_test_ext4write_offset_norun(struct unit_test_state *uts)
{
	u8 *src, *dst;
	int i;

	ut_assertok(bind_image(uts));
	src = fill_mem(SRC_ADDR, FILL_BYTE, FILE_SIZE);

	ut_assertok(run_commandf("ext4write host 0 %lx data.bin %x",
				 (ulong)SRC_ADDR, FILE_SIZE));
	ut_assert_nextlinen("64 bytes written in ");
	ut_assert_console_end();

	/* write 0x10 bytes over the last quarter of the file */
	memset(src, 0xff, 0x10);
	ut_assertok(run_commandf("ext4write host 0 %lx data.bin 10 30",
				 (ulong)SRC_ADDR));
	ut_assert_nextlinen("16 bytes written in ");
	ut_assert_console_end();

	/* the file is no longer, and only those bytes have changed */
	dst = fill_mem(LOAD_ADDR, 0, FILE_SIZE);
	ut_assertok(run_commandf("ext4load host 0 %lx data.bin",
				 (ulong)LOAD_ADDR));
	ut_assert_nextlinen("64 bytes read in ");
	ut_assert_console_end();
	ut_asserteq(FILE_SIZE, env_get_hex("filesize", 0));
	for (i = 0; i < 0x30; i++)
		ut_asserteq(FILL_BYTE, dst[i]);
	for (; i < FILE_SIZE; i++)
		ut_asserteq(0xff, dst[i]);

	unmap_sysmem(dst);
	unmap_sysmem(src);

	return 0;
}
CMD_TEST_ARGS(cmd_test_ext4write_offset_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that writing past the end of a file extends it, leaving a gap */
static int cmd_test_ext4write_extend_norun(struct unit_test_state *uts)
{
	u8 *src, *dst;
	int i;

	ut_assertok(bind_image(uts));
	src = fill_mem(SRC_ADDR, FILL_BYTE, FILE_SIZE);

	ut_assertok(run_commandf("ext4write host 0 %lx data.bin %x",
				 (ulong)SRC_ADDR, FILE_SIZE));
	ut_assert_nextlinen("64 bytes written in ");
	ut_assert_console_end();

	/* write 8 bytes at 0x60, which is 0x20 past the end */
	memset(src, 0xee, 8);
	ut_assertok(run_commandf("ext4write host 0 %lx data.bin 8 60",
				 (ulong)SRC_ADDR));
	ut_assert_nextlinen("8 bytes written in ");
	ut_assert_console_end();

	dst = fill_mem(LOAD_ADDR, 0, 0x68);
	ut_assertok(run_commandf("ext4load host 0 %lx data.bin",
				 (ulong)LOAD_ADDR));
	ut_assert_nextlinen("104 bytes read in ");
	ut_assert_console_end();
	ut_asserteq(0x68, env_get_hex("filesize", 0));

	/* the gap between the old end and the new data reads as zeroes */
	for (i = 0; i < FILE_SIZE; i++)
		ut_asserteq(FILL_BYTE, dst[i]);
	for (; i < 0x60; i++)
		ut_asserteq(0, dst[i]);
	for (; i < 0x68; i++)
		ut_asserteq(0xee, dst[i]);

	unmap_sysmem(dst);
	unmap_sysmem(src);

	return 0;
}
CMD_TEST_ARGS(cmd_test_ext4write_extend_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test a write too large for one journal transaction */
static int cmd_test_ext4write_large_norun(struct unit_test_state *uts)
{
	u8 *src, *dst;

	ut_assertok(bind_image(uts));
	src = fill_pattern(BIG_SRC_ADDR, LARGE_SIZE);

	ut_assertok(run_commandf("ext4write host 0 %lx big.bin %x",
				 (ulong)BIG_SRC_ADDR, LARGE_SIZE));
	ut_assert_nextlinen("%u bytes written in ", LARGE_SIZE);
	ut_assert_console_end();

	dst = fill_mem(BIG_LOAD_ADDR, 0, LARGE_SIZE);
	ut_assertok(run_commandf("ext4load host 0 %lx big.bin",
				 (ulong)BIG_LOAD_ADDR));
	ut_assert_nextlinen("%u bytes read in ", LARGE_SIZE);
	ut_assert_console_end();

	/* memcmp(), since a hex dump of a megabyte would bury the failure */
	ut_assertok(memcmp(src, dst, LARGE_SIZE));

	unmap_sysmem(dst);
	unmap_sysmem(src);

	return 0;
}
CMD_TEST_ARGS(cmd_test_ext4write_large_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that a write which runs out of space keeps what it wrote */
static int cmd_test_ext4write_full_norun(struct unit_test_state *uts)
{
	u8 *src, *dst;
	ulong size;

	ut_assertok(bind_image(uts));
	src = fill_pattern(BIG_SRC_ADDR, FULL_SIZE);

	ut_asserteq(1, run_commandf("ext4write host 0 %lx big.bin %x",
				    (ulong)BIG_SRC_ADDR, FULL_SIZE));
	ut_assertok(check_log(uts, "** Unable to write file big.bin **"));
	ut_assert_console_end();

	/* the file is as long as what fitted, which is most of the space */
	ut_assertok(run_command("ext4size host 0 big.bin", 0));
	ut_assert_console_end();
	size = env_get_hex("filesize", 0);
	ut_assert(size > LARGE_SIZE && size < FULL_SIZE);

	/* and holds the start of what was in memory */
	dst = fill_mem(BIG_LOAD_ADDR, 0, size);
	ut_assertok(run_commandf("ext4load host 0 %lx big.bin",
				 (ulong)BIG_LOAD_ADDR));
	ut_assert_nextlinen("%lu bytes read in ", size);
	ut_assert_console_end();
	ut_assertok(memcmp(src, dst, size));

	/* the file which was there already is untouched */
	ut_assertok(run_command("ext4size host 0 docs/old.txt", 0));
	ut_assert_console_end();
	ut_asserteq(OLD_SIZE, env_get_hex("filesize", 0));

	unmap_sysmem(dst);
	unmap_sysmem(src);

	return 0;
}
CMD_TEST_ARGS(cmd_test_ext4write_full_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that the filename reaches a subdirectory, with or without a slash */
static int cmd_test_ext4write_subdir_norun(struct unit_test_state *uts)
{
	u8 *src;

	ut_assertok(bind_image(uts));
	src = fill_mem(SRC_ADDR, 'A', FILE_SIZE);

	/* the path is relative to the root of the filesystem */
	ut_assertok(run_commandf("ext4write host 0 %lx docs/new.txt 20",
				 (ulong)SRC_ADDR));
	ut_assert_nextlinen("32 bytes written in ");
	ut_assert_console_end();

	/* so a leading slash makes no difference */
	ut_assertok(run_commandf("ext4write host 0 %lx /docs/new2.txt 10",
				 (ulong)SRC_ADDR));
	ut_assert_nextlinen("16 bytes written in ");
	ut_assert_console_end();

	ut_assertok(run_command("ext4size host 0 docs/new.txt", 0));
	ut_assert_console_end();
	ut_asserteq(0x20, env_get_hex("filesize", 0));

	ut_assertok(run_command("ext4size host 0 /docs/new2.txt", 0));
	ut_assert_console_end();
	ut_asserteq(0x10, env_get_hex("filesize", 0));

	unmap_sysmem(src);

	return 0;
}
CMD_TEST_ARGS(cmd_test_ext4write_subdir_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that a missing parent directory and a directory itself are refused */
static int cmd_test_ext4write_nodir_norun(struct unit_test_state *uts)
{
	u8 *src;

	ut_assertok(bind_image(uts));
	src = fill_mem(SRC_ADDR, FILL_BYTE, FILE_SIZE);

	/* ext4write creates the file but not the directory holding it */
	ut_asserteq(1, run_commandf("ext4write host 0 %lx nodir/data.bin 20",
				    (ulong)SRC_ADDR));
	ut_assertok(check_log(uts,
			      "** Unable to write file nodir/data.bin **"));
	ut_assert_console_end();

	/* a path which names a directory is refused, rather than written to */
	ut_asserteq(1, run_commandf("ext4write host 0 %lx docs 20",
				    (ulong)SRC_ADDR));
	ut_assertok(check_log(uts, "** Unable to write file docs **"));
	ut_assert_console_end();

	/* and the directory is still readable afterwards */
	ut_assertok(run_command("ext4size host 0 docs/old.txt", 0));
	ut_assert_console_end();
	ut_asserteq(OLD_SIZE, env_get_hex("filesize", 0));

	unmap_sysmem(src);

	return 0;
}
CMD_TEST_ARGS(cmd_test_ext4write_nodir_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that ext4write insists on a byte count */
static int cmd_test_ext4write_usage_norun(struct unit_test_state *uts)
{
	u8 *src;

	ut_assertok(bind_image(uts));
	src = fill_mem(SRC_ADDR, FILL_BYTE, FILE_SIZE);

	ut_asserteq(1, run_commandf("ext4write host 0 %lx data.bin",
				    (ulong)SRC_ADDR));
	ut_assertok(check_usage(uts));

	unmap_sysmem(src);

	return 0;
}
CMD_TEST_ARGS(cmd_test_ext4write_usage_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });
