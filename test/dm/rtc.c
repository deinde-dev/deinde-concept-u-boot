// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2015 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <console.h>
#include <dm.h>
#include <i2c.h>
#include <log.h>
#include <malloc.h>
#include <mapmem.h>
#include <rtc.h>
#include <asm/io.h>
#include <asm/rtc.h>
#include <asm/test.h>
#include <dm/test.h>
#include <test/test.h>
#include <test/ut.h>

/* Simple RTC sanity check */
static int dm_test_rtc_base(struct unit_test_state *uts)
{
	struct udevice *dev;

	ut_asserteq(-ENODEV, uclass_get_device_by_seq(UCLASS_RTC, 2, &dev));
	ut_assertok(uclass_get_device(UCLASS_RTC, 0, &dev));
	ut_assertok(uclass_get_device(UCLASS_RTC, 1, &dev));

	return 0;
}
DM_TEST(dm_test_rtc_base, UTF_SCAN_PDATA | UTF_SCAN_FDT);

static void show_time(const char *msg, struct rtc_time *time)
{
	printf("%s: %02d/%02d/%04d %02d:%02d:%02d\n", msg,
	       time->tm_mday, time->tm_mon, time->tm_year,
	       time->tm_hour, time->tm_min, time->tm_sec);
}

static int cmp_times(struct rtc_time *expect, struct rtc_time *time, bool show)
{
	bool same;

	same = expect->tm_sec == time->tm_sec;
	same &= expect->tm_min == time->tm_min;
	same &= expect->tm_hour == time->tm_hour;
	same &= expect->tm_mday == time->tm_mday;
	same &= expect->tm_mon == time->tm_mon;
	same &= expect->tm_year == time->tm_year;
	if (!same && show) {
		show_time("expected", expect);
		show_time("actual", time);
	}

	return same ? 0 : -EINVAL;
}

/* Set and get the time */
static int dm_test_rtc_set_get(struct unit_test_state *uts)
{
	struct rtc_time now, time, cmp;
	struct udevice *dev, *emul;
	long offset, check_offset, old_offset, old_base_time;
	int i;

	ut_assertok(uclass_get_device(UCLASS_RTC, 0, &dev));

	ut_assertok(i2c_emul_find(dev, &emul));
	ut_assertnonnull(emul);

	/* Get the offset, putting the RTC into manual mode */
	i = 0;
	do {
		check_offset = sandbox_i2c_rtc_set_offset(emul, false, 0);
		ut_assertok(dm_rtc_get(dev, &now));

		/* Tell the RTC to go into manual mode */
		old_offset = sandbox_i2c_rtc_set_offset(emul, false, 0);

		/* If the times changed in that period, read it again */
	} while (++i < 2 && check_offset != old_offset);
	ut_asserteq(check_offset, old_offset);

	old_base_time = sandbox_i2c_rtc_get_set_base_time(emul, -1);

	memset(&time, '\0', sizeof(time));
	time.tm_mday = 3;
	time.tm_mon = 6;
	time.tm_year = 2004;
	time.tm_sec = 0;
	time.tm_min = 18;
	time.tm_hour = 18;
	ut_assertok(dm_rtc_set(dev, &time));

	memset(&cmp, '\0', sizeof(cmp));
	ut_assertok(dm_rtc_get(dev, &cmp));
	ut_assertok(cmp_times(&time, &cmp, true));

	memset(&time, '\0', sizeof(time));
	time.tm_mday = 31;
	time.tm_mon = 8;
	time.tm_year = 2004;
	time.tm_sec = 0;
	time.tm_min = 18;
	time.tm_hour = 18;
	ut_assertok(dm_rtc_set(dev, &time));

	memset(&cmp, '\0', sizeof(cmp));
	ut_assertok(dm_rtc_get(dev, &cmp));
	ut_assertok(cmp_times(&time, &cmp, true));

	/* Increment by 1 second */
	offset = sandbox_i2c_rtc_set_offset(emul, false, 0);
	sandbox_i2c_rtc_set_offset(emul, false, offset + 1);

	memset(&cmp, '\0', sizeof(cmp));
	ut_assertok(dm_rtc_get(dev, &cmp));
	ut_asserteq(1, cmp.tm_sec);

	/* Check against original offset */
	sandbox_i2c_rtc_set_offset(emul, false, old_offset);
	ut_assertok(dm_rtc_get(dev, &cmp));
	ut_assertok(cmp_times(&now, &cmp, true));

	/* Back to the original offset */
	sandbox_i2c_rtc_set_offset(emul, false, 0);
	memset(&cmp, '\0', sizeof(cmp));
	ut_assertok(dm_rtc_get(dev, &cmp));
	ut_assertok(cmp_times(&now, &cmp, true));

	/* Increment the base time by 1 emul */
	sandbox_i2c_rtc_get_set_base_time(emul, old_base_time + 1);
	memset(&cmp, '\0', sizeof(cmp));
	ut_assertok(dm_rtc_get(dev, &cmp));
	if (now.tm_sec == 59) {
		ut_asserteq(0, cmp.tm_sec);
	} else {
		ut_asserteq(now.tm_sec + 1, cmp.tm_sec);
	}

	/* return RTC to normal mode */
	sandbox_i2c_rtc_set_offset(emul, true, 0);

	return 0;
}
DM_TEST(dm_test_rtc_set_get, UTF_SCAN_PDATA | UTF_SCAN_FDT);

