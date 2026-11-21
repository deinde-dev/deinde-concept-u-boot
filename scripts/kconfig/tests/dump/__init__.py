# SPDX-License-Identifier: GPL-2.0+
"""
U-Boot: show the Kconfig files as they are understood

The --dump option shows each entry in Kconfig syntax, once the menu tree is
complete. An entry has the dependencies of the menus and 'if' blocks around
it, and the conditions of its properties include its dependencies.
"""


def test(conf):
    assert conf.dump() == 0
    assert conf.stdout_matches('expected_stdout')
