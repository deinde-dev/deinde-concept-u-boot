// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the erofsload command
 *
 * These are manual tests, since they need a filesystem image. The Python side
 * makes the image and passes its path in; see
 * test/py/tests/test_erofsload.py
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <console.h>
#include <env.h>
#include <malloc.h>
#include <mapmem.h>
#include <test/cmd.h>
#include <test/test.h>
#include <test/ut.h>

#define ARG_IMAGE	0	/* fs_image: path to the filesystem image */

/* where the tests load the file to, away from anything else */
#define LOAD_ADDR	(CONFIG_SYS_LOAD_ADDR + 0x8000)

/* a second address, so a test can tell one load from another */
#define ALT_ADDR	(CONFIG_SYS_LOAD_ADDR + 0xc000)

/* contents of hello.txt, which the image holds */
#define HELLO		"Hello, world!\n"
#define HELLO_SIZE	14

/* size of docs/note.txt, which the image also holds */
#define NOTE_SIZE	5

/* size of comp.dat, which holds one repeated byte so that it compresses */
#define COMP_SIZE	8192
#define COMP_BYTE	0x5a

/*
 * The name of the function which logs a failed load, right-aligned in
 * CONFIG_LOGF_FUNC_PAD (20) characters as the log emits it
 */
#define FUNC_LOAD	"             do_load"

/**
 * check_log() - Check the next console line, which the fs layer logs
 *
 * The log format shows the function name only when CONFIG_LOGF_FUNC is
 * enabled, which is true of sandbox but of none of the other sandbox
 * variants, so the expected line differs between them.
 *
 * @uts: Test state
 * @msg: Rest of the line
 * Return: 0 if OK, 1 on failure
 */
static int check_log(struct unit_test_state *uts, const char *msg)
{
	if (IS_ENABLED(CONFIG_LOGF_FUNC))
		ut_assert_nextline("%s() %s", FUNC_LOAD, msg);
	else
		ut_assert_nextline("%s", msg);

	return 0;
}

/* the lines of the usage message which are too long to write inline */
#define USAGE_ARGS \
	"erofsload <interface> [<dev[:part]> [<addr> [<filename> [bytes [pos]]]]]"
#define USAGE_POS \
	"      If 'pos' is omitted, 0 is used. 'pos' requires 'bytes'."
#define USAGE_BYTES \
	"      'bytes' gives the size to load. If 'bytes' is 0 or omitted,"

