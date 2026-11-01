# SPDX-License-Identifier: GPL-2.0+
# Copyright 2026 Simon Glass <sjg@chromium.org>

"""Information about U-Boot's xPL phases, such as TPL and SPL

The phases are listed in scripts/xpl_phases in the U-Boot source tree
"""

import functools
import os

# Phases for a source tree which does not have scripts/xpl_phases
DEFAULT_PHASES = ('tpl', 'vpl', 'spl')


def parse_phases(text):
    """Parse the names of the xPL phases from scripts/xpl_phases

    Args:
        text (str): Contents of the file

    Returns:
        tuple of str: Lower-case name of each phase, in the order in which
            they run, e.g. ('tpl', 'vpl', 'spl')
    """
    phases = []
    for line in text.splitlines():
        fields = line.split()
        if fields and not fields[0].startswith('#'):
            phases.append(fields[0])
    return tuple(phases)


@functools.lru_cache
def read_phases(srcdir):
    """Read the names of the xPL phases from a U-Boot source tree

    Args:
        srcdir (str): Path to the source tree

    Returns:
        tuple of str: Lower-case name of each phase, in the order in which
            they run, e.g. ('tpl', 'vpl', 'spl')
    """
    fname = os.path.join(srcdir, 'scripts', 'xpl_phases')
    if not os.path.exists(fname):
        return DEFAULT_PHASES
    with open(fname, encoding='utf-8') as inf:
        return parse_phases(inf.read())
