.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: diskboot (command)

diskboot command
================

Synopsis
--------

::

    diskboot [<addr> [<dev>:<part>]]

Description
-----------

The diskboot command loads an image from the start of a partition on an IDE
drive and then boots it, in the way the bootm command does. It is the IDE
member of a small family of commands which share their code: scsiboot does
the same for a SCSI drive and usbboot for a USB storage device.

The image must sit at the very first block of the partition, with nothing
before it, since the command reads the header from that block alone. Both the
legacy uImage format and FIT are understood; the header gives the size, which
says how many further blocks to read.

Only the load is unconditional. The command boots what it has loaded when the
autostart environment variable is set to yes, and otherwise leaves the image
in memory and sets the loadaddr variable to its address, so that a later bootm
can start it.

Nothing probes the IDE controller at start-up, so run 'ide reset' first. Until
that has found the drives the command reports a bad device specification, even
where a drive is attached.

addr
    memory address to load the image to. It defaults to the value of
    CONFIG_SYS_LOAD_ADDR

dev:part
    drive number and partition number, both counted from 0 in the order the
    drives are found and both read as hexadecimal. A partition number of 0
    means the whole drive, and auto means the first bootable partition, or
    the first one where none is bootable. Leaving the partition out selects
    partition 1

    The argument as a whole defaults to the bootdevice environment variable,
    which a '-' also selects, and the command fails where that is unset too

Example
-------

This loads a Linux kernel from the first partition of the first drive of a
QEMU x86 machine, with qemu-x86 built for it and the drive already found by
'ide reset'. The partition holds a legacy uImage written to its first block,
and 0x2000000 is the CONFIG_SYS_LOAD_ADDR of that board::

    => diskboot 2000000 0:1

    Loading from ide device 0, partition 1: Name: hda1  Type: U-Boot
       Image Name:   Linux-6.12.0
       Created:      2026-10-13  14:47:28 UTC
       Image Type:   Intel x86 Linux Kernel Image (gzip compressed)
       Data Size:    4194304 Bytes = 4 MiB
       Load Address: 00100000
       Entry Point:  00100000
    => echo $?
    0

The name and the type come from the partition table rather than from the
image: part_dos.c calls every DOS partition it describes 'U-Boot' and the name
is made up from the interface, the drive letter and the partition number, so
an IDE drive always reads hd<letter><number>.

With autostart set to yes the same command goes on to boot what it has
loaded, announcing it with::

    Automatic boot of image at addr 0x02000000 ...

Sandbox builds the command but has no IDE controller to attach it to, so the
paths it can show there are the ones before a drive is reached. With no
argument the command has no device to work with, since sandbox leaves
bootdevice unset::

    => diskboot
    ** No device specified **
    => echo $?
    1

An address on its own leaves the device just as unspecified, and so does a
'-' in place of it::

    => diskboot 1000
    ** No device specified **
    => diskboot 1000 -
    ** No device specified **

Setting the variable gives the command somewhere to look, and naming a drive
which is not there fails before any transfer::

    => setenv bootdevice 99:1
    => diskboot 1000
    ** Bad device specification ide 99 **
    => echo $?
    1

The command takes two arguments at most, so a third produces the usage
message::

    => diskboot 1000 0:1 2
    diskboot - boot from IDE device

    Usage:
    diskboot loadAddr dev:part
    => echo $?
    1

Return value
------------

The return value $? is 0 (true) if the image is loaded, and 1 (false) if the
drive or partition cannot be found, if a block cannot be read or if the image
format is not recognised.

Where autostart is set to yes the value is that of the boot which follows, so
a failure to start the image is reported rather than the load being called a
success.

Configuration
-------------

The command is only available if CONFIG_CMD_IDE=y, which selects CONFIG_IDE to
build the driver. CONFIG_LEGACY_IMAGE_FORMAT=y adds support for the legacy
uImage format and CONFIG_FIT=y for FIT, with neither of them being a
requirement of the command.

See also
--------

* :doc:`ide<ide>` for probing the controller and for reading blocks without
  booting them
* :doc:`bootm<bootm>` for booting an image which is already in memory, which
  is what this command uses once the image is loaded
* :doc:`scsiboot<scsiboot>` for the same command on a SCSI drive
* :doc:`usbboot<usbboot>` for the same command on a USB storage device
* :doc:`nboot<nboot>` for the same command on a raw NAND chip
* :doc:`nvme<nvme>` for the ide equivalent on an NVMe namespace
* :doc:`part<part>` for listing the partitions a drive offers
* :doc:`read<read>` for reading blocks from a partition without treating them
  as an image
