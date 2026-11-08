#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+

# Copyright (c) 2016 Google, Inc
# Written by Simon Glass <sjg@chromium.org>
#

"""Stub for the former in-tree binman tool

binman is now maintained as a standalone 'binary-manager' package, rather than
living in the U-Boot tree. This stub runs a checkout of it if one is set up
(see u_boot_pylib/tooldir.py), or otherwise tells the user how to get it.
"""

import os
import sys

# Use the u_boot_pylib in this tree
sys.path.insert(0, os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                '..'))
# pylint: disable=wrong-import-position
from u_boot_pylib import tooldir


def main():
    """Run a checkout of binman, else print how to install the binary-manager package"""
    tooldir.run('binman')
    print(
        'binman is no longer part of U-Boot. It is now maintained as a\n'
        "separate package called 'binary-manager'.\n"
        '\n'
        'Install it with:\n'
        '\n'
        '    pip install binary-manager\n'
        '\n'
        "The U-Boot build runs the 'binman' on the PATH, or another copy if\n"
        "set with 'make BINMAN=/path/to/binman'.\n"
        '\n'
        'Documentation: https://binman.readthedocs.io/\n',
        file=sys.stderr)
    return 1


if __name__ == '__main__':
    sys.exit(main())
