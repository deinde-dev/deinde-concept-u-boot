// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the load, save and size commands
 *
 * These commands work with any filesystem, so the tests use sandbox's own
 * 'sandbox' filesystem, which reads and writes files of the host U-Boot is
 * running on. That needs no filesystem image and no block device.
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <env.h>
#include <malloc.h>
#include <mapmem.h>
#include <os.h>
#include <test/cmd.h>
#include <test/test.h>
#include <test/ut.h>

/*
 * Name of the file the tests read and write. It sits in the directory U-Boot
 * is started from and begins with a dot, so that git ignores it if a test
 * stops before removing it.
 */
#define TEST_FILE	".cmd_test_fs"

/* contents of the file, so that a test can tell one byte of it from another */
#define DATA		"0123456789abcdef"
#define DATA_SIZE	0x10

/* where the tests load the file to, away from anything else */
#define LOAD_ADDR	(CONFIG_SYS_LOAD_ADDR + 0x14000)

/* a second address, so a test can tell one load from another */
#define ALT_ADDR	(CONFIG_SYS_LOAD_ADDR + 0x18000)

/*
 * The names of the functions which log a failure, right-aligned in
 * CONFIG_LOGF_FUNC_PAD (20) characters as the log emits them
 */
#define FUNC_LOAD	"             do_load"
#define FUNC_WRITE	"            fs_write"

/**
 * check_log() - Check the next console line, which the fs layer logs
 *
 * The log format shows the function name only when CONFIG_LOGF_FUNC is
 * enabled, so the expected line differs between the sandbox variants.
 *
 * @uts: Test state
 * @func: Name of the function which logs the message, padded to the log's
 *	function width
 * @msg: Rest of the line
 * Return: 0 if OK, 1 on failure
 */
static int check_log(struct unit_test_state *uts, const char *func,
		     const char *msg)
{
	if (IS_ENABLED(CONFIG_LOGF_FUNC))
		ut_assert_nextline("%s() %s", func, msg);
	else
		ut_assert_nextline("%s", msg);

	return 0;
}

/**
 * make_file() - Write the file which the tests work on
 *
 * @uts: Test state
 * @data: Contents for the file
 * @size: Number of bytes to write
 * Return: 0 if OK, 1 on failure
 */
static int make_file(struct unit_test_state *uts, const char *data, uint size)
{
	int fd;

	fd = os_open(TEST_FILE, OS_O_WRONLY | OS_O_CREAT | OS_O_TRUNC);
	ut_assert(fd >= 0);
	ut_asserteq(size, os_write(fd, data, size));
	ut_assertok(os_close(fd));

	return 0;
}

/**
 * read_file() - Read back the file which the tests write
 *
 * @uts: Test state
 * @buf: Buffer to hold the contents
 * @size: Number of bytes to read
 * Return: 0 if OK, 1 on failure
 */
static int read_file(struct unit_test_state *uts, char *buf, uint size)
{
	int fd;

	fd = os_open(TEST_FILE, OS_O_RDONLY);
	ut_assert(fd >= 0);
	ut_asserteq(size, os_read(fd, buf, size));
	ut_assertok(os_close(fd));

	return 0;
}

/**
 * fill_mem() - Put data in memory for a save to write out
 *
 * @addr: Address to write to
 * @data: Data to write
 * @size: Number of bytes
 */
static void fill_mem(ulong addr, const char *data, uint size)
{
	void *buf = map_sysmem(addr, size);

	memcpy(buf, data, size);
	unmap_sysmem(buf);
}

/**
 * clear_mem() - Clear the memory a load reads into, so it can be checked
 *
 * @addr: Address to clear
 * @size: Number of bytes
 * Return: pointer to the cleared memory, which the caller unmaps
 */
static u8 *clear_mem(ulong addr, uint size)
{
	u8 *buf = map_sysmem(addr, size);

	memset(buf, 0, size);

	return buf;
}

/**
 * save_env() - Take a copy of an environment variable
 *
 * @uts: Test state
 * @name: Name of the variable to save
 * @savedp: Returns a copy of the value, or NULL if it is unset
 * Return: 0 if OK, 1 on failure
 */
static int save_env(struct unit_test_state *uts, const char *name,
		    char **savedp)
{
	const char *val = env_get(name);

	*savedp = NULL;
	if (val) {
		*savedp = strdup(val);
		ut_assertnonnull(*savedp);
	}

	return 0;
}

