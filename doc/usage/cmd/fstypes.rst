.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: fstypes (command)

fstypes command
===============

Synopsis
--------

::

    fstypes

Description
-----------

The fstypes command lists the filesystem types which this build of U-Boot
can read, as a single comma-separated line. These are the names which the
*fstype* command reports for a partition, and the filesystems which the
generic :doc:`ls<ls>`, :doc:`load<load>`, :doc:`save<save>` and
:doc:`size<size>` commands can work with.

The command takes no arguments. The list comes from the drivers which are
built in, so it says nothing about the devices attached: a type appears
whether or not any partition holds such a filesystem.

A build with no filesystem driver at all prints ``: <none>`` in place of
the list.

Example
-------

This is sandbox, which carries every filesystem U-Boot supports::

    => fstypes
    Supported filesystems: fat, ext4, iso9660, sandbox, cbfs, btrfs, squashfs, erofs, exfat, virtio

Return value
------------

The return value $? is set to 0 (true), since there is nothing which can
fail. Giving an argument shows the usage message and sets $? to 1 (false).

Configuration
-------------

The fstypes command is available if CONFIG_CMD_VFS=y, or if
CONFIG_CMD_FS_LEGACY=y on a build without the virtual filesystem layer.

See also
--------

* *fstype* for the type of one partition rather than the whole list
* :doc:`fsinfo<fsinfo>` for the block counts of one filesystem
* :doc:`ls<ls>` for listing a directory on any of the types shown
* :doc:`load<load>` for reading a file from any of the types shown
