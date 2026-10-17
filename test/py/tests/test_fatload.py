# SPDX-License-Identifier:      GPL-2.0+
#
# Copyright 2026 Simon Glass <sjg@chromium.org>

""" Tests for the fatload command

The tests themselves are C code in test/cmd/fatload.c, since all they need
from Python is a filesystem image. None of them writes to it, so one image
serves the whole class.
"""

import os

import pytest
from tests.fs_helper import FsHelper

# Contents of hello.txt, 14 bytes including the newline
HELLO = 'Hello, world!\n'

# Contents of docs/old.txt, 9 bytes including the newline
OLD = 'old file\n'


@pytest.mark.boardspec('sandbox')
@pytest.mark.buildconfigspec('cmd_fat')
class TestFatload:
    """Tests for the fatload command"""

    @pytest.fixture(scope='class')
    def fat_image(self, u_boot_config):
        """Create a FAT image holding a file and a directory with a file in it

        Args:
            u_boot_config (u_boot_config): U-Boot configuration

        Yields:
            str: Path to the FAT image
        """
        with FsHelper(u_boot_config, 'vfat', 1, 'test_fatload') as fsh:
            with open(f'{fsh.srcdir}/hello.txt', 'w',
                      encoding='ascii') as outf:
                outf.write(HELLO)
            os.mkdir(f'{fsh.srcdir}/docs')
            with open(f'{fsh.srcdir}/docs/old.txt', 'w',
                      encoding='ascii') as outf:
                outf.write(OLD)
            fsh.mk_fs()
            yield fsh.fs_img

    def test_fatload_base(self, ubman, fat_image):
        """Check that fatload reads a file and reports what it read"""
        ubman.run_ut('cmd', 'cmd_test_fatload_base', fs_image=fat_image)

    def test_fatload_part(self, ubman, fat_image):
        """Check that 'bytes' and 'pos' select part of a file"""
        ubman.run_ut('cmd', 'cmd_test_fatload_part', fs_image=fat_image)

    def test_fatload_default(self, ubman, fat_image):
        """Check that fatload falls back on loadaddr and bootfile"""
        ubman.run_ut('cmd', 'cmd_test_fatload_default', fs_image=fat_image)

    def test_fatload_subdir(self, ubman, fat_image):
        """Check that a filename is a path within the filesystem"""
        ubman.run_ut('cmd', 'cmd_test_fatload_subdir', fs_image=fat_image)

    def test_fatload_missing(self, ubman, fat_image):
        """Check that a failed load leaves filesize and fileaddr alone"""
        ubman.run_ut('cmd', 'cmd_test_fatload_missing', fs_image=fat_image)

    def test_fatload_baddev(self, ubman, fat_image):
        """Check that fatload complains about a device which is not there"""
        ubman.run_ut('cmd', 'cmd_test_fatload_baddev', fs_image=fat_image)

    def test_fatload_usage(self, ubman, fat_image):
        """Check that fatload shows its usage message when it cannot go on"""
        ubman.run_ut('cmd', 'cmd_test_fatload_usage', fs_image=fat_image)
