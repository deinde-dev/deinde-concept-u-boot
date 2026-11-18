.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: mkdir (command)

mkdir command
=============

Synopsis
--------

::

    mkdir [-p] <path>...

Description
-----------

The mkdir command creates a directory at each path given, in the VFS. The
parent directory must already exist, unless -p is given. The filesystem must
be mounted first using ``mount``.

If a directory cannot be created, the command reports it and carries on with
the others.

Not all filesystems support directory creation. If the filesystem does
not implement the mkdir operation, the command returns an error.

-p
    create any parent directories which are missing, and do not complain if
    the directory exists already

path
    absolute or relative path to the directory to create

Example
-------

::

    => mount hostfs /host
    => mkdir /host/newdir
    => ls /host
    DIR       4096 newdir
       ...

Create several levels at once::

    => mkdir /host/a/b/c
    Cannot create '/host/a/b/c': -2: No such file or directory
    => mkdir -p /host/a/b/c /host/a/d
    => tree /host/a
    /host/a
        b/
            c/
        d/

Configuration
-------------

The mkdir command is available when CONFIG_CMD_VFS=y.

Return value
------------

The return value $? is set to 0 (true) if every directory was created,
1 (false) otherwise.

See also
--------

* :doc:`fatmkdir<fatmkdir>` for the FAT-only form of this command
* :doc:`ext4write<ext4write>` for writing a file into a directory this creates
* :doc:`rm<rm>` for deleting a file, or a directory and all it contains
* :doc:`rmdir<rmdir>` for removing an empty directory
* :doc:`mount<mount>` for making a filesystem available to the VFS
