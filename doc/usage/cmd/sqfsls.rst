.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: sqfsls (command)

sqfsls command
==============

Synopsis
--------

::

    sqfsls <interface> [<dev[:part]>] [directory]

Description
-----------

The sqfsls command lists the contents of a directory on a SquashFS
filesystem. SquashFS is a read-only compressed filesystem, so a listing is
all U-Boot can do with a directory: there is no command to write to one.

Entries are shown in the order the filesystem holds them, which mksquashfs
sorts by name. A directory is shown with a trailing slash and no size; a file
is shown with its size in bytes, in decimal; a symbolic link is shown as
<SYM> and counted as a file. SquashFS stores no . and .. entries, so a
listing holds only what was put in the directory.

A symbolic link naming a directory can be listed, in which case the contents
of its target are shown.

An empty directory is reported as such by the SquashFS driver, which stops
before the listing code is reached. No counts follow, and the command reports
a failure.

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

This uses a SquashFS image bound to the sandbox host interface, holding a
file hello.txt of 14 bytes, a directory docs with one file in it, an empty
directory empty and a symbolic link sym pointing at docs::

    => sqfsls host 0
                docs/
                empty/
           14   hello.txt
        <SYM>   sym

    2 file(s), 2 dir(s)

The root is the default, so ``sqfsls host 0 /`` shows the same listing. A
sub-directory holds no . and .. entries of its own::

    => sqfsls host 0 docs
            5   note.txt

    1 file(s), 0 dir(s)

Listing the symbolic link shows what it points at::

    => sqfsls host 0 sym
            5   note.txt

    1 file(s), 0 dir(s)

An empty directory says so, and the command reports a failure::

    => sqfsls host 0 empty
    Empty directory.
    => echo $?
    1

A directory which is not there, or a path which names a file, is reported the
same way::

    => sqfsls host 0 nodir
    ** Cannot find directory. **
    => sqfsls host 0 hello.txt
    ** Cannot find directory. **

A device which is not there is reported before the filesystem is touched::

    => sqfsls host 1
    ** Bad device specification host 1 **
    Couldn't find partition host 1

Configuration
-------------

The sqfsls command is only available if CONFIG_CMD_SQUASHFS=y, which also
enables the SquashFS driver through CONFIG_FS_SQUASHFS.

Return value
------------

The return value $? is set to 0 (true) if the directory was listed,
1 (false) otherwise. An empty directory counts as a failure, as does a path
which does not name a directory.

A command line which cannot be acted on at all, such as one with no
interface or with more arguments than the command takes, shows the usage
message instead.

See also
--------

* *sqfsload* for reading one of the listed files into memory
* :doc:`ext4ls<ext4ls>` for the same operation on an ext4 filesystem
* :doc:`fatls<fatls>` for the same operation on a FAT filesystem
* :doc:`cramfsls<cramfsls>` for listing another read-only compressed
  filesystem, held in memory rather than on a block device
* :doc:`ls<ls>` for listing a directory on any supported filesystem
