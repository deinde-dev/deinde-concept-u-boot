.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: mw (command)

mw command
==========

Synopsis
--------

::

    mw [.b, .w, .l, .q] <address> <value> [<count>]

Description
-----------

The mw command writes a value to memory. With a count it writes the same value
to each location in turn, so it can fill a region with a constant. Nothing is
printed when the write succeeds.

address
    address to write to, hexadecimal. The offset set by the :doc:`base<base>`
    command is added to it

value
    value to write, hexadecimal. Only the bottom bits are used, as many as the
    data_size allows

count
    number of values to write, hexadecimal. Defaults to 1. Note that this is
    the number of values, not the number of bytes, unless .b is used

The data_size suffix sets the size of each value written (defaults to .l):

    =========  ===================
    data_size  Value size
    =========  ===================
    .b         byte
    .w         word (16 bits)
    .l         long (32 bits)
    .q         quadword (64 bits)
    =========  ===================

Unlike :doc:`md<md>`, this command keeps no state of its own: the data_size
applies to one command only and the address is not remembered, so an empty
command line repeats the same write rather than carrying on where it left off.

Example
-------

This clears 0x10 longs, writes a single long, fills six bytes and then four
16-bit words::

    => mw 10000 0 10
    => md 10000 4
    00010000: 00000000 00000000 00000000 00000000  ................
    => mw 10000 12345678
    => md 10000 4
    00010000: 12345678 00000000 00000000 00000000  xV4.............
    => mw.b 10004 ab 6
    => md.b 10000 10
    00010000: 78 56 34 12 ab ab ab ab ab ab 00 00 00 00 00 00  xV4.............
    => mw.w 10010 beef 4
    => md.w 10010 8
    00010010: beef beef beef beef 0000 0000 0000 0000  ................

Configuration
-------------

The mw command is available if CONFIG_CMD_MEMORY=y. Support for 64-bit values
(mw.q) is only available on 64-bit targets.

Return value
------------

The return value $? is 0 (true) if the write happens. It is 1 (false) if the
address or the value is missing, or more than three arguments are given, in
which case the usage message is shown. It is also 1, with nothing printed, if
the data_size suffix is not one of those listed above.

See also
--------

* :doc:`md<md>` for displaying memory, to check what was written
* :doc:`mm<mm>` and :doc:`nm<nm>` for modifying memory interactively
* :doc:`mwc<mwc>` for writing a value over and over, with a delay
* :doc:`cp<cp>` for filling a region with a copy of another one, rather than
  with a constant
* :doc:`ms<ms>` for finding a value which mw has written
* :doc:`random<random>` for filling a region with random data
* :doc:`base<base>` for the address offset applied to these commands
