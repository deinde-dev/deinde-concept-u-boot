// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests that the PCI Maximum Payload Size (MPS) command can set the sandbox
 * PCI Express device to safe mode and determine the correct payload size.
 *
 * Copyright 2023 Microsoft
 * Written by Stephen Carlson <stcarlso@linux.microsoft.com>
 */

#include <console.h>
#include <test/ut.h>

#define PCI_MPS_TEST(_name, _flags) UNIT_TEST(_name, _flags, pci_mps)

/* Last line of the usage message, too long to fit in 80 columns */
#define USAGE_LAST_LINE \
	"    - Set PCI Express MPS of all devices to support hotplug and peer-to-peer DMA"

/* Check that an invalid invocation shows the usage message */
static int check_usage(struct unit_test_state *uts, const char *cmd)
{
	ut_asserteq(1, run_command(cmd, 0));
	ut_assert_nextline("pci_mps - configure PCI Express MPS");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("pci_mps safe");
	ut_assert_nextline("    - Set PCI Express MPS of all devices to safe values");
	ut_assert_nextline("pci_mps peer2peer");
	ut_assert_nextline(USAGE_LAST_LINE);
	ut_assert_nextline_empty();
	ut_assert_console_end();

	return 0;
}

/* Test "pci_mps" command in safe "s" mode */
static int test_pci_mps_safe(struct unit_test_state *uts)
{
	/* Enumerate PCI Express first */
	ut_assertok(run_command("pci e", 0));
	ut_assert_console_end();

	/* Test pci_mps s */
	ut_assertok(run_command("pci_mps s", 0));
	ut_assert_nextline("Setting MPS of all devices to 256B");
	ut_assert_console_end();

	return 0;
}
PCI_MPS_TEST(test_pci_mps_safe, UTF_CONSOLE);

/* Test "pci_mps" command in peer-to-peer "p" mode */
static int test_pci_mps_peer2peer(struct unit_test_state *uts)
{
	/* Enumerate PCI Express first */
	ut_assertok(run_command("pci e", 0));
	ut_assert_console_end();

	/* Every device supports 128B, so that is what peer2peer asks for */
	ut_assertok(run_command("pci_mps peer2peer", 0));
	ut_assert_nextline("Setting MPS of all devices to 128B");
	ut_assert_console_end();

	/* Only the first letter is examined */
	ut_assertok(run_command("pci_mps p", 0));
	ut_assert_nextline("Setting MPS of all devices to 128B");
	ut_assert_console_end();

	return 0;
}
PCI_MPS_TEST(test_pci_mps_peer2peer, UTF_CONSOLE);

/* Test that "pci_mps" shows its usage message when it cannot do anything */
static int test_pci_mps_usage(struct unit_test_state *uts)
{
	/* No policy at all */
	ut_assertok(check_usage(uts, "pci_mps"));

	/* A word which is neither 'safe' nor 'peer2peer' */
	ut_assertok(check_usage(uts, "pci_mps bogus"));

	/* More arguments than the command takes */
	ut_assertok(check_usage(uts, "pci_mps safe peer2peer"));

	return 0;
}
PCI_MPS_TEST(test_pci_mps_usage, UTF_CONSOLE);