/* the lines of the load usage message which are too long to write inline */
#define LOAD_USAGE_ARGS \
	"load <interface> [<dev[:part]> [<addr> [<filename> [bytes [pos]]]]]"
#define LOAD_USAGE_PART \
	"    - Load binary file 'filename' from partition 'part' on device"

/**
 * check_load_usage() - Check the console holds the load usage message
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_load_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("load - load binary file from a filesystem");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline(LOAD_USAGE_ARGS);
	ut_assert_nextline(LOAD_USAGE_PART);
	ut_assert_nextline("       type 'interface' instance 'dev' to address 'addr' in memory.");
	ut_assert_nextline("      'bytes' gives the size to load in bytes.");
	ut_assert_nextline("      If 'bytes' is 0 or omitted, the file is read until the end.");
	ut_assert_nextline("      'pos' gives the file byte position to start reading from.");
	ut_assert_nextline("      If 'pos' is 0 or omitted, the file is read from the start.");
	ut_assert_console_end();

	return 0;
}

/* Test that load reads a file and reports what it read */
static int cmd_test_load_base(struct unit_test_state *uts)
{
	u8 *buf;

	ut_assertok(make_file(uts, DATA, DATA_SIZE));
	buf = clear_mem(LOAD_ADDR, DATA_SIZE);

	ut_assertok(run_commandf("load sandbox - %lx %s", (ulong)LOAD_ADDR,
				 TEST_FILE));
	ut_assert_nextlinen("16 bytes read in ");
	ut_assert_console_end();

	/* the file arrives in memory, and both variables describe the load */
	ut_asserteq_mem(DATA, buf, DATA_SIZE);
	ut_asserteq(DATA_SIZE, env_get_hex("filesize", 0));
	ut_asserteq(LOAD_ADDR, env_get_hex("fileaddr", 0));

	unmap_sysmem(buf);
	ut_assertok(os_unlink(TEST_FILE));

	return 0;
}
CMD_TEST(cmd_test_load_base, UTF_CONSOLE);

/* Test that 'bytes' limits a load and 'pos' moves where it starts */
static int cmd_test_load_part(struct unit_test_state *uts)
{
	u8 *buf;

	ut_assertok(make_file(uts, DATA, DATA_SIZE));
	buf = clear_mem(LOAD_ADDR, DATA_SIZE);

	/* the first five bytes are '01234' */
	ut_assertok(run_commandf("load sandbox - %lx %s 5", (ulong)LOAD_ADDR,
				 TEST_FILE));
	ut_assert_nextlinen("5 bytes read in ");
	ut_assert_console_end();
	ut_asserteq_mem("01234", buf, 5);
	ut_asserteq(5, env_get_hex("filesize", 0));

	/* nothing beyond them is written */
	ut_asserteq(0, buf[5]);

	/* asking for more than there is stops at the end of the file */
	ut_assertok(run_commandf("load sandbox - %lx %s 100", (ulong)LOAD_ADDR,
				 TEST_FILE));
	ut_assert_nextlinen("16 bytes read in ");
	ut_assert_console_end();
	ut_asserteq_mem(DATA, buf, DATA_SIZE);

	/* 'pos' starts the read part-way through the file */
	ut_assertok(run_commandf("load sandbox - %lx %s 5 7", (ulong)LOAD_ADDR,
				 TEST_FILE));
	ut_assert_nextlinen("5 bytes read in ");
	ut_assert_console_end();
	ut_asserteq_mem("789ab", buf, 5);

	/* a 'bytes' of 0 reads the rest of the file from 'pos' */
	ut_assertok(run_commandf("load sandbox - %lx %s 0 c", (ulong)LOAD_ADDR,
				 TEST_FILE));
	ut_assert_nextlinen("4 bytes read in ");
	ut_assert_console_end();
	ut_asserteq_mem("cdef", buf, 4);

	/* a 'pos' past the end of the file reads nothing, which is not an
	 * error
	 */
	ut_assertok(run_commandf("load sandbox - %lx %s 5 20", (ulong)LOAD_ADDR,
				 TEST_FILE));
	ut_assert_nextlinen("0 bytes read in ");
	ut_assert_console_end();
	ut_asserteq(0, env_get_hex("filesize", 1));

	unmap_sysmem(buf);
	ut_assertok(os_unlink(TEST_FILE));

	return 0;
}
CMD_TEST(cmd_test_load_part, UTF_CONSOLE);

