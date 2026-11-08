# SPDX-License-Identifier: GPL-2.0+
# Copyright 2026 Simon Glass <sjg@chromium.org>

"""Find checkouts of the Python tools which are no longer in the U-Boot tree

Tools such as dtoc and buildman are now separate packages, normally installed
with pip. A developer working on one of them may prefer to run it from a
checkout instead. This module finds such a checkout, using a setting in
~/.config/u-boot/tools.conf, for example:

    [tools]
    # Directory holding a checkout of each tool, e.g. ~/dev/dtoc
    dir = ~/dev

    # Use a different checkout for one tool
    dtoc = ~/dev/dtoc-test

The U_BOOT_TOOLS_DIR environment variable, if set, is used instead of the
file, as the directory holding a checkout of each tool.
"""

import configparser
import os
import sys

CONFIG_FILE = '~/.config/u-boot/tools.conf'


def find(name, fname=CONFIG_FILE):
    """Find a checkout of a tool

    Args:
        name (str): Name of the tool, which is also the name of its Python
            package, e.g. 'dtoc'
        fname (str): Config file to read

    Returns:
        str: Path to the top of the checkout, which holds the package
            directory, or None if there is no checkout for this tool
    """
    base = os.environ.get('U_BOOT_TOOLS_DIR')
    if base:
        path = os.path.join(base, name)
    else:
        config = configparser.ConfigParser()
        config.read(os.path.expanduser(fname))
        path = config.get('tools', name, fallback=None)
        if not path:
            base = config.get('tools', 'dir', fallback=None)
            if not base:
                return None
            path = os.path.join(base, name)
    path = os.path.expanduser(path)
    if not os.path.isfile(os.path.join(path, name, '__init__.py')):
        return None
    return path


def add_to_path(name):
    """Allow a tool's package to be imported from its checkout

    Args:
        name (str): Name of the tool, e.g. 'buildman'

    Returns:
        str: Path to the checkout, or None if there is none
    """
    path = find(name)
    if path and path not in sys.path:
        sys.path.insert(0, path)
    return path


def run(name):
    """Run a tool from its checkout, if there is one

    This replaces the current process with the tool, passing it the same
    arguments, so it only returns if there is no checkout

    Args:
        name (str): Name of the tool, e.g. 'dtoc'
    """
    path = find(name)
    if path:
        main = os.path.join(path, name, 'main.py')
        os.execv(sys.executable, [sys.executable, main] + sys.argv[1:])
