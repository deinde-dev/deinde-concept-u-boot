.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: cd (command)

cd command
==========

Synopsis
--------

::

    cd [<path>]

Description
-----------

The ``cd`` command changes the VFS current working directory. When a
relative path (one that does not start with ``/``) is given to commands
like ``ls``, ``cat``, ``load``, ``save`` or ``size``, it is resolved
relative to the current working directory.

With no argument, ``cd`` returns to the root directory ``/``.

The path must exist and must be a directory, or a symbolic link to one.
The special entries ``.`` and ``..`` are understood, and ``..`` at the
root stays at the root.

path
    Absolute or relative path (default ``/``)

Example
-------

::

    => mount hostfs /host
    => pwd
    /
    => cd /host
    => pwd
    /host
    => cd doc
    => pwd
    /host/doc
    => cd ..
    => pwd
    /host
    => cd
    => pwd
    /

A path which is not there leaves the working directory alone::

    => cd /host/nowhere
    Error: -2: No such file or directory

Configuration
-------------

The cd command is available when CONFIG_CMD_VFS=y.

Return value
------------

The return value $? is set to 0 (true) if the directory change
succeeded, 1 (false) otherwise.

See also
--------

* :doc:`pwd<pwd>` for showing the directory which this command sets
* :doc:`ls<ls>` for listing that directory, which needs no path once it
  is the current one
* :doc:`tree<tree>` for listing it along with the directories below it
* :doc:`mount<mount>` for making a filesystem available to change into
