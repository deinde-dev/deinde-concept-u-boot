.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: ext4write (command)

ext4write command
=================

Synopsis
--------

::

    ext4write <interface> <dev[:part]> <addr> <filename> <bytes> [<offset>]

Description
-----------

The ext4write command writes data from memory to a file on an ext2, ext3 or
ext4 filesystem, creating the file if it does not already exist. One driver
serves the whole family, so the name says which command was enabled rather
than which filesystem is on the device. Note that you can use the
:doc:`save command <save>` instead.

The filename is relative to the root of the filesystem, so a leading / makes
no difference. The parent directory must exist; ext4write does not create it.
Use :doc:`mkdir<mkdir>` for that.

interface
    interface for accessing the block device (mmc, sata, scsi, usb, ....)

dev
    device number

part
    partition number, defaults to 0 (whole device)

addr
    memory address to read the data from

filename
    path to the file to write

bytes
    number of bytes to write

offset
    byte position in the file to start writing at, defaulting to 0

addr, bytes and offset are hexadecimal numbers.

Writing at an offset updates the file in place: the bytes outside the range
written keep their old values and the file grows only if the data extends
beyond the end. Writing past the end leaves a gap, which reads as zeroes.

Example
-------

This uses an ext4 image bound to the sandbox host interface, holding a
directory docs with a file old.txt in it::

    => mw.b 1000000 5a 40
    => ext4write host 0 1000000 data.bin 40
    64 bytes written in 3 ms (20.5 KiB/s)
    => ext4ls host 0
                ./
                ../
                lost+found/
           64   data.bin
                docs/
    => ext4size host 0 data.bin
    => echo $filesize
    40

Giving an offset writes into the file without disturbing the rest of it::

    => mw.b 2000000 ff 10
    => ext4write host 0 2000000 data.bin 10 30
    16 bytes written in 1 ms (15.6 KiB/s)
    => ext4load host 0 3000000 data.bin
    64 bytes read in 1 ms (62.5 KiB/s)
    => md.b 3000000 40
    03000000: 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a  ZZZZZZZZZZZZZZZZ
    03000010: 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a  ZZZZZZZZZZZZZZZZ
    03000020: 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a  ZZZZZZZZZZZZZZZZ
    03000030: ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff  ................

Writing past the end extends the file, with the gap reading as zeroes::

    => mw.b 2000000 ee 8
    => ext4write host 0 2000000 data.bin 8 60
    8 bytes written in 2 ms (3.9 KiB/s)
    => ext4size host 0 data.bin
    => echo $filesize
    68
    => ext4load host 0 4000000 data.bin
    104 bytes read in 1 ms (101.6 KiB/s)
    => md.b 4000000 68
    04000000: 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a  ZZZZZZZZZZZZZZZZ
    04000010: 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a  ZZZZZZZZZZZZZZZZ
    04000020: 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a 5a  ZZZZZZZZZZZZZZZZ
    04000030: ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff  ................
    04000040: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
    04000050: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................
    04000060: ee ee ee ee ee ee ee ee                          ........

A directory which is already there can be written into::

    => mw.b 1000000 41 20
    => ext4write host 0 1000000 docs/new.txt 20
    32 bytes written in 2 ms (15.6 KiB/s)
    => ext4ls host 0 docs
            9   old.txt
                ./
           32   new.txt
                ../

Writing into a directory which does not exist fails, as does writing to a
path which names a directory::

    => ext4write host 0 1000000 nodir/data.bin 20
                fs_write() ** Unable to write file nodir/data.bin **
    => ext4write host 0 1000000 docs 20
                fs_write() ** Unable to write file docs **

Configuration
-------------

The ext4write command is only available if CONFIG_CMD_EXT4_WRITE=y.

Return value
------------

The return value $? is set to 0 (true) if the data was written,
1 (false) otherwise.

See also
--------

* :doc:`ext4load<ext4load>` for reading the data back into memory
* :doc:`ext4size<ext4size>` for finding the resulting file's size
* *ext4ls* for listing what is in a directory already
* :doc:`mkdir<mkdir>` for creating the directory to write into
* :doc:`save<save>` for the same operation on any supported filesystem
* :doc:`fatwrite<fatwrite>` for the same operation on a FAT filesystem
