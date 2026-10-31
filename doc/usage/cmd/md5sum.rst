.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: md5sum (command)

md5sum command
==============

Synopsis
--------

::

    md5sum address count [[*]sum]
    md5sum -v address count [*]sum

Description
-----------

The md5sum command computes the MD5 message digest of ``count`` bytes of
memory starting at ``address``, and prints it as a line of the form::

    md5 for <first> ... <last> ==> <digest>

where ``first`` and ``last`` are the first and last addresses covered, printed
as eight hex digits. The algorithm is named 'md5' rather than 'md5sum' on that
line, since it is the name of the algorithm and not of the command. A count of
zero gives the digest of the empty string, and ``last`` is then one below
``first``.

MD5 is specified by RFC 1321. It is long broken against a deliberate attack,
so it is useful for spotting a corrupt transfer rather than for telling
whether an image can be trusted. :doc:`hash<hash>` gives access to SHA-256 and
the other algorithms a board is built with.

With ``sum`` the digest is saved as well as printed. The destination is an
environment variable, unless it starts with ``*``, in which case the rest is a
hex address and the sixteen digest bytes are written there.

The ``-v`` option verifies rather than saves: the digest just computed is
compared with the one named by the last argument, and nothing is printed if
the two match. The argument is a hex address if it starts with ``*``, an
immediate hex digest if it is exactly 32 digits long, and otherwise the name
of an environment variable holding one. On a mismatch the command shows both
digests::

    md5 for <first> ... <last> ==> <computed> != <expected> ** ERROR **

address
    Address of the first byte to hash. Hexadecimal, '0x' prefix optional.

count
    Number of bytes to hash. Hexadecimal, '0x' prefix optional.

sum
    Where to save the digest, or what to compare it against with ``-v``: the
    name of an environment variable, or an address if written as
    ``*address``.

Example
-------

Hash four bytes of memory::

    => mw.b 7000 61 4
    => md5sum 7000 4
    md5 for 00007000 ... 00007003 ==> 74b87337454200d4d33f80c4663dc5e5

Save the digest in an environment variable, and in memory::

    => md5sum 7000 4 myvar
    md5 for 00007000 ... 00007003 ==> 74b87337454200d4d33f80c4663dc5e5
    => printenv myvar
    myvar=74b87337454200d4d33f80c4663dc5e5
    => md5sum 7000 4 *8000
    md5 for 00007000 ... 00007003 ==> 74b87337454200d4d33f80c4663dc5e5
    => md.b 8000 10
    00008000: 74 b8 73 37 45 42 00 d4 d3 3f 80 c4 66 3d c5 e5  t.s7EB...?..f=..

Verify against that address and against a digest given on the command line::

    => md5sum -v 7000 4 *8000
    => md5sum -v 7000 4 74b87337454200d4d33f80c4663dc5e5
    => echo $?
    0

A digest which does not match is reported::

    => md5sum -v 7000 4 d41d8cd98f00b204e9800998ecf8427e
    md5 for 00007000 ... 00007003 ==> 74b87337454200d4d33f80c4663dc5e5 != d41d8cd98f00b204e9800998ecf8427e ** ERROR **

Configuration
-------------

The command is only available if CONFIG_CMD_MD5SUM=y. The ``-v`` option needs
CONFIG_MD5SUM_VERIFY=y as well; without it the option is refused and the usage
message is shown.

Return value
------------

The return value $? is 0 (true) if the digest is computed, and if ``-v`` is
given, if it matches. It is 1 (false) if fewer than two arguments are given,
if an option other than ``-v`` is used, or if the digests differ.

See also
--------

* :doc:`hash<hash>` for MD5 and the other algorithms, with the algorithm named
  on the command line
* :doc:`crc32<crc32>` for a checksum, which is quicker and shorter
* :doc:`sm3sum<sm3sum>` for SM3
* *sha1sum* for SHA1, which has the same three forms
* :doc:`md<md>` for looking at the memory being hashed
