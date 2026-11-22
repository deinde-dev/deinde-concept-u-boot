# SPDX-License-Identifier:      GPL-2.0+
#
# Copyright 2026 Simon Glass <sjg@chromium.org>

""" Tests for the legacy filesystem commands which write to a filesystem

The tests themselves are C code in test/cmd/fs_legacy.c, since all they need
from Python is a filesystem image. Each of them changes the image, so every
test gets one of its own.

The commands which only read a filesystem are tested with sandbox's own
filesystem and need nothing from here.
"""

import os

import pytest
from tests.fs_helper import FsHelper

# Contents of hello.txt, 14 bytes including the newline
HELLO = 'Hello, world!\n'

# Contents of docs/note.txt, 5 bytes including the newline
NOTE = 'note\n'


@pytest.mark.boardspec('sandbox')
@pytest.mark.buildconfigspec('cmd_fs_legacy')
class TestFsLegacy:
    """Tests for the legacy mkdir, rm and mv commands"""

    @pytest.fixture
    def fat_image(self, u_boot_config):
        """Create a FAT image with a file, a directory and an empty directory

        Args:
            u_boot_config (u_boot_config): U-Boot configuration

        Yields:
            str: Path to the FAT image
        """
        with FsHelper(u_boot_config, 'vfat', 1, 'test_fs_legacy') as fsh:
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

    def test_mkdir_base(self, ubman, fat_image):
        """Check that mkdir creates a directory, which is then empty"""
        ubman.run_ut('cmd', 'cmd_test_mkdir_base', fs_image=fat_image)

    def test_mkdir_fail(self, ubman, fat_image):
        """Check that mkdir reports a directory it cannot create"""
        ubman.run_ut('cmd', 'cmd_test_mkdir_fail', fs_image=fat_image)
