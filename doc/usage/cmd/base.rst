.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: base (command)

base command
============

Synopsis
--------

::

    base [address]

Description
-----------

The *base* command sets or displays the address offset used by the memory
commands *cmp, cp, md, mdc, mm, ms, mw, mwc*.

Those commands add the offset to every address they are given, so with an
offset of 0x1000000 the command ``md 0`` shows the memory at 0x1000000. The
address they display is the one they actually use, with the offset already
included.

All other commands ignore the address defined by *base*.

address
    new base address as hexadecimal number. If no value is provided, the current
    value is displayed.

Example
-------

::

    => base
    Base Address: 0x00000000
    => base 1000000
    Base Address: 0x01000000
    => mw.l 0 deadbeef 4
    => md.l 0 4
    01000000: deadbeef deadbeef deadbeef deadbeef  ................
    => base 0
    Base Address: 0x00000000
    => md.l 1000000 4
    01000000: deadbeef deadbeef deadbeef deadbeef  ................

The write goes to 0x1000000 because the offset is added to the address 0
given to *mw*. Once the offset is back to zero the same memory is reached by
its own address.

Configuration
-------------

The base command is available if CONFIG_CMD_MEMORY=y.

Return value
------------

The return value $? is 0 (true) unless more than one argument is given.

See also
--------

* :doc:`md<md>` for displaying memory at an offset from the base address
* :doc:`mm<mm>` and :doc:`nm<nm>` for modifying it
* :doc:`cp<cp>` and :doc:`cmp<cmp>` for copying and comparing two regions
* :doc:`mdc<mdc>` and :doc:`mwc<mwc>` for the cyclic forms, which use the
  offset too
* *ms* for searching memory, which also uses the offset
