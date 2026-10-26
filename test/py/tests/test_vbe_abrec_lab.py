# SPDX-License-Identifier: GPL-2.0+
# Copyright 2026 Simon Glass <sjg@chromium.org>
"""Test the VBE A/B/recovery flow on a lab board

This drives a board whose firmware uses the abrec method from the U-Boot
prompt: it copies A to B and asks VPL to try B, breaks B so that VPL falls
back to A, then requests the recovery firmware. Each step resets the board and
checks which firmware VPL picked. Since it rewrites B and the state, it runs
in its own lab job.
"""

import os
import re

import pytest

# the abrec bootmeth in the control devicetree
NODE = '/bootstd/firmware0'
BOOTMETH = 'firmware0'

BLKSZ = 0x200


def get_layout(ubman):
    """Read the abrec properties from the control devicetree

    Returns:
        tuple:
            int: MMC device number of the storage
            dict of int: property values, by name
    """
    ubman.run_command('fdt addr $fdtcontroladdr')
    out = ubman.run_command(f'fdt print {NODE}')
    props = {name: int(val, 16)
             for name, val in re.findall(r'(\S+) = <(0x[0-9a-f]+)>;', out)}
    devnum = int(re.search(r'storage = "mmc(\d+)"', out).group(1))
    return devnum, props


def copy_a_to_b(ubman, devnum, props):
    """Copy the A firmware to the B slot"""
    count = props['slot-size'] // BLKSZ
    ubman.run_command(f'mmc dev {devnum}')
    with ubman.temporary_timeout(60 * 1000):
        out = ubman.run_command(
            f'mmc read $loadaddr {props["slot-a-offset"] // BLKSZ:x} '
            f'{count:x}')
        assert f'{count} blocks read: OK' in out
        out = ubman.run_command(
            f'mmc write $loadaddr {props["slot-b-offset"] // BLKSZ:x} '
            f'{count:x}')
        assert f'{count} blocks written: OK' in out


def break_b(ubman, devnum, props):
    """Zero the start of B, so that VPL cannot load it"""
    ubman.run_command(f'mmc dev {devnum}')
    ubman.run_command(f'mw.b $loadaddr 0 {BLKSZ:x}')
    out = ubman.run_command(
        f'mmc write $loadaddr {props["slot-b-offset"] // BLKSZ:x} 1')
    assert '1 blocks written: OK' in out


def vbe(ubman, cmd):
    """Run a vbe subcommand on the abrec bootmeth"""
    ubman.run_command(f'vbe select {BOOTMETH}')
    return ubman.run_command(f'vbe {cmd}')


def reset(ubman):
    """Reset the board, returning the 'vbe info' output

    The lab normally builds U-Boot and writes it to the board again when it
    restarts, which would replace the state and B, so turn that off while
    resetting. The hooks take this from the environment, where 'vars' may also
    hold labgrid options which override it
    """
    old = {var: os.environ.get(var) for var in ('bootstrap', 'build', 'vars')}
    os.environ['bootstrap'] = '0'
    os.environ['build'] = '0'
    if old['vars']:
        os.environ['vars'] = re.sub(r'-V (do-bootstrap|do-build) \S+',
                                    r'-V \1 0', old['vars'])
    try:
        ubman.restart_uboot()
    finally:
        for var, val in old.items():
            if val is None:
                os.environ.pop(var, None)
            else:
                os.environ[var] = val
    return vbe(ubman, 'info')


@pytest.mark.buildconfigspec('bootmeth_vbe_abrec')
@pytest.mark.role('vbe')
@pytest.mark.restart
def test_vbe_abrec_lab(ubman):
    """Try B, fall back to A when B cannot be loaded, and boot recovery"""
    devnum, props = get_layout(ubman)
    try:
        # start from A, with nothing requested
        vbe(ubman, 'try off')
        vbe(ubman, 'recovery off')
        info = reset(ubman)
        assert 'Picked: A' in info

        # VPL tries B, using up one of its tries
        copy_a_to_b(ubman, devnum, props)
        assert 'Try B: yes, 3 tries left' in vbe(ubman, 'try on')
        info = reset(ubman)
        assert 'Picked: B' in info
        assert 'Try B: yes, 2 tries left' in info
        assert 'Last result: trying' in info

        # a B which cannot be loaded is marked bad and A is used
        break_b(ubman, devnum, props)
        vbe(ubman, 'try on')
        info = reset(ubman)
        assert 'Picked: A' in info
        assert 'Last result: bad' in info
        assert 'Try B: no' in info

        # recovery firmware is optional
        if 'slot-recovery-offset' in props:
            assert 'Recovery: on' in vbe(ubman, 'recovery on')
            info = reset(ubman)
            assert 'Picked: Recovery' in info

            vbe(ubman, 'recovery off')
            info = reset(ubman)
            assert 'Picked: A' in info
    finally:
        # leave the board with a good B and nothing requested
        copy_a_to_b(ubman, devnum, props)
        vbe(ubman, 'try off')
        vbe(ubman, 'recovery off')
