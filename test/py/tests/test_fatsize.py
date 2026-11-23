# SPDX-License-Identifier:      GPL-2.0+
#
# Copyright 2026 Simon Glass <sjg@chromium.org>

""" Tests for the fatsize command

The tests themselves are C code in test/cmd/fatsize.c, since all they need
from Python is a filesystem image. None of them writes to it, so one image
serves the whole class.
"""

import os

import pytest
from tests.fs_helper import FsHelper

# Contents of hello.txt, 14 bytes including the newline
HELLO = 'Hello, world!\n'

# Contents of docs/note.txt, 5 bytes including the newline
NOTE = 'note\n'


@pytest.mark.boardspec('sandbox')
@pytest.mark.buildconfigspec('cmd_fat')
class TestFatsize:
    """Tests for the fatsize command"""

    @pytest.fixture(scope='class')
    def fat_image(self, u_boot_config):
        """Create a FAT image holding a file and a directory with a file in it

        Args:
            u_boot_config (u_boot_config): U-Boot configuration

        Yields:
            str: Path to the FAT image
        """
        with FsHelper(u_boot_config, 'vfat', 1, 'test_fatsize') as fsh:
            with open(f'{fsh.srcdir}/hello.txt', 'w',
                      encoding='ascii') as outf:
                outf.write(HELLO)
            os.mkdir(f'{fsh.srcdir}/docs')
            with open(f'{fsh.srcdir}/docs/note.txt', 'w',
                      encoding='ascii') as outf:
                outf.write(NOTE)
            fsh.mk_fs()
            yield fsh.fs_img

    def test_fatsize_base(self, ubman, fat_image):
        """Check that fatsize reports a file's size in hex, in filesize"""
        ubman.run_ut('cmd', 'cmd_test_fatsize_base', fs_image=fat_image)

    def test_fatsize_dir(self, ubman, fat_image):
        """Check that a directory is reported as zero bytes"""
        ubman.run_ut('cmd', 'cmd_test_fatsize_dir', fs_image=fat_image)

    def test_fatsize_missing(self, ubman, fat_image):
        """Check that a missing file fails and leaves filesize alone"""
        ubman.run_ut('cmd', 'cmd_test_fatsize_missing', fs_image=fat_image)

    def test_fatsize_baddev(self, ubman, fat_image):
        """Check that fatsize complains about a device which is not there"""
        ubman.run_ut('cmd', 'cmd_test_fatsize_baddev', fs_image=fat_image)

    def test_fatsize_usage(self, ubman, fat_image):
        """Check that fatsize shows its usage message when it cannot go on"""
        ubman.run_ut('cmd', 'cmd_test_fatsize_usage', fs_image=fat_image)
