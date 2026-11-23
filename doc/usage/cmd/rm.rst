.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: rm (command)

rm command
==========

Synopsis
--------

::

    rm [-rf] <path>...
    rm <interface> <dev[:part]> <filename>

Description
-----------

The rm command deletes a file.

Not all filesystems support file deletion. If the filesystem does not
implement the unlink operation, the command returns an error.

VFS form
~~~~~~~~

When the VFS is enabled, the first form is used. It deletes each file given.
The filesystem must be mounted first using ``mount``.

If a file cannot be deleted, the command reports it and carries on with the
others.

-r
    delete a directory too, along with everything in it. Symbolic links are
    deleted, not followed. A mount point cannot be deleted: unmount the
    filesystem first. If something in the directory cannot be deleted, the
    command stops work on that path, so part of the tree may be left

-f
    do not complain if a path does not exist

path
    absolute or relative path to the file, or directory, to delete

Legacy form
~~~~~~~~~~~

The second form is used when the VFS is not enabled and accesses a block
device directly without mounting. It deletes one file and has no options.

interface
    interface for accessing the block device (mmc, sata, scsi, usb, ....)

dev
    device number

part
    partition number. With no partition given, partition 1 is used, or the
    whole device if it has no partition table. A partition of 0 means the
    whole device and *auto* means the first bootable partition.

filename
    path of the file to delete, within the filesystem

part is a hexadecimal number.

A FAT filesystem has no separate operation for removing a directory, so
there this command deletes an empty directory as well as a file.

Example
-------

VFS::

    => mount hostfs /host
    => rm /host/old-file.txt
    => mount -t ext4 host 0:1 /mnt
    => rm /mnt/temp.bin /mnt/temp2.bin

A directory needs the -r option::

    => mkdir -p /host/tmp/a/b
    => save 1000 /host/tmp/a/file 10
    16 bytes written
    => rm /host/tmp
    Cannot remove '/host/tmp': -21: Is a directory
    => rm -r /host/tmp

A path which does not exist is an error, unless -f is given::

    => rm /host/missing
    Cannot remove '/host/missing': -2: No such file or directory
    => rm -f /host/missing

Legacy, here with a FAT image attached to sandbox's host device. The command
says nothing when it works::

    => host bind 0 fat.img
    => ls host 0
                docs/
                empty/
           14   hello.txt

    1 file(s), 2 dir(s)

    => rm host 0 hello.txt
    => rm host 0 empty
    => ls host 0
                docs/

    0 file(s), 1 dir(s)

A file which is not there is reported by the filesystem, and a directory
with anything left in it is refused::

    => rm host 0 nothere
    nothere: doesn't exist (-2)
    => rm host 0 docs
    Error: directory is not empty: 3

Configuration
-------------

The VFS form is available when CONFIG_CMD_VFS=y, which depends on
CONFIG_VFS=y. The legacy form is available when CONFIG_CMD_FS_LEGACY=y.

Return value
------------

The return value $? is set to 0 (true) if every path was deleted,
1 (false) otherwise.

See also
--------

* :doc:`fatrm<fatrm>` for the FAT-only form of this command
* :doc:`mkdir<mkdir>` for creating a directory
* :doc:`rmdir<rmdir>` for removing an empty directory
* :doc:`mv<mv>` for moving a file instead of deleting it
* :doc:`ls<ls>` for seeing which files are there
* :doc:`mount<mount>` for making a filesystem available to the VFS
