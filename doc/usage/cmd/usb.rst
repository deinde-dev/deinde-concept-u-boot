.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: usb (command)

usb command
===========

Synopsis
--------

::

    usb start
    usb reset
    usb stop [f]
    usb tree
    usb info [<dev>]
    usb test <dev> <port> <mode>
    usb storage
    usb dev [<dev>]
    usb part [<dev>]
    usb read <addr> <blk#> <cnt>
    usb write <addr> <blk#> <cnt>
    usb erase <blk#> <cnt>

Description
-----------

The usb command enumerates the USB buses and reports what is found on them. It
also carries the sub-commands which every block-device command shares, so the
storage devices behind it can be selected and read without any other command.

Nothing on the bus is known until it has been enumerated, so 'usb start' comes
before everything else. Each sub-command which needs a device says so when the
bus is stopped, rather than reporting that the device is missing.

Two different numbering schemes meet in this command. The USB devices are
numbered by their address on the bus, counted from 1, which is what 'usb tree'
and 'usb info' show. The storage devices are numbered separately, from 0, in
the order the mass-storage scan finds them, and that is the number 'usb dev',
'usb part' and the block transfers take. A device therefore has two numbers
and they are unrelated.

Every device number is read as decimal, while the addresses, block numbers
and counts the transfers take are read as hexadecimal.

usb start
~~~~~~~~~

This enumerates the buses, probing each device which answers, and then scans
for mass-storage devices. It does nothing at all if the bus is already
started, so a second 'usb start' is not a way to pick up a stick which has
been plugged in since; use 'usb reset' for that.

usb reset
~~~~~~~~~

This stops the bus and starts it again, so a device which has arrived or left
since the last scan is accounted for. The storage numbering comes out of the
scan, so a reset can hand the same stick a different number.

usb stop
~~~~~~~~

This shuts the bus down, after which every sub-command which needs a device
refuses to run. Any argument at all, which the help text writes as 'f' for
force, first moves the console input back to the serial port, so a USB
keyboard being used as the console does not take the input away with it.

usb tree
~~~~~~~~

This shows the devices as a tree, each with its address, its class, the speed
it is running at and the current it draws, and under that the driver and the
name of the device.

usb info
~~~~~~~~

This shows the descriptors of a device: the device descriptor, then each
configuration, interface and endpoint. With no argument it does this for every
device on every bus.

The number it takes is one less than the address shown by 'usb tree', since
the addresses start at 1 and this argument is counted from 0.

usb test
~~~~~~~~

This puts a device into one of the USB 2.0 test modes, by sending it a
SET_FEATURE request. All three arguments are needed. Only the first letter of
the mode matters:

J
    Test_J, the J state driven on the bus
K
    Test_K, the K state driven on the bus
S
    Test_SE0_NAK, single-ended zero with NAK responses
P
    Test_Packet, a repeating test packet
F
    Test_Force_Enable, which only a hub downstream port accepts

A port of 0 means the device's own upstream port; any other number is a
downstream port of a hub, so a device which is not a hub takes 0 alone. The
bus has to be started again before the device can be used normally.

usb storage
~~~~~~~~~~~

This shows the mass-storage devices alone, with the vendor, product and
revision strings each one reports, its type and its capacity. These are the
devices the block sub-commands below work on.

usb dev
~~~~~~~

This shows the current storage device, or selects a new one where a number is
given. The current device is where the partition listing and the block
transfers go when they are given no device of their own.

usb part
~~~~~~~~

This shows the partition table of the given storage device, or of every one of
them where no device is named. A device with no partition table is left out of
the listing rather than reported.

usb read
~~~~~~~~

This reads <cnt> blocks from the current storage device into memory at <addr>,
starting at block <blk#>. All three are read as hexadecimal. The block size
comes from the device, so the number of bytes moved is <cnt> times that.

usb write
~~~~~~~~~

This writes <cnt> blocks to the current storage device from memory at <addr>,
starting at block <blk#>, with the arguments read as for 'usb read'.

usb erase
~~~~~~~~~

This erases <cnt> blocks of the current storage device, starting at block
<blk#>. The help text leaves it out, since it comes from the shared block
handler rather than from this command, and it works only on a device whose
driver offers an erase operation.

Example
-------

Sandbox emulates a hub with four flash sticks and a keyboard behind it, so the
command has something to enumerate. One of the sticks is a single block long,
which the filesystem probe run during the scan cannot read a boot sector
from::

    => usb start
    starting USB...
              fs_devread() ** Read outside partition 0
              fs_devread()  ** fs_devread read error - block
             exfat_mount() failed to read boot sector
    Bus usb@1: 6 USB Device(s) found
           scanning usb for storage devices... 4 Storage Device(s) found

