.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: memsize (command)

memsize command
===============

Synopsis
--------

::

    memsize [envvar]

Description
-----------

The memsize command shows the amount of RAM in MiB in decimal notation,
rounded down to a whole MiB. Optionally the same value is assigned to the
environment variable *envvar*, in which case nothing is shown.

Examples
--------

This first example shows printing of ram size:

::

  => memsize
  256 MiB

This second example shows assign ram size to environment variable:

::

  => memsize memsz
  => env print memsz
  memsz=256

Return value
------------

The return value $? is 0 (true), unless the environment variable cannot be
set, in which case it is 1 (false). The command accepts at most one argument,
so given more it shows its usage message and returns 1 (false).

See also
--------

* :doc:`meminfo<meminfo>` for the memory map as well as the DRAM size
* :doc:`ufetch<ufetch>` for the DRAM size alongside the other board details
