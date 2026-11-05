# SPDX-License-Identifier:      GPL-2.0+
#
# Copyright 2026 Simon Glass <sjg@chromium.org>

""" Tests for the ext4ls command

The tests themselves are C code in test/cmd/ext4ls.c, since all they need
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
@pytest.mark.buildconfigspec('cmd_ext4')
class TestExt4ls:
    """Tests for the ext4ls command"""

    @pytest.fixture(scope='class')
    def ext4_image(self, u_boot_config):
        """Create an ext4 image with a file, a directory and an empty one

        Args:
            u_boot_config (u_boot_config): U-Boot configuration

        Yields:
            str: Path to the ext4 image
        """
        with FsHelper(u_boot_config, 'ext4', 1, 'test_ext4ls') as fsh:
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

    def test_ext4ls_base(self, ubman, ext4_image):
        """Check that ext4ls lists the root directory"""
        ubman.run_ut('cmd', 'cmd_test_ext4ls_base', fs_image=ext4_image)

    def test_ext4ls_subdir(self, ubman, ext4_image):
        """Check that a sub-directory is listed with its . and .. entries"""
        ubman.run_ut('cmd', 'cmd_test_ext4ls_subdir', fs_image=ext4_image)

    def test_ext4ls_missing(self, ubman, ext4_image):
        """Check that a path which is not a directory fails silently"""
        ubman.run_ut('cmd', 'cmd_test_ext4ls_missing', fs_image=ext4_image)

    def test_ext4ls_baddev(self, ubman, ext4_image):
        """Check that ext4ls complains about a device it cannot use"""
        ubman.run_ut('cmd', 'cmd_test_ext4ls_baddev', fs_image=ext4_image)

    def test_ext4ls_usage(self, ubman, ext4_image):
        """Check that ext4ls shows its usage message when it cannot go on"""
        ubman.run_ut('cmd', 'cmd_test_ext4ls_usage', fs_image=ext4_image)
