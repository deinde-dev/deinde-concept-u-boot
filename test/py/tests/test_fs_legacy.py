# SPDX-License-Identifier:      GPL-2.0+
#
# Copyright 2026 Simon Glass <sjg@chromium.org>

""" Tests for the legacy filesystem commands which write to a filesystem

The tests themselves are C code in test/cmd/fs_legacy.c, since all they need
from Python is a filesystem image. Each of them changes the image, so every
test gets one of its own.

The commands which only read a filesystem are tested with sandbox's own
filesystem and need nothing from here.

Which image a test gets depends on the command: a FAT filesystem can create
and delete a directory and rename a file, where an ext4 one is the one which
can create a symbolic link.
"""

import os

import pytest
from tests.fs_helper import FsHelper

# Contents of hello.txt, 14 bytes including the newline
HELLO = 'Hello, world!\n'

# Contents of docs/note.txt, 5 bytes including the newline
NOTE = 'note\n'


def fill_image(fsh):
    """Put a file, a directory and an empty directory in an image

    Args:
        fsh (FsHelper): Filesystem helper to make the image with
    """
    with open(f'{fsh.srcdir}/hello.txt', 'w', encoding='ascii') as outf:
        outf.write(HELLO)
    os.mkdir(f'{fsh.srcdir}/docs')
    with open(f'{fsh.srcdir}/docs/note.txt', 'w', encoding='ascii') as outf:
        outf.write(NOTE)
    os.mkdir(f'{fsh.srcdir}/empty')
    fsh.mk_fs()


@pytest.mark.boardspec('sandbox')
@pytest.mark.buildconfigspec('cmd_fs_legacy')
class TestFsLegacy:
    """Tests for the legacy mkdir, rm, mv and ln commands"""

    @pytest.fixture
    def fat_image(self, u_boot_config):
        """Create a FAT image, which supports mkdir, unlink and rename

        Args:
            u_boot_config (u_boot_config): U-Boot configuration

        Yields:
            str: Path to the FAT image
        """
        with FsHelper(u_boot_config, 'vfat', 1, 'test_fs_legacy') as fsh:
            fill_image(fsh)
            yield fsh.fs_img

    @pytest.fixture
    def ext4_image(self, u_boot_config):
        """Create an ext4 image, which is the one supporting symbolic links

        Args:
            u_boot_config (u_boot_config): U-Boot configuration

        Yields:
            str: Path to the ext4 image
        """
        with FsHelper(u_boot_config, 'ext4', 4, 'test_fs_legacy_e') as fsh:
            fill_image(fsh)
            yield fsh.fs_img

    def test_mkdir_base(self, ubman, fat_image):
        """Check that mkdir creates a directory, which is then empty"""
        ubman.run_ut('cmd', 'cmd_test_mkdir_base', fs_image=fat_image)

    def test_mkdir_fail(self, ubman, fat_image):
        """Check that mkdir reports a directory it cannot create"""
        ubman.run_ut('cmd', 'cmd_test_mkdir_fail', fs_image=fat_image)

    def test_rm_base(self, ubman, fat_image):
        """Check that rm deletes a file, and an empty directory"""
        ubman.run_ut('cmd', 'cmd_test_rm_base', fs_image=fat_image)

    def test_rm_fail(self, ubman, fat_image):
        """Check that rm reports a path it cannot delete"""
        ubman.run_ut('cmd', 'cmd_test_rm_fail', fs_image=fat_image)

    def test_mv_base(self, ubman, fat_image):
        """Check that mv renames a file, and moves one into a directory"""
        ubman.run_ut('cmd', 'cmd_test_mv_base', fs_image=fat_image)

    def test_mv_fail(self, ubman, fat_image):
        """Check that mv reports a file it cannot move"""
        ubman.run_ut('cmd', 'cmd_test_mv_fail', fs_image=fat_image)

    def test_ln_base(self, ubman, ext4_image):
        """Check that ln creates a symbolic link to a file"""
        ubman.run_ut('cmd', 'cmd_test_ln_base', fs_image=ext4_image)

    def test_ln_fail(self, ubman, ext4_image):
        """Check that ln reports a link it cannot create"""
        ubman.run_ut('cmd', 'cmd_test_ln_fail', fs_image=ext4_image)
