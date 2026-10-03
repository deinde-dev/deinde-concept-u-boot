#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+
#
# Copyright 2026 Canonical Ltd
#
"""Tests for boards.py - finding the boards which build a file or lines"""

import os
import shutil
import subprocess
import sys
import tempfile
import unittest

# Test configuration
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)
sys.path.insert(0, os.path.join(SCRIPT_DIR, '..'))

# pylint: disable=wrong-import-position
import boards
from boards import Cond, PHASES

UBOOT, SPL = PHASES[0], PHASES[1]


class TestMakefileRules(unittest.TestCase):
    """Tests for reading the conditions on a Makefile target"""

    def test_simple(self):
        """Test a rule with an option, one without and one not naming it"""
        text = ('obj-$(CONFIG_$(PHASE_)CLK) += clk-uclass.o \\\n'
                '\tclk-fixed.o\nobj-y += always.o\n')
        self.assertEqual(boards.makefile_rules(text, 'clk-fixed.o'),
                         [(Cond('CLK', True, False, None),)])
        self.assertEqual(boards.makefile_rules(text, 'always.o'), [()])
        self.assertEqual(boards.makefile_rules(text, 'other.o'), [])

    def test_several_rules(self):
        """Test that each rule naming a target is an alternative"""
        text = ('ifdef CONFIG_XPL_BUILD\nobj-$(CONFIG_SPL_CPU) += cpu/\n'
                'else\nobj-$(CONFIG_CPU) += cpu/\nendif\n')
        self.assertEqual(boards.makefile_rules(text, 'cpu/'), [
            (Cond('XPL_BUILD', False, False, None),
             Cond('SPL_CPU', False, False, None)),
            (Cond('XPL_BUILD', False, True, None),
             Cond('CPU', False, False, None))])

    def test_else_if(self):
        """Test that 'else ifdef' negates the test before it"""
        text = ('ifdef CONFIG_NET_LEGACY\nobj-y += net/\n'
                'else ifdef CONFIG_NET_LWIP\nobj-y += lwip/\nendif\n')
        self.assertEqual(boards.makefile_rules(text, 'lwip/'), [
            (Cond('NET_LEGACY', False, True, None),
             Cond('NET_LWIP', False, False, None))])

    def test_ifeq(self):
        """Test the forms of 'ifeq' on an option"""
        self.assertEqual(boards.make_cond('ifeq ($(CONFIG_FOO),y)'),
                         Cond('FOO', False, False, None))
        self.assertEqual(boards.make_cond('ifeq ($(CONFIG_$(PHASE_)FOO),)'),
                         Cond('FOO', True, True, None))
        self.assertEqual(boards.make_cond('ifneq ($(CONFIG_FOO),"abc")'),
                         Cond('FOO', False, True, ('"abc"',)))
        self.assertIsNone(boards.make_cond('ifeq ($(CC),clang)'))

    def test_make_variable(self):
        """Test that $(SOC) and the like map to the .config"""
        self.assertEqual(
            boards.make_cond('ifneq (,$(filter $(SOC), mx25 mx6))'),
            Cond('SYS_SOC', False, False, ('mx25', 'mx6')))
        self.assertEqual(boards.make_cond('ifeq ($(CPU),armv8)'),
                         Cond('SYS_CPU', False, False, ('armv8',)))

    def test_composite(self):
        """Test a part of a composite object, with a condition of its own"""
        text = ('spi-nor-y := sf_probe.o\n'
                'ifeq ($(CONFIG_$(PHASE_)TINY),y)\nspi-nor-y += tiny.o\n'
                'else\nspi-nor-y += core.o\nendif\n'
                'obj-$(CONFIG_SPI_FLASH) += spi-nor.o\n')
        flash = Cond('SPI_FLASH', False, False, None)
        self.assertEqual(boards.makefile_rules(text, 'sf_probe.o'),
                         [(flash,)])
        self.assertEqual(boards.makefile_rules(text, 'core.o'),
                         [(flash, Cond('TINY', True, True, None))])

    def test_braces(self):
        """Test that '${CONFIG_FOO}' is read like '$(CONFIG_FOO)'"""
        self.assertEqual(
            boards.makefile_rules('obj-${CONFIG_FOO} += foo.o\n', 'foo.o'),
            [(Cond('FOO', False, False, None),)])

    def test_machine(self):
        """Test an arch Makefile naming a machine without 'mach-'"""
        self.assertEqual(
            boards.makefile_rules('machine-$(CONFIG_ARCH_K3) += k3\n',
                                  'mach-k3/'),
            [(Cond('ARCH_K3', False, False, None),)])


