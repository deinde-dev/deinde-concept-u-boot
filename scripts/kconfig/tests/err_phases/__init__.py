# SPDX-License-Identifier: GPL-2.0+
"""
U-Boot: uses of 'phases' which are not supported

An option with a 'phases' line needs one in each of its entries, so that it is
clear which phases each entry applies to. An option inside a choice cannot
have phases. A symbol which comes from a 'phases' line, e.g. SPL_DM, cannot
have an entry of its own.

Each should fail with an error message.
"""


def test(conf):
    assert conf.oldaskconfig() != 0
    assert conf.stderr_contains('expected_stderr')
