# SPDX-License-Identifier:      GPL-2.0+
#
# Copyright 2026 Simon Glass <sjg@chromium.org>

""" Tests for the fatinfo command

The tests themselves are C code in test/cmd/fatinfo.c, since all they need
from Python is a pair of images. Neither is written to, so one of each serves
the whole class.
"""

import os

import pytest
from tests.fs_helper import FsHelper

# Contents of hello.txt, 14 bytes including the newline
HELLO = 'Hello, world!\n'

# Size of the image which holds no filesystem
RAW_SIZE = 64 * 1024


@pytest.mark.boardspec('sandbox')
@pytest.mark.buildconfigspec('cmd_fat')
class TestFatinfo:
    """Tests for the fatinfo command"""

    @pytest.fixture(scope='class')
    def fat_image(self, u_boot_config):
        """Create a 1MB FAT image with a file in it

        Args:
            u_boot_config (u_boot_config): U-Boot configuration

        Yields:
            str: Path to the FAT image
        """
        with FsHelper(u_boot_config, 'vfat', 1, 'test_fatinfo') as fsh:
            with open(f'{fsh.srcdir}/hello.txt', 'w',
                      encoding='ascii') as outf:
                outf.write(HELLO)
            fsh.mk_fs()
            yield fsh.fs_img

    @pytest.fixture(scope='class')
    def raw_image(self, u_boot_config):
        """Create an image of zeroes, so that it holds no filesystem

        Args:
            u_boot_config (u_boot_config): U-Boot configuration

        Yields:
            str: Path to the image
        """
        fname = os.path.join(u_boot_config.persistent_data_dir,
                             'fatinfo_raw.img')
        with open(fname, 'wb') as outf:
            outf.write(bytes(RAW_SIZE))
        yield fname

    def test_fatinfo_base(self, ubman, fat_image, raw_image):
        """Check that fatinfo reports the device and its filesystem"""
        ubman.run_ut('cmd', 'cmd_test_fatinfo_base', fs_image=fat_image,
                     raw_image=raw_image)

    def test_fatinfo_default(self, ubman, fat_image, raw_image):
        """Check that the device comes from bootdevice when left out"""
        ubman.run_ut('cmd', 'cmd_test_fatinfo_default', fs_image=fat_image,
                     raw_image=raw_image)

    def test_fatinfo_notfat(self, ubman, fat_image, raw_image):
        """Check that a device without a FAT filesystem is reported"""
        ubman.run_ut('cmd', 'cmd_test_fatinfo_notfat', fs_image=fat_image,
                     raw_image=raw_image)

    def test_fatinfo_baddev(self, ubman, fat_image, raw_image):
        """Check that fatinfo complains about a device which is not there"""
        ubman.run_ut('cmd', 'cmd_test_fatinfo_baddev', fs_image=fat_image,
                     raw_image=raw_image)

    def test_fatinfo_usage(self, ubman, fat_image, raw_image):
        """Check that fatinfo shows its usage message when it cannot go on"""
        ubman.run_ut('cmd', 'cmd_test_fatinfo_usage', fs_image=fat_image,
                     raw_image=raw_image)
