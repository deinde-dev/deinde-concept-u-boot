.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: fatls (command)

fatls command
=============

Synopsis
--------

::

    fatls <interface> [<dev[:part]>] [directory]

Description
-----------

The fatls command lists the contents of a directory on a FAT filesystem. The
filesystem-generic :doc:`ls command <ls>` reaches the same code, so it reads
FAT as well as every other supported filesystem.

Entries are shown in the order the directory holds them, which is not
alphabetical. A directory is shown with a trailing slash and no size; a file
is shown with its size in bytes, in decimal. A count of each kind follows the
listing. A FAT root directory holds no . and .. entries, so only a
sub-directory lists those.

interface
    interface for accessing the block device (mmc, sata, scsi, usb, ....)

dev
    device number; the help text shows this as optional, but leaving it out
    reports that no device is specified, since a FAT filesystem always needs
    a block device

part
    partition number, defaults to 0 (whole device)

directory
    path to the directory to list, defaults to /

Example
-------

This uses a FAT image bound to the sandbox host interface, holding a file
hello.txt of 14 bytes, a directory docs with one file in it, and an empty
directory empty::

    => fatls host 0
                docs/
                empty/
           14   hello.txt

    1 file(s), 2 dir(s)

    => fatls host 0 docs
                ./
                ../
            5   note.txt

    1 file(s), 2 dir(s)

A directory with nothing in it still lists . and ..::

    => fatls host 0 empty
                ./
                ../

    0 file(s), 2 dir(s)

A directory which does not exist prints nothing at all, so the return value is
the only way to tell it apart from an empty one::

    => fatls host 0 nodir
    => echo $?
    1

The same happens when the path names a file rather than a directory::

    => fatls host 0 hello.txt
    => echo $?
    1

A device which is not there is reported before the filesystem is touched::

    => fatls host 1
    ** Bad device specification host 1 **
    Couldn't find partition host 1

Configuration
-------------

The fatls command is only available if CONFIG_CMD_FAT=y.

Return value
------------

The return value $? is set to 0 (true) if the directory was listed,
1 (false) otherwise.

A command line which cannot be acted on at all, such as one with no
interface or with more arguments than the command takes, shows the usage
message instead.

See also
--------

* :doc:`fatload<fatload>` for reading one of the listed files into memory
* :doc:`fatsize<fatsize>` for asking about one file rather than listing them
  all
* :doc:`fatinfo<fatinfo>` for showing which filesystem is on the device
* :doc:`fatwrite<fatwrite>` for creating a file which the listing then shows
* :doc:`fatrm<fatrm>` for deleting one of the listed files
* :doc:`fatmkdir<fatmkdir>` for creating a directory to list
* :doc:`ext2ls<ext2ls>` for the same operation on an ext filesystem
* :doc:`ls<ls>` for listing a directory on any supported filesystem
