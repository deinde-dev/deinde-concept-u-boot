// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the legacy filesystem commands
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
#define FUNC_MKDIR	"            do_mkdir"
#define FUNC_REMOVE	"          fat_remove"

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
 * make_file() - Write a file which the tests work on
 *
 * @uts: Test state
 * @fname: Name of the file to write
 * @data: Contents for the file
 * @size: Number of bytes to write
 * Return: 0 if OK, 1 on failure
 */
static int make_file(struct unit_test_state *uts, const char *fname,
		     const char *data, uint size)
{
	int fd;

	fd = os_open(fname, OS_O_WRONLY | OS_O_CREAT | OS_O_TRUNC);
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

	ut_assertok(make_file(uts, TEST_FILE, DATA, DATA_SIZE));
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

	ut_assertok(make_file(uts, TEST_FILE, DATA, DATA_SIZE));
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

	ut_assertok(make_file(uts, TEST_FILE, DATA, DATA_SIZE));
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
	ut_assertok(make_file(uts, TEST_FILE, DATA, DATA_SIZE));

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

	ut_assertok(make_file(uts, TEST_FILE, DATA, DATA_SIZE));
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

/* Test that save refuses an option, since it has none */
static int cmd_test_save_badopt(struct unit_test_state *uts)
{
	fill_mem(LOAD_ADDR, DATA, DATA_SIZE);

	ut_asserteq(1, run_commandf("save -b sandbox - %lx %s %x",
				    (ulong)LOAD_ADDR, TEST_FILE, DATA_SIZE));
	ut_assertok(check_save_usage(uts));

	/*
	 * A filename which looks like an option is still a filename, so this
	 * reaches the filesystem, which cannot create the file since its
	 * directory is not there
	 */
	ut_asserteq(1, run_commandf("save sandbox - %lx -b/f %x",
				    (ulong)LOAD_ADDR, DATA_SIZE));
	ut_assert_nextline("** Unable to write file -b/f **");
	ut_assertok(check_log(uts, FUNC_WRITE,
			      "** Unable to write file -b/f **"));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_save_badopt, UTF_CONSOLE);

/**
 * check_size_usage() - Check the console holds the size usage message
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_size_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("size - determine a file's size");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("size <interface> <dev[:part]> <filename>");
	ut_assert_nextline("    - Find file 'filename' from 'dev' on 'interface'");
	ut_assert_nextline("      determine its size, and store in the 'filesize' variable.");
	ut_assert_console_end();

	return 0;
}

/* Test that size reports a file's size in the filesize variable */
static int cmd_test_size_base(struct unit_test_state *uts)
{
	ut_assertok(make_file(uts, TEST_FILE, DATA, DATA_SIZE));

	/* the command's only output is the variable it sets */
	ut_assertok(run_commandf("size sandbox - %s", TEST_FILE));
	ut_assert_console_end();
	ut_asserteq(DATA_SIZE, env_get_hex("filesize", 0));

	/* a shorter file is reported as such, so nothing is cached */
	ut_assertok(make_file(uts, TEST_FILE, DATA, 4));
	ut_assertok(run_commandf("size sandbox - %s", TEST_FILE));
	ut_assert_console_end();
	ut_asserteq(4, env_get_hex("filesize", 0));

	ut_assertok(os_unlink(TEST_FILE));

	return 0;
}
CMD_TEST(cmd_test_size_base, UTF_CONSOLE);

/* Test that a file which is not there leaves filesize alone */
static int cmd_test_size_missing(struct unit_test_state *uts)
{
	char *old_filesize;

	ut_assertok(save_env(uts, "filesize", &old_filesize));
	ut_assertok(env_set("filesize", "sentinel"));

	ut_asserteq(1, run_commandf("size sandbox - %s", TEST_FILE));
	ut_assert_console_end();
	ut_asserteq_str("sentinel", env_get("filesize"));

	ut_assertok(env_set("filesize", old_filesize));
	free(old_filesize);

	return 0;
}
CMD_TEST(cmd_test_size_missing, UTF_CONSOLE);

/* Test that size shows its usage message when it cannot go on */
static int cmd_test_size_usage(struct unit_test_state *uts)
{
	/* all three arguments are needed */
	ut_asserteq(1, run_command("size", 0));
	ut_assertok(check_size_usage(uts));

	ut_asserteq(1, run_command("size sandbox -", 0));
	ut_assertok(check_size_usage(uts));

	/* and there is nothing to give beyond the filename */
	ut_asserteq(1, run_commandf("size sandbox - %s extra", TEST_FILE));
	ut_assertok(check_size_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_size_usage, UTF_CONSOLE);

/* Test that size refuses an option, since it has none */
static int cmd_test_size_badopt(struct unit_test_state *uts)
{
	ut_assertok(make_file(uts, TEST_FILE, DATA, DATA_SIZE));

	ut_asserteq(1, run_commandf("size -b sandbox - %s", TEST_FILE));
	ut_assertok(check_size_usage(uts));

	/* a filename which looks like an option is still a filename */
	ut_asserteq(1, run_command("size sandbox - -b", 0));
	ut_assert_console_end();

	ut_assertok(os_unlink(TEST_FILE));

	return 0;
}
CMD_TEST(cmd_test_size_badopt, UTF_CONSOLE);

/*
 * The directory which the ls tests list, with a file, a sub-directory and a
 * symbolic link in it. It is a dot-directory, as the file above is
 */
#define LS_DIR		".cmd_test_fs_dir"
#define LS_FILE		LS_DIR "/hello.txt"
#define LS_SUBDIR	LS_DIR "/sub"
#define LS_LINK		LS_DIR "/link"

/* number of characters of a listing line taken by the type and the size */
#define LS_NAME_POS	15

/* enough for a listing line, which holds a name of up to 255 characters */
#define MAX_LINE	280

/**
 * struct ls_entry - an entry which a listing is expected to hold
 *
 * @type: Type of the entry, as sandboxfs prints it
 * @size: Expected size, or -1 not to check it
 * @name: Name of the entry
 */
struct ls_entry {
	const char *type;
	long size;
	const char *name;
};

/**
 * check_ls() - Check a listing holds exactly the expected entries
 *
 * sandboxfs lists a directory in the order the host holds it, which is
 * neither alphabetical nor something the host promises, so the lines are
 * matched without regard to the order they arrive in. The size of a
 * directory, and of a symbolic link, is the host filesystem's business, so an
 * entry can ask for its size not to be checked.
 *
 * @uts: Test state
 * @exp: Expected entries
 * @count: Number of expected entries
 * Return: 0 if OK, 1 on failure
 */
static int check_ls(struct unit_test_state *uts, const struct ls_entry *exp,
		    int count)
{
	bool seen[8];
	int i;

	ut_assert(count <= (int)ARRAY_SIZE(seen));
	memset(seen, '\0', sizeof(seen));

	for (i = 0; i < count; i++) {
		char line[MAX_LINE];
		int which;

		ut_assert(console_record_readline(line, sizeof(line)) >= 0);
		for (which = 0; which < count; which++) {
			const struct ls_entry *ent = &exp[which];
			char want[MAX_LINE];

			if (seen[which])
				continue;
			if (ent->size < 0) {
				if (strncmp(line, ent->type, 3) ||
				    strcmp(line + LS_NAME_POS, ent->name))
					continue;
			} else {
				snprintf(want, sizeof(want), "%s %10ld %s",
					 ent->type, ent->size, ent->name);
				if (strcmp(line, want))
					continue;
			}
			break;
		}
		ut_assertf(which < count, "unexpected line '%s'", line);
		seen[which] = true;
	}
	ut_assert_console_end();

	return 0;
}

/**
 * make_ls_dir() - Create the directory which the ls tests list
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int make_ls_dir(struct unit_test_state *uts)
{
	/*
	 * A test which stops part-way leaves the directory behind, so take it
	 * out first, ignoring whatever is not there
	 */
	os_unlink(LS_LINK);
	os_rmdir(LS_SUBDIR);
	os_unlink(LS_FILE);
	os_rmdir(LS_DIR);

	ut_assertok(os_mkdir(LS_DIR, 0755));
	ut_assertok(make_file(uts, LS_FILE, DATA, DATA_SIZE));
	ut_assertok(os_mkdir(LS_SUBDIR, 0755));
	ut_assertok(os_symlink("hello.txt", LS_LINK));

	return 0;
}

/**
 * remove_ls_dir() - Remove the directory which the ls tests list
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int remove_ls_dir(struct unit_test_state *uts)
{
	ut_assertok(os_unlink(LS_LINK));
	ut_assertok(os_rmdir(LS_SUBDIR));
	ut_assertok(os_unlink(LS_FILE));
	ut_assertok(os_rmdir(LS_DIR));

	return 0;
}

/* Test that ls lists a directory, with the type and size of each entry */
static int cmd_test_ls_base(struct unit_test_state *uts)
{
	static const struct ls_entry dir[] = {
		{ "DIR", -1, "." },
		{ "DIR", -1, ".." },
		{ "DIR", -1, "sub" },
		{ "SYM", -1, "link" },
		{ "   ", DATA_SIZE, "hello.txt" },
	};
	static const struct ls_entry sub[] = {
		{ "DIR", -1, "." },
		{ "DIR", -1, ".." },
	};

	ut_assertok(make_ls_dir(uts));

	ut_assertok(run_commandf("ls sandbox - %s", LS_DIR));
	ut_assertok(check_ls(uts, dir, ARRAY_SIZE(dir)));

	/* the device can be given as a partition, which sandboxfs ignores */
	ut_assertok(run_commandf("ls sandbox 0:0 %s", LS_DIR));
	ut_assertok(check_ls(uts, dir, ARRAY_SIZE(dir)));

	/* a directory with nothing in it still holds . and .. */
	ut_assertok(run_commandf("ls sandbox - %s", LS_SUBDIR));
	ut_assertok(check_ls(uts, sub, ARRAY_SIZE(sub)));

	ut_assertok(remove_ls_dir(uts));

	return 0;
}
CMD_TEST(cmd_test_ls_base, UTF_CONSOLE);

/* Test that ls lists the root directory when none is given */
static int cmd_test_ls_default(struct unit_test_state *uts)
{
	/*
	 * The default is '/', which for sandboxfs is the root of the host's
	 * own filesystem, so there is nothing to assert on but the success of
	 * the command
	 */
	ut_assertok(run_command("ls sandbox -", 0));
	console_record_reset();

	return 0;
}
CMD_TEST(cmd_test_ls_default, UTF_CONSOLE);

/* Test that ls reports a directory it cannot list */
static int cmd_test_ls_missing(struct unit_test_state *uts)
{
	ut_assertok(make_ls_dir(uts));

	/* the return value is all there is to tell this from an empty listing */
	ut_asserteq(1, run_commandf("ls sandbox - %s/nodir", LS_DIR));
	ut_assert_console_end();

	/* a file is not a directory, so it cannot be listed either */
	ut_asserteq(1, run_commandf("ls sandbox - %s", LS_FILE));
	ut_assert_console_end();

	ut_assertok(remove_ls_dir(uts));

	return 0;
}
CMD_TEST(cmd_test_ls_missing, UTF_CONSOLE);

/* the lines of the ls usage message which are too long to write inline */
#define LS_USAGE_PART \
	"    - List files in directory 'directory' of partition 'part' on"

/**
 * check_ls_usage() - Check the console holds the ls usage message
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_ls_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("ls - list files in a directory (default /)");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("ls <interface> [<dev[:part]> [directory]]");
	ut_assert_nextline(LS_USAGE_PART);
	ut_assert_nextline("      device type 'interface' instance 'dev'.");
	ut_assert_console_end();

	return 0;
}

/* Test that ls shows its usage message when it cannot go on */
static int cmd_test_ls_usage(struct unit_test_state *uts)
{
	/* the interface is the one argument which has no default */
	ut_asserteq(1, run_command("ls", 0));
	ut_assertok(check_ls_usage(uts));

	/* and there is nothing to give beyond the directory */
	ut_asserteq(1, run_commandf("ls sandbox - %s extra", LS_DIR));
	ut_assertok(check_ls_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_ls_usage, UTF_CONSOLE);

/* Test that ls refuses an option, since it has none */
static int cmd_test_ls_badopt(struct unit_test_state *uts)
{
	ut_assertok(make_ls_dir(uts));

	ut_asserteq(1, run_commandf("ls -b sandbox - %s", LS_DIR));
	ut_assertok(check_ls_usage(uts));

	/* a directory which looks like an option is still a directory */
	ut_asserteq(1, run_command("ls sandbox - -b", 0));
	ut_assert_console_end();

	ut_assertok(remove_ls_dir(uts));

	return 0;
}
CMD_TEST(cmd_test_ls_badopt, UTF_CONSOLE);

/*
 * The commands which change a filesystem need one which can be written to,
 * so their tests are manual: the Python side makes a FAT image and passes its
 * path in; see test/py/tests/test_fs_legacy.py
 */
#define ARG_IMAGE	0	/* fs_image: path to the filesystem image */

/* name of the directory which the mkdir tests create */
#define NEW_DIR		"newdir"

/**
 * bind_image() - Attach the image which the Python side made
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
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
 * check_empty_dir() - Check a listing which shows an empty directory
 *
 * A directory with nothing in it still holds its own entry and its parent's,
 * which a generic listing shows with a trailing slash.
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_empty_dir(struct unit_test_state *uts)
{
	ut_assert_nextline("            ./");
	ut_assert_nextline("            ../");
	ut_assert_nextline_empty();
	ut_assert_nextline("0 file(s), 2 dir(s)");
	ut_assert_nextline_empty();
	ut_assert_console_end();

	return 0;
}

/* Test that mkdir creates a directory, which is then empty */
static int cmd_test_mkdir_base_norun(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_FAT_WRITE))
		return -EAGAIN;

	ut_assertok(bind_image(uts));

	/* the command says nothing when it works */
	ut_assertok(run_commandf("mkdir host 0 %s", NEW_DIR));
	ut_assert_console_end();

	ut_assertok(run_commandf("ls host 0 %s", NEW_DIR));
	ut_assertok(check_empty_dir(uts));

	/* a directory can be created within it, naming a partition this time */
	ut_assertok(run_commandf("mkdir host 0:0 %s/deep", NEW_DIR));
	ut_assert_console_end();

	ut_assertok(run_commandf("ls host 0 %s/deep", NEW_DIR));
	ut_assertok(check_empty_dir(uts));

	/*
	 * Put the image back as it was found, so that the test can be run
	 * again on the same image. On FAT the rm command removes an empty
	 * directory as well as a file
	 */
	ut_assertok(run_commandf("rm host 0 %s/deep", NEW_DIR));
	ut_assertok(run_commandf("rm host 0 %s", NEW_DIR));
	ut_assert_console_end();

	return 0;
}
CMD_TEST_ARGS(cmd_test_mkdir_base_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that mkdir reports a directory it cannot create */
static int cmd_test_mkdir_fail_norun(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_FAT_WRITE))
		return -EAGAIN;

	ut_assertok(bind_image(uts));

	/* the filesystem reports it, then the command logs it */
	ut_asserteq(1, run_command("mkdir host 0 docs", 0));
	ut_assert_nextline("docs: already exists");
	ut_assertok(check_log(uts, FUNC_MKDIR,
			      "** Unable to create a directory \"docs\" **"));
	ut_assert_console_end();

	/* only the last part of the path is created, so a/b must exist */
	ut_asserteq(1, run_command("mkdir host 0 a/b/c", 0));
	ut_assert_nextline("a/b: doesn't exist (-2)");
	ut_assertok(check_log(uts, FUNC_MKDIR,
			      "** Unable to create a directory \"a/b/c\" **"));
	ut_assert_console_end();

	return 0;
}
CMD_TEST_ARGS(cmd_test_mkdir_fail_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that mkdir reports a filesystem which cannot create a directory */
static int cmd_test_mkdir_unsup(struct unit_test_state *uts)
{
	/* sandboxfs has no mkdir operation, so the fs layer refuses it */
	ut_asserteq(1, run_command("mkdir sandbox - foo", 0));
	ut_assertok(check_log(uts, FUNC_MKDIR,
			      "** Unable to create a directory \"foo\" **"));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_mkdir_unsup, UTF_CONSOLE);

/**
 * check_mkdir_usage() - Check the console holds the mkdir usage message
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_mkdir_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("mkdir - create a directory");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("mkdir <interface> <dev[:part]> <directory>");
	ut_assert_nextline("    - Create a directory 'directory' of partition 'part' on");
	ut_assert_nextline("      device type 'interface' instance 'dev'.");
	ut_assert_console_end();

	return 0;
}

/* Test that mkdir shows its usage message when it cannot go on */
static int cmd_test_mkdir_usage(struct unit_test_state *uts)
{
	/* all three arguments are needed */
	ut_asserteq(1, run_command("mkdir", 0));
	ut_assertok(check_mkdir_usage(uts));

	ut_asserteq(1, run_command("mkdir sandbox foo", 0));
	ut_assertok(check_mkdir_usage(uts));

	/* and there is nothing to give beyond the directory */
	ut_asserteq(1, run_command("mkdir sandbox - foo bar", 0));
	ut_assertok(check_mkdir_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_mkdir_usage, UTF_CONSOLE);

/* Test that mkdir refuses an option, since it has none */
static int cmd_test_mkdir_badopt(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("mkdir -b sandbox - foo", 0));
	ut_assertok(check_mkdir_usage(uts));

	/* a directory which looks like an option is still a directory */
	ut_asserteq(1, run_command("mkdir sandbox - -b", 0));
	ut_assertok(check_log(uts, FUNC_MKDIR,
			      "** Unable to create a directory \"-b\" **"));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_mkdir_badopt, UTF_CONSOLE);

/* contents of hello.txt in the image, which the Python side writes */
#define HELLO		"Hello, world!\n"
#define HELLO_SIZE	0xe

/* Test that rm deletes a file, and an empty directory */
static int cmd_test_rm_base_norun(struct unit_test_state *uts)
{
	char *old_filesize;

	if (!IS_ENABLED(CONFIG_FAT_WRITE))
		return -EAGAIN;

	ut_assertok(bind_image(uts));
	ut_assertok(save_env(uts, "filesize", &old_filesize));

	/* the file is there to start with, which the size command shows */
	ut_assertok(run_command("size host 0 hello.txt", 0));
	ut_assert_console_end();
	ut_asserteq(HELLO_SIZE, env_get_hex("filesize", 0));

	/* the command says nothing when it works */
	ut_assertok(run_command("rm host 0 hello.txt", 0));
	ut_assert_console_end();

	/* and the file is gone, so even its size cannot be found */
	ut_asserteq(1, run_command("size host 0 hello.txt", 0));
	ut_assert_console_end();

	/* an empty directory goes the same way, since FAT has no rmdir */
	ut_assertok(run_command("ls host 0 empty", 0));
	ut_assertok(check_empty_dir(uts));
	ut_assertok(run_command("rm host 0 empty", 0));
	ut_assert_console_end();
	ut_asserteq(1, run_command("ls host 0 empty", 0));
	ut_assert_console_end();

	/* put the image back as it was found, so the test can be repeated */
	fill_mem(LOAD_ADDR, HELLO, HELLO_SIZE);
	ut_assertok(run_commandf("save host 0 %lx hello.txt %x",
				 (ulong)LOAD_ADDR, HELLO_SIZE));
	ut_assert_nextlinen("14 bytes written in ");
	ut_assertok(run_command("mkdir host 0 empty", 0));
	ut_assert_console_end();

	ut_assertok(env_set("filesize", old_filesize));
	free(old_filesize);

	return 0;
}
CMD_TEST_ARGS(cmd_test_rm_base_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that rm reports a path it cannot delete */
static int cmd_test_rm_fail_norun(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_FAT_WRITE))
		return -EAGAIN;

	ut_assertok(bind_image(uts));

	/* the filesystem reports a file which is not there */
	ut_asserteq(1, run_command("rm host 0 nothere", 0));
	ut_assertok(check_log(uts, FUNC_REMOVE,
			      "nothere: doesn't exist (-2)"));
	ut_assert_console_end();

	/* a directory is refused while anything is left in it */
	ut_asserteq(1, run_command("rm host 0 docs", 0));
	ut_assert_nextline("Error: directory is not empty: 3");
	ut_assert_console_end();

	return 0;
}
CMD_TEST_ARGS(cmd_test_rm_fail_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that rm reports a filesystem which cannot delete a file */
static int cmd_test_rm_unsup(struct unit_test_state *uts)
{
	/*
	 * sandboxfs has no unlink operation, so the fs layer refuses it; the
	 * return value is all there is to see, since nothing reports it
	 */
	ut_asserteq(1, run_commandf("rm sandbox - %s", TEST_FILE));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_rm_unsup, UTF_CONSOLE);

/**
 * check_rm_usage() - Check the console holds the rm usage message
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_rm_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("rm - delete a file");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("rm <interface> <dev[:part]> <filename>");
	ut_assert_nextline("    - delete a file with the name 'filename' on");
	ut_assert_nextline("      device type 'interface' instance 'dev'.");
	ut_assert_console_end();

	return 0;
}

/* Test that rm shows its usage message when it cannot go on */
static int cmd_test_rm_usage(struct unit_test_state *uts)
{
	/* all three arguments are needed */
	ut_asserteq(1, run_command("rm", 0));
	ut_assertok(check_rm_usage(uts));

	ut_asserteq(1, run_commandf("rm sandbox %s", TEST_FILE));
	ut_assertok(check_rm_usage(uts));

	/* and there is nothing to give beyond the filename */
	ut_asserteq(1, run_commandf("rm sandbox - %s extra", TEST_FILE));
	ut_assertok(check_rm_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_rm_usage, UTF_CONSOLE);

/* Test that rm refuses an option, since it has none */
static int cmd_test_rm_badopt(struct unit_test_state *uts)
{
	ut_asserteq(1, run_commandf("rm -b sandbox - %s", TEST_FILE));
	ut_assertok(check_rm_usage(uts));

	/* a filename which looks like an option is still a filename */
	ut_asserteq(1, run_command("rm sandbox - -b", 0));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_rm_badopt, UTF_CONSOLE);

/* Test that mv renames a file, and moves one into a directory */
static int cmd_test_mv_base_norun(struct unit_test_state *uts)
{
	char *old_filesize;

	if (!IS_ENABLED(CONFIG_FAT_RENAME))
		return -EAGAIN;

	ut_assertok(bind_image(uts));
	ut_assertok(save_env(uts, "filesize", &old_filesize));

	/* the command says nothing when it works */
	ut_assertok(run_command("mv host 0 hello.txt greeting.txt", 0));
	ut_assert_console_end();

	/* the file is there under its new name and gone under the old one */
	ut_assertok(run_command("size host 0 greeting.txt", 0));
	ut_assert_console_end();
	ut_asserteq(HELLO_SIZE, env_get_hex("filesize", 0));
	ut_asserteq(1, run_command("size host 0 hello.txt", 0));
	ut_assert_console_end();

	/* a destination which is a directory keeps the name of the file */
	ut_assertok(run_command("mv host 0 greeting.txt docs", 0));
	ut_assert_console_end();
	ut_assertok(run_command("size host 0 docs/greeting.txt", 0));
	ut_assert_console_end();
	ut_asserteq(HELLO_SIZE, env_get_hex("filesize", 0));

	/* put the image back as it was found, so the test can be repeated */
	ut_assertok(run_command("mv host 0 docs/greeting.txt hello.txt", 0));
	ut_assert_console_end();

	ut_assertok(env_set("filesize", old_filesize));
	free(old_filesize);

	return 0;
}
CMD_TEST_ARGS(cmd_test_mv_base_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that mv reports a file it cannot move */
static int cmd_test_mv_fail_norun(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_FAT_RENAME))
		return -EAGAIN;

	ut_assertok(bind_image(uts));

	/* nothing reports a source file which is not there */
	ut_asserteq(1, run_command("mv host 0 nothere there", 0));
	ut_assert_console_end();

	return 0;
}
CMD_TEST_ARGS(cmd_test_mv_fail_norun, UTF_CONSOLE | UTF_MANUAL,
	      { "fs_image", UT_ARG_STR });

/* Test that mv reports a filesystem which cannot move a file */
static int cmd_test_mv_unsup(struct unit_test_state *uts)
{
	/* sandboxfs has no rename operation, and nothing reports that */
	ut_asserteq(1, run_commandf("mv sandbox - %s other", TEST_FILE));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_mv_unsup, UTF_CONSOLE);

/**
 * check_mv_usage() - Check the console holds the mv usage message
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_mv_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("mv - rename/move a file/directory");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("mv <interface> <dev[:part]> <old_path> <new_path>");
	ut_assert_nextline("    - renames/moves a file/directory in 'dev' on 'interface' from");
	ut_assert_nextline("      'old_path' to 'new_path'");
	ut_assert_console_end();

	return 0;
}

/* Test that mv shows its usage message when it cannot go on */
static int cmd_test_mv_usage(struct unit_test_state *uts)
{
	/* all four arguments are needed */
	ut_asserteq(1, run_command("mv", 0));
	ut_assertok(check_mv_usage(uts));

	ut_asserteq(1, run_commandf("mv sandbox - %s", TEST_FILE));
	ut_assertok(check_mv_usage(uts));

	/* and there is nothing to give beyond the new name */
	ut_asserteq(1, run_commandf("mv sandbox - %s other extra", TEST_FILE));
	ut_assertok(check_mv_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_mv_usage, UTF_CONSOLE);

/* Test that mv refuses an option, since it has none */
static int cmd_test_mv_badopt(struct unit_test_state *uts)
{
	ut_asserteq(1, run_commandf("mv -b sandbox - %s other", TEST_FILE));
	ut_assertok(check_mv_usage(uts));

	/* a path which looks like an option is still a path */
	ut_asserteq(1, run_command("mv sandbox - -b -c", 0));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_mv_badopt, UTF_CONSOLE);
