# SPDX-License-Identifier: GPL-2.0+
"""
U-Boot: use of the value of an option which is not in the phase

A phased entry which mentions an option that is not in a phase sees it as 'n'
in that phase. This makes no sense where the value itself is needed: in a
range, in a comparison and as the default of an option which is not a bool.

Each should fail with an error message.
"""


def test(conf):
    assert conf.oldaskconfig() != 0
    assert conf.stderr_contains('expected_stderr')