The tree shows the hub at address 1 with the five devices behind it. Which
stick lands at which address varies from run to run, so a later run may order
them differently::

    => usb tree
    USB device tree:
      1  Hub (480 Mb/s, 100mA)
      |  sandbox hub 2345
      |
      |+-2  Mass Storage (480 Mb/s, 100mA)
      |    sandbox flash flash-stick@3
      |
      |+-3  Human Interface (1.5 Mb/s, 100mA)
      |    sandbox keyboard keyb@4
      |
      |+-4  Mass Storage (480 Mb/s, 100mA)
      |    sandbox flash flash-stick@0
      |
      |+-5  Mass Storage (480 Mb/s, 100mA)
      |    sandbox flash flash-stick@1
      |
      |+-6  Mass Storage (480 Mb/s, 100mA)
           sandbox flash flash-stick@2

The storage devices have their own numbering, which says nothing about where
on the bus they sit::

    => usb storage
      Device 0: Vendor: sandbox Rev: 1.0 Prod: flash
                Type: Hard Disk
                Capacity: 20.0 MB = 0.0 GB (40960 x 512)
      Device 1: Vendor: sandbox Rev: 1.0 Prod: flash
                Type: Hard Disk
                Capacity: 4.0 MB = 0.0 GB (8192 x 512)
      Device 2: Vendor: sandbox Rev: 1.0 Prod: flash
                Type: Hard Disk
                Capacity: 19.0 MB = 0.0 GB (38912 x 512)
      Device 3: Vendor: sandbox Rev: 1.0 Prod: flash
                Type: Hard Disk
                Capacity: 0.0 MB = 0.0 GB (1 x 512)

The scan leaves the first of them current::

    => usb dev

    usb device 0: Vendor: sandbox Rev: 1.0 Prod: flash
                Type: Hard Disk
                Capacity: 20.0 MB = 0.0 GB (40960 x 512)

Two of the four sticks carry a partition table, and those are the two the
listing shows::

    => usb part

    Partition Map for usb device 0  --   Partition Type: DOS

    Part	Start Sector	Num Sectors	UUID		Type
      1	2048      	36864     	5b82558f-01	0c Boot
      2	38912     	2048      	5b82558f-02	83


    Partition Map for usb device 2  --   Partition Type: DOS

    Part	Start Sector	Num Sectors	UUID		Type
      1	2048      	36864     	52eb499c-01	0c

Reading blocks needs no partition, since it goes to the whole device::

    => usb read 1000 0 2

    usb read: device 0 block # 0, count 2 ... 2 blocks read: OK

The descriptors of the device at address 2 are asked for as device 1, one less
than the tree shows::

    => usb info 1
    config for device 1
    2: Mass Storage,  USB Revision 2.0
     - sandbox flash flash-stick@3
     - Class: (from Interface) Mass Storage
     - PacketSize: 0  Configurations: 1
     - Vendor: 0x1234  Product 0x5678 Version 0.0
       Configuration: 0
       - Interfaces: 1 Bus Powered 100mA
         Interface: 0
         - Alternate Setting 0, Endpoints: 2
         - Class Mass Storage, UFI, Bulk only
         - Endpoint 1 Out Bulk MaxPacket 1024
         - Endpoint 2 In Bulk MaxPacket 1024

That device is not a hub, so the only port it has is its own upstream one::

    => usb test 1 0 J
    Setting Test_J mode on upstream facing port...
    Test mode successfully set. Use 'usb start' to return to normal operation.

Once the bus is stopped the devices are gone, and the command says which
sub-command to run rather than reporting a missing device::

    => usb stop
    stopping USB..
    => usb tree
    USB is stopped. Please issue 'usb start' first.
    => echo $?
    1

Return value
------------

The return value $? is 0 (true) where the sub-command does what it was asked
and 1 (false) otherwise.

Starting and stopping the bus always report success, even where no controller
answers, since a board with nothing plugged in is not an error.

The value is 1 where the bus is stopped and the sub-command needs it, where a
block transfer moves fewer blocks than were asked for, and where 'usb test'
cannot set the mode. 'usb info' is the exception: it reports a device which is
not there and still returns 0.

Configuration
-------------

The command is only available if CONFIG_CMD_USB=y. CONFIG_USB_STORAGE=y adds
the mass-storage scan and with it the storage sub-commands, so without it the
command offers the bus and the descriptors alone.

See also
--------

* :doc:`usbboot<usbboot>` for loading and booting an image from a USB stick
* :doc:`ums<ums>` for going the other way and exporting a device to a host
* :doc:`part<part>` for listing partitions of any block device, not just USB
* :doc:`lsblk<lsblk>` for listing every block device the board has
* :doc:`read<read>` for reading from a partition rather than from the whole
  device
* :doc:`ide<ide>`, :doc:`nvme<nvme>`, :doc:`virtio<virtio>` and *scsi* for the
  same set of sub-commands on other kinds of drive
* :doc:`dm<dm>` for showing the driver-model tree the USB devices are bound
  into
