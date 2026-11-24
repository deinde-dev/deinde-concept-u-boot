// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the bind and unbind commands
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <dm.h>
#include <dm/device-internal.h>
#include <dm/test.h>
#include <test/cmd.h>
#include <test/ut.h>

#define BIND_TEST	"/bind-test"
#define BIND_CHILD1	"/bind-test/bind-test-child1"
#define BIND_CHILD2	"/bind-test/bind-test-child2"

/* First of the two lines which both usage messages end with */
#define DM_TREE_LINE \
	"Use 'dm tree' to list all devices registered in the driver model,"

/* Check whether a device is bound at a devicetree node path */
static bool is_bound(const char *path)
{
	struct udevice *dev;

	return !device_find_global_by_ofnode(ofnode_path(path), &dev);
}

/* Test binding and unbinding by devicetree node path */
static int cmd_test_bind_base(struct unit_test_state *uts)
{
	/* the bind-test devices come from the devicetree */
	ut_assert(is_bound(BIND_TEST));
	ut_assert(is_bound(BIND_CHILD1));
	ut_assert(is_bound(BIND_CHILD2));

	/* unbinding a child leaves its parent and its sibling alone */
	ut_assertok(run_command("unbind " BIND_CHILD1, 0));
	ut_assert(!is_bound(BIND_CHILD1));
	ut_assert(is_bound(BIND_TEST));
	ut_assert(is_bound(BIND_CHILD2));
	ut_assert_console_end();

	/* and it can be bound again */
	ut_assertok(run_command("bind " BIND_CHILD1 " phy_sandbox", 0));
	ut_assert(is_bound(BIND_CHILD1));
	ut_assert_console_end();

	/* unbinding the parent takes the children with it */
	ut_assertok(run_command("unbind " BIND_TEST, 0));
	ut_assert(!is_bound(BIND_TEST));
	ut_assert(!is_bound(BIND_CHILD1));
	ut_assert(!is_bound(BIND_CHILD2));
	ut_assert_console_end();

	/* binding the parent again brings the children back from the tree */
	ut_assertok(run_command("bind " BIND_TEST " simple_bus", 0));
	ut_assert(is_bound(BIND_TEST));
	ut_assert(is_bound(BIND_CHILD1));
	ut_assert(is_bound(BIND_CHILD2));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_bind_base, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test binding and unbinding by uclass and sequence number */
static int cmd_test_bind_class(struct unit_test_state *uts)
{
	struct udevice *parent, *dev;
	int seq;

	ut_assertok(device_find_global_by_ofnode(ofnode_path(BIND_CHILD2),
						 &parent));
	seq = dev_seq(parent);
	device_find_first_child(parent, &dev);
	ut_assertnull(dev);

	/* bind a new device as a child of bind-test-child2 */
	ut_assertok(run_commandf("bind simple_bus %d simple_bus", seq));
	ut_assert_console_end();

	device_find_first_child(parent, &dev);
	ut_assertnonnull(dev);
	ut_asserteq_str("simple_bus", dev->name);

	/* remove it again, by the uclass and sequence number it was given */
	ut_assertok(run_commandf("unbind simple_bus %d", dev_seq(dev)));
	ut_assert_console_end();

	device_find_first_child(parent, &dev);
	ut_assertnull(dev);

	/* the parent and the driver name select the same device */
	ut_assertok(run_commandf("bind simple_bus %d simple_bus", seq));
	ut_assertok(run_commandf("unbind simple_bus %d simple_bus", seq));
	ut_assert_console_end();

	device_find_first_child(parent, &dev);
	ut_assertnull(dev);

	/* unbinding a child which is not there is not an error */
	ut_assertok(run_commandf("unbind simple_bus %d simple_bus", seq));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_bind_class, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* Test the error paths of bind and unbind */
static int cmd_test_bind_missing(struct unit_test_state *uts)
{
	/* a node path which is not in the devicetree */
	ut_asserteq(1, run_command("bind /not-a-node simple_bus", 0));
	ut_assert_nextline("/not-a-node is not a valid node path");
	ut_assert_console_end();

	ut_asserteq(1, run_command("unbind /not-a-node", 0));
	ut_assert_nextline("/not-a-node is not a valid node path");
	ut_assert_console_end();

	/* a driver name which no driver has */
	ut_asserteq(1, run_command("bind " BIND_TEST " not-a-driver", 0));
	ut_assert_nextline("not-a-driver is not a valid driver name");
	ut_assert_console_end();

	ut_asserteq(1, run_command("bind simple_bus 0 not-a-driver", 0));
	ut_assert_nextline("Cannot find driver 'not-a-driver'");
	ut_assert_console_end();

	ut_asserteq(1, run_command("unbind simple_bus 0 not-a-driver", 0));
	ut_assert_nextline("Cannot find driver 'not-a-driver'");
	ut_assert_console_end();

	/* a uclass name which no uclass has */
	ut_asserteq(1, run_command("bind not-a-class 0 simple_bus", 0));
	ut_assert_nextline("not-a-class is not a valid uclass");
	ut_assert_console_end();

	ut_asserteq(1, run_command("unbind not-a-class 0", 0));
	ut_assert_nextline("not-a-class is not a valid uclass");
	ut_assert_console_end();

	/* a sequence number which that uclass does not reach */
	ut_asserteq(1, run_command("bind simple_bus 99 simple_bus", 0));
	ut_assert_nextline("Cannot find device 99 of class simple_bus");
	ut_assert_console_end();

	ut_asserteq(1, run_command("unbind simple_bus 99", 0));
	ut_assert_nextline("Cannot find device 99 of class simple_bus");
	ut_assert_console_end();

	/* a node which is in the tree but has no device bound to it */
	ut_assertok(run_command("unbind " BIND_CHILD1, 0));
	ut_assert_console_end();

	ut_asserteq(1, run_command("unbind " BIND_CHILD1, 0));
	ut_assert_nextline("Cannot find a device with path " BIND_CHILD1);
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_bind_missing, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);

/* The last two lines of the usage message, which both commands share */
static int check_usage_tail(struct unit_test_state *uts)
{
	ut_assert_nextline(DM_TREE_LINE);
	ut_assert_nextline("their path, class, sequence and current driver.");
	ut_assert_nextline_empty();
	ut_assert_console_end();

	return 0;
}

/* Check the usage message shown by the bind command */
static int check_bind_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("bind - Bind a device to a driver");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("bind <node path> <driver>");
	ut_assert_nextline("bind <class> <seq> <driver>");
	ut_assertok(check_usage_tail(uts));

	return 0;
}

/* Check the usage message shown by the unbind command */
static int check_unbind_usage(struct unit_test_state *uts)
{
	ut_assert_nextline("unbind - Unbind a device from a driver");
	ut_assert_nextline_empty();
	ut_assert_nextline("Usage:");
	ut_assert_nextline("unbind <node path>");
	ut_assert_nextline("unbind <class> <seq>");
	ut_assert_nextline("unbind <class> <seq> <driver>");
	ut_assertok(check_usage_tail(uts));

	return 0;
}

/* Check the usage message shown for a wrong number of arguments */
static int cmd_test_bind_usage(struct unit_test_state *uts)
{
	/* bind needs a driver name with a node path */
	ut_asserteq(1, run_command("bind", 0));
	ut_assertok(check_bind_usage(uts));

	ut_asserteq(1, run_command("bind " BIND_TEST, 0));
	ut_assertok(check_bind_usage(uts));

	/* and a driver name with a uclass and a sequence number */
	ut_asserteq(1, run_command("bind simple_bus 0", 0));
	ut_assertok(check_bind_usage(uts));

	/* unbind takes a node path on its own */
	ut_asserteq(1, run_command("unbind", 0));
	ut_assertok(check_unbind_usage(uts));

	ut_asserteq(1, run_command("unbind " BIND_TEST " simple_bus", 0));
	ut_assertok(check_unbind_usage(uts));

	/* but needs a sequence number with a uclass */
	ut_asserteq(1, run_command("unbind simple_bus", 0));
	ut_assertok(check_unbind_usage(uts));

	/*
	 * neither command has an option, so one is refused rather than read
	 * as a uclass name, which is what it meant before the getopt
	 * conversion
	 */
	ut_asserteq(1, run_command("bind -x " BIND_TEST " simple_bus", 0));
	ut_assertok(check_bind_usage(uts));

	ut_asserteq(1, run_command("unbind -x " BIND_TEST, 0));
	ut_assertok(check_unbind_usage(uts));

	return 0;
}
CMD_TEST(cmd_test_bind_usage, UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT);
