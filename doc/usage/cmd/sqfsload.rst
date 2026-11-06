.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: sqfsload (command)

sqfsload command
================

Synopsis
--------

::

    sqfsload <interface> [<dev[:part]> [addr [filename [bytes [pos]]]]]

Description
-----------

The sqfsload command reads a file from a SquashFS filesystem into memory.
SquashFS is a read-only compressed filesystem, so U-Boot decompresses the
data as it reads it; the command reports the size of the file as it is once
expanded, not the size it takes up on the device.

The number of bytes read is saved in the environment variable filesize and
the address they were read to in fileaddr, both in hexadecimal. Neither is
changed if the file cannot be read.

A path may name a symbolic link, or lead through one, in which case the link
is followed. Anything which is not a regular file, such as a directory, is
refused.

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
    byte offset in the file to start reading from; the SquashFS driver does
    not support this, so anything other than 0 fails

part, addr, bytes and pos are hexadecimal numbers.

Example
-------

This uses a SquashFS image bound to the sandbox host interface, holding a
file hello.txt which contains ``Hello, world!`` and a newline::

    => sqfsload host 0 1000000 hello.txt
    14 bytes read in 1 ms (13.7 KiB/s)
    => echo $filesize $fileaddr
    e 1000000
    => md.b 1000000 e
    01000000: 48 65 6c 6c 6f 2c 20 77 6f 72 6c 64 21 0a        Hello, world!.

Giving bytes reads the start of the file, and asking for more bytes than the
file holds stops at the end of it::

    => sqfsload host 0 1000000 hello.txt 5
    5 bytes read in 1 ms (4.9 KiB/s)
    => md.b 1000000 5
    01000000: 48 65 6c 6c 6f                                   Hello
    => sqfsload host 0 1000000 hello.txt 100
    14 bytes read in 1 ms (13.7 KiB/s)

Reading from a position other than the start is not supported::

    => sqfsload host 0 1000000 hello.txt 5 7
    Error: reading at a specific offset in a squashfs file is not supported yet.
                 do_load() Failed to load 'hello.txt'

A path is taken as relative to the root of the filesystem and may lead
through a symbolic link, here sym pointing at the docs directory::

    => sqfsload host 0 1000000 docs/note.txt
    5 bytes read in 1 ms (4.9 KiB/s)
    => sqfsload host 0 1000000 sym/note.txt
    5 bytes read in 1 ms (4.9 KiB/s)

With no filename, bootfile is used, and with no address, loadaddr::

    => setenv bootfile hello.txt
    => setenv loadaddr 2000000
    => sqfsload host 0
    14 bytes read in 1 ms (13.7 KiB/s)
    => echo $fileaddr
    2000000

A file which is not there is reported by the SquashFS driver, as is a path
which names a directory::

    => sqfsload host 0 1000000 missing.txt
    File not found.
                 do_load() Failed to load 'missing.txt'
    => sqfsload host 0 1000000 docs
    Unable to recover entry's size.
                 do_load() Failed to load 'docs'

Leaving out the filename without a bootfile to fall back on fails before the
filesystem is read::

    => setenv bootfile
    => sqfsload host 0 1000000
    ** No boot file defined **

Configuration
-------------

The sqfsload command is only available if CONFIG_CMD_SQUASHFS=y, which also
enables the SquashFS driver through CONFIG_FS_SQUASHFS.

Return value
------------

The return value $? is set to 0 (true) if the file was read, 1 (false)
otherwise. Reading fewer bytes than asked for is not an error: the transfer
stops at the end of the file.

See also
--------

* :doc:`sqfsls<sqfsls>` for finding out which files are there to read
* :doc:`ext4load<ext4load>` for the same operation on an ext4 filesystem
* :doc:`fatload<fatload>` for the same operation on a FAT filesystem
* :doc:`cramfsload<cramfsload>` for reading from another read-only
  compressed filesystem, held in memory rather than on a block device
* :doc:`load<load>` for reading a file from any supported filesystem
