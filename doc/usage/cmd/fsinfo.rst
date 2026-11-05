.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: fsinfo (command)

fsinfo command
==============

Synopsis
--------

::

    fsinfo <interface> <dev[:part]>

Description
-----------

The fsinfo command displays filesystem statistics for a partition including
block size, total blocks, used blocks, and free blocks. Both raw byte counts
and human-readable sizes are shown.

interface
    interface for accessing the block device (mmc, sata, scsi, usb, ....)

dev
    device number

part
    partition number, defaults to 1

Example
-------

::

    => fsinfo mmc 0:1
    Block size: 4096 bytes
    Total blocks: 16384 (67108864 bytes, 64 MiB)
    Used blocks: 2065 (8458240 bytes, 8.1 MiB)
    Free blocks: 14319 (58650624 bytes, 55.9 MiB)

Configuration
-------------

The fsinfo command is only available if CONFIG_CMD_FS_GENERIC=y.

Return value
------------

The return value $? is set to 0 (true) if the command succeeded and to 1
(false) otherwise. If the filesystem does not support statfs, an error
message is displayed.

See also
--------

* :doc:`fstypes<fstypes>` for the filesystem types this build can read
* :doc:`ls<ls>` for listing a directory on the filesystem
