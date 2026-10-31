.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: unzip (command)

unzip command
=============

Synopsis
--------

::

    unzip srcaddr dstaddr [dstsize]

Description
-----------

The unzip command decompresses a gzip stream held in memory, writing the
result somewhere else in memory. It reads what :doc:`zip<zip>` writes, and
what the *gzip* tool on the host produces, so a compressed file can be read
into memory with :doc:`load<load>` and expanded there.

U-Boot decompresses images by itself when booting them, so this command is
mostly useful for data which is not part of a boot image, or for checking that
a stream is intact before it is used. *gzwrite* is the one to use where the
result is larger than memory, since it writes to a block device as it goes.

srcaddr
    address of the gzip stream, hexadecimal

dstaddr
    address to write the uncompressed data to, hexadecimal

dstsize
    room available at *dstaddr*, in bytes, hexadecimal. Decompression stops
    with an error when this is used up, leaving a part of the data behind.
    Without it the limit is 4 GiB less one byte, which in practice means no
    limit at all, so a corrupt or hostile stream can write over whatever
    follows the destination

The stream is read as far as its own length says, so the source length is not
a parameter. The header is checked for the deflate method and for reserved
flag bits, but not for the gzip magic number, so data which happens to carry
an 8 in its third byte is handed to the decompressor rather than being
refused up front.

On success the uncompressed size is printed and left in the *filesize*
environment variable, so the next command can pick it up without it being
worked out by hand.

Example
-------

This compresses a file read into memory and expands it again, then checks
that the round trip gives back what it started with::

    => mount hostfs - /host
    => load 1000000 /host/README
    74839 bytes read
    => zip 1000000 $filesize 2000000
    Compressed size: 31284 = 0x7A34
    => unzip 2000000 3000000 20000
    Uncompressed size: 74839 = 0x12457
    => printenv filesize
    filesize=12457
    => crc32 1000000 12457
    crc32 for 01000000 ... 01012456 ==> f107c103
    => crc32 3000000 12457
    crc32 for 03000000 ... 03012456 ==> f107c103

A destination too small for the result stops the decompression part-way, -5
being zlib's Z_BUF_ERROR::

    => unzip 2000000 3000000 10000
    Error: inflate() returned -5

Something which is not a gzip stream is refused by the header check::

    => unzip 1000000 3000000 20000
    Error: Bad gzipped data

Configuration
-------------

The command is only available if CONFIG_CMD_UNZIP=y. It is enabled by default
where CONFIG_CMD_BOOTI=y, and it selects CONFIG_GZIP=y for the decompression
code itself. The *gzwrite* command comes with it.

Return value
------------

The return value $? is 0 (true) if the stream is decompressed. It is 1 (false)
if the decompression fails, including when the destination is too small, or if
the command is given fewer than two or more than three arguments.

See also
--------

* :doc:`zip<zip>` for producing a gzip stream in memory
* *gzwrite* for decompressing one onto a block device
* :doc:`lzmadec<lzmadec>` for the same job on an LZMA stream
* :doc:`unlz4<unlz4>` for the same job on an lz4 frame
* :doc:`load<load>` for reading a compressed file into memory first
* :doc:`md<md>` for looking at the result
* :doc:`bootm<bootm>` for booting an image, which decompresses it as a step
