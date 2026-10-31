.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: hash (command)

hash command
============

Synopsis
--------

::

    hash algorithm address count [[*]hash_dest]
    hash -v algorithm address count [*]hash

Description
-----------

The hash command computes a message digest over ``count`` bytes of memory
starting at ``address``, using any of the digest algorithms U-Boot has been
built with. It is the general form of the one-algorithm commands
:doc:`sm3sum<sm3sum>`, :doc:`crc32<crc32>`, :doc:`md5sum<md5sum>` and
*sha1sum*, each of which names its algorithm rather than taking it as an
argument.

The digest is written as a line of the form::

    <algorithm> for <first> ... <last> ==> <digest>

where ``first`` and ``last`` are the first and last addresses covered, printed
as eight hex digits, and the digest is a string of hex digits whose length
depends on the algorithm. A count of zero gives the digest of the empty
string, and ``last`` is then one below ``first``.

The algorithm name is matched in lower case, so 'MD5' and 'md5' both work.
The names available depend on the build: md5 (CONFIG_MD5), sha1
(CONFIG_SHA1), sha256 (CONFIG_SHA256), sha384 (CONFIG_SHA384), sha512
(CONFIG_SHA512), sm3_256 (CONFIG_SM3), crc16-ccitt (CONFIG_CRC16), crc8
(CONFIG_CRC8 together with CONFIG_HASH_CRC8) and crc32 (CONFIG_CRC32). An
unknown name gives 'Unknown hash algorithm' followed by the usage message.

With ``hash_dest`` the digest is saved as well as printed. The destination is
an environment variable, unless it starts with ``*``, in which case the rest
is a hex address and the raw digest bytes are written there. Nothing is saved
when the argument is left out.

The ``-v`` option verifies rather than saves: the digest just computed is
compared with the one named by the last argument, and nothing is printed if
the two match. The argument is a hex address if it starts with ``*``, an
immediate hex digest if it has exactly the right number of digits for the
algorithm, and otherwise the name of an environment variable holding one. A
variable which is unset, or whose value is the wrong length, gives 'Expected
<n> hex digits in env var' and then 'ERROR: <name> does not contain a valid
<algorithm> sum'. On a mismatch the command shows both digests::

    <algorithm> for <first> ... <last> ==> <computed> != <expected> ** ERROR **

algorithm
    Name of the digest algorithm, in either case.

address
    Address of the first byte to hash. Hexadecimal, '0x' prefix optional.

count
    Number of bytes to hash. Hexadecimal, '0x' prefix optional.

hash_dest
    Where to save the digest: the name of an environment variable, or an
    address if written as ``*address``.

hash
    What to compare the digest against: an immediate hex digest, the name of
    an environment variable holding one, or an address written as
    ``*address``.

Example
-------

Hash four bytes of memory with two different algorithms::

    => mw.b 1000 61 4
    => hash md5 1000 4
    md5 for 00001000 ... 00001003 ==> 74b87337454200d4d33f80c4663dc5e5
    => hash sha256 1000 4
    sha256 for 00001000 ... 00001003 ==> 61be55a8e2f6b4e172338bddf184d6dbee29c98853e0a0485ecee7f27b9af0b4

Save a digest in an environment variable, then use it to verify the same
memory::

    => hash md5 1000 4 myvar
    md5 for 00001000 ... 00001003 ==> 74b87337454200d4d33f80c4663dc5e5
    => printenv myvar
    myvar=74b87337454200d4d33f80c4663dc5e5
    => hash -v md5 1000 4 myvar
    => echo $?
    0

Save it to memory instead, and verify against that address::

    => hash md5 1000 4 *2000
    md5 for 00001000 ... 00001003 ==> 74b87337454200d4d33f80c4663dc5e5
    => md.b 2000 10
    00002000: 74 b8 73 37 45 42 00 d4 d3 3f 80 c4 66 3d c5 e5  t.s7EB...?..f=..
    => hash -v md5 1000 4 *2000
    => echo $?
    0

A digest given on the command line works too, and a wrong one is reported::

    => hash -v md5 1000 4 74b87337454200d4d33f80c4663dc5e5
    => hash -v md5 1000 4 ffffffffffffffffffffffffffffffff
    md5 for 00001000 ... 00001003 ==> 74b87337454200d4d33f80c4663dc5e5 != ffffffffffffffffffffffffffffffff ** ERROR **

Configuration
-------------

The command is only available if CONFIG_CMD_HASH=y. The ``-v`` option needs
CONFIG_HASH_VERIFY=y as well; without it the option is refused and the usage
message is shown.

Return value
------------

The return value $? is 0 (true) if the digest is computed, and if ``-v`` is
given, if it matches. It is 1 (false) if fewer than three arguments are given,
if the algorithm is not known, if an option other than ``-v`` is used, or if
the digests differ.

See also
--------

* :doc:`sm3sum<sm3sum>` for SM3 as a command of its own
* :doc:`crc32<crc32>` for a checksum rather than a digest, which can also be
  stored to memory and verified
* :doc:`md5sum<md5sum>` for MD5 as a command of its own, and *sha1sum* for
  SHA1
* :doc:`md<md>` for looking at the memory being hashed
