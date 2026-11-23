.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: pwd (command)

pwd command
===========

Synopsis
--------

::

    pwd

Description
-----------

The ``pwd`` command prints the current VFS working directory. The
working directory is changed with :doc:`cd`. It is ``/`` until something
changes it.

The command takes no arguments.

Example
-------

::

    => pwd
    /
    => mount hostfs /host
    => cd /host/doc
    => pwd
    /host/doc
    => cd /
    => pwd
    /

Configuration
-------------

The pwd command is available when CONFIG_CMD_VFS=y.

Return value
------------

The return value $? is set to 0 (true). Giving the command an argument
shows the usage message and sets it to 1 (false).

See also
--------

* :doc:`cd<cd>` for changing the directory which this command shows
* :doc:`ls<ls>` for listing that directory
* :doc:`mount<mount>` for listing the filesystems which can be reached
  from it
