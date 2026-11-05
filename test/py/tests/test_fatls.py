# SPDX-License-Identifier:      GPL-2.0+
#
# Copyright 2026 Simon Glass <sjg@chromium.org>

""" Tests for the fatls command

The tests themselves are C code in test/cmd/fatls.c, since all they need from
Python is a filesystem image. None of them writes to it, so one image serves
the whole class.
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
class TestFatls:
    """Tests for the fatls command"""

    @pytest.fixture(scope='class')
    def fat_image(self, u_boot_config):
        """Create a FAT image with a file, a directory and an empty directory

        Args:
            u_boot_config (u_boot_config): U-Boot configuration

        Yields:
            str: Path to the FAT image
        """
        with FsHelper(u_boot_config, 'vfat', 1, 'test_fatls') as fsh:
            with open(f'{fsh.srcdir}/hello.txt', 'w',
                      encoding='ascii') as outf:
                outf.write(HELLO)
            os.mkdir(f'{fsh.srcdir}/docs')
            with open(f'{fsh.srcdir}/docs/note.txt', 'w',
                      encoding='ascii') as outf:
                outf.write(NOTE)
            os.mkdir(f'{fsh.srcdir}/empty')
            fsh.mk_fs()
            yield fsh.fs_img

    def test_fatls_base(self, ubman, fat_image):
        """Check that fatls lists the root directory and counts what it found"""
        ubman.run_ut('cmd', 'cmd_test_fatls_base', fs_image=fat_image)

    def test_fatls_subdir(self, ubman, fat_image):
        """Check that a sub-directory is listed with its . and .. entries"""
        ubman.run_ut('cmd', 'cmd_test_fatls_subdir', fs_image=fat_image)

    def test_fatls_missing(self, ubman, fat_image):
        """Check that a path which is not a directory fails silently"""
        ubman.run_ut('cmd', 'cmd_test_fatls_missing', fs_image=fat_image)

    def test_fatls_baddev(self, ubman, fat_image):
        """Check that fatls complains about a device which is not there"""
        ubman.run_ut('cmd', 'cmd_test_fatls_baddev', fs_image=fat_image)

    def test_fatls_usage(self, ubman, fat_image):
        """Check that fatls shows its usage message when it cannot go on"""
        ubman.run_ut('cmd', 'cmd_test_fatls_usage', fs_image=fat_image)

    def test_fatls_badopt(self, ubman, fat_image):
        """Check that fatls refuses an option, since it has none"""
        ubman.run_ut('cmd', 'cmd_test_fatls_badopt', fs_image=fat_image)