/* Test that load falls back on the loadaddr and bootfile variables */
static int cmd_test_load_default(struct unit_test_state *uts)
{
	char *old_bootfile, *old_loadaddr;
	u8 *buf;

	ut_assertok(make_file(uts, DATA, DATA_SIZE));
	buf = clear_mem(ALT_ADDR, DATA_SIZE);

	/* other tests use these, so put them back afterwards */
	ut_assertok(save_env(uts, "bootfile", &old_bootfile));
	ut_assertok(save_env(uts, "loadaddr", &old_loadaddr));

	ut_assertok(env_set("bootfile", NULL));
	ut_asserteq(1, run_commandf("load sandbox - %lx", (ulong)LOAD_ADDR));
	ut_assert_nextline("** No boot file defined **");
	ut_assert_console_end();

	/* with both set, neither need be given on the command line */
	ut_assertok(env_set("bootfile", TEST_FILE));
	ut_assertok(env_set_hex("loadaddr", ALT_ADDR));
	ut_assertok(run_command("load sandbox -", 0));
	ut_assert_nextlinen("16 bytes read in ");
	ut_assert_console_end();
	ut_asserteq_mem(DATA, buf, DATA_SIZE);
	ut_asserteq(ALT_ADDR, env_get_hex("fileaddr", 0));

	ut_assertok(env_set("bootfile", old_bootfile));
	ut_assertok(env_set("loadaddr", old_loadaddr));
	free(old_bootfile);
	free(old_loadaddr);
	unmap_sysmem(buf);
	ut_assertok(os_unlink(TEST_FILE));

	return 0;
}
CMD_TEST(cmd_test_load_default, UTF_CONSOLE);

/* Test that a failed load leaves filesize and fileaddr alone */
static int cmd_test_load_missing(struct unit_test_state *uts)
{
	char *old_filesize, *old_fileaddr;

	ut_assertok(save_env(uts, "filesize", &old_filesize));
	ut_assertok(save_env(uts, "fileaddr", &old_fileaddr));

	ut_assertok(env_set("filesize", "sentinel"));
	ut_assertok(env_set("fileaddr", "sentinel"));

	ut_asserteq(1, run_commandf("load sandbox - %lx %s", (ulong)LOAD_ADDR,
				    TEST_FILE));
	ut_assertok(check_log(uts, FUNC_LOAD,
			      "Failed to load '" TEST_FILE "'"));
	ut_assert_console_end();

	ut_asserteq_str("sentinel", env_get("filesize"));
	ut_asserteq_str("sentinel", env_get("fileaddr"));

	ut_assertok(env_set("filesize", old_filesize));
	ut_assertok(env_set("fileaddr", old_fileaddr));
	free(old_filesize);
	free(old_fileaddr);

	return 0;
}
CMD_TEST(cmd_test_load_missing, UTF_CONSOLE);

