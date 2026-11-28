// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for qfw command
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#include <console.h>
#include <dm.h>
#include <mapmem.h>
#include <qfw.h>
#include <asm/global_data.h>
#include <dm/test.h>
#include <test/cmd.h>
#include <test/ut.h>

DECLARE_GLOBAL_DATA_PTR;

/* address used by the tests which read a firmware file into memory */
#define READ_ADDR	((ulong)CONFIG_SYS_LOAD_ADDR + 0x5000)

/* name of a file which QEMU never provides */
#define MISSING_FILE	"no-such-file"

/**
 * check_usage() - Check the usage message the command shows
 *
 * The message describes every sub-command, so only its first lines and the
 * last one are checked
 *
 * @uts: Test state
 * Return: 0 if OK, other value on error
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("qfw - QEMU firmware interface");
	ut_assert_nextline_empty();
	if (IS_ENABLED(CONFIG_SYS_LONGHELP)) {
		ut_assert_nextline("Usage:");
		ut_assert_skip_to_linen("    - e820");
	}
	ut_assert_console_end();

	return 0;
}

/* Test 'qfw list' command */
static int cmd_test_qfw_list(struct unit_test_state *uts)
{
	struct fw_cfg_file_iter iter;
	struct fw_file *file;
	struct udevice *dev;

	ut_assertok(uclass_first_device_err(UCLASS_QFW, &dev));

	ut_assertok(run_command("qfw list", 0));
	ut_assert_nextline("    Addr     Size Sel Name");
	ut_assert_nextlinen("--");

	for (file = qfw_file_iter_init(dev, &iter); !qfw_file_iter_end(&iter);
	     file = qfw_file_iter_next(&iter)) {
		ut_assert_nextline("%8lx %8x %3x %-56s", file->addr,
				   be32_to_cpu(file->cfg.size),
				   be16_to_cpu(file->cfg.select),
				   file->cfg.name);
	}
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_qfw_list, UTF_CONSOLE);

/* Test 'qfw cpus' command */
static int cmd_test_qfw_cpus(struct unit_test_state *uts)
{
	struct udevice *dev;

	ut_assertok(uclass_first_device_err(UCLASS_QFW, &dev));

	ut_assertok(run_command("qfw cpus", 0));
	ut_assert_nextline("%d cpu(s) online", qfw_online_cpus(dev));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_qfw_cpus, UTF_CONSOLE);

/* Test 'qfw dump' command */
static int cmd_test_qfw_dump(struct unit_test_state *uts)
{
	if (IS_ENABLED(CONFIG_SANDBOX) || !IS_ENABLED(CONFIG_X86))
		return -EAGAIN;

	ut_assertok(run_command("qfw dump", 0));
	ut_assert_nextline("signature   = QEMU");
	ut_assert_nextlinen("id");
	ut_assert_nextlinen("uuid        = 00000000-0000-0000-0000-000000000000");
	ut_assert_nextline("ram_size    = %#.*lx", 2 * (int)sizeof(ulong),
			   (ulong)gd->ram_size);
	ut_assert_skip_to_linen("file dir le =");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_qfw_dump, UTF_CONSOLE);

/* Test 'qfw table' command */
static int cmd_test_qfw_table(struct unit_test_state *uts)
{
	if (IS_ENABLED(CONFIG_SANDBOX))
		return -EAGAIN;

	ut_assertok(run_command("qfw table", 0));
	ut_assert_nextline("  0 alloc: align 10 zone fseg name 'etc/acpi/rsdp'");
	if (IS_ENABLED(CONFIG_X86))
		ut_assert_nextline("  1 alloc: align 40 zone high name 'etc/acpi/tables'");

	/*
	 * we can't really test anything else as it may vary, so just check that
	 * there is more output after this
	 */
	ut_asserteq(true, ut_check_console_end(uts));

	return 0;
}
CMD_TEST(cmd_test_qfw_table, UTF_CONSOLE);

/* Test 'qfw table' where there is no table loader, as on sandbox */
static int cmd_test_qfw_table_missing(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_SANDBOX))
		return -EAGAIN;

	ut_asserteq(1, run_command("qfw table", 0));
	ut_assert_nextline("error: can't find etc/table-loader");
	ut_assert_nextline("Error %dE", -ENOENT);
	ut_assert_nextline("Command 'table' failed: Error 1");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_qfw_table_missing, UTF_CONSOLE);

