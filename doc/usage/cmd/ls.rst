.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: ls (command)

ls command
==========

Synopsis
--------

::

    ls [<path>]
    ls <interface> [<dev[:part]> [directory]]

Description
-----------

The ls command lists the contents of a directory.

VFS form
~~~~~~~~

When the VFS is enabled, the first form is used. Entries are sorted
alphabetically. If no path is given, the current working directory is
listed.

Directories are shown with the ``DIR`` prefix. Regular files show their
size in bytes.

The filesystem must be mounted first using ``mount``.

The ``ls`` command is also available as ``fs ls``; see :doc:`fs`.

path
    absolute or relative path to list (default: current working directory)

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

directory
    directory to list, within the filesystem (default /)

part is a hexadecimal number.

Each entry of the listing shows a file's size, or nothing for a directory,
whose name then ends in a slash. The listing ends with a count of each.
Entries arrive in the order the filesystem holds them, which need not be
alphabetical, and nothing is printed when a directory cannot be read.

Example
-------

VFS::

    => mount host 0:0 /mnt
    => ls /mnt
    DIR          0 .
    DIR          0 ..
    DIR          0 subdir
                12 testfile.txt

Legacy, here with a FAT image attached to sandbox's host device::

    => host bind 0 fat.img
    => ls host 0
                docs/
                empty/
           14   hello.txt

    1 file(s), 2 dir(s)

    => ls host 0:0 docs
                ./
                ../
            5   note.txt

    1 file(s), 2 dir(s)

Configuration
-------------

The VFS form is available when CONFIG_CMD_VFS=y, which depends on
CONFIG_VFS=y. The legacy form is available when CONFIG_CMD_FS_LEGACY=y.

Return value
------------

The return value $? is set to 0 (true) if the directory was listed,
1 (false) otherwise.

See also
--------

* :doc:`ext2ls<ext2ls>` for listing an ext filesystem without mounting it
* :doc:`ext4ls<ext4ls>` for that under its ext4 name
* :doc:`fatls<fatls>` for listing a FAT filesystem without mounting it
* :doc:`sqfsls<sqfsls>` for listing a SquashFS filesystem without mounting it
* :doc:`erofsls<erofsls>` for listing an EROFS filesystem without mounting it
* :doc:`cramfsls<cramfsls>` for listing a CRAMFS image held in memory,
  which has no block device to mount
* :doc:`fstypes<fstypes>` for the filesystem types this command can read
* :doc:`mount<mount>` for making a filesystem available to this command
* :doc:`size<size>` for asking about one file rather than listing them all
* :doc:`load<load>` for reading one of the files into memory
* :doc:`fs<fs>` for listing with the same command which mounts
* :doc:`host<host>` for the sandbox form which lists a directory on the host
