// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright 2021 Google LLC
 */

#include <cros_ec.h>
#include <dm.h>
#include <asm/test.h>
#include <dm/test.h>
#include <test/ut.h>

static int dm_test_cros_ec_hello(struct unit_test_state *uts)
{
	struct udevice *dev;
	uint val;

	ut_assertok(uclass_first_device_err(UCLASS_CROS_EC, &dev));

	ut_assertok(cros_ec_hello(dev, NULL));

	val = 0xdead1357;
	ut_assertok(cros_ec_hello(dev, &val));
	ut_asserteq(0xdead1357, val);

	sandbox_cros_ec_set_test_flags(dev, CROSECT_BREAK_HELLO);
	ut_asserteq(-ENOTSYNC, cros_ec_hello(dev, &val));
	ut_asserteq(0x12345678, val);

	return 0;
}
DM_TEST(dm_test_cros_ec_hello, UTF_SCAN_FDT);

static int dm_test_cros_ec_sku_id(struct unit_test_state *uts)
{
	struct udevice *dev;

	ut_assertok(uclass_first_device_err(UCLASS_CROS_EC, &dev));
	ut_asserteq(1234, cros_ec_get_sku_id(dev));

	/* try the command */
	ut_assertok(run_command("crosec sku", 0));
	ut_assert_nextline("1234");
	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_cros_ec_sku_id, UTF_SCAN_FDT | UTF_CONSOLE);

static int dm_test_cros_ec_features(struct unit_test_state *uts)
{
	struct udevice *dev;
	u64 feat;

	ut_assertok(uclass_first_device_err(UCLASS_CROS_EC, &dev));
	ut_assertok(cros_ec_get_features(dev, &feat));
	ut_asserteq_64(1U << EC_FEATURE_FLASH | 1U << EC_FEATURE_I2C |
		1u << EC_FEATURE_VSTORE |
		1ULL << EC_FEATURE_UNIFIED_WAKE_MASKS | 1ULL << EC_FEATURE_ISH,
		feat);

	ut_asserteq(true, cros_ec_check_feature(dev, EC_FEATURE_I2C));
	ut_asserteq(false, cros_ec_check_feature(dev, EC_FEATURE_MOTION_SENSE));
	ut_asserteq(true, cros_ec_check_feature(dev, EC_FEATURE_ISH));

	/* try the command */
	ut_assertok(run_command("crosec features", 0));
	ut_assert_nextline("flash");
	ut_assert_nextline("i2c");
	ut_assert_nextline("vstore");
	ut_assert_nextline("unified_wake_masks");
	ut_assert_nextline("ish");
	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_cros_ec_features, UTF_SCAN_FDT | UTF_CONSOLE);

static int dm_test_cros_ec_switches(struct unit_test_state *uts)
{
	struct udevice *dev;

	ut_assertok(uclass_first_device_err(UCLASS_CROS_EC, &dev));
	ut_asserteq(0, cros_ec_get_switches(dev));

	/* try the command */
	ut_assertok(run_command("crosec switches", 0));
	ut_assert_console_end();

	/* Open the lid and check the switch changes */
	sandbox_cros_ec_set_test_flags(dev, CROSECT_LID_OPEN);
	ut_asserteq(EC_SWITCH_LID_OPEN, cros_ec_get_switches(dev));

	/* try the command */
	ut_assertok(run_command("crosec switches", 0));
	ut_assert_nextline("lid open");
	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_cros_ec_switches, UTF_SCAN_FDT | UTF_CONSOLE);

static int dm_test_cros_ec_events(struct unit_test_state *uts)
{
	struct udevice *dev;
	u32 events;

	ut_assertok(uclass_first_device_err(UCLASS_CROS_EC, &dev));
	ut_assertok(cros_ec_get_host_events(dev, &events));
	ut_asserteq(0, events);

	/* try the command */
	ut_assertok(run_command("crosec events", 0));
	ut_assert_nextline("00000000");
	ut_assert_console_end();

	/* Open the lid and check the event appears */
	sandbox_cros_ec_set_test_flags(dev, CROSECT_LID_OPEN);
	ut_assertok(cros_ec_get_host_events(dev, &events));
	ut_asserteq(EC_HOST_EVENT_MASK(EC_HOST_EVENT_LID_OPEN), events);

	/* try the command */
	ut_assertok(run_command("crosec events", 0));
	ut_assert_nextline("00000002");
	ut_assert_nextline("lid_open");
	ut_assert_console_end();

	/* Clear the event */
	ut_assertok(cros_ec_clear_host_events(dev,
		EC_HOST_EVENT_MASK(EC_HOST_EVENT_LID_OPEN)));
	ut_assertok(cros_ec_get_host_events(dev, &events));
	ut_asserteq(0, events);

	return 0;
}
DM_TEST(dm_test_cros_ec_events, UTF_SCAN_FDT | UTF_CONSOLE);

