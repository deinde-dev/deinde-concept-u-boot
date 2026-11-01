.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: ms (command)

ms command
==========

Synopsis
--------

::

    ms [.b, .w, .l, .q, .s] [-q] [-l<n>] <address> <count> <value>...

Description
-----------

The ms command searches a region of memory for a value, a sequence of values,
or a string. Each match is shown as a line of memory in the same format as
:doc:`md<md>`, with '--' between lines which are not next to each other, then
the number of matches found.

The search stops once it has shown as many matches as the limit allows, saying
so, and repeating the command carries on from where it stopped.

address
    start address to search, hexadecimal. The offset set by the
    :doc:`base<base>` command is added to it

count
    number of objects to search, hexadecimal. Note that this is not the same
    as the number of bytes, unless .b is used

value
    value to look for, hexadecimal. Several can be given, in which case they
    must appear in memory in the order they are given. With .s the value is
    text instead and all the values are joined, so 'hello there' matches
    'hellothere'

-q
    quiet: show no output at all. The environment variables below are still
    set, which is what makes this useful in a script

-l<n>
    show at most <n> matches, hexadecimal. The default is 10 (0xa)

The data_size suffix sets the size of each value searched for (defaults to
.l):

    =========  ===================
    data_size  Value size
    =========  ===================
    .b         byte
    .w         word (16 bits)
    .l         long (32 bits)
    .q         quadword (64 bits)
    .s         string
    =========  ===================

The command sets three environment variables each time it runs:

memmatches
    number of matches found, hexadecimal

memaddr
    address of the last match shown, or 0 if there is none

mempos
    offset of that match from the start of the search, in objects rather than
    bytes

The address and the data_size become the defaults for the next memory
command, as they do for :doc:`md<md>`. The length still to search is
remembered for the next ms command.

Example
-------

This looks for a 32-bit value which appears twice::

    => mw 10000 0 40
    => mw.l 10010 12345678
    => mw.l 10064 12345678
    => ms.l 10000 40 12345678
    00010010: 12345678 00000000 00000000 00000000  xV4.............
    --
    00010060: 00000000 12345678 00000000 00000000  ....xV4.........
    2 matches
    => echo $memmatches $memaddr $mempos
    2 10064 19

The last match is at 0x10064, which is 0x19 longs from the start of the
search. Quiet mode sets the same variables and prints nothing::

    => ms.l -q 10000 40 12345678
    => echo $memmatches
    2

A search which finds nothing says so and fails::

    => ms.l 10000 40 deadbeef
    0 matches
    => echo $?
    1

The .s suffix searches for text::

    => mw 20000 0 20
    => mw.l 20010 6c6c6568
    => mw.b 20014 6f
    => ms.s 20000 80 hello
    00020010: 68 65 6c 6c 6f 00 00 00 00 00 00 00 00 00 00 00  hello...........
    1 match

With a limit the command stops early and says that there may be more::

    => mw.b 20030 68 4
    => ms.b -l2 20000 80 68
    00020010: 68 65 6c 6c 6f 00 00 00 00 00 00 00 00 00 00 00  hello...........
    --
    00020030: 68 68 68 68 00 00 00 00 00 00 00 00 00 00 00 00  hhhh............
    2 matches (repeat command to check for more)

Configuration
-------------

The ms command is available if CONFIG_CMD_MEM_SEARCH=y. Support for 64-bit
values (ms.q) is only available on 64-bit targets.

Return value
------------

The return value $? is 0 (true) if at least one match is found. It is 1
(false) if there is no match, if the address, the count or the value is
missing, or if an option is not recognised, in which case the usage message
is shown. It is also 1, with nothing printed, if the data_size suffix is not
one of those listed above.

See also
--------

* :doc:`md<md>` for displaying the memory a match points at
* :doc:`mw<mw>` for writing a value to memory, which ms can then find
* :doc:`cmp<cmp>` for comparing two regions, rather than searching one
* :doc:`base<base>` for the address offset applied to these commands
