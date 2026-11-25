.. SPDX-License-Identifier: GPL-2.0-or-later

.. index::
   single: read (command)

read command
============

Synopsis
--------

::

    read <interface> <dev[:part|#partname]> <addr> <blk#> <cnt>

Description
-----------

The read command can be used for raw reading data from a block device
(or a partition therein), i.e. without going through a file system.

The block device is specified using the <interface> (e.g. "mmc") and
<dev> parameters. If the block device has a partition table, one can
optionally specify a partition number (using the :part syntax) or
partition name (using the #partname syntax). The command then reads
the <cnt> blocks of data starting at block number <blk#> of the given
device/partition to the memory address <addr>.

Leaving the partition out reads from partition 1, or from the whole
device where there is no partition table, while partition 0 always means
the whole device. Within a partition the block number counts from the
start of that partition and a request reaching past its end is refused;
on the whole device it is a block number on the device itself and the
only limit is the one the device reports.

The <addr>, <blk#> and <cnt> values are hexadecimal. Nothing is printed
when the command succeeds.

Examples
--------

Read the first two blocks of partition 1 of mmc device 2 into memory at
address 1000, then show the start of what arrived::

    => read mmc 2:1 1000 0 2
    => md.l 1000 4
    00001000: 12345678 12345678 12345678 12345678  xV4.xV4.xV4.xV4.

Partition 1 of that device holds 16 blocks, so asking for a block beyond
them fails::

    => read mmc 2:1 1000 10 1
    read out of range

The same block can be reached by the partition's name, or as block 0x30
of the whole device, which is where partition 1 begins::

    => read mmc 2#data 1000 0 1
    => read mmc 2:0 1000 30 1

Configuration
-------------

The read command is only available if CONFIG_CMD_READ=y.

Return value
------------

The command sets the return value $? to 0 (true) on success or to 1 (false) in
case of an error.

See also
--------

* :doc:`write<write>` for the opposite direction, from memory to a device
* :doc:`clone<clone>` for copying blocks straight from one device to another,
  without passing them through memory first
* :doc:`part<part>` for listing the partitions of a device, which gives the
  partition numbers and names this command takes
* :doc:`ide<ide>` for reading blocks from an IDE drive without going through a
  partition
* :doc:`nvme<nvme>` for the same on an NVMe namespace
