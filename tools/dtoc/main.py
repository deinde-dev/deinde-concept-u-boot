#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+
#
# Copyright (C) 2016 Google, Inc
# Written by Simon Glass <sjg@chromium.org>
#

"""Stub for the former in-tree dtoc tool

dtoc is now maintained as a standalone 'dtoc' package, rather than living in
the U-Boot tree. This stub runs a checkout of it if one is set up (see
u_boot_pylib/tooldir.py), or otherwise tells the user how to get it.
"""

import os
import sys

# Use the u_boot_pylib in this tree
sys.path.insert(0, os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                '..'))
# pylint: disable=wrong-import-position
from u_boot_pylib import tooldir


def main():
    """Run a checkout of dtoc, else print how to install the dtoc package"""
    tooldir.run('dtoc')
    print(
        'dtoc is no longer part of U-Boot. It is now maintained as a\n'
        "separate package called 'dtoc'.\n"
        '\n'
        'Install it with:\n'
        '\n'
        '    pip install dtoc\n'
        '\n'
        'Documentation: https://dtoc.readthedocs.io/\n',
        file=sys.stderr)
    return 1


if __name__ == '__main__':
    sys.exit(main())
