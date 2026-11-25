// SPDX-License-Identifier: GPL-2.0+
/*
 * Test for armffa command
 *
 * Copyright 2022-2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * Authors:
 *   Abdellatif El Khlifi <abdellatif.elkhlifi@arm.com>
 */

#include <command.h>
#include <dm.h>
#include <mapmem.h>
#include <vsprintf.h>
#include <asm/sandbox_arm_ffa.h>
#include <test/cmd.h>
#include <test/ut.h>

/*
 * Names of the functions which log the messages the tests check. The log
 * right-aligns each in CONFIG_LOGF_FUNC_PAD (20) characters and shows a
 * longer one in full.
 */
#define FUNC_GETPART	"      do_ffa_getpart"
#define FUNC_PING	"         do_ffa_ping"
#define FUNC_DEVLIST	"      do_ffa_devlist"
#define FUNC_INFO_GET	"sandbox_ffa_partition_info_get"
#define FUNC_SMC	"sandbox_arm_ffa_smccc_smc"
#define FUNC_ERROR_LOG	" ffa_print_error_log"
#define FUNC_PARTS_HDLR	"ffa_get_partitions_info_hdlr"

/*
 * Prefix which the log puts in front of a message when CONFIG_LOGF_FUNC is
 * enabled. That is true of sandbox but not of sandbox64, the other board
 * carrying the FF-A transport, so the expected line differs between them.
 */
#define LOGF(func)	(IS_ENABLED(CONFIG_LOGF_FUNC) ? func "() " : "")

/* the two partitions which the emulator gives the first service UUID */
#define SP1_EXEC_CTXT		0x5687
#define SP1_PROPERTIES		0x89325621
#define SP2_EXEC_CTXT		0x9587
#define SP2_PROPERTIES		0x45325621

/**
 * check_probe() - Check the line the FF-A device logs as it probes
 *
 * The driver asks the emulator for every partition while it probes, which it
 * does once per driver-model state, so this is the first line a test sees.
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_probe(struct unit_test_state *uts)
{
	ut_assert_nextline("%sFFA_PARTITION_INFO_GET (4)", LOGF(FUNC_INFO_GET));

	return 0;
}

/**
 * check_usage() - Check the usage message which the command shows
 *
 * @uts: Test state
 * Return: 0 if OK, 1 on failure
 */
static int check_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("armffa - Arm FF-A test command");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("armffa getpart <partition UUID>");
	ut_assert_nextline("       - lists the partition(s) info");
	ut_assert_nextline("ping <partition ID>");
	ut_assert_nextline("       - sends a data pattern to the specified partition");
	ut_assert_nextline("devlist");
	ut_assert_nextline("       - displays information about the FF-A device/driver");
	ut_assert_nextline_empty();
	ut_assert_console_end();

	return 0;
}

