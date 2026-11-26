.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: mount (command)

mount command
=============

Synopsis
--------

::

    mount [<dev> <mountpoint>]
    mount <iface> <dev:part> <mountpoint>
    mount -t <type> <iface> <dev:part> <mountpoint>
    mount -a

Description
-----------

The mount command attaches a filesystem to the VFS namespace at the given
mount point. On first use the VFS is automatically initialised with an
empty root filesystem at ``/``.

Without arguments, lists all current mounts showing the mount point and
the name of the UCLASS_FS device.

With two arguments, mounts the named UCLASS_FS device at *mountpoint*.

With three arguments, auto-detects the filesystem type on the block
device and mounts it. Each registered VFS filesystem driver is tried in
turn until one succeeds.

With ``-t``, specifies the filesystem type explicitly instead of
auto-detecting.

A filesystem mounted from a block device gets a device of its own, named
after the driver and numbered in the order the filesystems are mounted,
so the first ext4 image mounted in a session appears as ``ext4_fs.0``.

With ``-a``, mounts each filesystem named in the ``fstab`` environment
variable. The variable holds a list of entries separated by ``;``, each
of them the arguments of a mount command, so
``hostfs /host; host 0:0 /mnt`` mounts the host filesystem at ``/host``
and the first partition of host device 0 at ``/mnt``.

The ``mount`` command is also available as ``fs mount``; see :doc:`fs`.

dev
    name of a UCLASS_FS device (e.g. ``hostfs`` for the sandbox host
    filesystem)

mountpoint
    absolute path where the filesystem should appear (must start with
    ``/``)

iface
    block device interface (e.g. ``host``, ``mmc``, ``scsi``)

dev\:part
    device and partition number (e.g. ``0:0``, ``0:1``)

type
    filesystem type (e.g. ``ext4``, ``fat``)

Example
-------

Mount the sandbox host filesystem, then list the mounts::

    => mount hostfs /host
    => mount
    /host                hostfs

Auto-detect a filesystem on a block device, here an ext4 image attached
to sandbox's host device::

    => host bind 0 ext4.img
    => mount host 0:0 /mnt
    => mount
    /host                hostfs
    /mnt                 ext4_fs.0
    => ls /mnt
    DIR          0 .
    DIR          0 ..
               913 Kbuild
    DIR          0 lost+found
              4096 testing

Mount the same image a second time, with its type given::

    => mount -t ext4 host 0:0 /data
    => mount
    /host                hostfs
    /mnt                 ext4_fs.0
    /data                ext4_fs.1

Mount everything listed in ``fstab``::

    => setenv fstab 'hostfs /host; host 0:0 /mnt'
    => mount -a
    => mount
    /host                hostfs
    /mnt                 ext4_fs.2

A mount point which is in use, or a device which is not there, is
reported along with the error number::

    => mount hostfs /host
    mount failed: -16: Device or resource busy
    => mount nodev /other
    mount failed: -19: No such device

Configuration
-------------

The mount command is available when CONFIG_CMD_VFS=y.

Return value
------------

The return value $? is set to 0 (true) if the mount succeeded or the
mount list was printed, 1 (false) otherwise.

See also
--------

* :doc:`umount<umount>` for detaching a filesystem which this command
  has attached
* :doc:`df<df>` for the space used and free in each mount
* :doc:`ls<ls>` for listing a directory of a mounted filesystem
* :doc:`cd<cd>` for changing into one, so that later paths can be
  relative
* :doc:`fs<fs>` for mounting, unmounting, listing and copying under a
  single command
