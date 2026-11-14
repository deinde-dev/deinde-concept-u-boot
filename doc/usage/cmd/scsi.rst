.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: scsi (command)

scsi command
============

Synopsis
--------

::

    scsi scan
    scsi info
    scsi device [<dev>]
    scsi part [<dev>]
    scsi read <addr> <blk#> <cnt>
    scsi write <addr> <blk#> <cnt>
    scsi erase <blk#> <cnt>

Description
-----------

The scsi command provides access to the drives attached to the SCSI
controllers which U-Boot knows about, including the SATA drives reached
through an AHCI controller. It scans the buses for drives, lists the drives it
has found, selects the one which later commands work on, shows its partitions
and copies raw blocks between the drive and memory.

Apart from the scan and erase sub-commands it is the same command as ide, nvme,
usb and virtio provide, so the rest of the sub-commands and their output are
shared with those.

dev
    device number, counted from 0 in the order the drives are found

addr
    memory address to read into or write from

blk#
    first block to transfer, numbered from 0

cnt
    number of blocks to transfer

scsi scan
~~~~~~~~~

This probes each controller, asks every target and logical unit it supports
what it is, and binds a block device for each drive which answers. The drives
found are listed as they turn up. Boot scanning does the same thing through the
SCSI bootdev hunter, so the command is needed only where nothing has scanned
yet, or to look again once a drive is attached.

scsi info
~~~~~~~~~

This lists the drives, giving the target and logical-unit numbers, the vendor,
product and revision strings the drive reports, its type and its capacity.
Nothing is printed when no drive is present.

scsi device
~~~~~~~~~~~

With no argument this shows the current device; with one it makes that device
current. The read, write, erase and part sub-commands all work on the current
device.

scsi part
~~~~~~~~~

This prints the partition table of the given device, or of every device when
none is given.

scsi read
~~~~~~~~~

This reads cnt blocks starting at block blk# into memory at addr.

scsi write
~~~~~~~~~~

This writes cnt blocks from memory at addr to the drive, starting at block
blk#.

scsi erase
~~~~~~~~~~

This erases cnt blocks starting at block blk#, by asking the drive to unmap
them. A drive which does not support that reports an error, since there is no
read-modify-write fallback.

Example
-------

Sandbox has an emulated SCSI controller with a single drive behind it, backed
by a file, so the output below is from a sandbox run.

Scanning the bus lists the drives it finds, with the vendor, product and
revision each one reports::

    => scsi scan
    scanning bus for devices...
      Device 0: (0:0) Vendor: SANDBOX Prod.: FAKE DISK Rev: 1.0
                Type: Hard Disk
                Capacity: 2.0 MB = 0.0 GB (4096 x 512)

The listing afterwards shows the same drive::

    => scsi info
    Device 0: (0:0) Vendor: SANDBOX Prod.: FAKE DISK Rev: 1.0
                Type: Hard Disk
                Capacity: 2.0 MB = 0.0 GB (4096 x 512)

Asking for the current device names the interface as well; selecting one says
that it is now current::

    => scsi device

    scsi device 0: (0:0) Vendor: SANDBOX Prod.: FAKE DISK Rev: 1.0
                Type: Hard Disk
                Capacity: 2.0 MB = 0.0 GB (4096 x 512)
    => scsi device 0

    Device 0: (0:0) Vendor: SANDBOX Prod.: FAKE DISK Rev: 1.0
                Type: Hard Disk
                Capacity: 2.0 MB = 0.0 GB (4096 x 512)
    ... is now current device

The emulated drive has no partition table, so the listing reports that and
still succeeds::

    => scsi part

    no scsi partition table available
    => echo $?
    0

Each transfer says what it is about to do, then how it went. Here two blocks
are written, read back and erased::

    => scsi write 100000 100 2

    scsi write: device 0 block # 256, count 2 ... 2 blocks written: OK
    => scsi read 200000 100 2

    scsi read: device 0 block # 256, count 2 ... 2 blocks read: OK
    => scsi erase 100 2

    scsi erase: device 0 block # 256, count 2 ... 2 blocks erased: OK

A transfer which runs off the end of the drive fails. The driver refuses it
outright, so the error code appears where the number of blocks belongs::

    => scsi read 100000 fff 2

    scsi read: device 0 block # 4095, count 2 ... -5 blocks read: ERROR
    => echo $?
    1

Return value
------------

The return value $? is 0 (true) if the sub-command succeeds and 1 (false) if
it fails, if the sub-command is not recognised or if its arguments are wrong.

A transfer succeeds only when every block asked for is transferred, the number
done being shown before the verdict. Where the driver fails the transfer
outright that number is the negative error code rather than a block count.

Note that 'scsi info' returns 0 even when no drive is present, and so does
'scsi part', which reports a missing partition table rather than failing.

Configuration
-------------

The command is only available if CONFIG_CMD_SCSI=y, which needs CONFIG_SCSI=y
for the uclass itself. A driver for the controller is needed as well, such as
CONFIG_AHCI_PCI or CONFIG_DWC_AHCI, both of which need CONFIG_SCSI_AHCI=y.

See also
--------

* :doc:`scsiboot<scsiboot>` for loading and starting an image held on a SCSI
  drive, rather than reading blocks into memory
* :doc:`part<part>` for listing and querying partitions on any block device,
  not just a SCSI one
* :doc:`read<read>` for reading blocks from a partition rather than from the
  start of the drive
* :doc:`write<write>` for writing blocks to a partition
* :doc:`ide<ide>` for the same set of sub-commands on an IDE drive, along with
  the reset which that interface provides
* :doc:`nvme<nvme>` for the same set of sub-commands on an NVMe namespace,
  along with the detail which that interface provides
* :doc:`usb<usb>` for the same set of sub-commands on a USB storage device,
  along with the enumeration of the bus it sits on
* :doc:`virtio<virtio>` for the same set of sub-commands on a virtio
  block-device
* :doc:`lsblk<lsblk>` for listing every block device the board has
* :doc:`blkcache<blkcache>` for the cache which sits in front of the reads
  these sub-commands do
