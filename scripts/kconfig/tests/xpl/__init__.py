# SPDX-License-Identifier: GPL-2.0+
"""
U-Boot: the configuration of each xPL phase

The phases are listed in scripts/xpl_phases and syncconfig writes a
configuration for each one.

This checks that:
- an option with a twin for some phase, e.g. SPL_SERIAL, has the value of its
  twin in each phase, so is not enabled in a phase with no twin
- an option with no twin has the same value in every phase
- an option listed in scripts/conf_nospl is left out of the xPL phases
- an option for one phase has no prefix in that phase
- the files giving the list of phases are correct
"""


def test(conf):
    # KCONFIG_SPL makes syncconfig write the configuration of each phase too
    assert conf.syncconfig('config', extra_env={'KCONFIG_SPL': '1'}) == 0
    assert conf.file_matches('auto.conf', 'expected_auto_conf')
    assert conf.file_matches('auto_spl.conf', 'expected_auto_spl_conf')
    assert conf.file_matches('auto_tpl.conf', 'expected_auto_tpl_conf')
    assert conf.file_matches('xpl.conf', 'expected_xpl_conf')
    assert conf.file_matches('xpl_phases.h', 'expected_xpl_phases_h')