class TestTree(unittest.TestCase):
    """Tests which need a small source tree"""

    def setUp(self):
        """Set up a tree with a few Makefiles"""
        self.srcdir = tempfile.mkdtemp()
        files = {
            'Makefile': 'libs-$(CONFIG_CMD_DOOM) += lib/doom/\n',
            'drivers/Makefile': 'obj-$(CONFIG_$(PHASE_)CLK) += clk/\n',
            'drivers/clk/Makefile': ('obj-y += clk-uclass.o\n'
                                     'obj-$(CONFIG_CLK_MPFS) += mpfs/\n'),
            'drivers/clk/clk-uclass.c': 'int a;\n',
            'drivers/clk/included.c': 'int b;\n',
            'drivers/clk/mpfs/Makefile': 'obj-y += mpfs_clk.o\n',
            'drivers/clk/mpfs/mpfs_clk.c': (
                '#include "local.h"\nint a;\n'
                '#if CONFIG_IS_ENABLED(FAST)\nint fast;\n#endif\n'
                '#ifndef CONFIG_XPL_BUILD\nint proper;\n#endif\n'),
            'drivers/clk/mpfs/local.h': 'int c;\n',
            'lib/doom/Makefile': 'obj-y += doom.o\n',
            'lib/doom/doom.c': 'int d;\n',
            'board/acme/rocket/Makefile': 'obj-y += rocket.o\n',
            'board/acme/rocket/rocket.c': 'int e;\n',
        }
        for path, text in files.items():
            fname = os.path.join(self.srcdir, path)
            os.makedirs(os.path.dirname(fname), exist_ok=True)
            with open(fname, 'w', encoding='utf-8') as outf:
                outf.write(text)
        subprocess.run(['git', 'init', '-q'], cwd=self.srcdir, check=True)
        subprocess.run(['git', 'add', '.'], cwd=self.srcdir, check=True)
        self.configs = {
            'rocket': {'CONFIG_CLK': 'y', 'CONFIG_CLK_MPFS': 'y',
                       'CONFIG_SPL': 'y', 'CONFIG_SPL_CLK': 'y',
                       'CONFIG_SPL_FAST': 'y', 'CONFIG_SYS_ARCH': '"riscv"',
                       'CONFIG_SYS_VENDOR': '"acme"',
                       'CONFIG_SYS_BOARD': '"rocket"'},
            'plain': {'CONFIG_CLK': 'y', 'CONFIG_CMD_DOOM': 'y',
                      'CONFIG_SYS_ARCH': '"arm"'},
        }

    def tearDown(self):
        """Remove the tree"""
        shutil.rmtree(self.srcdir)

    def test_file_rules(self):
        """Test walking up the tree for the conditions on a file"""
        clk = Cond('CLK', True, False, None)
        self.assertEqual(boards.file_rules(self.srcdir,
                                           'drivers/clk/clk-uclass.c'),
                         [[()], [(clk,)]])
        self.assertEqual(
            boards.file_rules(self.srcdir, 'drivers/clk/mpfs/mpfs_clk.c'),
            [[()], [(Cond('CLK_MPFS', False, False, None),)], [(clk,)]])
        # The top-level Makefile names lib/doom/ by its full path
        self.assertEqual(boards.file_rules(self.srcdir, 'lib/doom/doom.c'),
                         [[()], [(Cond('CMD_DOOM', False, False, None),)]])
        self.assertIsNone(boards.file_rules(self.srcdir,
                                            'drivers/clk/included.c'))

    def test_file_boards(self):
        """Test finding the boards and phases which build a file"""
        self.assertEqual(
            boards.file_boards(self.srcdir, 'drivers/clk/clk-uclass.c',
                               self.configs),
            {'rocket': [UBOOT, SPL], 'plain': [UBOOT]})
        self.assertEqual(
            boards.file_boards(self.srcdir, 'lib/doom/doom.c', self.configs),
            {'plain': [UBOOT]})
        # Only the board in board/acme/rocket builds rocket.c
        self.assertEqual(
            list(boards.file_boards(self.srcdir, 'board/acme/rocket/rocket.c',
                                    self.configs)), ['rocket'])

    def test_path_needs(self):
        """Test the options implied by a file's place in the tree"""
        need = boards.Need
        self.assertEqual(
            boards.path_needs(self.srcdir, 'arch/arm/cpu/armv8/x.c'),
            [need('SYS_ARCH', 'arm'), need('SYS_CPU', 'armv8')])
        self.assertEqual(
            boards.path_needs(self.srcdir, 'board/acme/rocket/rocket.c'),
            [need('SYS_VENDOR', 'acme'), need('SYS_BOARD', 'rocket')])
        self.assertEqual(boards.path_needs(self.srcdir, 'drivers/x.c'), [])

    @unittest.skipUnless(shutil.which('unifdef'), 'unifdef not installed')
    def test_lines(self):
        """Test narrowing to the boards and phases which build some lines"""
        path = 'drivers/clk/mpfs/mpfs_clk.c'
        res = boards.find_boards(self.srcdir, f'{path}', (4, 4), self.configs)
        # CONFIG_IS_ENABLED(FAST) holds only in SPL, where SPL_FAST is set
        self.assertEqual(res.boards, {'rocket': ['SPL']})
        res = boards.find_boards(self.srcdir, path, (7, 7), self.configs)
        self.assertEqual(res.boards, {'rocket': ['U-Boot']})

    def test_includers(self):
        """Test that a local header goes by the files which include it"""
        res = boards.find_boards(self.srcdir, 'drivers/clk/mpfs/local.h',
                                 None, self.configs)
        self.assertEqual(res.boards, {'rocket': ['U-Boot', 'SPL']})
        res = boards.find_boards(self.srcdir, 'drivers/clk/included.c', None,
                                 self.configs)
        self.assertEqual(res.boards, {})
        self.assertIn('nothing beside it includes it', res.note)

    def test_kconfig(self):
        """Test that a Kconfig line goes by the entry it sits in"""
        with open(os.path.join(self.srcdir, 'drivers/clk/Kconfig'), 'w',
                  encoding='utf-8') as outf:
            outf.write('config CLK_MPFS\n\tbool "x"\n\tselect REGMAP\n'
                       '\tif you want it\n\nconfig CMD_DOOM\n\tbool\n')
        res = boards.find_boards(self.srcdir, 'drivers/clk/Kconfig', (3, 3),
                                 self.configs)
        self.assertEqual(res.boards, {'rocket': ['U-Boot']})
        res = boards.find_boards(self.srcdir, 'drivers/clk/Kconfig', None,
                                 self.configs)
        self.assertEqual(sorted(res.boards), ['plain', 'rocket'])

    def test_special_files(self):
        """Test a defconfig, a shared header and a missing file"""
        res = boards.find_boards(self.srcdir, 'configs/rocket_defconfig',
                                 None, self.configs)
        self.assertEqual(res.note, 'no such file')
        os.makedirs(os.path.join(self.srcdir, 'configs'))
        with open(os.path.join(self.srcdir, 'configs/rocket_defconfig'), 'w',
                  encoding='utf-8'):
            pass
        res = boards.find_boards(self.srcdir, 'configs/rocket_defconfig',
                                 None, self.configs)
        self.assertEqual(res.boards, {'rocket': ['U-Boot']})
        os.makedirs(os.path.join(self.srcdir, 'include'))
        with open(os.path.join(self.srcdir, 'include/clk.h'), 'w',
                  encoding='utf-8'):
            pass
        res = boards.find_boards(self.srcdir, 'include/clk.h', None,
                                 self.configs)
        self.assertIn('shared header', res.note)


