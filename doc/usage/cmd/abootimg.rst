.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: abootimg (command)

abootimg command
================

Synopsis
--------

::

    abootimg addr <boot_img_addr> [<vendor_boot_img_addr> [<init_boot_img_addr>]]
    abootimg dump dtb
    abootimg get ver [varname]
    abootimg get recovery_dtbo [addr_var [size_var]]
    abootimg get dtb_load_addr [varname]
    abootimg get dtb --index=<num> [addr_var [size_var]]
    abootimg get ramdisk [addr_var [size_var]]

Description
-----------

The abootimg command looks inside an Android boot image which is already in
memory and reports what it finds: the version of the image header, the address
and size of the ramdisk, the address and size of each device-tree blob in the
DTB area and the load address the header asks for the DT. It neither loads nor
boots anything, so the image must be read into memory first, with
:doc:`load<load>`, :doc:`fatload<fatload>` or whatever suits the board.

Each sub-command either prints the value it finds, in hex and without a 0x
prefix, or writes it to the environment variables named on the command line.
The second form is the useful one in a boot script, since the value can then be
passed to :doc:`bootm<bootm>` or :doc:`fdt<fdt>`.

An Android boot is described by up to three images, depending on the version of
the boot image header. Version 2 and earlier keep the kernel, the ramdisk and
the DTB area in a single boot image. Version 3 moves the ramdisk and the DTB
area into a vendor_boot image and version 4 adds an init_boot image holding the
generic ramdisk, so the command has to be told where all of them are. See
:doc:`../../android/boot-image` for the layout of each version.

abootimg addr
~~~~~~~~~~~~~

This sets the addresses the other sub-commands work on. The values are read as
hex, with or without a 0x prefix, and are remembered until the command is used
again. Where it is never used, the boot image is taken from $loadaddr and the
other two images are treated as absent.

abootimg dump dtb
~~~~~~~~~~~~~~~~~

This prints the size, the model and the compatible string of every DT blob in
the DTB area, in the order they appear.

abootimg get ver
~~~~~~~~~~~~~~~~

This reports the version of the boot image header, as a decimal number.

abootimg get recovery_dtbo
~~~~~~~~~~~~~~~~~~~~~~~~~~

This reports the address and size of the recovery DTBO area, which only a
version 1 or version 2 header has.

abootimg get dtb_load_addr
~~~~~~~~~~~~~~~~~~~~~~~~~~

This reports the address the header asks for the DT to be loaded at. It needs a
version 2 or later header, since earlier ones have no such field.

abootimg get dtb
~~~~~~~~~~~~~~~~

This reports the address and size of one DT blob in the DTB area, chosen by
--index, where the first blob is index 0. The index is the only form the
sub-command accepts; it cannot search the area by name.

abootimg get ramdisk
~~~~~~~~~~~~~~~~~~~~

This reports the address and size of the ramdisk and shows a line giving the
same address and the size in KiB. For a version 3 or later image the ramdisk
cannot be used where it lies, so it is first assembled at the address the
header gives, or at $ramdisk_addr_r where the header has none. For version 4 a
bootconfig section is added to the end of it, into which any androidboot.*
parameter is moved out of $bootargs.

Example
-------

This uses the boot image which test/py/tests/test_android/test_abootimg.py
builds, a version 2 image with two DT blobs in its DTB area, loaded on sandbox
from the host filesystem::

    => host load hostfs - 1000000 boot.img
    8192 bytes read in 0 ms
    => abootimg addr 1000000
    => abootimg get ver
    2
    => abootimg get ver v
    => printenv v
    v=2
    => abootimg dump dtb
    ## DTB area contents (concat format):
     - DTB #0:
               (DTB)size = 125
              (DTB)model = x1
         (DTB)compatible = y1,z1
     - DTB #1:
               (DTB)size = 125
              (DTB)model = x2
         (DTB)compatible = y2,z2
    => abootimg get dtb --index=1 dtb_addr dtb_size
    => printenv dtb_addr dtb_size
    dtb_addr=100187d
    dtb_size=7d
    => abootimg get dtb_load_addr
    11f00000
    => abootimg get ramdisk rd_addr rd_size
    RAM disk load addr 0x01001000 size 1 KiB
    => printenv rd_addr rd_size
    rd_addr=1001000
    rd_size=10

The DT blob can then be handed to the :doc:`fdt<fdt>` command, which needs it
to be aligned, so it is copied first::

    => cp.b $dtb_addr 4000 $dtb_size
    => fdt addr 4000
    Working FDT set to 4000
    => fdt get value model / model
    => printenv model
    model=x2

Return value
------------

The return value $? is 0 (true) if the value asked for is reported. It is set
to 1 (false) if no sub-command is named, if one is given the wrong number of
arguments, if the image has no Android boot image header, if the field asked
for is missing from the version of header the image has, or if an environment
variable cannot be set.

See also
--------

* :doc:`bootm<bootm>` for booting an Android boot image once its pieces have
  been found
* :doc:`fdt<fdt>` for working with a DT blob from the DTB area
* :doc:`bcb<bcb>` for the Bootloader Control Block, which says what to boot
  and which A/B slot to take it from
* :doc:`bootflow<bootflow>` for the standard-boot method which boots Android
  without any of these sub-commands
* :doc:`../../android/boot-image` for the format of the images themselves