/**
 * check_usage() - Check the console holds the erofsload usage message
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline(
		"erofsload - load binary file from a EROFS filesystem");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline(USAGE_ARGS);
	ut_assert_nextline("    - Load binary file 'filename' from 'dev' on 'interface'");
	ut_assert_nextline("      to address 'addr' from EROFS filesystem.");
	ut_assert_nextline("      'pos' gives the file position to start loading from.");
	ut_assert_nextline(USAGE_POS);
	ut_assert_nextline(USAGE_BYTES);
	ut_assert_nextline("      the load stops on end of file.");
	ut_assert_nextline("      If either 'pos' or 'bytes' are not aligned to");
	ut_assert_nextline("      ARCH_DMA_MINALIGN then a misaligned buffer warning will");
	ut_assert_nextline("      be printed and performance will suffer for the load.");
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

/* Test that erofsload reads a file and reports what it read */
static int cmd_test_erofsload_base_norun(struct unit_test_state *uts)
{
	u8 *buf;

	ut_assertok(bind_image(uts));
	buf = clear_mem(LOAD_ADDR, HELLO_SIZE);

	ut_assertok(run_commandf("erofsload host 0 %lx hello.txt",
				 (ulong)LOAD_ADDR));
	ut_assert_nextlinen("14 bytes read in ");
	ut_assert_console_end();

	/* the file arrives in memory, and both variables describe the load */
	ut_asserteq_mem(HELLO, buf, HELLO_SIZE);
	ut_asserteq(HELLO_SIZE, env_get_hex("filesize", 0));
	ut_asserteq(LOAD_ADDR, env_get_hex("fileaddr", 0));

	unmap_sysmem(buf);

	return 0;
}
CMD_TEST_ARGS(cmd_test_erofsload_base_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that a compressed file is expanded as it is read */
static int cmd_test_erofsload_comp_norun(struct unit_test_state *uts)
{
	u8 *buf;
	int i;

	ut_assertok(bind_image(uts));
	buf = clear_mem(LOAD_ADDR, COMP_SIZE + 1);

	/* the size reported is the size of the file once expanded */
	ut_assertok(run_commandf("erofsload host 0 %lx comp.dat",
				 (ulong)LOAD_ADDR));
	ut_assert_nextlinen("8192 bytes read in ");
	ut_assert_console_end();
	ut_asserteq(COMP_SIZE, env_get_hex("filesize", 0));

	for (i = 0; i < COMP_SIZE; i++)
		ut_asserteq(COMP_BYTE, buf[i]);

	/* nothing beyond the file is written */
	ut_asserteq(0, buf[COMP_SIZE]);

	unmap_sysmem(buf);

	return 0;
}
CMD_TEST_ARGS(cmd_test_erofsload_comp_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that 'bytes' limits a load and 'pos' moves where it starts */
static int cmd_test_erofsload_part_norun(struct unit_test_state *uts)
{
	u8 *buf;

	ut_assertok(bind_image(uts));
	buf = clear_mem(LOAD_ADDR, HELLO_SIZE);

	/* the first five bytes are 'Hello' */
	ut_assertok(run_commandf("erofsload host 0 %lx hello.txt 5",
				 (ulong)LOAD_ADDR));
	ut_assert_nextlinen("5 bytes read in ");
	ut_assert_console_end();
	ut_asserteq_mem("Hello", buf, 5);
	ut_asserteq(5, env_get_hex("filesize", 0));

	/* nothing beyond them is written */
	ut_asserteq(0, buf[5]);

	/* asking for more than there is stops at the end of the file */
	ut_assertok(run_commandf("erofsload host 0 %lx hello.txt 100",
				 (ulong)LOAD_ADDR));
	ut_assert_nextlinen("14 bytes read in ");
	ut_assert_console_end();
	ut_asserteq_mem(HELLO, buf, HELLO_SIZE);

	/* 'pos' starts the read part-way through the file */
	ut_assertok(run_commandf("erofsload host 0 %lx hello.txt 5 7",
				 (ulong)LOAD_ADDR));
	ut_assert_nextlinen("5 bytes read in ");
	ut_assert_console_end();
	ut_asserteq_mem("world", buf, 5);

	/* a 'bytes' of 0 reads the rest of the file from 'pos' */
	ut_assertok(run_commandf("erofsload host 0 %lx hello.txt 0 7",
				 (ulong)LOAD_ADDR));
	ut_assert_nextlinen("7 bytes read in ");
	ut_assert_console_end();
	ut_asserteq_mem("world!\n", buf, 7);

	/* a 'pos' past the end of the file reads nothing, which is not an
	 * error
	 */
	ut_assertok(run_commandf("erofsload host 0 %lx hello.txt 5 20",
				 (ulong)LOAD_ADDR));
	ut_assert_nextlinen("0 bytes read in ");
	ut_assert_console_end();
	ut_asserteq(0, env_get_hex("filesize", 1));

	unmap_sysmem(buf);

	return 0;
}
CMD_TEST_ARGS(cmd_test_erofsload_part_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that erofsload falls back on the loadaddr and bootfile variables */
static int cmd_test_erofsload_default_norun(struct unit_test_state *uts)
{
	char *old_bootfile, *old_loadaddr;
	u8 *buf;

	ut_assertok(bind_image(uts));
	buf = clear_mem(ALT_ADDR, HELLO_SIZE);

	/* other tests use these, so put them back afterwards */
	ut_assertok(save_env(uts, "bootfile", &old_bootfile));
	ut_assertok(save_env(uts, "loadaddr", &old_loadaddr));

	ut_assertok(env_set("bootfile", NULL));
	ut_asserteq(1, run_commandf("erofsload host 0 %lx", (ulong)LOAD_ADDR));
	ut_assert_nextline("** No boot file defined **");
	ut_assert_console_end();

	/* with both set, neither need be given on the command line */
	ut_assertok(env_set("bootfile", "hello.txt"));
	ut_assertok(env_set_hex("loadaddr", ALT_ADDR));
	ut_assertok(run_command("erofsload host 0", 0));
	ut_assert_nextlinen("14 bytes read in ");
	ut_assert_console_end();
	ut_asserteq_mem(HELLO, buf, HELLO_SIZE);
	ut_asserteq(ALT_ADDR, env_get_hex("fileaddr", 0));

	ut_assertok(env_set("bootfile", old_bootfile));
	ut_assertok(env_set("loadaddr", old_loadaddr));
	free(old_bootfile);
	free(old_loadaddr);
	unmap_sysmem(buf);

	return 0;
}
CMD_TEST_ARGS(cmd_test_erofsload_default_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that a filename is a path, whose last part may be a symbolic link */
static int cmd_test_erofsload_subdir_norun(struct unit_test_state *uts)
{
	ut_assertok(bind_image(uts));

	/* the path is relative to the root of the filesystem */
	ut_assertok(run_commandf("erofsload host 0 %lx docs/note.txt",
				 (ulong)LOAD_ADDR));
	ut_assert_nextlinen("5 bytes read in ");
	ut_assert_console_end();
	ut_asserteq(NOTE_SIZE, env_get_hex("filesize", 0));

	/* so a leading slash makes no difference */
	ut_assertok(run_commandf("erofsload host 0 %lx /docs/note.txt",
				 (ulong)LOAD_ADDR));
	ut_assert_nextlinen("5 bytes read in ");
	ut_assert_console_end();

	/* symfile points at hello.txt, so the link is followed */
	ut_assertok(run_commandf("erofsload host 0 %lx symfile",
				 (ulong)LOAD_ADDR));
	ut_assert_nextlinen("14 bytes read in ");
	ut_assert_console_end();

	/* a link part-way along a path is not followed, so this fails */
	ut_asserteq(1, run_commandf("erofsload host 0 %lx sym/note.txt",
				    (ulong)LOAD_ADDR));
	ut_assertok(check_log(uts, "Failed to load 'sym/note.txt'"));
	ut_assert_console_end();

	/* a directory holds no file data, so it cannot be loaded */
	ut_asserteq(1, run_commandf("erofsload host 0 %lx docs",
				    (ulong)LOAD_ADDR));
	ut_assertok(check_log(uts, "Failed to load 'docs'"));
	ut_assert_console_end();

	return 0;
}
CMD_TEST_ARGS(cmd_test_erofsload_subdir_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that a failed load leaves filesize and fileaddr alone */
static int cmd_test_erofsload_missing_norun(struct unit_test_state *uts)
{
	ut_assertok(bind_image(uts));

	ut_assertok(env_set("filesize", "sentinel"));
	ut_assertok(env_set("fileaddr", "sentinel"));

	ut_asserteq(1, run_commandf("erofsload host 0 %lx missing.txt",
				    (ulong)LOAD_ADDR));
	ut_assertok(check_log(uts, "Failed to load 'missing.txt'"));
	ut_assert_console_end();

	ut_asserteq_str("sentinel", env_get("filesize"));
	ut_asserteq_str("sentinel", env_get("fileaddr"));

	return 0;
}
CMD_TEST_ARGS(cmd_test_erofsload_missing_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that erofsload complains about a device which is not there */
static int cmd_test_erofsload_baddev_norun(struct unit_test_state *uts)
{
	ut_assertok(bind_image(uts));

	ut_asserteq(1, run_commandf("erofsload host 1 %lx hello.txt",
				    (ulong)LOAD_ADDR));
	ut_assert_nextline("** Bad device specification host 1 **");
	ut_assert_nextline("Couldn't find partition host 1");
	ut_assertok(check_log(uts, "Can't set block device"));
	ut_assert_console_end();

	return 0;
}
CMD_TEST_ARGS(cmd_test_erofsload_baddev_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that erofsload shows its usage message when it cannot go on */
static int cmd_test_erofsload_usage_norun(struct unit_test_state *uts)
{
	ut_assertok(bind_image(uts));

	/* the interface is the one argument which has no default */
	ut_asserteq(1, run_command("erofsload", 0));
	ut_assertok(check_usage(uts));

	/* an address which is not a number is refused, not taken as 0 */
	ut_asserteq(1, run_command("erofsload host 0 zz hello.txt", 0));
	ut_assertok(check_usage(uts));

	/* and there is nothing to give beyond 'pos' */
	ut_asserteq(1, run_commandf("erofsload host 0 %lx hello.txt 5 7 1",
				    (ulong)LOAD_ADDR));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST_ARGS(cmd_test_erofsload_usage_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });
