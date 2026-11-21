# SPDX-License-Identifier: GPL-2.0+
"""
U-Boot: a phased entry which selects an option with a single symbol

If an option with xPL phases selects one which is the same in every phase,
enabling the option in an xPL phase enables the other one everywhere. This is
easy to do by accident, so it gives a warning, once for each select or imply.

There is no warning if the option is written with the prefix of the primary
phase, which shows that it is intended, if it has a twin for the phase, or if
the entry is only for the primary phase.
"""


def test(conf):
    assert conf.dump() == 0
    assert conf.stderr_matches('expected_stderr')
