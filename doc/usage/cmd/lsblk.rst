.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: lsblk (command)

lsblk command
=============

Synopsis
--------

::

    lsblk

Description
-----------

The lsblk command lists every block-device driver in the image, along with the
block devices attached to each of them. It gives one line per driver, in
alphabetical order, with the devices after a colon:

* the name of the driver, as it appears in its U_BOOT_DRIVER() declaration
* the block-device type of each attached device, such as mmc or usb, and the
  device number within that type
* '<none>' where the driver has no device attached

A device appears here once it is bound, which happens when the devicetree is
scanned, so a driver can list devices which have never been probed. Nothing in
the listing says whether a device is present or usable; an interface which
enumerates its devices at run time, such as usb or scsi, shows nothing until it
is scanned.

The device numbers are the ones the load and list commands take, so the listing
says which interface name and number to give to a command such as fatload or
ls.

The command takes no arguments.

Example
-------

This shows the drivers of a sandbox build. Three MMC devices come from the
devicetree, while the other drivers have nothing attached::

    => lsblk
    Block Driver          Devices
    -----------------------------
    blkmap_blk          : <none>
    efi_blk             : <none>
    ide_blk             : <none>
    mmc_blk             : mmc 2, mmc 1, mmc 0
    nvme-blk            : <none>
    nvmxip-blk          : <none>
    rkmtd_blk           : <none>
    sandbox_host_blk    : <none>
    scsi_blk            : <none>
    usb_storage_blk     : <none>
    virtio-blk          : <none>

Binding a file to the host driver gives that driver a device as well::

    => host bind 0 /tmp/b/sandbox/persistent-data/mmc1.img
    => lsblk
    Block Driver          Devices
    -----------------------------
    blkmap_blk          : <none>
    efi_blk             : <none>
    ide_blk             : <none>
    mmc_blk             : mmc 2, mmc 1, mmc 0
    nvme-blk            : <none>
    nvmxip-blk          : <none>
    rkmtd_blk           : <none>
    sandbox_host_blk    : host 0
    scsi_blk            : <none>
    usb_storage_blk     : <none>
    virtio-blk          : <none>

Return value
------------

The return value $? is 0 (true) unless the block-device uclass is missing, in
which case the command reports that it cannot get the BLK uclass and $? is set
to 1 (false).

See also
--------

* :doc:`part<part>` for the partitions on one of the devices listed here
* :doc:`blkmap<blkmap>` for making a block device out of memory or part of
  another device
* :doc:`blkcache<blkcache>` for the cache which sits in front of these devices
* :doc:`host<host>` for attaching a file on the host to the sandbox host driver
* *mmcinfo* for the details of the current MMC device
