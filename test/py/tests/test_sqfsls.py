# SPDX-License-Identifier:      GPL-2.0+
#
# Copyright 2026 Simon Glass <sjg@chromium.org>

""" Tests for the sqfsls command

The tests themselves are C code in test/cmd/sqfsls.c, since all they need
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
class TestSqfsls:
    """Tests for the sqfsls command"""

    @pytest.fixture(scope='class')
    def sqfs_image(self, u_boot_config):
        """Create a SquashFS image with a file, directories and a symlink

        Args:
            u_boot_config (u_boot_config): U-Boot configuration

        Yields:
            str: Path to the SquashFS image
        """
        with FsHelper(u_boot_config, 'squashfs', 1, 'test_sqfsls') as fsh:
            with open(f'{fsh.srcdir}/hello.txt', 'w',
                      encoding='ascii') as outf:
                outf.write(HELLO)
            os.mkdir(f'{fsh.srcdir}/docs')
            with open(f'{fsh.srcdir}/docs/note.txt', 'w',
                      encoding='ascii') as outf:
                outf.write(NOTE)
            os.mkdir(f'{fsh.srcdir}/empty')
            os.symlink('docs', f'{fsh.srcdir}/sym')
            fsh.mk_fs()
            yield fsh.fs_img

    def test_sqfsls_base(self, ubman, sqfs_image):
        """Check that sqfsls lists the root directory and counts what it found"""
        ubman.run_ut('cmd', 'cmd_test_sqfsls_base', fs_image=sqfs_image)

    def test_sqfsls_subdir(self, ubman, sqfs_image):
        """Check that a sub-directory is listed, by name and through a symlink"""
        ubman.run_ut('cmd', 'cmd_test_sqfsls_subdir', fs_image=sqfs_image)

    def test_sqfsls_empty(self, ubman, sqfs_image):
        """Check that an empty directory says so and reports a failure"""
        ubman.run_ut('cmd', 'cmd_test_sqfsls_empty', fs_image=sqfs_image)

    def test_sqfsls_missing(self, ubman, sqfs_image):
        """Check that a path which is not a directory is reported"""
        ubman.run_ut('cmd', 'cmd_test_sqfsls_missing', fs_image=sqfs_image)

    def test_sqfsls_baddev(self, ubman, sqfs_image):
        """Check that sqfsls complains about a device which is not there"""
        ubman.run_ut('cmd', 'cmd_test_sqfsls_baddev', fs_image=sqfs_image)

    def test_sqfsls_usage(self, ubman, sqfs_image):
        """Check that sqfsls shows its usage message when it cannot go on"""
        ubman.run_ut('cmd', 'cmd_test_sqfsls_usage', fs_image=sqfs_image)

    def test_sqfsls_badopt(self, ubman, sqfs_image):
        """Check that sqfsls refuses an option, since it has none"""
        ubman.run_ut('cmd', 'cmd_test_sqfsls_badopt', fs_image=sqfs_image)
