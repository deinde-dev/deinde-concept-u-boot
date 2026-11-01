# SPDX-License-Identifier: GPL-2.0+
# Copyright 2026 Simon Glass <sjg@chromium.org>

"""Tests for the u_boot_pylib.xpl module"""

import os
import tempfile
import unittest

from u_boot_pylib import xpl


class TestReadPhases(unittest.TestCase):
    """Tests for read_phases()"""

    def setUp(self):
        xpl.read_phases.cache_clear()

    def test_read(self):
        """The phases are read in order, skipping comments and blank lines"""
        with tempfile.TemporaryDirectory() as srcdir:
            os.mkdir(os.path.join(srcdir, 'scripts'))
            with open(os.path.join(srcdir, 'scripts', 'xpl_phases'), 'w',
                      encoding='utf-8') as outf:
                outf.write('# comment\n\ntpl\tbootph-pre-sram\n'
                           'mpl  bootph-monitor\nspl\tbootph-pre-ram\n')
            self.assertEqual(('tpl', 'mpl', 'spl'), xpl.read_phases(srcdir))

    def test_parse(self):
        """The phases are parsed from text, e.g. read from a git commit"""
        self.assertEqual(('tpl', 'spl'),
                         xpl.parse_phases('# comment\n\ntpl a\n  spl\tb\n'))
        self.assertEqual((), xpl.parse_phases(''))

    def test_missing(self):
        """A tree without the file has the original phases"""
        with tempfile.TemporaryDirectory() as srcdir:
            self.assertEqual(xpl.DEFAULT_PHASES, xpl.read_phases(srcdir))

    def test_source(self):
        """The phases in this source tree"""
        srcdir = os.path.join(os.path.dirname(os.path.realpath(__file__)),
                              '..', '..')
        self.assertEqual(('tpl', 'vpl', 'spl'), xpl.read_phases(srcdir))
