# SPDX-License-Identifier:      GPL-2.0+
#
# Copyright 2026 Simon Glass <sjg@chromium.org>

""" Tests for the sqfsload command

The tests themselves are C code in test/cmd/sqfsload.c, since all they need
from Python is a filesystem image. SquashFS is read-only, so one image serves
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
@pytest.mark.buildconfigspec('cmd_squashfs')
@pytest.mark.requiredtool('mksquashfs')
class TestSqfsload:
    """Tests for the sqfsload command"""

    @pytest.fixture(scope='class')
    def sqfs_image(self, u_boot_config):
        """Create a SquashFS image with a file, a directory and a symlink

        Args:
            u_boot_config (u_boot_config): U-Boot configuration

        Yields:
            str: Path to the SquashFS image
        """
        with FsHelper(u_boot_config, 'squashfs', 1, 'test_sqfsload') as fsh:
            with open(f'{fsh.srcdir}/hello.txt', 'w',
                      encoding='ascii') as outf:
                outf.write(HELLO)
            os.mkdir(f'{fsh.srcdir}/docs')
            with open(f'{fsh.srcdir}/docs/note.txt', 'w',
                      encoding='ascii') as outf:
                outf.write(NOTE)
            os.symlink('docs', f'{fsh.srcdir}/sym')
            fsh.mk_fs()
            yield fsh.fs_img

    def test_sqfsload_base(self, ubman, sqfs_image):
        """Check that sqfsload reads a file and reports what it read"""
        ubman.run_ut('cmd', 'cmd_test_sqfsload_base', fs_image=sqfs_image)

    def test_sqfsload_part(self, ubman, sqfs_image):
        """Check that 'bytes' limits a load and 'pos' is refused"""
        ubman.run_ut('cmd', 'cmd_test_sqfsload_part', fs_image=sqfs_image)

    def test_sqfsload_default(self, ubman, sqfs_image):
        """Check that loadaddr and bootfile are used when not given"""
        ubman.run_ut('cmd', 'cmd_test_sqfsload_default', fs_image=sqfs_image)

    def test_sqfsload_subdir(self, ubman, sqfs_image):
        """Check that a path may lead through a sub-directory or a symlink"""
        ubman.run_ut('cmd', 'cmd_test_sqfsload_subdir', fs_image=sqfs_image)

    def test_sqfsload_missing(self, ubman, sqfs_image):
        """Check that a failed load leaves filesize and fileaddr alone"""
        ubman.run_ut('cmd', 'cmd_test_sqfsload_missing', fs_image=sqfs_image)

    def test_sqfsload_baddev(self, ubman, sqfs_image):
        """Check that sqfsload complains about a device which is not there"""
        ubman.run_ut('cmd', 'cmd_test_sqfsload_baddev', fs_image=sqfs_image)

    def test_sqfsload_usage(self, ubman, sqfs_image):
        """Check that sqfsload shows its usage message when it cannot go on"""
        ubman.run_ut('cmd', 'cmd_test_sqfsload_usage', fs_image=sqfs_image)
