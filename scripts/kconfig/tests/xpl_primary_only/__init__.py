# SPDX-License-Identifier: GPL-2.0+
"""
U-Boot: a list of phases with only the primary phase

The list of phases is read when a 'phases' line is parsed and again when the
configuration of each phase is written. It must be read only once.

This checks that a list with no xPL phases works, since the number of those
cannot be used to tell whether the list is read yet.
"""


def test_config(conf):
    assert conf.olddefconfig('config') == 0
    assert conf.config_contains('expected_config')


def test_syncconfig(conf):
    # KCONFIG_SPL makes syncconfig write the configuration of each phase too
    assert conf.syncconfig('config', extra_env={'KCONFIG_SPL': '1'}) == 0
