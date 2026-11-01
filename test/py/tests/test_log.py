# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2016, Google Inc.
#
# U-Boot Verified Boot Test

"""
This tests U-Boot logging. The 'log format' and 'log rec' commands are checked
by cmd_test_log_format in test/cmd/log.c; what is left here needs U-Boot to be
restarted, which only a Python test can do.
"""

import pytest

@pytest.mark.buildconfigspec('debug_uart')
@pytest.mark.boardspec('sandbox')
@pytest.mark.restart
def test_log_dropped(ubman):
    """Test dropped 'log' message when debug_uart is activated"""

    ubman.restart_uboot()
    output = ubman.get_spawn_output().replace('\r', '')
    assert (not 'debug: main' in output)
