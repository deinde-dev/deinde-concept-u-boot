# SPDX-License-Identifier: GPL-2.0+
"""
U-Boot: mistakes in the list of phases

scripts/xpl_phases lists the phases, one to a line, each with the devicetree
tag which marks the nodes it needs. A line marked 'primary' gives the primary
phase, which has '-' for its tag.

This checks that a mistake in the list is reported, with its line number where
it has one. Each case is a list of phases and the message which it should
produce. The list has a fixed name, so each case has a source tree of its own,
which is created here.
"""

import os
import shutil

# Contents of scripts/xpl_phases (None to leave it out) and the message expected
CASES = [
    ('spl\n', 'xpl_phases:1: Missing devicetree tag'),
    ('# comment\nspl bootph-pre-ram wibble\n',
     "xpl_phases:2: Unexpected 'wibble'"),
    ('SPL bootph-pre-ram\n', "xpl_phases:1: Invalid phase name 'SPL'"),
    ('1pl bootph-pre-ram\n', "xpl_phases:1: Invalid phase name '1pl'"),
    ('tpl bootph-pre-sram\ntpl bootph-pre-ram\n',
     "xpl_phases:2: Phase name 'tpl' is already in use"),
    ('tools bootph-pre-ram\n',
     "xpl_phases:1: Phase name 'tools' is already in use"),
    (''.join(f'p{seq} bootph-pre-ram\n' for seq in range(8)),
     'xpl_phases:8: Too many phases'),
    (None, 'Cannot open'),
    ('spl a\nppl - primary\nqpl - primary\n',
     'xpl_phases:3: Only one phase can be primary'),
    ('spl a\nspl - primary\n',
     "xpl_phases:2: Phase name 'spl' is already in use"),
    ('ppl - primary\nppl a\n',
     "xpl_phases:2: Phase name 'ppl' is already in use"),
    ('spl a\nppl primary\n',
     "xpl_phases:2: Missing devicetree tag before 'primary'"),
    ('spl a\nppl bootph-all primary\n',
     "xpl_phases:2: The primary phase has no devicetree tag: use '-'"),
    ('spl a\nppl - primary wibble\n', "xpl_phases:2: Unexpected 'wibble'"),
]


def test(conf, tmp_path):
    test_dir = os.path.dirname(__file__)
    for seq, (phases, expect) in enumerate(CASES):
        srctree = tmp_path / str(seq)
        os.makedirs(srctree / 'scripts')
        shutil.copy(os.path.join(test_dir, 'Kconfig'), srctree)
        (srctree / 'scripts' / 'conf_nospl').write_text('')
        if phases is not None:
            (srctree / 'scripts' / 'xpl_phases').write_text(phases)

        # KCONFIG_SPL makes syncconfig write the configuration of each phase,
        # which is when the list is read
        env = {'KCONFIG_SPL': '1', 'srctree': str(srctree)}
        assert conf.syncconfig('config', extra_env=env) != 0
        assert expect in conf.stderr
