.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: mmcinfo (command)

mmcinfo command
===============

Synopsis
--------

::

    mmcinfo

Description
-----------

The mmcinfo command shows what is known about the current MMC device: the
driver's name for it, the identification fields of the card, the speed and
width of the bus in use, and the capacity of the card and of each of its
hardware partitions.

It is the older name for :doc:`mmc info<mmc>` and runs the same code, so the
two print the same report. The current device is the one set by 'mmc dev';
where none has been set, the command uses device 0.

Which fields appear depends on the card and on the build:

* the OEM and name fields are decoded differently for SD and for MMC
* the mode and the card and host capabilities need CONFIG_MMC_VERBOSE=y
* the erase-group size needs CONFIG_MMC_WRITE=y
* the user, boot, RPMB and general-purpose capacities, and the state of the
  two boot areas, are shown for an MMC card of version 4.41 or later
* the write-protect group size needs CONFIG_MMC_HW_PARTITIONING=y as well

The command takes no arguments. See :doc:`mmc<mmc>` for what each field means
and for the rest of the operations on an MMC device.

Example
-------

This shows the first emulated MMC device of a sandbox build. The devices must
be probed before the command can find one, which 'mmc list' does::

    => mmc list
    mmc2: 2 (SD)
    mmc1: 1 (SD)
    mmc0: 0 (SD)
    => mmcinfo
    Device: mmc0
    Manufacturer ID: 0
    OEM: 0
    Name: Bus Speed: 1000000
    Mode: MMC legacy
    Rd Block Len: 512
    SD version 3.0
    High Capacity: Yes
    Capacity: 1 MiB
    Bus Width: 1-bit
    Erase Group Size: 512 Bytes

The emulated card has an all-zero identification register, so its name is six
NUL bytes; printf() stops at the first of them and drops the newline with it,
which is why the bus speed shares the line with the name.

Return value
------------

The return value $? is 0 (true) if the report is shown. It is set to 1 (false)
if there is no MMC device at all, or if the current device cannot be
initialised, in which case the command reports which slot it tried.

See also
--------

* :doc:`mmc<mmc>` for the same report as 'mmc info', along with reading,
  writing and partitioning an MMC device
* :doc:`lsblk<lsblk>` for the MMC devices listed with the block devices of
  every other driver
