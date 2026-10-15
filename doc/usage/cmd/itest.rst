.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: itest (command)

itest command
=============

Synopsis
--------

::

    itest[.b | .w | .l | .q | .s] [*]<value1> <op> [*]<value2>

Description
-----------

The itest command is used to compare two values. The return value $? is set
accordingly.

By default it is assumed that the values are 4 byte integers. By appending a
postfix (.b, .w, .l, .q, .s) the size can be specified:

======= ======================================================
postfix meaning
======= ======================================================
.b      1 byte integer
.w      2 byte integer
.l      4 byte integer
.q      8 byte integer (only available on a 64-bit build)
.s      string
======= ======================================================

A postfix which is not in the table is reported with 'Invalid data width
specifier' and the comparison is not made.

value1, value2
    values to compare. Numeric values are hexadecimal. If '*' is prefixed a
    hexadecimal address is passed, which points to the value to be compared.
    With the .s postfix a value of the form ${name} is the value of
    environment variable *name*, or an empty string if it is not set.
    Anything else after a '$' is reported with 'Invalid variable reference'.

op
    operator, see table

    ======== ======================
    operator meaning
    ======== ======================
    -lt      less than
    <        less than
    -le      less or equal
    <=       less or equal
    -eq      equal
    ==       equal
    -ne      not equal
    !=       not equal
    <>       not equal
    -ge      greater or equal
    >=       greater or equal
    -gt      greater than
    >        greater than
    ======== ======================

    An operator which is not in the table is reported with
    'Unknown operator' and the comparison is not made.

Examples
--------

The itest command sets the result variable $? to true (0) or false (1):

::

    => itest 3 < 4; echo $?
    0
    => itest 3 == 4; echo $?
    1

This value can be used in the :doc:`if <if>` command:

::

    => if itest 0x3002 < 0x4001; then echo true; else echo false; fi
    true

Numbers will be truncated according to the postfix before comparing:

::

    => if itest.b 0x3002 < 0x4001; then echo true; else echo false; fi
    false

Postfix .s causes a string compare. The string '0xa1234' is alphabetically
smaller than '0xb'.

::

    => if itest.s 0xa1234 < 0xb; then echo true; else echo false; fi
    true

A value prefixed by '*' is a pointer to the value in memory.

::

    => mw.l 0x4000 4 1
    => mw.l 0x4004 3 1
    => md.l 0x4000 2
    00004000: 00000004 00000003                    ........
    => if itest *0x4000 == 4; then echo true; else echo false; fi
    true
    => if itest *0x4004 == 3; then echo true; else echo false; fi
    true

With the .s postfix the address holds the string to compare, terminated by a
zero byte.

A value of the form ${name} is the value of an environment variable. It must
be quoted, so that the shell does not expand it before the command sees it:

::

    => setenv target sandbox
    => if itest.s '${target}' == sandbox; then echo true; else echo false; fi
    true
    => if itest.s '${nosuch}' == ''; then echo true; else echo false; fi
    true

Configuration
-------------

The command is only available if CONFIG_CMD_ITEST=y.

Return value
------------

The return value $? is 0 (true) if the condition is true and 1 (false)
otherwise. It is also 1 if the comparison cannot be made at all, for
instance because the operator or the postfix is not a known one.

See also
--------

* :doc:`if<if>` for acting on the result
* :doc:`test<test>` for the shell built-in, which also compares strings and
  checks whether a file exists
* :doc:`cmp<cmp>` for comparing two regions of memory rather than two values
* :doc:`setexpr<setexpr>` for computing with values rather than comparing them
* :doc:`md<md>` for looking at the memory a '*' prefix reads
