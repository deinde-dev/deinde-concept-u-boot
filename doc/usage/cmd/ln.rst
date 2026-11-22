.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: ln (command)

ln command
==========

Synopsis
--------

::

    ln <target> <linkname>
    ln <interface> <dev[:part]> <target> <linkname>

Description
-----------

The ln command creates a symbolic link. The symbolic link *linkname* is
created pointing to *target*. The target string is stored as-is in the
symlink and is not resolved at creation time.

The filesystem must support symbolic links (e.g. ext4). FAT filesystems do
not support symlinks, nor does sandbox's own filesystem.

VFS form
~~~~~~~~

When the VFS is enabled, the first form is used. The filesystem must be
mounted first using ``mount``.

target
    string to store as the symlink target

linkname
    absolute or relative path for the new symbolic link

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

target
    string to store as the symlink target

linkname
    path of the symbolic link to create, within the filesystem

part is a hexadecimal number.

A link which is there already is replaced, which the filesystem reports.

Example
-------

VFS::

    => mount host 0:0 /mnt
    => ln testfile.txt /mnt/link.txt
    => stat /mnt/link.txt
      File: link.txt
      Size: 12
      Type: symbolic link

Legacy, here with an ext4 image attached to sandbox's host device. The
command says nothing when it works::

    => host bind 0 ext4.img
    => ln host 0 hello.txt mylink
    => ls host 0
                ./
                ../
                lost+found/
                docs/
                empty/
           14   hello.txt
        <SYM>   mylink

    2 file(s), 5 dir(s)

The size of the link is the size of the file it leads to::

    => size host 0 mylink
    => echo $filesize
    e

A path whose directory is not there is reported by the filesystem and then
by the filesystem layer::

    => ln host 0 hello.txt nodir/link
    Invalid path
    ** Unable to create link hello.txt -> nodir/link **

Configuration
-------------

The VFS form is available when CONFIG_CMD_VFS=y, which depends on
CONFIG_VFS=y. The legacy form is available when CONFIG_CMD_FS_LEGACY=y.

Return value
------------

The return value $? is set to 0 (true) if the symlink was created,
1 (false) otherwise.

See also
--------

* :doc:`ls<ls>` for seeing the link in a directory
* :doc:`load<load>` for reading the file a link leads to
* :doc:`rm<rm>` for deleting a link, on a filesystem which can
* :doc:`mount<mount>` for making a filesystem available to the VFS
