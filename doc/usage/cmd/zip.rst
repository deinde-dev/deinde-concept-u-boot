.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: zip (command)

zip command
===========

Synopsis
--------

::

    zip srcaddr srcsize dstaddr [dstsize]

Description
-----------

The zip command compresses a region of memory with zlib's deflate method,
writing a gzip stream somewhere else in memory. The :doc:`unzip<unzip>`
command expands such a stream again, and :doc:`gzwrite<gzwrite>` writes one
to a block device as it decompresses it.

Compression uses the Z_BEST_SPEED level, the quickest setting, since the point
is to move data about on a board rather than to produce the smallest file.
Data which does not compress - an image which is compressed already, or random
data - comes out a little larger than it went in, so the destination must be
clear of the source rather than merely past its end.

srcaddr
    address of the data to compress, hexadecimal

srcsize
    number of bytes to compress, hexadecimal

dstaddr
    address to write the gzip stream to, hexadecimal

dstsize
    room available at *dstaddr*, in bytes, hexadecimal. Compression stops with
    an error when this is used up, leaving a part of a stream behind. Without
    it there is no limit and whatever the stream needs is written, however far
    past *dstaddr* that reaches

On success the compressed size is printed and left in the *filesize*
environment variable, so the command which consumes the stream can be given it
without it being typed out.

An srcsize of zero produces no output at all, and reports a compressed size of
zero.

Example
-------

This reads a file into memory and compresses it::

    => mount hostfs - /host
    => load 1000000 /host/README
    74839 bytes read
    => zip 1000000 $filesize 2000000
    Compressed size: 31284 = 0x7A34
    => printenv filesize
    filesize=7a34

A destination too small for the stream fails part-way through, and says how
much of the source is left::

    => zip 1000000 8000 2000000 100
    Deflate need more space to compress left 512 bytes

Configuration
-------------

The command is only available if CONFIG_CMD_ZIP=y, which is not the default.
It selects CONFIG_GZIP_COMPRESSED=y for the compression code itself.

The stream carries a gzip header where CONFIG_GZIP=y, which is the default and
is what the decompression side of U-Boot needs. A board which turns that off
keeps zip but writes a bare zlib stream instead, which
:doc:`unzip<unzip>` cannot read.

Return value
------------

The return value $? is 0 (true) if the region is compressed. It is 1 (false)
if the compression fails, including when the destination is too small, or if
the command is given fewer than three or more than four arguments.

See also
--------

* :doc:`unzip<unzip>` for expanding a gzip stream held in memory
* :doc:`gzwrite<gzwrite>` for writing one to a block device
* :doc:`lzmadec<lzmadec>` and :doc:`unlz4<unlz4>` for decompressing other
  formats, neither of which U-Boot can compress
* :doc:`load<load>` for reading a file into memory first
* :doc:`crc32<crc32>` for checking that a round trip through the two commands
  gives back what it started with
