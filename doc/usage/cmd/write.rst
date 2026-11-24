.. SPDX-License-Identifier: GPL-2.0-or-later

.. index::
   single: write (command)

write command
=============

Synopsis
--------

::

    write <interface> <dev[:part|#partname]> <addr> <blk#> <cnt>

Description
-----------

The write command can be used for raw writing data to a block device
(or partition therein), i.e. without going through a file system.

The block device is specified using the <interface> (e.g. "mmc") and
<dev> parameters. If the block device has a partition table, one can
optionally specify a partition number (using the :part syntax) or
partition name (using the #partname syntax). The command then writes
the <cnt> blocks of data at the memory address <addr> to block number
<blk#> of the given device/partition.

Leaving the partition out writes to partition 1, or to the whole device
where there is no partition table, while partition 0 always means the
whole device. Within a partition the block number counts from the
start of that partition and a request reaching past its end is refused;
on the whole device it is a block number on the device itself and the
only limit is the one the device reports.

The <addr>, <blk#> and <cnt> values are hexadecimal. Nothing is printed
when the command succeeds.

Examples
--------

Write two blocks from memory at address 1000 to the start of partition 1
of mmc device 2, then clear the memory and read the blocks back to show
that they arrived::

    => mw.l 1000 12345678 100
    => write mmc 2:1 1000 0 2
    => mw.l 1000 0 100
    => read mmc 2:1 1000 0 2
    => md.l 1000 4
    00001000: 12345678 12345678 12345678 12345678  xV4.xV4.xV4.xV4.

Partition 1 of that device holds 16 blocks, so writing beyond them
fails, leaving the device untouched::

    => write mmc 2:1 1000 10 1
    write out of range

Configuration
-------------

The write command is only available if CONFIG_CMD_WRITE=y.

Return value
------------

The command sets the return value $? to 0 (true) on success and to 1 (false) in
case of an error.

See also
--------

* :doc:`read<read>` for the opposite direction, from a device to memory
* :doc:`clone<clone>` for copying blocks straight from one device to another,
  without passing them through memory first
* :doc:`part<part>` for listing the partitions of a device, which gives the
  partition numbers and names this command takes
* :doc:`ide<ide>` for writing blocks to an IDE drive without going through a
  partition
* :doc:`nvme<nvme>` for the same on an NVMe namespace
