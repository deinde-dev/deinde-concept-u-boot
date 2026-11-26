.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: chid (command)

chid command
============

Synopsis
--------

::

    chid compat
    chid list
    chid show
    chid detail <variant>

Description
-----------

The chid command provides access to Computer Hardware IDs (CHIDs) generated from
SMBIOS data. CHIDs are used by Windows Update and fwupd for hardware
identification and firmware-update targeting.

CHIDs are generated according to Microsoft's specification, which defines 15
different hardware ID variants (HardwareID-00 through HardwareID-14), each
using different combinations of SMBIOS fields. The variants range from most
specific (including all identifying fields) to least specific (manufacturer
only).

Subcommands
-----------

show
    Show the relevant SMBIOS values for the current board. These are used to
    calculate CHIDs.

compat
    Find the compatible string that matches the current hardware's CHID and
    set the fdtcompat environment variable. This examines the devicetree under
    /chid for nodes with hardware-id child nodes containing CHID data that
    matches the system's generated CHIDs.

list
    Display all 15 CHID variants with their generated UUIDs

detail <variant>
    Display details for a specific CHID variant (0-14), including which
    SMBIOS fields are used and the generated UUID

Examples
--------

These come from a sandbox run, where the SMBIOS values are the ones in the
test devicetree::

    => chid show
    Manufacturer:      Sandbox Corp
    Family:            Sandbox_Family
    Product Name:      Sandbox Computer
    Product SKU:       SANDBOX-SKU
    Baseboard Manuf:   Sandbox Boards
    Baseboard Product: Sandbox Motherboard
    BIOS Vendor:       Deinde Concept U-Boot
    BIOS Version:      2026.12-rc3-g905df42b432c
    BIOS Major:        26
    BIOS Minor:        12
    Enclosure Type:    2
    =>

This shows how to obtain the CHIDs for all variants::

    => chid list
    HardwareID-00: a5814c69-1123-51e7-a780-18fb27318dda
    HardwareID-01: 746b256a-521b-5c35-81ba-8833f6287510
    HardwareID-02: 60901d1a-bf05-5546-b8a5-47970d4ea96c
    HardwareID-03: a07db1d8-e0f9-5d47-a507-8c358eb7edf4
    HardwareID-04: 5efd2af9-23d6-5fe6-bdb0-e040b9e5b054
    HardwareID-05: 48aede6f-65db-51a5-8905-fdabdbc0685e
    HardwareID-06: d1c25f0a-6eb8-5c23-961c-e46694384fa0
    HardwareID-07: 84398dad-5de1-553b-87b9-1b7c4b02c505
    HardwareID-08: ed1fa480-f06b-52ba-b203-56562c52c7e2
    HardwareID-09: c0185db1-6111-5432-955a-e5ecdac0d351
    HardwareID-10: 6b7ad4ae-1f41-52dd-8499-ab9ab9e8688c
    HardwareID-11: 72523836-a3fc-57b0-8d3c-4ce56cbf6b96
    HardwareID-12: e93b37fb-5592-55dd-8e4b-9df3fbc9abde
    HardwareID-13: d67e233f-5cad-541c-87f8-ee9bca569e5e
    HardwareID-14: 45c5e2e7-db48-556b-aae4-0a03c5a15eae

The first three variants take in the BIOS version, so their UUIDs are
different for every build. The rest depend only on the board, so they stay
the same from one build to the next.

Show details for a specific variant::

    => chid detail 14
    HardwareID-14: 45c5e2e7-db48-556b-aae4-0a03c5a15eae
    Fields: Manufacturer

Find compatible string and set environment variable::

    => chid compat
    sandbox,test-device-1
    => printenv fdtcompat
    fdtcompat=sandbox,test-device-1

Configuration
-------------

The chid command is available when ``CONFIG_CMD_CHID`` is enabled, which
needs ``CONFIG_CHID`` and the SMBIOS parser it relies on.

Return value
------------

The return value $? is 0 (true) on success and 1 (false) on failure. A
sub-command fails when the SMBIOS data cannot be read, when no CHID matches
any node under /chid, or when the variant given to ``chid detail`` is not in
the range 0 to 14, each case reporting what went wrong.

The usage message appears instead, with $? set to 1 (false), when no
sub-command is given, when it is not one of the four on offer, or when the
arguments after it are not what that sub-command takes.

See also
--------

* :doc:`/develop/chid` for how the CHID data and the /chid nodes are built
* :doc:`smbios<smbios>` for showing the SMBIOS tables the CHIDs come from