static int dm_test_cros_ec_vstore(struct unit_test_state *uts)
{
	const int size = EC_VSTORE_SLOT_SIZE;
	u8 test_data[size], data[size];
	struct udevice *dev;
	u32 locked;
	int i;

	ut_assertok(uclass_first_device_err(UCLASS_CROS_EC, &dev));
	ut_asserteq(true, cros_ec_vstore_supported(dev));

	ut_asserteq(4, cros_ec_vstore_info(dev, &locked));
	ut_asserteq(0, locked);

	/* Write some data */
	for (i = 0; i < size; i++)
		test_data[i] = ' ' + i;
	ut_assertok(cros_ec_vstore_write(dev, 2, test_data, size));

	/* Check it is locked */
	ut_asserteq(4, cros_ec_vstore_info(dev, &locked));
	ut_asserteq(1 << 2, locked);

	/* Read it back and compare */
	ut_assertok(cros_ec_vstore_read(dev, 2, data));
	ut_asserteq_mem(test_data, data, size);

	/* Try another slot to make sure it is empty */
	ut_assertok(cros_ec_vstore_read(dev, 0, data));
	for (i = 0; i < size; i++)
		ut_asserteq(0, data[i]);

	return 0;
}
DM_TEST(dm_test_cros_ec_vstore, UTF_SCAN_FDT);

/* Test that the EC refuses flash accesses which go past the end of its flash */
static int dm_test_cros_ec_flash(struct unit_test_state *uts)
{
	struct ec_response_flash_info info;
	u8 data[4] = {1, 2, 3, 4}, buf[4];
	struct udevice *dev;
	uint size;

	ut_assertok(uclass_first_device_err(UCLASS_CROS_EC, &dev));
	ut_assertok(cros_ec_read_flashinfo(dev, &info));
	size = info.flash_size;

	/* the last few bytes can be written and read back */
	ut_assertok(cros_ec_flash_write(dev, data, size - 4, 4));
	ut_assertok(cros_ec_flash_read(dev, buf, size - 4, 4));
	ut_asserteq_mem(data, buf, 4);

	/* but nothing past the end */
	ut_asserteq(-EC_RES_INVALID_PARAM,
		    cros_ec_flash_erase(dev, size - 4, 8));
	ut_asserteq(-1, cros_ec_flash_write(dev, data, size - 2, 4));
	ut_asserteq(-1, cros_ec_flash_read(dev, buf, size - 2, 4));

	return 0;
}
DM_TEST(dm_test_cros_ec_flash, UTF_SCAN_FDT);

/* Test the host-event masks, and the events read through EC_CMD_HOST_EVENT */
static int dm_test_cros_ec_host_event_mask(struct unit_test_state *uts)
{
	u64 lid_open = EC_HOST_EVENT_MASK(EC_HOST_EVENT_LID_OPEN);
	struct udevice *dev;
	u32 mask;

	ut_assertok(uclass_first_device_err(UCLASS_CROS_EC, &dev));

	/* the EC supports unified host events, so these use them */
	ut_asserteq(0, cros_ec_get_events_b(dev));
	sandbox_cros_ec_set_test_flags(dev, CROSECT_LID_OPEN);
	ut_asserteq(lid_open, cros_ec_get_events_b(dev));
	ut_assertok(cros_ec_clear_events_b(dev, lid_open));
	ut_asserteq(0, cros_ec_get_events_b(dev));

	/* each mask is separate */
	ut_assertok(cros_ec_set_event_mask(dev, EC_CMD_HOST_EVENT_SET_SCI_MASK,
					   0x1234));
	ut_assertok(cros_ec_set_event_mask(dev, EC_CMD_HOST_EVENT_SET_WAKE_MASK,
					   0x5678));
	ut_assertok(cros_ec_get_event_mask(dev, EC_CMD_HOST_EVENT_GET_SCI_MASK,
					   &mask));
	ut_asserteq(0x1234, mask);
	ut_assertok(cros_ec_get_event_mask(dev, EC_CMD_HOST_EVENT_GET_WAKE_MASK,
					   &mask));
	ut_asserteq(0x5678, mask);

	/* the lid-shutdown setting is a bit in the SMI mask */
	ut_asserteq(0, cros_ec_get_lid_shutdown_mask(dev));
	ut_assertok(cros_ec_set_lid_shutdown_mask(dev, true));
	ut_assert_nextline("EC: enabled lid close event");
	ut_asserteq(1, cros_ec_get_lid_shutdown_mask(dev));
	ut_assertok(cros_ec_get_event_mask(dev, EC_CMD_HOST_EVENT_GET_SMI_MASK,
					   &mask));
	ut_asserteq(EC_HOST_EVENT_MASK(EC_HOST_EVENT_LID_CLOSED), mask);

	ut_assertok(cros_ec_set_lid_shutdown_mask(dev, false));
	ut_assert_nextline("EC: disabled lid close event");
	ut_asserteq(0, cros_ec_get_lid_shutdown_mask(dev));
	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_cros_ec_host_event_mask, UTF_SCAN_FDT | UTF_CONSOLE);