static int dm_test_rtc_read_write(struct unit_test_state *uts)
{
	struct rtc_time time;
	struct udevice *dev, *emul;
	long old_offset;
	u8 buf[4], reg;

	ut_assertok(uclass_get_device(UCLASS_RTC, 0, &dev));

	memcpy(buf, "car", 4);
	ut_assertok(dm_rtc_write(dev, REG_AUX0, buf, 4));
	memset(buf, '\0', sizeof(buf));
	ut_assertok(dm_rtc_read(dev, REG_AUX0, buf, 4));
	ut_asserteq(memcmp(buf, "car", 4), 0);

	reg = 'b';
	ut_assertok(dm_rtc_write(dev, REG_AUX0, &reg, 1));
	memset(buf, '\0', sizeof(buf));
	ut_assertok(dm_rtc_read(dev, REG_AUX0, buf, 4));
	ut_asserteq(memcmp(buf, "bar", 4), 0);

	reg = 't';
	ut_assertok(dm_rtc_write(dev, REG_AUX2, &reg, 1));
	memset(buf, '\0', sizeof(buf));
	ut_assertok(dm_rtc_read(dev, REG_AUX1, buf, 3));
	ut_asserteq(memcmp(buf, "at", 3), 0);

	ut_assertok(i2c_emul_find(dev, &emul));
	ut_assertnonnull(emul);

	old_offset = sandbox_i2c_rtc_set_offset(emul, false, 0);
	ut_assertok(dm_rtc_get(dev, &time));

	ut_assertok(dm_rtc_read(dev, REG_SEC, &reg, 1));
	ut_asserteq(time.tm_sec, reg);
	ut_assertok(dm_rtc_read(dev, REG_MDAY, &reg, 1));
	ut_asserteq(time.tm_mday, reg);

	sandbox_i2c_rtc_set_offset(emul, true, old_offset);

	return 0;
}
DM_TEST(dm_test_rtc_read_write, UTF_SCAN_PDATA | UTF_SCAN_FDT);

/* Test 'rtc list' command */
static int dm_test_rtc_cmd_list(struct unit_test_state *uts)
{
	run_command("rtc list", 0);
	ut_assert_nextline("RTC #0 - rtc@43");
	ut_assert_nextline("RTC #1 - rtc@61");
	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_rtc_cmd_list, UTF_SCAN_PDATA | UTF_SCAN_FDT | UTF_CONSOLE);

