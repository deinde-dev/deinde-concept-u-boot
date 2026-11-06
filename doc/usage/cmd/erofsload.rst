.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: erofsload (command)

erofsload command
=================

Synopsis
--------

::

    erofsload <interface> [<dev[:part]> [<addr> [<filename> [bytes [pos]]]]]

Description
-----------

The erofsload command reads a file from an EROFS filesystem into memory.
EROFS is a read-only compressed filesystem, so U-Boot expands the data as it
reads it; the command reports the size of the file as it is once expanded,
not the space it takes up on the device.

The number of bytes read is saved in the environment variable filesize and
the address they were read to in fileaddr, both in hexadecimal. Neither is
changed if the file cannot be read.

A path may name a symbolic link, in which case the link is followed. The
driver resolves the target from the root of the filesystem, so a relative
target only works for a link in the root directory. A symbolic link part-way
along a path is not followed: only the last component can be one.

Unlike :doc:`sqfsload<sqfsload>`, the EROFS driver can start reading at a
position other than the beginning of the file, so pos works as the help text
describes.

interface
    interface for accessing the block device (mmc, sata, scsi, usb, ....)

dev
    device number

part
    partition number, defaults to 0 (whole device)

addr
    load address, defaults to environment variable loadaddr or, if that is
    not set, to configuration variable CONFIG_SYS_LOAD_ADDR

filename
    path to file, defaults to environment variable bootfile

bytes
    maximum number of bytes to load, 0 or omitted meaning the whole file

pos
    byte offset in the file to start reading from, defaulting to 0; giving
    it requires bytes as well

part, addr, bytes and pos are hexadecimal numbers.

Example
-------

This uses an EROFS image bound to the sandbox host interface, holding files
f4096 and f7812 of those sizes, a directory subdir with a file of 100 bytes
in it, and a symbolic link symfile pointing at f7812::

    => erofsload host 0 1000000 f4096
    4096 bytes read in 1 ms (3.9 MiB/s)
    => echo $filesize $fileaddr
    1000 1000000

Giving bytes reads only the start of the file::

    => erofsload host 0 1000000 f4096 800
    2048 bytes read in 1 ms (2 MiB/s)

Giving pos as well starts the read part-way into the file::

    => erofsload host 0 1000000 f7812 800 1000
    2048 bytes read in 1 ms (2 MiB/s)

A path is taken as relative to the root of the filesystem::

    => erofsload host 0 1000000 subdir/subdir-file
    100 bytes read in 1 ms (97.7 KiB/s)

A symbolic link naming a file is followed::

    => erofsload host 0 1000000 symfile
    7812 bytes read in 1 ms (7.5 MiB/s)

With no filename, bootfile is used, and with no address, loadaddr::

    => setenv bootfile f4096
    => setenv loadaddr 2000000
    => erofsload host 0
    4096 bytes read in 1 ms (3.9 MiB/s)
    => echo $fileaddr
    2000000

A file which is not there is reported by name, with nothing from the EROFS
driver itself::

    => erofsload host 0 1000000 missing
                 do_load() Failed to load 'missing'

An image which is not EROFS fails before any file is looked for::

    => erofsload host 0 1000000 f4096
                 do_load() Can't set block device

Leaving out the filename without a bootfile to fall back on fails before the
filesystem is read::

    => setenv bootfile
    => erofsload host 0 1000000
    ** No boot file defined **

Configuration
-------------

The erofsload command is only available if CONFIG_CMD_EROFS=y, which also
enables the EROFS driver through CONFIG_FS_EROFS.

Return value
------------

The return value $? is set to 0 (true) if the file was read, 1 (false)
otherwise. Reading fewer bytes than asked for is not an error: the transfer
stops at the end of the file.

See also
--------

* :doc:`erofsls<erofsls>` for finding out which files are there to read
* :doc:`sqfsload<sqfsload>` for the same operation on a SquashFS filesystem,
  which cannot read from a position other than the start
* :doc:`ext4load<ext4load>` for the same operation on an ext4 filesystem
* :doc:`fatload<fatload>` for the same operation on a FAT filesystem
* :doc:`cramfsload<cramfsload>` for reading from another read-only
  compressed filesystem, held in memory rather than on a block device
* :doc:`load<load>` for reading a file from any supported filesystem
