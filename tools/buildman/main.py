#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+
#
# Copyright (c) 2012 The Chromium OS Authors.
#

"""Stub for the former in-tree buildman tool

buildman is now maintained as a standalone 'buildman' package, rather than
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
    """Run a checkout of buildman, else print how to install the buildman package"""
    tooldir.run('buildman')
    print(
        'buildman is no longer part of U-Boot. It is now maintained as a\n'
        "separate package called 'buildman'.\n"
        '\n'
        'Install it with:\n'
        '\n'
        '    pip install buildman\n'
        '\n'
        'Documentation: https://buildman.readthedocs.io/\n',
        file=sys.stderr)
    return 1


if __name__ == '__main__':
    sys.exit(main())
