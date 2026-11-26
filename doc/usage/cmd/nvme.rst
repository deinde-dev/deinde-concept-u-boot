.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: nvme (command)

nvme command
============

Synopsis
--------

::

    nvme scan
    nvme detail
    nvme info
    nvme device [<dev>]
    nvme part [<dev>]
    nvme read <addr> <blk#> <cnt>
    nvme write <addr> <blk#> <cnt>

Description
-----------

The nvme command provides access to the namespaces of the NVM Express
controllers which U-Boot has found, normally on a PCI bus. Each namespace
appears as a block device, so the command lists them, selects the one which
later commands work on, shows its partitions and copies raw blocks between it
and memory.

Apart from the scan and detail sub-commands it is the same command as ide,
scsi and usb provide, so the rest of the sub-commands and their output are
shared with those.

dev
    device number, counted from 0 in the order the namespaces are found

addr
    memory address to read into or write from

blk#
    first block to transfer, numbered from 0

cnt
    number of blocks to transfer

nvme scan
~~~~~~~~~

This probes each controller and binds a block device for every namespace it
reports. Nothing is printed, so use 'nvme info' afterwards to see what turned
up. Boot scanning does the same thing through the NVMe bootdev hunter, so the
command is needed only where nothing has scanned yet, or to look again after a
drive is plugged in.

nvme detail
~~~~~~~~~~~

This shows what the current namespace and its controller support: the optional
admin and NVM commands, the format attributes, the LBA formats with the one in
use marked as current, the data-protection capabilities and the metadata
capabilities.

nvme info
~~~~~~~~~

This lists the namespaces, giving the vendor, product and revision strings the
controller reports, the type and the capacity. Nothing is printed when there
is no namespace.

nvme device
~~~~~~~~~~~

With no argument this shows the current device; with one it makes that device
current. The detail, read, write and part sub-commands all work on the current
device.

nvme part
~~~~~~~~~

This prints the partition table of the given device, or of every device when
none is given.

nvme read
~~~~~~~~~

This reads cnt blocks starting at block blk# into memory at addr.

nvme write
~~~~~~~~~~

This writes cnt blocks from memory at addr to the namespace, starting at block
blk#.

Example
-------

These examples use the emulated controller which sandbox provides when it is
started with its test devicetree (the -T flag). It has a single 2MB namespace
which holds no partition table.

Scan for namespaces and list what is found::

    => nvme scan
    => nvme info
    Device 0: Vendor:  Rev: 1.0 Prod: 0123456789ab
                Type: Hard Disk
                Capacity: 2.0 MB = 0.0 GB (4096 x 512)

Show the current device::

    => nvme device

    nvme device 0: Vendor:  Rev: 1.0 Prod: 0123456789ab
                Type: Hard Disk
                Capacity: 2.0 MB = 0.0 GB (4096 x 512)

Show what the namespace and its controller support::

    => nvme detail
    Blk device 0: Optional Admin Command Support:
        Namespace Management/Attachment: no
        Firmware Commit/Image download: no
        Format NVM: no
        Security Send/Receive: no
    Blk device 0: Optional NVM Command Support:
        Reservation: no
        Save/Select field in the Set/Get features: no
        Write Zeroes: no
        Dataset Management: no
        Write Uncorrectable: no
    Blk device 0: Format NVM Attributes:
        Support Cryptographic Erase: No
        Support erase a particular namespace: Yes
        Support format a particular namespace: Yes
    Blk device 0: LBA Format Support:
    Blk device 0: End-to-End DataProtect Capabilities:
        As last eight bytes: No
        As first eight bytes: No
        Support Type3: No
        Support Type2: No
        Support Type1: No
    Blk device 0: Metadata capabilities:
        As part of a separate buffer: No
        As part of an extended data LBA: No

The partition listing reports the missing table but still succeeds::

    => nvme part

    no nvme partition table available
    => echo $?
    0

Write a block from memory, read it back to a different address and compare
the two::

    => mw.b 1000 a5 200
    => nvme write 1000 2 1

    nvme write: device 0 block # 2, count 1 ... 1 blocks written: OK
    => nvme read 2000 2 1

    nvme read: device 0 block # 2, count 1 ... 1 blocks read: OK
    => cmp.b 1000 2000 200
    Total of 512 byte(s) were the same

Selecting a device which does not exist fails::

    => nvme device 1

    Device 1: unknown device
    => echo $?
    1

Return value
------------

The return value $? is 0 (true) if the sub-command succeeds and 1 (false) if
it fails, if the sub-command is not recognised or if its arguments are wrong.

Note that 'nvme info' returns 0 even when no namespace is present, and so does
'nvme part', which reports a missing partition table rather than failing.

Configuration
-------------

The command is only available if CONFIG_CMD_NVME=y, which needs CONFIG_NVME
and is on by default with it. A driver for the controller is needed as well,
such as CONFIG_NVME_PCI.

See also
--------

* :doc:`ide<ide>` for the same set of sub-commands on an IDE drive, along with
  the reset which that interface provides
* :doc:`virtio<virtio>` for the same set of sub-commands on a virtio
  block-device, along with the listing which that interface provides
* :doc:`part<part>` for listing and querying partitions on any block device,
  not just an NVMe one
* :doc:`read<read>` for reading blocks from a partition rather than from the
  start of the namespace
* :doc:`write<write>` for writing blocks to a partition
* :doc:`usb<usb>` for the same set of sub-commands on a USB storage device,
  along with the enumeration of the bus it sits on
* :doc:`scsi<scsi>` for the same set of sub-commands on a SCSI drive, along
  with the erase which that interface provides
* :doc:`scsiboot<scsiboot>` and :doc:`usbboot<usbboot>` for loading and
  starting an image held on a SCSI drive or a USB storage device, there being
  no such command for NVMe