/* Test 'rtc read' and 'rtc write' commands */
static int dm_test_rtc_cmd_rw(struct unit_test_state *uts)
{
	run_command("rtc dev 0", 0);
	ut_assert_nextline("RTC #0 - rtc@43");
	ut_assert_console_end();

	run_command("rtc write 0x30 aabb", 0);
	ut_assert_console_end();

	run_command("rtc read 0x30 2", 0);
	ut_assert_nextline("00000030: aa bb                                            ..");
	ut_assert_console_end();

	run_command("rtc dev 1", 0);
	ut_assert_nextline("RTC #1 - rtc@61");
	ut_assert_console_end();

	run_command("rtc write 0x30 ccdd", 0);
	ut_assert_console_end();

	run_command("rtc read 0x30 2", 0);
	ut_assert_nextline("00000030: cc dd                                            ..");
	ut_assert_console_end();

	/*
	 * Switch back to device #0, check that its aux registers
	 * still have the same values.
	 */
	run_command("rtc dev 0", 0);
	ut_assert_nextline("RTC #0 - rtc@43");
	ut_assert_console_end();

	run_command("rtc read 0x30 2", 0);
	ut_assert_nextline("00000030: aa bb                                            ..");
	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_rtc_cmd_rw, UTF_SCAN_PDATA | UTF_SCAN_FDT | UTF_CONSOLE);

/* Test 'rtc read' and 'rtc write' with a memory address */
static int dm_test_rtc_cmd_mem(struct unit_test_state *uts)
{
	ulong addr;
	u8 *buf;

	buf = calloc(1, 4);
	ut_assertnonnull(buf);
	addr = map_to_sysmem(buf);

	ut_assertok(run_command("rtc dev 0", 0));
	ut_assert_nextline("RTC #0 - rtc@43");

	/* write the four aux registers from memory */
	buf[0] = 0x12;
	buf[1] = 0x34;
	buf[2] = 0x56;
	buf[3] = 0x78;
	ut_assertok(run_commandf("rtc write 0x30 4 0x%lx", addr));
	ut_assert_console_end();

	ut_assertok(run_command("rtc read 0x30 4", 0));
	ut_assert_nextline("00000030: 12 34 56 78                                      .4Vx");
	ut_assert_console_end();

	/* read them back into memory, which shows nothing */
	memset(buf, '\0', 4);
	ut_assertok(run_commandf("rtc read 0x30 4 0x%lx", addr));
	ut_assert_console_end();

	ut_asserteq(0x12, buf[0]);
	ut_asserteq(0x34, buf[1]);
	ut_asserteq(0x56, buf[2]);
	ut_asserteq(0x78, buf[3]);

	free(buf);

	return 0;
}
DM_TEST(dm_test_rtc_cmd_mem, UTF_SCAN_PDATA | UTF_SCAN_FDT | UTF_CONSOLE);

/* Test the errors which the rtc command reports */
static int dm_test_rtc_cmd_err(struct unit_test_state *uts)
{
	ut_assertok(run_command("rtc dev 0", 0));
	ut_assert_nextline("RTC #0 - rtc@43");

	/* a device which does not exist leaves the current one alone */
	ut_asserteq(1, run_command("rtc dev 2", 0));
	ut_assert_nextline("Cannot find RTC #2: err=-19");
	ut_assertok(run_command("rtc dev", 0));
	ut_assert_nextline("RTC #0 - rtc@43");

	/* a hex string with an odd number of digits */
	ut_asserteq(1, run_command("rtc write 0x30 abc", 0));
	ut_assert_nextline("invalid hex string");

	/* a hex string which is not hex */
	ut_asserteq(1, run_command("rtc write 0x30 zz", 0));
	ut_assert_nextline("invalid hex string");

	/* a count of zero reads nothing */
	ut_assertok(run_command("rtc read 0x30 0", 0));

	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_rtc_cmd_err, UTF_SCAN_PDATA | UTF_SCAN_FDT | UTF_CONSOLE);

