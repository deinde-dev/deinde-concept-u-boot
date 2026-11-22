# SPDX-License-Identifier:      GPL-2.0+
#
# Copyright 2026 Simon Glass <sjg@chromium.org>

""" Tests for the erofsload command

The tests themselves are C code in test/cmd/erofsload.c, since all they need
from Python is a filesystem image. EROFS is read-only, so one image serves
the whole class.
"""

import os

import pytest
from tests.fs_helper import FsHelper

# Contents of hello.txt, 14 bytes including the newline
HELLO = 'Hello, world!\n'

# Contents of docs/note.txt, 5 bytes including the newline
NOTE = 'note\n'

# Size of comp.dat, which holds one repeated byte so that it compresses
COMP_SIZE = 8192


@pytest.mark.boardspec('sandbox')
@pytest.mark.buildconfigspec('cmd_erofs')
@pytest.mark.requiredtool('mkfs.erofs')
class TestErofsload:
    """Tests for the erofsload command"""

    @pytest.fixture(scope='class')
    def erofs_image(self, u_boot_config):
        """Create an EROFS image with files, a directory and symbolic links

        Args:
            u_boot_config (u_boot_config): U-Boot configuration

        Yields:
            str: Path to the EROFS image
        """
        with FsHelper(u_boot_config, 'erofs', 1, 'test_erofsload') as fsh:
            with open(f'{fsh.srcdir}/hello.txt', 'w',
                      encoding='ascii') as outf:
                outf.write(HELLO)
            os.mkdir(f'{fsh.srcdir}/docs')
            with open(f'{fsh.srcdir}/docs/note.txt', 'w',
                      encoding='ascii') as outf:
                outf.write(NOTE)
            os.symlink('docs', f'{fsh.srcdir}/sym')
            os.symlink('hello.txt', f'{fsh.srcdir}/symfile')
            with open(f'{fsh.srcdir}/comp.dat', 'wb') as outf:
                outf.write(b'\x5a' * COMP_SIZE)
            fsh.mk_fs()
            yield fsh.fs_img

    def test_erofsload_base(self, ubman, erofs_image):
        """Check that erofsload reads a file and reports what it read"""
        ubman.run_ut('cmd', 'cmd_test_erofsload_base', fs_image=erofs_image)

    def test_erofsload_comp(self, ubman, erofs_image):
        """Check that a compressed file is expanded as it is read"""
        ubman.run_ut('cmd', 'cmd_test_erofsload_comp', fs_image=erofs_image)

    def test_erofsload_part(self, ubman, erofs_image):
        """Check that 'bytes' limits a load and 'pos' moves where it starts"""
        ubman.run_ut('cmd', 'cmd_test_erofsload_part', fs_image=erofs_image)

    def test_erofsload_default(self, ubman, erofs_image):
        """Check that loadaddr and bootfile are used when not given"""
        ubman.run_ut('cmd', 'cmd_test_erofsload_default',
                     fs_image=erofs_image)

    def test_erofsload_subdir(self, ubman, erofs_image):
        """Check which parts of a path may be a symbolic link"""
        ubman.run_ut('cmd', 'cmd_test_erofsload_subdir', fs_image=erofs_image)

    def test_erofsload_missing(self, ubman, erofs_image):
        """Check that a failed load leaves filesize and fileaddr alone"""
        ubman.run_ut('cmd', 'cmd_test_erofsload_missing',
                     fs_image=erofs_image)

    def test_erofsload_baddev(self, ubman, erofs_image):
        """Check that erofsload complains about a device which is not there"""
        ubman.run_ut('cmd', 'cmd_test_erofsload_baddev', fs_image=erofs_image)

    def test_erofsload_usage(self, ubman, erofs_image):
        """Check that erofsload shows its usage message when it cannot go on"""
        ubman.run_ut('cmd', 'cmd_test_erofsload_usage', fs_image=erofs_image)
