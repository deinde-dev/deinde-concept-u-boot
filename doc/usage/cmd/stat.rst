.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: stat (command)

stat command
============

Synopsis
--------

::

    stat <path>

Description
-----------

The stat command displays metadata about a file or directory in the VFS:
its name, its size in bytes and its type, which is a regular file, a
directory, a symbolic link or unknown. For a symbolic link a ``Link:``
line gives the target as well.

Where the filesystem records times, three more lines follow, labelled
``Modify``, ``Access`` and ``Birth``, each giving a date and time in the
form ``YYYY-MM-DD HH:MM:SS``. FAT and ext4 provide them; the host
filesystem which sandbox uses does not, so the example below has none.

The filesystem must be mounted first using ``mount``.

path
    absolute or relative path in the VFS

Example
-------

::

    => mount hostfs /host
    => stat /host/README
      File: README
      Size: 74783
      Type: regular file
    => stat /host/doc
      File: doc
      Size: 4096
      Type: directory
    => ln README /host/link
    => stat /host/link
      File: link
      Size: 74783
      Type: symbolic link
      Link: README
    => stat /host/nowhere
    Error: -2: No such file or directory

Configuration
-------------

The stat command is available when CONFIG_CMD_VFS=y.

Return value
------------

The return value $? is set to 0 (true) if the file was found, 1 (false)
otherwise.

See also
--------

* :doc:`ls<ls>` for the same information about every file of a directory
* :doc:`size<size>` for putting a file's size in an environment variable
  rather than printing it
* :doc:`ln<ln>` for creating the symbolic links this command reports
* :doc:`mount<mount>` for making a filesystem available to this command
