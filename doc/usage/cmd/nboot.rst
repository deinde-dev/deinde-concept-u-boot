.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: nboot (command)

nboot command
=============

Synopsis
--------

::

    nboot
    nboot <partition>
    nboot <loadAddr> [<partition>]
    nboot <loadAddr> [<dev> [<offset>]]

Description
-----------

The nboot command loads an image from raw NAND flash and then boots it, in
the way the bootm command does. It is the raw-NAND member of a family of
load-and-boot commands which usbboot and scsiboot provide for USB and SCSI
storage.

The image must start at the offset the command is given, since the command
reads the first page from there and takes the image header from the start of
it. Both the legacy uImage format and FIT are understood; the header gives
the size, which says how much more to read.

Bad blocks are skipped as the image is read, so a transfer which runs into
one ends further into the chip than its size alone would suggest, in the way
*nand read* does.

Only the load is unconditional. The command boots what it has loaded when
the ``autostart`` environment variable is set to yes, and otherwise leaves
the image in memory and makes its address the default load address, so that
a later *bootm* with no argument starts it. Note that the ``loadaddr``
variable is left alone.

The arguments are positional and what each one means depends on how many
there are, which is what the three forms in the synopsis describe. A single
argument is read as a partition name where one of that name exists, and as a
load address otherwise.

loadAddr
    memory address to load the image to, read as hexadecimal. It defaults to
    the value of CONFIG_SYS_LOAD_ADDR

partition
    name of an mtdparts partition to load from, which supplies both the chip
    and the offset within it. The partition must be on a NAND device; the
    command refuses one which is not. This form needs
    CONFIG_CMD_MTDPARTS=y

dev
    number of the NAND chip to load from, read as hexadecimal. It defaults
    to the value of the ``bootdevice`` environment variable, and the command
    fails where that is unset too

offset
    offset within the chip to load from, read as hexadecimal. It defaults
    to 0

A name which is not a partition is read as a chip number instead, and a
number which does not parse comes out as 0, so a misspelt partition name
loads from the start of chip 0 rather than being refused.

The command accepts the same .jffs2, .e and .i suffixes *nand read* takes,
and ignores them: a plain read is what it does in each case. Any other
suffix is refused.

Example
-------

The sandbox build emulates two NAND chips, so the command has something to
read. With a uImage in memory at 0x2000000, write it to the first block of
nand0::

    => nand erase 0 2000

    NAND erase: device 0 offset 0x0, size 0x2000
    Erasing at 0x0 -- 100% complete.
    OK
    => nand write 2000000 0 200

    NAND write: device 0 offset 0x0, size 0x200
     512 bytes written: OK

Loading it back reports what the header holds. Since ``autostart`` is unset,
the image is left in memory at the address given::

    => nboot 1000000 0

    Loading from nand0, offset 0x0
       Image Name:   nboot test image
       Created:      1970-01-01   0:00:00 UTC
       Image Type:   Sandbox Linux Kernel Image (uncompressed)
       Data Size:    64 Bytes = 64 Bytes
       Load Address: 01000000
       Entry Point:  01000000
    => echo $?
    0

An mtdparts partition may be named instead of a chip and an offset::

    => setenv mtdids nand0=nand0
    => setenv mtdparts nand0:1m(boot),2m(kernel),-(rootfs)
    => nboot 1000000 kernel

    Loading from nand0, offset 0x100000
       Image Name:   nboot test image
    ...

A misspelt name is not refused, since it is read as a chip number once the
partition lookup has failed::

    => nboot 1000000 kernel2
    incorrect device type in kernel2

    Loading from nand0, offset 0x0
    ** Unknown image type
    => echo $?
    1

With no argument at all the command has nowhere to look, since sandbox
leaves ``bootdevice`` unset::

    => nboot

    ** No boot device **
    => echo $?
    1

Setting that variable gives it a chip::

    => setenv bootdevice 0
    => nboot 1000000

    Loading from nand0, offset 0x0
       Image Name:   nboot test image
    ...

A chip which is not there fails before any transfer::

    => nboot 1000000 9

    ** Device 9 not available
    => echo $?
    1

An erased block holds nothing which looks like an image, so the read
succeeds and the format check is what fails::

    => nand erase 0 2000
    ...
    => nboot 1000000 0

    Loading from nand0, offset 0x0
    ** Unknown image type
    => echo $?
    1

A suffix the command does not know is refused before anything is read::

    => nboot.foo 1000000 0
    Unknown nand load suffix '.foo'
    => echo $?
    1

The command takes three arguments at most, so a fourth produces the usage
message::

    => nboot 1000000 0 0 0
    nboot - boot from NAND device

    Usage:
    nboot [partition] | [[[loadAddr] dev] offset]
    => echo $?
    1

Return value
------------

The return value $? is 0 (true) if the image is loaded, and 1 (false) if no
chip is named, if the chip does not exist, if a page cannot be read, if the
image format is not recognised or if the suffix is unknown.

Where ``autostart`` is set to yes the value is that of the boot which
follows, so a failure to start the image is reported rather than the load
being called a success.

Configuration
-------------

The command is only available if CONFIG_CMD_NAND=y, which also provides the
nand command. CONFIG_LEGACY_IMAGE_FORMAT=y adds support for the legacy
uImage format and CONFIG_FIT=y for FIT, with neither of them being a
requirement of the command; without either one, nothing is ever recognised
as an image. CONFIG_CMD_MTDPARTS=y adds the partition-name form.

See also
--------

* :doc:`nand<nand>` for writing the image into the chip in the first place,
  and for reading it back without booting it
* :doc:`mtdparts<mtdparts>` for defining the partitions this command can
  name
* :doc:`bootm<bootm>` for booting an image which is already in memory, which
  is what this command uses once the image is loaded
* :doc:`iminfo<iminfo>` for checking a loaded image without booting it
* :doc:`usbboot<usbboot>` for the same command on a USB storage device
* :doc:`scsiboot<scsiboot>` for the same command on a SCSI drive
