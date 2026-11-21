# SPDX-License-Identifier: GPL-2.0+
"""
U-Boot: options which are defined once for several phases

An entry with a 'phases' line creates a symbol for each phase listed, e.g.
SPL_SERIAL and TPL_SERIAL from SERIAL. The names of the phases come from
scripts/xpl_phases, with 'all' meaning every phase. The entry refers to
other options by their base name, which means the symbol for the phase in each
case.

This checks that:
- each phase has its own value
- a '!' before the name of a phase leaves that phase out
- 'depends on' and 'select' use the symbol for the phase
- a twin which is written out by hand is used where there is one
- an option whose only twin is for another phase is off
- several entries for an option are combined
- an 'if' block around a later entry applies only to what it adds
- an option can be left out of the primary phase
- an option for the primary phase only is left out of the others
- enclosing menus and 'if' blocks are translated too

The dump shows the entries which are created for each phase. The files which
syncconfig writes show the options which each phase sees.
"""


def test_config(conf):
    assert conf.olddefconfig('config') == 0
    assert conf.config_matches('expected_config')


def test_dump(conf):
    assert conf.dump() == 0
    assert conf.stdout_matches('expected_dump')


def test_syncconfig(conf):
    # KCONFIG_SPL makes syncconfig write the configuration of each phase too
    assert conf.syncconfig('expected_config',
                           extra_env={'KCONFIG_SPL': '1'}) == 0
    assert conf.file_matches('auto.conf', 'expected_auto_conf')
    assert conf.file_matches('auto_spl.conf', 'expected_auto_spl_conf')
    assert conf.file_matches('auto_tpl.conf', 'expected_auto_tpl_conf')
