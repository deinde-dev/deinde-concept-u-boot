.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: erofsls (command)

erofsls command
===============

Synopsis
--------

::

    erofsls <interface> [<dev[:part]>] [directory]

Description
-----------

The erofsls command lists the contents of a directory on an EROFS
filesystem. EROFS is a read-only compressed filesystem, so a listing is all
U-Boot can do with a directory: there is no command to write to one.

Entries are shown in the order the filesystem holds them, which mkfs.erofs
sorts by name. A directory is shown with a trailing slash and no size; a file
is shown with its size in bytes, in decimal, which for a compressed file is
the size once expanded; a symbolic link is shown as <SYM> and counted as a
file. EROFS keeps . and .. entries, so a directory with nothing else in it
still lists those two.

A symbolic link naming a directory can be listed, in which case the contents
of its target are shown. The driver resolves the target from the root of the
filesystem, so a relative target only works for a link in the root
directory.

A path which is not there, or which names something other than a directory,
is reported through the return value alone: the EROFS driver prints nothing.
The same goes for an image which is not EROFS at all.

interface
    interface for accessing the block device (mmc, sata, scsi, usb, ....)

dev
    device number

part
    partition number, defaults to 0 (whole device)

directory
    path to the directory to list, defaults to /

Example
-------

This uses an EROFS image bound to the sandbox host interface, holding files
f4096 and f7812 of those sizes, a directory subdir with one file in it, and
symbolic links symdir pointing at subdir and symfile pointing at f7812::

    => erofsls host 0
                ./
                ../
         4096   f4096
         7812   f7812
                subdir/
        <SYM>   symdir
        <SYM>   symfile

    4 file(s), 3 dir(s)

The root is the default, so ``erofsls host 0 /`` shows the same listing. A
sub-directory has . and .. entries of its own::

    => erofsls host 0 subdir
                ./
                ../
          100   subdir-file

    1 file(s), 2 dir(s)

Listing a symbolic link shows what it points at::

    => erofsls host 0 symdir
                ./
                ../
          100   subdir-file

    1 file(s), 2 dir(s)

A directory which is not there, or a path which names a file, produces no
output at all; only the return value says what happened::

    => erofsls host 0 nodir
    => echo $?
    1
    => erofsls host 0 f4096
    => echo $?
    1

A device which is not there is reported before the filesystem is touched::

    => erofsls host 1
    ** Bad device specification host 1 **
    Couldn't find partition host 1

Configuration
-------------

The erofsls command is only available if CONFIG_CMD_EROFS=y, which also
enables the EROFS driver through CONFIG_FS_EROFS.

Return value
------------

The return value $? is set to 0 (true) if the directory was listed,
1 (false) otherwise. A path which does not name a directory counts as a
failure, as does an image which the EROFS driver cannot read.

A command line which cannot be acted on at all, such as one with no
interface or with more arguments than the command takes, shows the usage
message instead.

See also
--------

* *erofsload* for reading one of the listed files into memory
* :doc:`sqfsls<sqfsls>` for the same operation on a SquashFS filesystem,
  which holds no . and .. entries
* :doc:`ext4ls<ext4ls>` for the same operation on an ext4 filesystem
* :doc:`fatls<fatls>` for the same operation on a FAT filesystem
* :doc:`cramfsls<cramfsls>` for listing another read-only compressed
  filesystem, held in memory rather than on a block device
* :doc:`ls<ls>` for listing a directory on any supported filesystem
