// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the saveenv command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <command.h>
#include <env.h>
#include <env_internal.h>
#include <os.h>
#include <asm/state.h>
#include <asm/test.h>
#include <test/cmd.h>
#include <test/ut.h>

/**
 * env_test_fname() - Work out a filename to hold the test environment
 *
 * This sits beside the sandbox binary, so that the sandbox variants running
 * at the same time do not share a file. The eraseenv test does the same with
 * a name of its own.
 *
 * @buf: Buffer to hold the name
 * @size: Size of buffer
 * Return: 0 if OK, -ENOENT if the program name is not known
 */
static int env_test_fname(char *buf, int size)
{
	struct sandbox_state *state = state_get_current();
	const char *prog, *slash;

	prog = state->prog_fname ? state->prog_fname : state->argv[0];
	if (!prog)
		return -ENOENT;

	slash = strrchr(prog, '/');
	if (slash)
		snprintf(buf, size, "%.*s/uboot-save-test.env",
			 (int)(slash - prog), prog);
	else
		snprintf(buf, size, "uboot-save-test.env");

	return 0;
}

/**
 * check_saved_var() - Check that a saved environment holds a variable
 *
 * @uts: Test state
 * @fname: File holding the environment which was saved
 * @str: 'name=value' string which must be present
 * Return: 0 if OK, 1 on failure
 */
static int check_saved_var(struct unit_test_state *uts, const char *fname,
			   const char *str)
{
	const char *p, *end;
	bool found = false;
	env_t *env;
	void *buf;
	int size;

	ut_assertok(os_read_file(fname, &buf, &size));
	env = buf;
	end = (char *)buf + size;

	/* the environment is stored as '\0'-terminated 'name=value' strings */
	for (p = (char *)env->data; p < end && *p; p += strlen(p) + 1) {
		if (!strcmp(p, str))
			found = true;
	}
	os_free(buf);
	ut_assertf(found, "'%s' is missing from the saved environment", str);

	return 0;
}

/* Test 'saveenv' when there is nowhere to save */
static int cmd_test_saveenv_nowhere(struct unit_test_state *uts)
{
	/* the default location cannot save anything */
	ut_asserteq(1, run_command("saveenv", 0));
	ut_assert_nextline("Saving Environment to nowhere... not possible");
	ut_assert_console_end();

	/* the command takes no options */
	ut_asserteq(1, run_command("saveenv -x", 0));
	ut_assert_nextlinen("saveenv - save environment variables");
	ut_assert_nextline_empty();
	ut_assert_nextlinen("Usage:");
	ut_assert_nextlinen("saveenv");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_saveenv_nowhere, UTF_CONSOLE);

/* Test 'saveenv' writing the environment to persistent storage */
static int cmd_test_saveenv_base(struct unit_test_state *uts)
{
	char fname[256];
	int fd;

	ut_assertok(env_test_fname(fname, sizeof(fname)));
	os_unlink(fname);

	/* take over the environment, so that there is somewhere to save */
	ut_assertok(sandbox_env_set_file(fname));

	ut_assertok(env_set("ut_save", "cheese"));
	ut_assertok(run_command("saveenv", 0));
	ut_assert_nextline("Saving Environment to sandbox... OK");
	ut_assert_console_end();

	/* the file now exists and holds the variable */
	fd = os_open(fname, OS_O_RDONLY);
	ut_assert(fd >= 0);
	os_close(fd);
	ut_assertok(check_saved_var(uts, fname, "ut_save=cheese"));

	/* a later change is not in the stored copy until it is saved again */
	ut_assertok(env_set("ut_save", "pickle"));
	ut_assertok(check_saved_var(uts, fname, "ut_save=cheese"));
	ut_assertok(run_command("saveenv", 0));
	ut_assert_nextline("Saving Environment to sandbox... OK");
	ut_assert_console_end();
	ut_assertok(check_saved_var(uts, fname, "ut_save=pickle"));

	/* put the previous location back for whatever runs next */
	ut_assertok(sandbox_env_set_file(NULL));
	ut_assertok(env_set("ut_save", NULL));
	os_unlink(fname);

	return 0;
}
CMD_TEST(cmd_test_saveenv_base, UTF_CONSOLE);