/* Test the rtc command showing its usage message */
static int dm_test_rtc_cmd_usage(struct unit_test_state *uts)
{
	static const char * const cmds[] = {
		"rtc",				/* no sub-command */
		"rtc bogus",			/* an unknown sub-command */
		"rtc -x",			/* an option, which it has none of */
		"rtc read",			/* no register */
		"rtc read 0x30",		/* no count */
		"rtc read 0x30 4 1000 2000",	/* too many arguments */
		"rtc write 0x30",		/* nothing to write */
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(cmds); i++) {
		ut_asserteq(1, run_command(cmds[i], 0));
		ut_assert_nextline("rtc - RTC subsystem");
		ut_assert_nextline_empty();

		/*
		 * The rest of the message is the long help, which
		 * CONFIG_SYS_LONGHELP can leave out
		 */
		console_record_reset();
	}

	ut_assert_console_end();

	return 0;
}
DM_TEST(dm_test_rtc_cmd_usage, UTF_SCAN_PDATA | UTF_SCAN_FDT | UTF_CONSOLE);

/* Reset the time */
static int dm_test_rtc_reset(struct unit_test_state *uts)
{
	struct rtc_time now;
	struct udevice *dev, *emul;
	long old_base_time, base_time;
	int i;

	ut_assertok(uclass_get_device(UCLASS_RTC, 0, &dev));
	ut_assertok(dm_rtc_get(dev, &now));

	ut_assertok(i2c_emul_find(dev, &emul));
	ut_assertnonnull(emul);

	i = 0;
	do {
		old_base_time = sandbox_i2c_rtc_get_set_base_time(emul, 0);

		ut_asserteq(0, sandbox_i2c_rtc_get_set_base_time(emul, -1));

		ut_assertok(dm_rtc_reset(dev));
		base_time = sandbox_i2c_rtc_get_set_base_time(emul, -1);

		/*
		 * Resetting the RTC should put the base time back to normal.
		 * Allow for a one-timeadjustment in case the time flips over
		 * while this test process is pre-empted (either by a second
		 * or a daylight-saving change), since reset_time() in
		 * i2c_rtc_emul.c reads the time from the OS.
		 */
	} while (++i < 2 && base_time != old_base_time);
	ut_asserteq(old_base_time, base_time);

	return 0;
}
DM_TEST(dm_test_rtc_reset, UTF_SCAN_PDATA | UTF_SCAN_FDT);

/* Check that two RTC devices can be used independently */
static int dm_test_rtc_dual(struct unit_test_state *uts)
{
	struct rtc_time now1, cmp;
	struct udevice *dev1, *dev2;
	struct udevice *emul1, *emul2;

	ut_assertok(uclass_get_device(UCLASS_RTC, 0, &dev1));
	ut_assertok(uclass_get_device(UCLASS_RTC, 1, &dev2));

	ut_assertok(i2c_emul_find(dev1, &emul1));
	ut_assertnonnull(emul1);
	ut_assertok(i2c_emul_find(dev2, &emul2));
	ut_assertnonnull(emul2);

	/*
	 * Put both emulators in manual mode before reading, to avoid a race
	 * with system time. With use_system_time=true, times are based on
	 * os_localtime() which advances with real time; with it false, they
	 * use the fixed base_time captured at bind. Use a large offset
	 * difference (10s) to account for the fact that each emulator has
	 * its own base_time, which could differ slightly.
	 */
	sandbox_i2c_rtc_set_offset(emul1, false, 0);
	ut_assertok(dm_rtc_get(dev1, &now1));

	sandbox_i2c_rtc_set_offset(emul2, false, 10);
	memset(&cmp, '\0', sizeof(cmp));
	ut_assertok(dm_rtc_get(dev2, &cmp));
	ut_asserteq(-EINVAL, cmp_times(&now1, &cmp, false));

	memset(&cmp, '\0', sizeof(cmp));
	ut_assertok(dm_rtc_get(dev1, &cmp));
	ut_assertok(cmp_times(&now1, &cmp, true));

	return 0;
}
DM_TEST(dm_test_rtc_dual, UTF_SCAN_PDATA | UTF_SCAN_FDT);
