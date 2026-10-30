.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: crc32 (command)

crc32 command
=============

Synopsis
--------

::

    crc32 address count [addr]
    crc32 -v address count crc

Description
-----------

The crc32 command computes the CRC-32 checksum of ``count`` bytes of memory
starting at ``address``, and prints it as a line of the form::

    crc32 for <first> ... <last> ==> <checksum>

where ``first`` and ``last`` are the first and last addresses covered, printed
as eight hex digits. A count of zero gives a checksum of 0, and ``last`` is
then one below ``first``.

The code is lib/crc32.c, derived from zlib, so this is the usual CRC-32 of
Ethernet and the zip format, and the same one U-Boot checks its own images
with.

With ``addr`` the checksum is also stored, as four bytes at that address, most
significant first. Unlike :doc:`hash<hash>` the argument is always an address:
crc32 does not accept an environment variable here, and a name is read as a
hex number, which is almost always 0. Use ``hash crc32`` where a variable is
wanted.

The ``-v`` option verifies rather than stores: the checksum just computed is
compared with the one named by the last argument, and nothing is printed if
the two match. Here a variable *is* accepted: the argument is an address if it
starts with ``*``, an immediate eight-digit hex checksum if it is exactly that
long, and otherwise the name of an environment variable holding one. On a
mismatch the command shows both values::

    crc32 for <first> ... <last> ==> <computed> != <expected> ** ERROR **

address
    Address of the first byte to checksum. Hexadecimal, '0x' prefix optional.

count
    Number of bytes to checksum. Hexadecimal, '0x' prefix optional.

addr
    Address to store the checksum at, as four bytes.

crc
    What to compare the checksum against: an immediate hex value, the name of
    an environment variable holding one, or an address written as
    ``*address``.

Example
-------

Checksum four bytes of memory::

    => mw.b 7000 61 4
    => crc32 7000 4
    crc32 for 00007000 ... 00007003 ==> ad98e545

Store the checksum as well, and look at the four bytes it wrote::

    => crc32 7000 4 8000
    crc32 for 00007000 ... 00007003 ==> ad98e545
    => md.b 8000 4
    00008000: ad 98 e5 45                                      ...E

Verify against that address, against a variable and against a value given on
the command line::

    => crc32 -v 7000 4 *8000
    => setenv sum ad98e545
    => crc32 -v 7000 4 sum
    => crc32 -v 7000 4 ad98e545
    => echo $?
    0

A checksum which does not match is reported::

    => crc32 -v 7000 4 deadbeef
    crc32 for 00007000 ... 00007003 ==> ad98e545 != deadbeef ** ERROR **

Configuration
-------------

The command is only available if CONFIG_CMD_CRC32=y, which is the default.
The ``-v`` option needs CONFIG_CRC32_VERIFY=y as well; without it the option
is refused and the usage message is shown.

A board which wants crc32 and no other digest leaves the algorithm lookup
table out, in which case a smaller version of the code runs and the output
line starts with 'CRC32 for' rather than 'crc32 for'. The ``-v`` option is
not available there, since CONFIG_CRC32_VERIFY brings the full version in.

Return value
------------

The return value $? is 0 (true) if the checksum is computed, and if ``-v`` is
given, if it matches. It is 1 (false) if fewer than two arguments are given,
if an option other than ``-v`` is used, or if the checksums differ.

See also
--------

* :doc:`hash<hash>` for other algorithms, and for saving a crc32 to an
  environment variable
* :doc:`sm3sum<sm3sum>` for SM3, which is a digest rather than a checksum
* *md5sum* and *sha1sum* for MD5 and SHA1
* :doc:`md<md>` for looking at the memory being checksummed
