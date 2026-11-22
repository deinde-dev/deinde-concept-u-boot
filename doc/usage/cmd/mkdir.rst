.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: mkdir (command)

mkdir command
=============

Synopsis
--------

::

    mkdir [-p] <path>...
    mkdir <interface> <dev[:part]> <directory>

Description
-----------

The mkdir command creates a directory.

Not all filesystems support directory creation. If the filesystem does
not implement the mkdir operation, the command returns an error.

VFS form
~~~~~~~~

When the VFS is enabled, the first form is used. It creates a directory at
each path given. The parent directory must already exist, unless -p is given.
The filesystem must be mounted first using ``mount``.

If a directory cannot be created, the command reports it and carries on with
the others.

-p
    create any parent directories which are missing, and do not complain if
    the directory exists already

path
    absolute or relative path to the directory to create

Legacy form
~~~~~~~~~~~

The second form is used when the VFS is not enabled and accesses a block
device directly without mounting. It creates one directory and has no
options, so only the last part of the path is created and the rest of it
must exist already.

interface
    interface for accessing the block device (mmc, sata, scsi, usb, ....)

dev
    device number

part
    partition number. With no partition given, partition 1 is used, or the
    whole device if it has no partition table. A partition of 0 means the
    whole device and *auto* means the first bootable partition.

directory
    path of the directory to create, within the filesystem

part is a hexadecimal number.

Example
-------

VFS::

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

Legacy, here with a FAT image attached to sandbox's host device. The command
says nothing when it works::

    => host bind 0 fat.img
    => mkdir host 0 newdir
    => ls host 0 newdir
                ./
                ../

    0 file(s), 2 dir(s)

A directory which is there already, or a path whose parent is not, is
reported by the filesystem and then by the command::

    => mkdir host 0 docs
    docs: already exists
    ** Unable to create a directory "docs" **
    => mkdir host 0 a/b/c
    a/b: doesn't exist (-2)
    ** Unable to create a directory "a/b/c" **

Configuration
-------------

The VFS form is available when CONFIG_CMD_VFS=y, which depends on
CONFIG_VFS=y. The legacy form is available when CONFIG_CMD_FS_LEGACY=y.

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
* :doc:`mv<mv>` for renaming a directory once it is created
* :doc:`ls<ls>` for seeing what a directory holds
* :doc:`mount<mount>` for making a filesystem available to the VFS
