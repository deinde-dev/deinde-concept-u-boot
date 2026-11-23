.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: tree (command)

tree command
============

Synopsis
--------

::

    tree [<path>]

Description
-----------

The tree command lists a directory and the directories below it, each
level indented by four more spaces. A directory's name ends in ``/`` and
a symbolic link's in ``@``. The entries ``.`` and ``..`` are left out.

The listing covers the directory named and the two levels below it;
anything deeper is not shown.

If no path is given, the current working directory is listed. For the
root, each mount point is listed along with what is below it.

The path is printed before the directory is read, so it appears even
when the listing fails.

The filesystem must be mounted first using ``mount``.

path
    absolute or relative path to list (default: current working directory)

Example
-------

::

    => mount hostfs /host
    => tree /host/board/samsung/smdkv310
    /host/board/samsung/smdkv310
        tools/
            mksmdkv310spl.c
        MAINTAINERS
        Makefile
        Kconfig
        smdkv310.c

A relative path is resolved against the working directory::

    => cd /host/board/samsung
    => tree n1
    /host/board/samsung/n1
        bose.config
        n1.env
        MAINTAINERS
        Kconfig

Only a directory can be listed::

    => tree /host/README
    /host/README
    tree failed: -20: Not a directory

Configuration
-------------

The tree command is available when CONFIG_CMD_VFS=y.

Return value
------------

The return value $? is set to 0 (true) if the directory was listed,
1 (false) otherwise.

See also
--------

* :doc:`ls<ls>` for listing one directory without recursing into it
* :doc:`cd<cd>` for setting the directory which this command lists when
  given no path
* :doc:`stat<stat>` for the size and type of a single file
* :doc:`mount<mount>` for the mount points which are listed at the root
