.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: rm (command)

rm command
==========

Synopsis
--------

::

    rm [-rf] <path>...

Description
-----------

The rm command deletes each file given, in the VFS. The filesystem must be
mounted first using ``mount``.

If a file cannot be deleted, the command reports it and carries on with the
others.

Not all filesystems support file deletion. If the filesystem does not
implement the unlink operation, the command returns an error.

-r
    delete a directory too, along with everything in it. Symbolic links are
    deleted, not followed. A mount point cannot be deleted: unmount the
    filesystem first. If something in the directory cannot be deleted, the
    command stops work on that path, so part of the tree may be left

-f
    do not complain if a path does not exist

path
    absolute or relative path to the file, or directory, to delete

Example
-------

::

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

Configuration
-------------

The rm command is available when CONFIG_CMD_VFS=y.

Return value
------------

The return value $? is set to 0 (true) if every path was deleted,
1 (false) otherwise.

See also
--------

* :doc:`fatrm<fatrm>` for the FAT-only form of this command
* :doc:`mkdir<mkdir>` for creating a directory
* :doc:`rmdir<rmdir>` for removing an empty directory
* :doc:`mount<mount>` for making a filesystem available to the VFS
