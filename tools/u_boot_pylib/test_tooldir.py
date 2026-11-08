# SPDX-License-Identifier: GPL-2.0+
# Copyright 2026 Simon Glass <sjg@chromium.org>

"""Tests for the u_boot_pylib.tooldir module"""

import os
import sys
import tempfile
import unittest
from unittest import mock

from u_boot_pylib import tooldir


class TestToolDir(unittest.TestCase):
    """Tests for finding and running checkouts of tools"""

    def setUp(self):
        self.tmpdir = tempfile.TemporaryDirectory()
        self.base = self.tmpdir.name
        self.conf = os.path.join(self.base, 'tools.conf')
        self.orig_env = os.environ.pop('U_BOOT_TOOLS_DIR', None)

    def tearDown(self):
        os.environ.pop('U_BOOT_TOOLS_DIR', None)
        if self.orig_env is not None:
            os.environ['U_BOOT_TOOLS_DIR'] = self.orig_env
        self.tmpdir.cleanup()

    def make_checkout(self, path, name):
        """Create a checkout of a tool, with its package directory

        Args:
            path (str): Directory for the checkout, relative to the base
            name (str): Name of the tool

        Returns:
            str: Full path to the checkout
        """
        top = os.path.join(self.base, path)
        os.makedirs(os.path.join(top, name))
        with open(os.path.join(top, name, '__init__.py'), 'w',
                  encoding='utf-8'):
            pass
        return top

    def write_conf(self, text):
        """Write the config file"""
        with open(self.conf, 'w', encoding='utf-8') as outf:
            outf.write(text)

    def test_dir(self):
        """A checkout is found in the directory given by 'dir'"""
        top = self.make_checkout('dtoc', 'dtoc')
        self.write_conf(f'[tools]\ndir = {self.base}\n')
        self.assertEqual(top, tooldir.find('dtoc', self.conf))

    def test_tool(self):
        """A setting for a tool takes precedence over 'dir'"""
        self.make_checkout('dtoc', 'dtoc')
        top = self.make_checkout('dtoc-test', 'dtoc')
        self.write_conf(f'[tools]\ndir = {self.base}\ndtoc = {top}\n')
        self.assertEqual(top, tooldir.find('dtoc', self.conf))

    def test_env(self):
        """The environment variable is used instead of the file"""
        self.make_checkout('dtoc', 'dtoc')
        other = os.path.join(self.base, 'other')
        top = self.make_checkout(os.path.join('other', 'dtoc'), 'dtoc')
        self.write_conf(f'[tools]\ndir = {self.base}\n')
        os.environ['U_BOOT_TOOLS_DIR'] = other
        self.assertEqual(top, tooldir.find('dtoc', self.conf))

    def test_none(self):
        """Nothing is found without a setting, or without a package"""
        self.make_checkout('dtoc', 'dtoc')
        self.assertIsNone(tooldir.find('dtoc', self.conf))

        # A directory which does not hold the package is not a checkout
        os.makedirs(os.path.join(self.base, 'buildman'))
        self.write_conf(f'[tools]\ndir = {self.base}\n')
        self.assertIsNone(tooldir.find('buildman', self.conf))
        self.assertIsNone(tooldir.find('binman', self.conf))

    def test_add_to_path(self):
        """A checkout can be added to the import path"""
        top = self.make_checkout('buildman', 'buildman')
        os.environ['U_BOOT_TOOLS_DIR'] = self.base
        orig_path = list(sys.path)
        try:
            self.assertEqual(top, tooldir.add_to_path('buildman'))
            self.assertEqual(top, sys.path[0])
        finally:
            sys.path[:] = orig_path
        os.environ['U_BOOT_TOOLS_DIR'] = os.path.join(self.base, 'missing')
        self.assertIsNone(tooldir.add_to_path('buildman'))

    def test_run(self):
        """A checkout is run with the same arguments, else nothing happens"""
        top = self.make_checkout('dtoc', 'dtoc')
        os.environ['U_BOOT_TOOLS_DIR'] = self.base
        with mock.patch.object(sys, 'argv', ['dtoc', '-h']), \
             mock.patch.object(os, 'execv') as execv:
            tooldir.run('dtoc')
            execv.assert_called_once_with(
                sys.executable,
                [sys.executable, os.path.join(top, 'dtoc', 'main.py'), '-h'])

            execv.reset_mock()
            tooldir.run('buildman')
            execv.assert_not_called()
