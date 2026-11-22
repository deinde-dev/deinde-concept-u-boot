.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: mv (command)

mv command
==========

Synopsis
--------

::

    mv <old_path> <new_path>
    mv <interface> <dev[:part]> <old_path> <new_path>

Description
-----------

The mv command renames or moves a file or directory within a filesystem.
Both paths must be on the same filesystem - cross-filesystem moves are not
supported.

Where *new_path* is an existing directory, the file keeps its name and is
moved into that directory. Where it is an existing file, that file is
replaced without asking.

Not all filesystems support renaming. If the filesystem does not implement
the rename operation, the command returns an error. A FAT filesystem needs
CONFIG_FAT_RENAME for it.

VFS form
~~~~~~~~

When the VFS is enabled, the first form is used. The filesystem must be
mounted first using ``mount``.

old_path
    absolute or relative path to the existing file or directory

new_path
    absolute or relative path for the new name/location

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

old_path
    path to the existing file or directory, within the filesystem

new_path
    path for the new name or location, within the filesystem

part is a hexadecimal number.

Example
-------

VFS::

    => mount hostfs /host
    => mv /host/old-name.txt /host/new-name.txt
    => mv /host/file.txt /host/subdir/file.txt

Legacy, here with a FAT image attached to sandbox's host device. The command
says nothing when it works::

    => host bind 0 fat.img
    => mv host 0 hello.txt greeting.txt
    => ls host 0
                docs/
                empty/
           14   greeting.txt

    1 file(s), 2 dir(s)

    => mv host 0 greeting.txt docs
    => ls host 0 docs
                ./
                ../
            5   note.txt
           14   greeting.txt

    2 file(s), 2 dir(s)

Nothing is printed when the file cannot be moved either, so only the return
value says what happened::

    => mv host 0 nothere there
    => echo $?
    1

Configuration
-------------

The VFS form is available when CONFIG_CMD_VFS=y, which depends on
CONFIG_VFS=y. The legacy form is available when CONFIG_CMD_FS_LEGACY=y.

Return value
------------

The return value $? is set to 0 (true) if the rename succeeded,
1 (false) otherwise.

See also
--------

* :doc:`mkdir<mkdir>` for creating the directory to move a file into
* :doc:`rm<rm>` for deleting a file rather than moving it
* :doc:`ls<ls>` for seeing where the file has gone
* :doc:`mount<mount>` for making a filesystem available to the VFS
