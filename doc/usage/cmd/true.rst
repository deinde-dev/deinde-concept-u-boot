.. SPDX-License-Identifier: GPL-2.0-or-later

.. index::
   single: true (command)

true command
============

Synopsis
--------

::

    true

Description
-----------

The true command sets the return value $? to 0 (true).

Example
-------

::

    => true; echo $?
    0
    =>

Return value
------------

The return value $? is always 0.

Configuration
-------------

The true command is only available if CONFIG_HUSH_PARSER=y.

See also
--------

* :doc:`false<false>` for a command which always fails
* :doc:`test<test>` for working out whether an expression is true
