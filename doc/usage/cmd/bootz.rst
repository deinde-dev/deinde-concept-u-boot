.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: bootz (command)

bootz command
=============

Synopsis
--------

::

    bootz [<addr> [<initrd>[:<size>]] [<fdt>]]

Description
-----------

The bootz command is used to boot a Linux kernel in 'zImage' format.

addr
    address of kernel image, defaults to the current default load address.
    That starts out as CONFIG_SYS_LOAD_ADDR and follows the loadaddr
    environment variable wherever it is set.

initrd
    address of the initial RAM disk. Use '-' to boot a kernel with a device
    tree but without an initial RAM disk.

size
    size of the initial RAM disk. This parameter must be specified for raw
    initial RAM disks.

fdt
    address of the device tree.

The zImage format is defined by the architecture, so the command works only
where U-Boot has a bootz_setup() for it. A platform which has none reports::

    Your platform's zImage format isn't supported yet!

and one whose implementation does not recognise the header says so in its own
words; sandbox, for instance, prints ``Unrecognized zImage``.

Example
-------

This is the boot log of an OrangePi PC board:

::

    => load mmc 0:2 $fdt_addr_r dtb
    23093 bytes read in 7 ms (3.1 MiB/s)
    => load mmc 0:2 $kernel_addr_r vmlinuz
    5079552 bytes read in 215 ms (22.5 MiB/s)
    => load mmc 0:2 $ramdisk_addr_r initrd.img
    23854965 bytes read in 995 ms (22.9 MiB/s)
    => bootz $kernel_addr_r $ramdisk_addr_r:$filesize $fdt_addr_r
    Kernel image @ 0x42000000 [ 0x000000 - 0x4d8200 ]
    ## Flattened Device Tree blob at 43000000
       Booting using the fdt blob at 0x43000000
    EHCI failed to shut down host controller.
       Loading Ramdisk to 48940000, end 49ffff75 ... OK
       Loading Device Tree to 48937000, end 4893fa34 ... OK

    Starting kernel ...

Configuration
-------------

The bootz command is only available if CONFIG_CMD_BOOTZ=y.

Return value
------------

Normally this command does not return. If an error occurs, the return value $?
is set to 1 (false). If the operating system returns to U-Boot, the system is
reset.

See also
--------

* :doc:`booti<booti>` for booting a Linux kernel in the newer Image format
* :doc:`bootm<bootm>` for booting a kernel wrapped in a FIT or legacy image,
  which carries its own load address and compression type
* :doc:`bootefi<bootefi>` for booting a kernel through the UEFI sub-system
* :doc:`load<load>` for reading the zImage, the device tree and the initial
  RAM disk into memory in the first place
