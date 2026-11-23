.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: umount (command)

umount command
==============

Synopsis
--------

::

    umount <mountpoint>
    umount -a

Description
-----------

The umount command detaches a mounted filesystem from the VFS namespace.

The path must name a mount point exactly, as the :doc:`mount<mount>`
command lists it: a path inside the filesystem, or the root ``/``, is
not one and is refused. The mount point itself stays in the listing of
the directory above it, although there is nothing behind it to list.

With ``-a``, unmounts all currently mounted filesystems. That succeeds
whether or not anything is mounted.

The ``umount`` command is also available as ``fs umount``; see :doc:`fs`.

mountpoint
    path of an existing mount point to unmount

Example
-------

The command says nothing when it works::

    => mount hostfs /host
    => umount /host

A path which is not a mount point is reported along with the error
number, and so is a mount point which has gone already::

    => umount /host/doc
    umount failed: -2: No such file or directory
    => umount /host
    umount failed: -2: No such file or directory

Unmount everything::

    => mount hostfs /host
    => mount hostfs /other
    => mount
    /host                hostfs
    /other               hostfs
    => umount -a
    => mount

Configuration
-------------

The umount command is available when CONFIG_CMD_VFS=y.

Return value
------------

The return value $? is set to 0 (true) if the unmount succeeded,
1 (false) otherwise.

See also
--------

* :doc:`mount<mount>` for attaching a filesystem, and for the list of
  mount points which this command takes
* :doc:`df<df>` for the space used in each mount before it goes
* :doc:`ls<ls>` for listing the root, which still shows the mount point
  once the filesystem is detached
