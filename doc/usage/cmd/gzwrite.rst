.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: gzwrite (command)

gzwrite command
===============

Synopsis
--------

::

    gzwrite <interface> <dev> <addr> length [wbuf [offs [outsize]]]

Description
-----------

The gzwrite command decompresses a gzip stream held in memory and writes the
result straight to a block device, a buffer at a time. It is
:doc:`unzip<unzip>` for an image too large to expand in RAM: only the write
buffer is needed, not room for the whole of the output.

The write is raw, at a byte offset into the device rather than into a
partition, so writing at offset zero overwrites the partition table along
with everything else. This is how a whole-card image is installed, and it is
also how a card is destroyed by a mistyped device number.

The CRC-32 at the end of the stream is checked against what was decompressed,
and both are shown if they differ, so a truncated or corrupt image is
reported rather than being written and forgotten. The data has already
reached the device by then.

interface
    block device interface the image is written to, such as *mmc* or *scsi*

dev
    device number within that interface, hexadecimal

addr
    address of the gzip stream, hexadecimal

length
    length of the gzip stream in bytes, hexadecimal. :doc:`zip<zip>` and
    :doc:`load<load>` both leave this in the *filesize* environment variable

wbuf
    size of the write buffer in bytes, hexadecimal, 1 MiB by default. It must
    be a non-zero multiple of the device's block size, and is best made a
    multiple of the erase block size on a device which erases, since each
    buffer becomes one write

offs
    byte offset into the device to start writing at, hexadecimal, zero by
    default. It must be a multiple of the device's block size

outsize
    expected size of the uncompressed data in bytes, hexadecimal. By default
    the size recorded at the end of the stream is used, and giving this is a
    way of checking it. The recorded size is only 32 bits wide, so for output
    of 4 GiB or more it holds the size modulo 4 GiB and *outsize* has to be
    given for the right amount of space to be reserved

While the write runs, the number of bytes decompressed so far is shown against
the total, on a line of its own which is rewritten as it goes. The reading
comes from before the last write rather than after it, so with a write buffer
smaller than the output the last line shown is short of the total; the summary
line which follows gives the real figure.

Example
-------

This compresses a file into memory and writes it to the first mmc device,
then reads back the 147 blocks it occupies and checks them against the CRC-32
gzwrite reported::

    => mount hostfs - /host
    => load 1000000 /host/README
    74839 bytes read
    => zip 1000000 $filesize 2000000
    Compressed size: 31284 = 0x7A34
    => gzwrite mmc 0 2000000 $filesize

    74839/74839
    	74839 bytes, crc 0xf107c103
    => mmc read 3000000 0 93
    MMC read: dev # 0, block # 0, count 147 ... 147 blocks read: OK
    => crc32 3000000 12457
    crc32 for 03000000 ... 03012456 ==> f107c103

The same image written in 64 KiB buffers, 4 KiB into the device::

    => gzwrite mmc 0 2000000 $filesize 10000 1000

    65536/74839
    	74839 bytes, crc 0xf107c103

A stream corrupted after it was made is written as far as it decompresses,
and the two CRC-32 values are shown::

    => mw.b 2004000 5a 20
    => gzwrite mmc 0 2000000 $filesize

    74820/74839
    	uncompressed 74820 of 74839
    	crcs == 0xf107c103/0x973fa45b

A write buffer or an offset which is not a multiple of the block size is
refused, as is output which does not match *outsize*::

    => gzwrite mmc 0 2000000 $filesize 100
    gzwrite: size 256 not a multiple of 512
    => gzwrite mmc 0 2000000 $filesize 200 100
    gzwrite: start offset 256 not a multiple of 512
    => gzwrite mmc 0 2000000 $filesize 1000 0 9000
    size of 9000 doesn't match trailer low bits 8000

The sandbox mmc device holds 1 MiB, so an image which expands past that is
refused before anything is written::

    => zip 1000000 200000 2000000
    Compressed size: 9180 = 0x23DC
    => gzwrite mmc 0 2000000 $filesize
    gzwrite: uncompressed size 2097152 exceeds device size

A device which does not exist gives no message at all, so the return value is
the only sign that nothing was written.

Configuration
-------------

The command comes with :doc:`unzip<unzip>` and is only available if
CONFIG_CMD_UNZIP=y, which is the default where CONFIG_CMD_BOOTI=y.

It looks the device up through the partition code, so on a board with
CONFIG_PARTITIONS=n the lookup always fails and the command can write
nothing, whatever device is named.

A board can report progress in its own way by providing
gzwrite_progress_init(), gzwrite_progress() and gzwrite_progress_finish(),
which are weak functions in lib/gunzip.c

Return value
------------

The return value $? is 0 (true) if the whole stream is decompressed and
written and its CRC-32 matches. It is 1 (false) if the device cannot be
found, if the write buffer or the offset is misaligned, if the output does
not fit the device or does not match *outsize*, if the decompression fails,
if a write fails, or if the command is given fewer than four arguments.

See also
--------

* :doc:`unzip<unzip>` for decompressing into memory instead
* :doc:`zip<zip>` for producing a gzip stream in memory
* :doc:`load<load>` for reading a compressed image into memory first
* :doc:`mmc<mmc>` for selecting a device and reading back what was written
* :doc:`part<part>` for the partition table the write at offset zero replaces
