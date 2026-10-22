# SPDX-License-Identifier: GPL-2.0+
# Copyright 2026 Simon Glass <sjg@chromium.org>

"""Tests for the panic command"""

import pytest

# Text which hang() shows once panic() gives up
HANG_MSG = '### ERROR ### Please RESET the board ###'

@pytest.mark.boardspec('sandbox')
@pytest.mark.buildconfigspec('panic_hang')
@pytest.mark.restart
def test_panic_base(ubman):
    """Check that panic shows its message and stops U-Boot"""
    with ubman.disable_check('error_please_reset'):
        ubman.run_command('panic "Unrecoverable error"',
                          wait_for_prompt=False)
        ubman.wait_for('Unrecoverable error')
        ubman.wait_for(HANG_MSG)
    assert ubman.validate_exited()

@pytest.mark.boardspec('sandbox')
@pytest.mark.buildconfigspec('panic_hang')
@pytest.mark.restart
def test_panic_no_message(ubman):
    """Check that panic works with no message at all"""
    with ubman.disable_check('error_please_reset'):
        ubman.run_command('panic', wait_for_prompt=False)
        ubman.wait_for(HANG_MSG)
    assert ubman.validate_exited()

@pytest.mark.boardspec('sandbox')
@pytest.mark.buildconfigspec('panic_hang')
@pytest.mark.restart
def test_panic_percent(ubman):
    """Check that a percent sign in the message reaches the console"""
    with ubman.disable_check('error_please_reset'):
        ubman.run_command('panic "100% done"', wait_for_prompt=False)
        ubman.wait_for('100% done')
        ubman.wait_for(HANG_MSG)
    assert ubman.validate_exited()

@pytest.mark.boardspec('sandbox')
def test_panic_usage(ubman):
    """Check that a second argument shows the usage message

    The command takes at most one argument, so two words leave U-Boot
    running rather than panicking.
    """
    out = ubman.run_command('panic one two')
    assert 'panic - Panic with optional message' in out
    assert 'panic [message]' in out
