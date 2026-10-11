/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2023 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __TEST_CMD_H__
#define __TEST_CMD_H__

#include <test/test.h>

/* Declare a new command test */
#define CMD_TEST(_name, _flags) UNIT_TEST(_name, _flags, cmd)

/**
 * CMD_TEST_ARGS() - Declare a command test which takes arguments
 *
 * Like CMD_TEST() but for a test which the Python side runs with run_ut(),
 * passing values the test reads with ut_str() and friends. Such a test needs
 * UTF_MANUAL, since it cannot run without that setup.
 *
 * Example:
 *   CMD_TEST_ARGS(cmd_test_mine_norun, UTF_CONSOLE | UTF_MANUAL,
 *		   { "fs_image", UT_ARG_STR });
 *
 * @_name:	Name of test function
 * @_flags:	Flags for the test (see enum ut_flags)
 * @...:	Argument definitions (struct ut_arg_def initialisers)
 */
#define CMD_TEST_ARGS(_name, _flags, ...) \
	UNIT_TEST_ARGS(_name, _flags, cmd, __VA_ARGS__)

#endif /* __TEST_CMD_H__ */
