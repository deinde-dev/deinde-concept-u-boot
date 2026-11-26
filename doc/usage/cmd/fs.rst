.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: fs (command)

fs command
==========

Synopsis
--------

::

    fs mount [<dev> <mountpoint>]   - list or create mounts
    fs umount <mountpoint> | -a     - unmount one or all filesystems
    fs ls [<path>]                  - list a directory (default /)
    fs cp <source> <dest>           - copy a file

Description
-----------

The ``fs`` command provides access to the virtual filesystem (VFS) layer. It
allows filesystems to be mounted at paths within a unified namespace, then
listed and navigated using absolute paths.

Each of the sub-commands is also a command in its own right, which takes more
options; see `See also`_ below. The ``fs`` form groups them under one name.

On first use the VFS is automatically initialised with an empty root filesystem
at ``/``.

See :doc:`/develop/vfs` for more information about the VFS layer.


fs mount
~~~~~~~~

Without arguments, lists all current mounts showing the mount point and the
name of the UCLASS_FS device mounted there.

With two arguments, mounts the filesystem device *dev* at *mountpoint*. The
device is looked up by name in UCLASS_FS, so it must exist already; the
:doc:`mount<mount>` command can create one from a block device and a
partition.

dev
    Name of a UCLASS_FS device (e.g. ``hostfs`` for the sandbox host
    filesystem)

mountpoint
    Absolute path where the filesystem should be mounted (must start with
    ``/``). A filesystem can be mounted inside another one, but not at a
    mount point which is in use


fs umount
~~~~~~~~~

Unmounts the filesystem at *mountpoint* and removes the corresponding
UCLASS_MOUNT device. With ``-a``, unmounts every filesystem, which is not an
error when there are none.

mountpoint
    Path of an existing mount point


fs ls
~~~~~

Lists the contents of a directory. If no path is given, lists the root
directory ``/``.

At the root level, mount points appear as directories. Within a mounted
filesystem, actual directory entries are shown along with any child mount
points. Each line gives ``DIR`` for a directory, the size in bytes and the
name; the sandbox host filesystem reports no size for a directory.

path
    Path to list, absolute or relative to the current directory
    (default ``/``)


fs cp
~~~~~

Copies a file from *source* to *dest*. Both paths can be on different
mounted filesystems, enabling cross-filesystem copies. The file is copied
in chunks of 4KB and the number of bytes copied is reported.

source
    Absolute or relative path to the source file

dest
    Absolute or relative path to the destination file (created if it does
    not exist)


Example
-------

Mount the sandbox host filesystem, look around in it and copy a file::

    => fs mount hostfs /host
    => fs mount
    /host                hostfs
    => fs ls /
    DIR          0 host
    => fs ls /host/test/ulib
    DIR          0 .
    DIR          0 ..
               305 Makefile
              1400 ulib_test.c
    => fs cp /host/Kbuild /host/copy
    913 bytes copied
    => fs umount /host

Configuration
-------------

The ``fs`` command is available when CONFIG_CMD_VFS=y, which depends on
CONFIG_VFS=y.

Return value
------------

The return value $? is set to 0 (true) if the command succeeded, 1 (false)
otherwise. A sub-command which is not recognised, or which is given too many
arguments, shows the usage message and returns 1.

See also
--------

* :doc:`mount<mount>` for mounting a filesystem type on a block device, or
  everything named in the 'fstab' variable
* :doc:`umount<umount>` for unmounting on its own, with completion of the
  mount point
* :doc:`ls<ls>` for listing a directory on its own, with the current
  directory as the default
* :doc:`cd<cd>` and :doc:`pwd<pwd>` for the current directory, which a
  relative path is taken from
* :doc:`df<df>` for the space used and free in each mount
* :doc:`tree<tree>` for listing a directory and those below it
