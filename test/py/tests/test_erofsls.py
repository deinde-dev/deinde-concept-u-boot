# SPDX-License-Identifier:      GPL-2.0+
#
# Copyright 2026 Simon Glass <sjg@chromium.org>

""" Tests for the erofsls command

The tests themselves are C code in test/cmd/erofsls.c, since all they need
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
class TestErofsls:
    """Tests for the erofsls command"""

    @pytest.fixture(scope='class')
    def erofs_image(self, u_boot_config):
        """Create an EROFS image with files, directories and symbolic links

        Args:
            u_boot_config (u_boot_config): U-Boot configuration

        Yields:
            str: Path to the EROFS image
        """
        with FsHelper(u_boot_config, 'erofs', 1, 'test_erofsls') as fsh:
            with open(f'{fsh.srcdir}/hello.txt', 'w',
                      encoding='ascii') as outf:
                outf.write(HELLO)
            os.mkdir(f'{fsh.srcdir}/docs')
            with open(f'{fsh.srcdir}/docs/note.txt', 'w',
                      encoding='ascii') as outf:
                outf.write(NOTE)
            os.mkdir(f'{fsh.srcdir}/empty')
            os.symlink('docs', f'{fsh.srcdir}/sym')
            os.symlink('hello.txt', f'{fsh.srcdir}/symfile')
            with open(f'{fsh.srcdir}/comp.dat', 'wb') as outf:
                outf.write(b'\x5a' * COMP_SIZE)
            fsh.mk_fs()
            yield fsh.fs_img

    def test_erofsls_base(self, ubman, erofs_image):
        """Check that erofsls lists the root directory and counts what it found"""
        ubman.run_ut('cmd', 'cmd_test_erofsls_base', fs_image=erofs_image)

    def test_erofsls_subdir(self, ubman, erofs_image):
        """Check that a sub-directory is listed, by name and through a symlink"""
        ubman.run_ut('cmd', 'cmd_test_erofsls_subdir', fs_image=erofs_image)

    def test_erofsls_empty(self, ubman, erofs_image):
        """Check that an empty directory still has its . and .. entries"""
        ubman.run_ut('cmd', 'cmd_test_erofsls_empty', fs_image=erofs_image)

    def test_erofsls_missing(self, ubman, erofs_image):
        """Check that a path which is not a directory is reported"""
        ubman.run_ut('cmd', 'cmd_test_erofsls_missing', fs_image=erofs_image)

    def test_erofsls_baddev(self, ubman, erofs_image):
        """Check that erofsls complains about a device which is not there"""
        ubman.run_ut('cmd', 'cmd_test_erofsls_baddev', fs_image=erofs_image)

    def test_erofsls_usage(self, ubman, erofs_image):
        """Check that erofsls shows its usage message when it cannot go on"""
        ubman.run_ut('cmd', 'cmd_test_erofsls_usage', fs_image=erofs_image)
