#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+
#
# Copyright (c) 2012 The Chromium OS Authors.
#

"""Stub for the former in-tree buildman tool

buildman is now maintained as a standalone 'buildman' package, rather than
living in the U-Boot tree. This stub just tells the user how to get it.
"""

import sys


def main():
    """Print instructions for installing the buildman package"""
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
