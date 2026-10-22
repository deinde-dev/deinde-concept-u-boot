# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.

"""Create Android test disk images"""

import collections
import os

import utils
from test_android import test_abootimg


def setup_android_image(config, log):
    """Create a 20MB disk image with Android partitions

    Args:
        config (ArbitraryAttributeContainer): Configuration
        log (multiplexed_log.Logfile): Log to write to
    """
    Partition = collections.namedtuple('part', 'start,size,name')
    parts = {}
    disk_data = None

    mmc_dev = 7
    fname = os.path.join(config.source_dir, f'mmc{mmc_dev}.img')
    utils.run_and_log_no_ubman(log, f'qemu-img create {fname} 20M')
    utils.run_and_log_no_ubman(log, f'cgpt create {fname}')

    ptr = 40

    # Number of sectors in 1MB
    sect_size = 512
    sect_1mb = (1 << 20) // sect_size

    required_parts = [
        {'num': 1, 'label':'misc', 'size': '1M'},
        {'num': 2, 'label':'boot_a', 'size': '4M'},
        {'num': 3, 'label':'boot_b', 'size': '4M'},
        {'num': 4, 'label':'vendor_boot_a', 'size': '4M'},
        {'num': 5, 'label':'vendor_boot_b', 'size': '4M'},
    ]

    for part in required_parts:
        size_str = part['size']
        if 'M' in size_str:
            size = int(size_str[:-1]) * sect_1mb
        else:
            size = int(size_str)
        utils.run_and_log_no_ubman(
            log,
            f"cgpt add -i {part['num']} -b {ptr} -s {size} -l {part['label']} -t basicdata {fname}")
        ptr += size

    utils.run_and_log_no_ubman(log, f'cgpt boot -p {fname}')
    out = utils.run_and_log_no_ubman(log, f'cgpt show -q {fname}')

    # Create a dict (indexed by partition number) containing the above info
    for line in out.splitlines():
        start, size, num, name = line.split(maxsplit=3)
        parts[int(num)] = Partition(int(start), int(size), name)

    with open(fname, 'rb') as inf:
        disk_data = inf.read()

    images = [
        (2, 'bootv4.img', test_abootimg.boot_img_hex),
        (4, 'vendor_boot.img', test_abootimg.vboot_img_hex),
    ]
    for partnum, img_name, hex_data in images:
        test_abootimg.AbootimgTestDiskImage(config, log, img_name, hex_data)
        img_path = os.path.join(config.result_dir, img_name)
        with open(img_path, 'rb') as inf:
            data = inf.read()
        start = parts[partnum].start * sect_size
        disk_data = disk_data[:start] + data + disk_data[start + len(data):]

    with open(fname, 'wb') as outf:
        outf.write(disk_data)

    mmc_dev = 8
    fname = os.path.join(config.source_dir, f'mmc{mmc_dev}.img')
    utils.run_and_log_no_ubman(log, f'qemu-img create {fname} 20M')
    utils.run_and_log_no_ubman(log, f'cgpt create {fname}')

    ptr = 40

    # Number of sectors in 1MB
    sect_size = 512
    sect_1mb = (1 << 20) // sect_size

    required_parts = [
        {'num': 1, 'label':'misc', 'size': '1M'},
        {'num': 2, 'label':'boot_a', 'size': '4M'},
        {'num': 3, 'label':'boot_b', 'size': '4M'},
    ]

    for part in required_parts:
        size_str = part['size']
        if 'M' in size_str:
            size = int(size_str[:-1]) * sect_1mb
        else:
            size = int(size_str)
        utils.run_and_log_no_ubman(
            log,
            f"cgpt add -i {part['num']} -b {ptr} -s {size} -l {part['label']} -t basicdata {fname}")
        ptr += size

    utils.run_and_log_no_ubman(log, f'cgpt boot -p {fname}')
    out = utils.run_and_log_no_ubman(log, f'cgpt show -q {fname}')

    # Create a dict (indexed by partition number) containing the above info
    for line in out.splitlines():
        start, size, num, name = line.split(maxsplit=3)
        parts[int(num)] = Partition(int(start), int(size), name)

    with open(fname, 'rb') as inf:
        disk_data = inf.read()

    images = [
        (2, 'boot.img', test_abootimg.img_hex),
    ]
    for partnum, img_name, hex_data in images:
        test_abootimg.AbootimgTestDiskImage(config, log, img_name, hex_data)
        img_path = os.path.join(config.result_dir, img_name)
        with open(img_path, 'rb') as inf:
            data = inf.read()
        start = parts[partnum].start * sect_size
        disk_data = disk_data[:start] + data + disk_data[start + len(data):]

    with open(fname, 'wb') as outf:
        outf.write(disk_data)
