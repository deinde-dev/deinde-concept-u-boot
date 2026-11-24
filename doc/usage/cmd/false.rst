.. SPDX-License-Identifier: GPL-2.0-or-later

.. index::
   single: false (command)

false command
=============

Synopsis
--------

::

    false

Description
-----------

The false command sets the return value $? to 1 (false).

Example
-------

::

    => false; echo $?
    1
    =>

Return value
------------

The return value $? is always 1.

Configuration
-------------

The false command is only available if CONFIG_HUSH_PARSER=y.

See also
--------

* :doc:`true<true>` for a command which always succeeds
* :doc:`test<test>` for working out whether an expression is true
