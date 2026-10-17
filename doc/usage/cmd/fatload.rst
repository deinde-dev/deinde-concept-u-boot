.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: fatload (command)

fatload command
===============

Synopsis
--------

::

    fatload <interface> [<dev[:part]> [<addr> [<filename> [bytes [pos]]]]]

Description
-----------

The fatload command is used to read a file from a FAT filesystem into memory.
You can always use the :doc:`load command <load>` instead.

The number of transferred bytes is saved in the environment variable filesize.
The load address is saved in the environment variable fileaddr. Neither is
touched when the load fails, so both still describe whatever was loaded last.

interface
    interface for accessing the block device (mmc, sata, scsi, usb, ....)

dev
    device number

part
    partition number, defaults to 0 (whole device)

addr
    load address, defaults to environment variable loadaddr or if loadaddr is
    not set to configuration variable CONFIG_SYS_LOAD_ADDR

filename
    path to file, defaults to environment variable bootfile

bytes
    maximum number of bytes to load; 0, which is also the default, means read
    to the end of the file

pos
    number of bytes to skip, defaulting to 0; a position at or past the end of
    the file reads nothing and is not an error

addr, bytes, pos are hexadecimal numbers. Giving pos requires giving bytes
as well, since the arguments are positional.

If either 'pos' or 'bytes' are not aligned according to the minimum alignment
requirement for DMA transfer (ARCH_DMA_MINALIGN) the data is copied through an
aligned buffer, a sector at a time, so performance will suffer for the load.
Nothing is printed when this happens, since the message which notes it is a
debug one.

Example
-------

::

    => fatload mmc 0:1 ${kernel_addr_r} snp.efi
    149280 bytes read in 11 ms (12.9 MiB/s)
    =>
    => fatload mmc 0:1 ${kernel_addr_r} snp.efi 1000000
    149280 bytes read in 9 ms (15.8 MiB/s)
    =>
    => fatload mmc 0:1 ${kernel_addr_r} snp.efi 1000000 100
    149024 bytes read in 10 ms (14.2 MiB/s)
    =>
    => fatload mmc 0:1 ${kernel_addr_r} snp.efi 10
    16 bytes read in 1 ms (15.6 KiB/s)
    =>

Configuration
-------------

The fatload command is only available if CONFIG_CMD_FAT=y.

Return value
------------

The return value $? is set to 0 (true) if the file was successfully loaded,
even if the number of bytes is less than the specified length.

If an error occurs, the return value $? is set to 1 (false). A missing file,
a path which names a directory, a device or partition which is not there and
an unset bootfile with no filename given are all errors.

A command line which cannot be acted on at all, such as one with no
interface, with an address which is not a number, or with more arguments
than the command takes, shows the usage message instead.

See also
--------

* :doc:`fatsize<fatsize>` for finding a file's size without reading it
* :doc:`fatwrite<fatwrite>` for writing memory back out to a file
* :doc:`fatinfo<fatinfo>` for showing which filesystem is on the device
* :doc:`ext2load<ext2load>` for the same operation on an ext filesystem
* :doc:`ext4load<ext4load>` for that operation under its ext4 name
* :doc:`load<load>` for the same operation on any supported filesystem