/* Test 'armffa getpart', which lists the partitions offering a service */
static int cmd_test_armffa_base(struct unit_test_state *uts)
{
	ut_assertok(run_command("armffa getpart " SANDBOX_SERVICE1_UUID, 0));
	ut_assertok(check_probe(uts));
	ut_assert_nextline("%sFFA_PARTITION_INFO_GET (%d)", LOGF(FUNC_INFO_GET),
			   SANDBOX_SP_COUNT_PER_VALID_SERVICE);
	ut_assert_nextline("%sPartition: id = %x , exec_ctxt %x , properties %x",
			   LOGF(FUNC_GETPART), SANDBOX_SP1_ID, SP1_EXEC_CTXT,
			   SP1_PROPERTIES);
	ut_assert_nextline("%sPartition: id = %x , exec_ctxt %x , properties %x",
			   LOGF(FUNC_GETPART), SANDBOX_SP2_ID, SP2_EXEC_CTXT,
			   SP2_PROPERTIES);
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_armffa_base, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test 'armffa devlist', which reports the device and its driver */
static int cmd_test_armffa_devlist(struct unit_test_state *uts)
{
	struct udevice *dev;

	/* probe the device here, so its own output is out of the way */
	ut_assertok(uclass_first_device_err(UCLASS_FFA, &dev));
	console_record_reset();

	ut_assertok(run_command("armffa devlist", 0));

	/*
	 * the command prints both addresses with PHYS_ADDR_LN, which is
	 * "%llx" here, since sandbox selects PHYS_64BIT
	 */
	ut_assert_nextline("%sdevice %s, addr %llx, driver %s, ops %llx",
			   LOGF(FUNC_DEVLIST), dev->name,
			   (unsigned long long)map_to_sysmem(dev),
			   dev->driver->name,
			   (unsigned long long)map_to_sysmem(dev->driver->ops));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_armffa_devlist, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test 'armffa ping', which sends a data pattern to a partition */
static int cmd_test_armffa_ping(struct unit_test_state *uts)
{
	int i;

	ut_assertok(run_commandf("armffa ping 0x%x", SANDBOX_SP1_ID));
	ut_assertok(check_probe(uts));
	ut_assert_nextline("%sSP response:", LOGF(FUNC_PING));
	ut_assert_nextline("[LSB]");

	/* the emulator answers with five words of 0xff bytes */
	for (i = 0; i < 5; i++)
		ut_assert_nextline("%sffffffffffffffff", LOGF(FUNC_PING));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_armffa_ping, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test 'armffa getpart' with a UUID which gets no partition back */
static int cmd_test_armffa_getpart_err(struct unit_test_state *uts)
{
	/* a well-formed UUID which no partition has */
	ut_asserteq(1, run_command("armffa getpart " SANDBOX_SERVICE3_UUID, 0));
	ut_assertok(check_probe(uts));
	ut_assert_nextline("%sFFA_PARTITION_INFO_GET (-2)",
			   LOGF(FUNC_INFO_GET));
	ut_assert_nextline("%sFF-A ABI internal failure (-22)", LOGF(FUNC_SMC));
	ut_assert_nextline("%sINVALID_PARAMETERS: Unrecognized UUID",
			   LOGF(FUNC_ERROR_LOG));
	ut_assert_nextline("%sFailure in querying partition(s) info (error code: -22)",
			   LOGF(FUNC_GETPART));
	ut_assert_console_end();

	/* a UUID which the driver cannot parse never reaches the emulator */
	ut_asserteq(1, run_command("armffa getpart " SANDBOX_SERVICE4_UUID, 0));
	ut_assert_nextline("%sinvalid UUID", LOGF(FUNC_PARTS_HDLR));
	ut_assert_nextline("%sFailure in querying partition(s) info (error code: -22)",
			   LOGF(FUNC_GETPART));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_armffa_getpart_err, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test 'armffa ping' with a partition ID which no partition has */
static int cmd_test_armffa_ping_err(struct unit_test_state *uts)
{
	ut_asserteq(1, run_command("armffa ping 0x1111", 0));
	ut_assertok(check_probe(uts));
	ut_assert_nextline("%sFF-A ABI internal failure (-22)", LOGF(FUNC_SMC));
	ut_assert_nextline("%sSending direct request error (-22)",
			   LOGF(FUNC_PING));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_armffa_ping_err, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test the cases where the command shows its usage message */
static int cmd_test_armffa_usage(struct unit_test_state *uts)
{
	/* no sub-command at all */
	ut_asserteq(1, run_command("armffa", 0));
	ut_assertok(check_usage(uts));

	/* a sub-command which does not exist */
	ut_asserteq(1, run_command("armffa bogus", 0));
	ut_assertok(check_usage(uts));

	/* one argument too many for the sub-command */
	ut_asserteq(1, run_command("armffa devlist extra", 0));
	ut_assertok(check_usage(uts));

	/* getpart and ping each want their one argument */
	ut_asserteq(1, run_command("armffa getpart", 0));
	ut_assert_nextline("%sMissing argument", LOGF(FUNC_GETPART));
	ut_assertok(check_usage(uts));

	ut_asserteq(1, run_command("armffa ping", 0));
	ut_assert_nextline("%sMissing argument", LOGF(FUNC_PING));
	ut_assertok(check_usage(uts));

	/* a partition ID of zero is not a partition */
	ut_asserteq(1, run_command("armffa ping 0", 0));
	ut_assert_nextline("%sPartition ID can not be 0", LOGF(FUNC_PING));
	ut_assertok(check_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_armffa_usage, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);
