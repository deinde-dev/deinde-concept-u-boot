.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: size (command)

size command
============

Synopsis
--------

::

    size <path>
    size <interface> <dev[:part]> <filename>

Description
-----------

The size command determines the size of a file and sets the environment
variable filesize to this value, in hexadecimal.

If the command fails, the filesize environment variable is not changed.

VFS form
~~~~~~~~

When the VFS is enabled, the first form is used. The filesystem must be
mounted first using ``mount``.

path
    absolute or relative path to the file in the VFS

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

filename
    path to file

Example
-------

VFS::

    => mount host 0:0 /mnt
    => size /mnt/testfile.txt
    => echo $filesize
    c

Legacy::

    => size mmc 0:1 snp.efi
    => echo $filesize
    24720

Configuration
-------------

The VFS form is available when CONFIG_CMD_VFS=y. The legacy form is
available when CONFIG_CMD_FS_LEGACY=y.

Return value
------------

The return value $? is set to 0 (true) if the file was found,
1 (false) otherwise.

See also
--------

* :doc:`fatsize<fatsize>` for the FAT-only form of this command
* :doc:`ext4size<ext4size>` for the ext-only form of this command
* :doc:`ext2ls<ext2ls>` for listing an ext filesystem, which shows sizes too
* :doc:`ext2load<ext2load>` for reading a file, which also sets filesize
* :doc:`fstypes<fstypes>` for the filesystem types this command can read
* :doc:`load<load>` for reading a file into memory, which also sets filesize
* :doc:`save<save>` for writing memory out to a file
* :doc:`mount<mount>` for making a filesystem available to the VFS form
* :doc:`host<host>` for the sandbox form which sizes a file on the host
