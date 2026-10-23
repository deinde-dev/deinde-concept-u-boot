#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+
#
# Copyright (C) 2016 Google, Inc
# Written by Simon Glass <sjg@chromium.org>
#

"""Stub for the former in-tree dtoc tool

dtoc is now maintained as a standalone 'dtoc' package, rather than living in
the U-Boot tree. This stub just tells the user how to get it.
"""

import sys


def main():
    """Print instructions for installing the dtoc package"""
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
