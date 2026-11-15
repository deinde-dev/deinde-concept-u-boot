# SPDX-License-Identifier: GPL-2.0+
# Network settings for the Snapper 9260 in the lab, used by test_net.py
#
# The lab's DHCP server hands out an address but no server, so the tests use
# kea (192.168.4.12), which runs the TFTP server. The lab sets the board's MAC
# address, since it has none stored.

env__net_dhcp_server = True

env__net_static_env_vars = [
    ('serverip', '192.168.4.12'),
]

# A 1 MiB file of random data on kea, in /srv/tftp
env__net_tftp_readable_file = {
    'fn': 'ubtest-readable.bin',
    'addr': 0x20000000,
    'size': 1048576,
    'crc32': '06e44ebb',
}
