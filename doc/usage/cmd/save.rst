.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: save (command)

save command
============

Synopsis
--------

::

    save <addr> <path> <bytes> [pos]
    save <interface> <dev[:part]> <addr> <filename> bytes [pos]

Description
-----------

The save command writes data from memory to a file. The number of bytes
is always given, since memory holds nothing to say where the data ends.

VFS form
~~~~~~~~

When the VFS is enabled, the first form is used. The filesystem must be
mounted first using ``mount``.

addr
    memory address to read from (hexadecimal)

path
    absolute path in the VFS, e.g. ``/host/output.bin``

bytes
    number of bytes to write (hexadecimal)

pos
    file byte position to start writing at (hexadecimal, default 0)

Legacy form
~~~~~~~~~~~

The second form is used when the VFS is not enabled and accesses a block
device directly without mounting.

interface
    interface for accessing the block device (mmc, sata, scsi, usb, ....)

dev
    device number

part
    partition number. With no partition given, partition 1 is used, or the
    whole device if it has no partition table. A partition of 0 means the
    whole device and *auto* means the first bootable partition.

addr
    memory address to read from

filename
    path to the file, which is created if it does not exist

bytes
    number of bytes to write

pos
    file byte position to start writing at

part, addr, bytes, pos are hexadecimal numbers.

Writing at a *pos* which is part-way through an existing file leaves the
rest of the file as it is, so the file is not truncated to the number of
bytes written.

Example
-------

VFS::

    => mount hostfs /host
    => save 1000000 /host/data.bin 1000
    4096 bytes written
    => save 1000000 /host/data.bin 100 800
    256 bytes written

Legacy, here with sandbox's own filesystem, which writes files of the host
U-Boot is running on::

    => save sandbox - 1000000 /tmp/data.bin 1000
    4096 bytes written in 0 ms
    => save sandbox - 1000000 /tmp/data.bin 100 800
    256 bytes written in 0 ms

Configuration
-------------

The VFS form is available when CONFIG_CMD_VFS=y, which depends on
CONFIG_VFS=y. The legacy form is available when CONFIG_CMD_FS_LEGACY=y.

Return value
------------

The return value $? is set to 0 (true) if the file was successfully
written, 1 (false) otherwise.

See also
--------

* :doc:`fatwrite<fatwrite>` for the FAT-only form of this command
* :doc:`ext4write<ext4write>` for the ext4-only form of this command
* :doc:`fstypes<fstypes>` for the filesystem types this command can write
* :doc:`load<load>` for reading the data back into memory
* :doc:`size<size>` for finding the size of a file written this way
* :doc:`mount<mount>` for making a filesystem available to the VFS
* :doc:`pcap<pcap>` for producing a packet capture in memory to write out
* :doc:`host<host>` for the sandbox form which writes to the host filesystem
