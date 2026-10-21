// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the sound command
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <console.h>
#include <dm.h>
#include <asm/test.h>
#include <test/cmd.h>
#include <test/ut.h>

/*
 * The sandbox sound driver counts and sums the audio data it is given in its
 * private data, so each test needs a driver-model tree of its own; otherwise
 * the totals carry over from one test to the next
 */
#define SOUND_TEST_FLAGS	(UTF_CONSOLE | UTF_DM | UTF_SCAN_FDT)

/*
 * Bytes of audio data a second of sound takes: 48000 samples a second, two
 * channels and 16 bits a sample, as the sandbox i2s driver reports
 */
#define SECOND_BYTES		192000

/* Bytes and samples a millisecond of sound takes */
#define MSEC_BYTES		(SECOND_BYTES / 1000)
#define MSEC_SAMPLES		(MSEC_BYTES / 2)

/*
 * What one sample adds to the sum the driver keeps, which it takes a byte at
 * a time. The amplitude is 16000, which is 0x3e80, so a positive sample adds
 * 0x3e + 0x80 and a negative one, 0xc180, adds 0xc1 + 0x80
 */
#define POS_SUM			190
#define NEG_SUM			321

/*
 * Samples of each sign in a period of a 400Hz square wave: 48000 / 400 gives
 * 60 samples a channel for each half of the period, so 120 across the two
 */
#define HALF_400HZ		120

/* Sum of a second of a 400Hz wave, which is 400 whole periods */
#define SECOND_SUM_400HZ	(400 * HALF_400HZ * (POS_SUM + NEG_SUM))

/*
 * Sum of a millisecond of a 500Hz wave of each sign. A period of that wave is
 * 96 samples a channel, so a millisecond is exactly half of one and holds
 * samples of a single sign
 */
#define MSEC_POS_500HZ		(MSEC_SAMPLES * POS_SUM)
#define MSEC_NEG_500HZ		(MSEC_SAMPLES * NEG_SUM)

/* The last line of the help text, which the usage message ends with */
#define SOUND_USAGE_LAST	"  freq - frequency in Hz"

/**
 * get_sound() - Find the sound device, with its probe message out of the way
 *
 * The sandbox i2s driver warns that it is silent when it probes, which the
 * first command to use the device would otherwise print in the middle of its
 * own output
 *
 * @uts: Test state
 * @devp: Returns the sound device
 * Return: 0 if OK, -ve on error
 */
static int get_sound(struct unit_test_state *uts, struct udevice **devp)
{
	ut_assertok(uclass_first_device_err(UCLASS_SOUND, devp));
	console_record_reset();

	return 0;
}

/* Test 'sound init', which sets the device up and plays nothing */
static int cmd_test_sound_base(struct unit_test_state *uts)
{
	struct udevice *dev;

	ut_assertok(get_sound(uts, &dev));

	ut_assertok(run_command("sound init", 0));
	ut_asserteq(1, sandbox_get_setup_called(dev));
	ut_asserteq(0, sandbox_get_sound_count(dev));
	ut_assert_console_end();

	/* each init sets the device up again */
	ut_assertok(run_command("sound init", 0));
	ut_asserteq(2, sandbox_get_setup_called(dev));
	ut_asserteq(0, sandbox_get_sound_count(dev));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_sound_base, SOUND_TEST_FLAGS);

/* Test 'sound play' with a length and a frequency */
static int cmd_test_sound_play(struct unit_test_state *uts)
{
	struct udevice *dev;

	ut_assertok(get_sound(uts, &dev));

	/* half a period of a 500Hz wave, so every sample is positive */
	ut_assertok(run_command("sound play 1 500", 0));
	ut_asserteq(MSEC_BYTES, sandbox_get_sound_count(dev));
	ut_asserteq(MSEC_POS_500HZ, sandbox_get_sound_sum(dev));

	/* the play sets the device up, so no init is needed first */
	ut_asserteq(1, sandbox_get_setup_called(dev));

	/* and all the data has been sent, so nothing is left playing */
	ut_asserteq(false, sandbox_get_sound_active(dev));
	ut_assert_console_end();

	/* a whole period adds a half of each sign */
	ut_assertok(run_command("sound play 2 500", 0));
	ut_asserteq(3 * MSEC_BYTES, sandbox_get_sound_count(dev));
	ut_asserteq(2 * MSEC_POS_500HZ + MSEC_NEG_500HZ,
		    sandbox_get_sound_sum(dev));
	ut_asserteq(2, sandbox_get_setup_called(dev));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_sound_play, SOUND_TEST_FLAGS);

/* Test the length and frequency 'sound play' falls back on */
static int cmd_test_sound_default(struct unit_test_state *uts)
{
	struct udevice *dev;

	ut_assertok(get_sound(uts, &dev));

	/* without arguments the beep lasts a second at 400Hz */
	ut_assertok(run_command("sound play", 0));
	ut_asserteq(SECOND_BYTES, sandbox_get_sound_count(dev));
	ut_asserteq(SECOND_SUM_400HZ, sandbox_get_sound_sum(dev));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_sound_default, SOUND_TEST_FLAGS);

/* Test a tune, which is a list of lengths and frequencies */
static int cmd_test_sound_tune(struct unit_test_state *uts)
{
	struct udevice *dev;

	ut_assertok(get_sound(uts, &dev));

	/*
	 * Three beeps: a rest, since a frequency of zero gives a period of no
	 * samples and so a silent wave, then a whole period of 500Hz, then a
	 * length with no frequency of its own, which keeps 500Hz
	 */
	ut_assertok(run_command("sound play 2 0 2 500 1", 0));
	ut_asserteq(5 * MSEC_BYTES, sandbox_get_sound_count(dev));
	ut_asserteq(2 * MSEC_POS_500HZ + MSEC_NEG_500HZ,
		    sandbox_get_sound_sum(dev));
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_sound_tune, SOUND_TEST_FLAGS);

/* Test the usage errors */
static int cmd_test_sound_usage(struct unit_test_state *uts)
{
	struct udevice *dev;

	ut_assertok(get_sound(uts, &dev));

	/* the command needs a sub-command */
	ut_asserteq(1, run_command("sound", 0));
	ut_assert_skip_to_line(SOUND_USAGE_LAST);
	ut_assert_nextline_empty();
	ut_assert_console_end();

	ut_asserteq(1, run_command("sound fish", 0));
	ut_assert_skip_to_line(SOUND_USAGE_LAST);
	ut_assert_nextline_empty();
	ut_assert_console_end();

	/* none of this reaches the device */
	ut_asserteq(0, sandbox_get_setup_called(dev));
	ut_asserteq(0, sandbox_get_sound_count(dev));

	return 0;
}
CMD_TEST(cmd_test_sound_usage, SOUND_TEST_FLAGS);
