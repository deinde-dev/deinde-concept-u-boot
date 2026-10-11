# SPDX-License-Identifier:      GPL-2.0+
#
# Copyright 2026 Simon Glass <sjg@chromium.org>

""" Tests for the ext4write command

The tests themselves are C code in test/cmd/ext4write.c, since all they need
from Python is a filesystem image. Each test writes to the image, so each
gets one of its own, which is checked with fsck.ext4 afterwards.
"""

import os

import pytest
from tests.fs_helper import FsHelper
from tests.test_fs.fstest_helpers import assert_fs_integrity

# Contents of docs/old.txt, 9 bytes including the newline
OLD = 'old file\n'


@pytest.mark.boardspec('sandbox')
@pytest.mark.buildconfigspec('cmd_ext4_write')
class TestExt4write:
    """Tests for the ext4write command"""

    @pytest.fixture
    def ext4_image(self, u_boot_config):
        """Create an ext4 image holding a directory with a file in it

        Args:
            u_boot_config (u_boot_config): U-Boot configuration

        Yields:
            str: Path to the ext4 image
        """
        with FsHelper(u_boot_config, 'ext4', 8, 'test_ext4write') as fsh:
            os.mkdir(f'{fsh.srcdir}/docs')
            with open(f'{fsh.srcdir}/docs/old.txt', 'w',
                      encoding='ascii') as outf:
                outf.write(OLD)
            fsh.mk_fs()
            yield fsh.fs_img

            # whatever the test wrote, the filesystem must still be sound
            assert_fs_integrity('ext4', fsh.fs_img)

    def test_ext4write_base(self, ubman, ext4_image):
        """Check that ext4write creates a file and reports what it wrote"""
        ubman.run_ut('cmd', 'cmd_test_ext4write_base', fs_image=ext4_image)

    def test_ext4write_offset(self, ubman, ext4_image):
        """Check that writing at an offset leaves the rest of the file alone"""
        ubman.run_ut('cmd', 'cmd_test_ext4write_offset', fs_image=ext4_image)

    def test_ext4write_extend(self, ubman, ext4_image):
        """Check that writing past the end extends the file, leaving a gap"""
        ubman.run_ut('cmd', 'cmd_test_ext4write_extend', fs_image=ext4_image)

    def test_ext4write_large(self, ubman, ext4_image):
        """Check a write too large for one journal transaction"""
        ubman.run_ut('cmd', 'cmd_test_ext4write_large', fs_image=ext4_image)

    def test_ext4write_full(self, ubman, ext4_image):
        """Check that a write which runs out of space keeps what it wrote"""
        ubman.run_ut('cmd', 'cmd_test_ext4write_full', fs_image=ext4_image)

    def test_ext4write_subdir(self, ubman, ext4_image):
        """Check that the filename reaches a subdirectory"""
        ubman.run_ut('cmd', 'cmd_test_ext4write_subdir', fs_image=ext4_image)

    def test_ext4write_nodir(self, ubman, ext4_image):
        """Check that a missing directory and a directory itself are refused"""
        ubman.run_ut('cmd', 'cmd_test_ext4write_nodir', fs_image=ext4_image)

    def test_ext4write_usage(self, ubman, ext4_image):
        """Check that ext4write insists on a byte count"""
        ubman.run_ut('cmd', 'cmd_test_ext4write_usage', fs_image=ext4_image)

    def test_ext4write_badopt(self, ubman, ext4_image):
        """Check that ext4write refuses an option, since it has none"""
        ubman.run_ut('cmd', 'cmd_test_ext4write_badopt', fs_image=ext4_image)