/* Test 'qfw arch' command */
static int cmd_test_qfw_arch(struct unit_test_state *uts)
{
	/*
	 * this test is really only useful on x86, which has some entries, but
	 * since the implementation of the 'qfw arch' command is generic, we can
	 * expect that it works on ARM too
	 */
	ut_assertok(run_command("qfw arch", 0));
	if (IS_ENABLED(CONFIG_X86)) {
		ut_assert_nextline("acpi tables = 0x00000000");
		ut_asserteq(true, ut_check_console_end(uts));
	}

	return 0;
}
CMD_TEST(cmd_test_qfw_arch, UTF_CONSOLE);

/* Test 'qfw read' command */
static int cmd_test_qfw_read(struct unit_test_state *uts)
{
	char *ptr = map_sysmem(0x1000, 0x100);

	if (IS_ENABLED(CONFIG_SANDBOX) || !IS_ENABLED(CONFIG_X86))
		return -EAGAIN;

	ut_assertok(run_command("qfw read 1000 etc/acpi/rsdp", 0));
	ut_asserteq_strn("RSD PTR ", ptr);

	return 0;
}
CMD_TEST(cmd_test_qfw_read, UTF_CONSOLE);

/*
 * Test that 'qfw read' accepts one of the files sandbox's emulator offers.
 * That emulator provides no contents for a file, so only the address and the
 * name are checked here
 */
static int cmd_test_qfw_read_file(struct unit_test_state *uts)
{
	if (!IS_ENABLED(CONFIG_SANDBOX))
		return -EAGAIN;

	ut_assertok(run_commandf("qfw read %lx test-one", READ_ADDR));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_qfw_read_file, UTF_CONSOLE);

/* Test 'qfw read' with a filename QEMU does not offer */
static int cmd_test_qfw_read_missing(struct unit_test_state *uts)
{
	struct udevice *dev;

	ut_assertok(uclass_first_device_err(UCLASS_QFW, &dev));

	/* the message below only comes out if the file list itself is readable */
	ut_assertok(qfw_read_firmware_list(dev));

	ut_asserteq(1, run_commandf("qfw read %lx %s", READ_ADDR,
				    MISSING_FILE));
	ut_assert_nextline("error: can't find %s", MISSING_FILE);
	ut_assert_nextline("Command 'read' failed: Error 1");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_qfw_read_missing, UTF_CONSOLE);

/* Test 'qfw load' with no address to load to */
static int cmd_test_qfw_load(struct unit_test_state *uts)
{
	/*
	 * both addresses are given, so the environment cannot affect this; a
	 * zero address means the command has nowhere to put the kernel
	 */
	ut_asserteq(1, run_command("qfw load 0 0", 0));
	ut_assert_nextline("missing load or initrd address");
	ut_assert_nextline("Command 'load' failed: Error 1");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_qfw_load, UTF_CONSOLE);

/* Test 'qfw e820' command */
static int cmd_test_qfw_e820(struct unit_test_state *uts)
{
	int ret;

	ret = run_command("qfw e820", 0);
	if (!IS_ENABLED(CONFIG_X86)) {
		ut_asserteq(1, ret);
		ut_assert_nextline("Not supported on this architecture");
		ut_assert_nextline("Command 'e820' failed: Error 1");
		ut_assert_console_end();

		return 0;
	}

	ut_assertok(ret);
	ut_assert_nextline("        Addr        Size  Type");

	return 0;
}
CMD_TEST(cmd_test_qfw_e820, UTF_CONSOLE);

/* Test the command lines which show the usage message */
static int cmd_test_qfw_usage(struct unit_test_state *uts)
{
	/* no sub-command at all */
	ut_asserteq(1, run_command("qfw", 0));
	ut_assertok(check_usage(uts));

	/* a sub-command which does not exist */
	ut_asserteq(1, run_command("qfw nosuch", 0));
	ut_assertok(check_usage(uts));

	/* an option, which the command does not take */
	ut_asserteq(1, run_command("qfw -x", 0));
	ut_assertok(check_usage(uts));

	/* more arguments than the sub-command accepts */
	ut_asserteq(1, run_command("qfw list extra", 0));
	ut_assertok(check_usage(uts));

	/* 'read' needs a filename as well as an address */
	ut_asserteq(1, run_commandf("qfw read %lx", READ_ADDR));
	ut_assertok(check_usage(uts));

	/* more arguments than the command itself accepts */
	ut_asserteq(1, run_commandf("qfw read %lx test-one extra", READ_ADDR));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_qfw_usage, UTF_CONSOLE);