class TestPhases(unittest.TestCase):
    """Tests for the per-phase handling of conditions and sources"""

    def test_cond_holds(self):
        """Test a condition against a board and phase"""
        cfg = {'CONFIG_SPL_CLK': 'y', 'CONFIG_SYS_SOC': '"mx6"'}
        clk = Cond('CLK', True, False, None)
        self.assertFalse(boards.cond_holds(clk, cfg, UBOOT))
        self.assertTrue(boards.cond_holds(clk, cfg, SPL))
        xpl = Cond('XPL_BUILD', False, False, None)
        self.assertFalse(boards.cond_holds(xpl, cfg, UBOOT))
        self.assertTrue(boards.cond_holds(xpl, cfg, SPL))
        soc = Cond('SYS_SOC', False, False, ('mx25', 'mx6'))
        self.assertTrue(boards.cond_holds(soc, cfg, UBOOT))
        self.assertFalse(boards.cond_holds(soc._replace(negate=True), cfg,
                                           UBOOT))

    def test_phase_source(self):
        """Test rewriting the macros unifdef cannot expand"""
        text = ('#if CONFIG_IS_ENABLED(FOO) && IS_ENABLED(CONFIG_BAR)\n'
                'x = CONFIG_VAL(SIZE);\n')
        self.assertEqual(
            boards.phase_source(text, SPL),
            '#if defined(CONFIG_SPL_FOO) && defined(CONFIG_BAR)\n'
            'x = CONFIG_SPL_SIZE;\n')

    def test_directive_symbols(self):
        """Test that only options in directives are collected"""
        text = ('#if defined(CONFIG_A) || \\\n defined(CONFIG_B)\n'
                'if (CONFIG_C) {}\n#endif\n')
        self.assertEqual(boards.directive_symbols(text),
                         {'CONFIG_A', 'CONFIG_B'})

    def test_unifdef_defs(self):
        """Test the definitions written for unifdef"""
        defs = boards.unifdef_defs(
            {'CONFIG_A': 'y', 'CONFIG_N': '3', 'CONFIG_S': '"x"'},
            {'CONFIG_A', 'CONFIG_B', 'CONFIG_N', 'CONFIG_S',
             'CONFIG_XPL_BUILD'}, SPL)
        self.assertEqual(defs.splitlines(), [
            '#define CONFIG_A 1', '#undef CONFIG_B', '#define CONFIG_N 3',
            '#define CONFIG_S "x"', '#define CONFIG_XPL_BUILD 1'])

    def test_parse_spec(self):
        """Test splitting a file and lines"""
        self.assertEqual(boards.parse_spec('a/b.c'), ('a/b.c', None))
        self.assertEqual(boards.parse_spec('a/b.c:12'), ('a/b.c', (12, 12)))
        self.assertEqual(boards.parse_spec('a/b.c:12-20'),
                         ('a/b.c', (12, 20)))


if __name__ == '__main__':
    unittest.main(argv=['test_boards.py'], verbosity=2)
