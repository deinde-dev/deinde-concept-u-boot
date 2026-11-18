.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: rmdir (command)

rmdir command
=============

Synopsis
--------

::

    rmdir <path>...

Description
-----------

The rmdir command removes each directory given, in the VFS. A directory must be
empty to be removed. The filesystem must be mounted first using ``mount``.

If a directory cannot be removed, the command reports it and carries on with
the others.

Not all filesystems support removing a directory. If the filesystem does not
implement the rmdir operation, the command returns an error.

path
    absolute or relative path to the directory to remove

Example
-------

::

    => mount hostfs /host
    => mkdir /host/tmp1
    => mkdir /host/tmp2
    => rmdir /host/tmp1 /host/tmp2

A directory which has something in it is not removed::

    => mkdir /host/tmp1
    => save 1000 /host/tmp1/file 10
    16 bytes written
    => rmdir /host/tmp1
    Cannot remove '/host/tmp1': -39: Directory not empty

Nor is a file::

    => rmdir /host/tmp1/file
    Cannot remove '/host/tmp1/file': -20: Not a directory

Configuration
-------------

The rmdir command is available when CONFIG_CMD_VFS=y.

Return value
------------

The return value $? is set to 0 (true) if every directory was removed,
1 (false) otherwise.

See also
--------

* :doc:`mkdir<mkdir>` for creating a directory
* :doc:`rm<rm>` for deleting a file
* :doc:`mount<mount>` for making a filesystem available to the VFS
