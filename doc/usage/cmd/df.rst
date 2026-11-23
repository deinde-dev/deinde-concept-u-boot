.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: df (command)

df command
==========

Synopsis
--------

::

    df [<path>]

Description
-----------

The df command displays filesystem usage statistics. With no arguments
it shows all mounts. With a path, it shows the filesystem containing
that path.

The columns are the mount point, the block size of the filesystem and
the total, used and free space in bytes. The *Size* column repeats the
total in units of KiB, MiB or GiB and appears only when a path is given,
since it describes a single filesystem.

Not all filesystems report their usage. ext4 provides full statistics;
FAT and sandbox hostfs do not, so they show dashes in the listing of all
mounts, and naming one reports that the filesystem has no such function.

path
    absolute or relative path within a mounted filesystem (optional)

Example
-------

Here sandbox has an ext4 image mounted at ``/mnt`` and its own host
filesystem at ``/host``::

    => df
    Filesystem        Blksz      Total       Used       Free  Size
    /mnt               4096    2097152     114688    1982464
    /host                 -          -          -          -
    => df /mnt
    Filesystem        Blksz      Total       Used       Free  Size
    /mnt               4096    2097152     114688    1982464  2.0 MiB

A filesystem which does not report its usage, and a path with no
filesystem mounted behind it, are reported along with the error number::

    => df /host
    Error: -38: Function not implemented
    => df /nowhere
    Error: -2: No such file or directory

Configuration
-------------

The df command is available when CONFIG_CMD_VFS=y.

Return value
------------

The return value $? is set to 0 (true) if the statistics were
retrieved, 1 (false) otherwise.

See also
--------

* :doc:`mount<mount>` for attaching a filesystem, which this command
  then reports on
* :doc:`umount<umount>` for detaching one
* :doc:`size<size>` for the size of a single file
* :doc:`ls<ls>` for the sizes of the files in a directory