/* Test that load shows its usage message when it cannot go on */
static int cmd_test_load_usage(struct unit_test_state *uts)
{
	/* the interface is the one argument which has no default */
	ut_asserteq(1, run_command("load", 0));
	ut_assertok(check_load_usage(uts));

	/* an address which is not a number is refused, not taken as 0 */
	ut_asserteq(1, run_commandf("load sandbox - zz %s", TEST_FILE));
	ut_assertok(check_load_usage(uts));

	/* and there is nothing to give beyond 'pos' */
	ut_asserteq(1, run_commandf("load sandbox - %lx %s 5 7 1",
				    (ulong)LOAD_ADDR, TEST_FILE));
	ut_assertok(check_load_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_load_usage, UTF_CONSOLE);

/* Test that load refuses an option, since it has none */
static int cmd_test_load_badopt(struct unit_test_state *uts)
{
	ut_assertok(make_file(uts, DATA, DATA_SIZE));

	ut_asserteq(1, run_commandf("load -b sandbox - %lx %s",
				    (ulong)LOAD_ADDR, TEST_FILE));
	ut_assertok(check_load_usage(uts));

	/* a filename which looks like an option is still a filename */
	ut_asserteq(1, run_commandf("load sandbox - %lx -b", (ulong)LOAD_ADDR));
	ut_assertok(check_log(uts, FUNC_LOAD, "Failed to load '-b'"));
	ut_assert_console_end();

	ut_assertok(os_unlink(TEST_FILE));

	return 0;
}
CMD_TEST(cmd_test_load_badopt, UTF_CONSOLE);

/* a path through a directory which is not there, so a save fails */
#define BAD_FILE	".cmd_test_fs_nodir/f"

/* the lines of the save usage message which are too long to write inline */
#define SAVE_USAGE_ARGS \
	"save <interface> <dev[:part]> <addr> <filename> bytes [pos]"
#define SAVE_USAGE_PART \
	"    - Save binary file 'filename' to partition 'part' on device"

/**
 * check_save_usage() - Check the console holds the save usage message
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_save_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("save - save file to a filesystem");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline(SAVE_USAGE_ARGS);
	ut_assert_nextline(SAVE_USAGE_PART);
	ut_assert_nextline("      type 'interface' instance 'dev' from addr 'addr' in memory.");
	ut_assert_nextline("      'bytes' gives the size to save in bytes and is mandatory.");
	ut_assert_nextline("      'pos' gives the file byte position to start writing to.");
	ut_assert_nextline("      If 'pos' is 0 or omitted, the file is written from the start.");
	ut_assert_console_end();

	return 0;
}

/* Test that save writes memory to a file and reports what it wrote */
static int cmd_test_save_base(struct unit_test_state *uts)
{
	char buf[DATA_SIZE];
	long long size;

	fill_mem(LOAD_ADDR, DATA, DATA_SIZE);

	ut_assertok(run_commandf("save sandbox - %lx %s %x", (ulong)LOAD_ADDR,
				 TEST_FILE, DATA_SIZE));
	ut_assert_nextlinen("16 bytes written in ");
	ut_assert_console_end();

	/* the file holds what was in memory and nothing more */
	ut_assertok(os_get_filesize(TEST_FILE, &size));
	ut_asserteq(DATA_SIZE, size);
	ut_assertok(read_file(uts, buf, DATA_SIZE));
	ut_asserteq_mem(DATA, buf, DATA_SIZE);

	ut_assertok(os_unlink(TEST_FILE));

	return 0;
}
CMD_TEST(cmd_test_save_base, UTF_CONSOLE);

/* Test that 'pos' writes part-way into a file which is already there */
static int cmd_test_save_pos(struct unit_test_state *uts)
{
	char buf[DATA_SIZE];
	long long size;

	ut_assertok(make_file(uts, DATA, DATA_SIZE));
	fill_mem(ALT_ADDR, "XY", 2);

	ut_assertok(run_commandf("save sandbox - %lx %s 2 4", (ulong)ALT_ADDR,
				 TEST_FILE));
	ut_assert_nextlinen("2 bytes written in ");
	ut_assert_console_end();

	/* the two bytes land at offset 4 and the file keeps its length */
	ut_assertok(os_get_filesize(TEST_FILE, &size));
	ut_asserteq(DATA_SIZE, size);
	ut_assertok(read_file(uts, buf, DATA_SIZE));
	ut_asserteq_mem("0123XY6789abcdef", buf, DATA_SIZE);

	ut_assertok(os_unlink(TEST_FILE));

	return 0;
}
CMD_TEST(cmd_test_save_pos, UTF_CONSOLE);

/* Test that save reports a file it cannot write */
static int cmd_test_save_fail(struct unit_test_state *uts)
{
	fill_mem(LOAD_ADDR, DATA, DATA_SIZE);

	ut_asserteq(1, run_commandf("save sandbox - %lx %s %x",
				    (ulong)LOAD_ADDR, BAD_FILE, DATA_SIZE));

	/* the filesystem reports it, then the fs layer logs it */
	ut_assert_nextline("** Unable to write file %s **", BAD_FILE);
	ut_assertok(check_log(uts, FUNC_WRITE,
			      "** Unable to write file " BAD_FILE " **"));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_save_fail, UTF_CONSOLE);

/* Test that save shows its usage message when it cannot go on */
static int cmd_test_save_usage(struct unit_test_state *uts)
{
	/* the command has no defaults at all */
	ut_asserteq(1, run_command("save", 0));
	ut_assertok(check_save_usage(uts));

	/* 'bytes' must be given, since a file is not written to the end */
	ut_asserteq(1, run_commandf("save sandbox - %lx %s", (ulong)LOAD_ADDR,
				    TEST_FILE));
	ut_assertok(check_save_usage(uts));

	/* and there is nothing to give beyond 'pos' */
	ut_asserteq(1, run_commandf("save sandbox - %lx %s 2 4 1",
				    (ulong)LOAD_ADDR, TEST_FILE));
	ut_assertok(check_save_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_save_usage, UTF_CONSOLE);
