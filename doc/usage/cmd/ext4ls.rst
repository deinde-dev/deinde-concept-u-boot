.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: ext4ls (command)

ext4ls command
==============

Synopsis
--------

::

    ext4ls <interface> <dev[:part]> [directory]

Description
-----------

The ext4ls command lists the contents of a directory on an ext4 filesystem.
U-Boot has a single driver for the ext2, ext3 and ext4 family, so ext4ls,
:doc:`ext2ls<ext2ls>` and the filesystem-generic :doc:`ls command <ls>` all
reach the same code and read any of the three.

Entries are shown in the order the directory holds them, which is not
alphabetical and not stable across filesystems. A directory is shown with a
trailing slash and no size; a file is shown with its size in bytes, in
decimal. The . and .. entries are listed like any other, as is the
lost+found directory which mkfs.ext4 creates.

Whether a count of files and directories follows the listing depends on which
driver is built. The original one (CONFIG_FS_EXT4=y) uses U-Boot's generic
listing code, which prints the counts; the newer one (CONFIG_FS_EXT4L=y),
which sandbox uses, lists the entries only.

interface
    interface for accessing the block device (mmc, sata, scsi, usb, ....)

dev
    device number; this is not optional, since an ext4 filesystem always
    needs a block device

part
    partition number, defaults to 0 (whole device)

directory
    path to the directory to list, defaults to /

Example
-------

This uses an ext4 image bound to the sandbox host interface, holding a file
hello.txt of 14 bytes, a directory docs with one file in it, and an empty
directory empty::

    => ext4ls host 0
                docs/
           14   hello.txt
                ./
                lost+found/
                empty/
                ../
    => ext4ls host 0 /docs
            5   note.txt
                ./
                ../

A directory with nothing in it still lists . and ..::

    => ext4ls host 0 /empty
                ./
                ../

A directory which does not exist prints nothing, so the return value is the
only way to tell it apart from an empty one::

    => ext4ls host 0 /nodir
    => echo $?
    1

The same happens when the path names a file rather than a directory::

    => ext4ls host 0 hello.txt
    => echo $?
    1

A device which is not there is reported before the filesystem is touched::

    => ext4ls host 1
    ** Bad device specification host 1 **
    Couldn't find partition host 1

Configuration
-------------

The ext4ls command is only available if CONFIG_CMD_EXT4=y.

Return value
------------

The return value $? is set to 0 (true) if the directory was listed,
1 (false) otherwise.

A command line which cannot be acted on at all, such as one with no
interface or with more arguments than the command takes, shows the usage
message instead.

See also
--------

* :doc:`ext4load<ext4load>` for reading one of the listed files into memory
* :doc:`ext4size<ext4size>` for asking about one file rather than listing
  them all
* :doc:`ext4write<ext4write>` for creating a file which the listing then
  shows
* :doc:`ext2ls<ext2ls>` for the same command under its ext2 name
* :doc:`fatls<fatls>` for the same operation on a FAT filesystem
* :doc:`ls<ls>` for listing a directory on any supported filesystem
