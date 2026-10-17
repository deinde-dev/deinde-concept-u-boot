.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: booti (command)

booti command
=============

Synopsis
--------

::

    booti [<addr> [<initrd>[:<size>]] [<fdt>]]

Description
-----------

The booti command is used to boot a Linux kernel in flat or compressed
'Image' format. Which compressed formats are supported is configurable.

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

To support compressed Image files the following environment variables must be
set:

kernel_comp_size
    size of the compressed file. The value has to be at least the size of
    loaded image for decompression to succeed. For the booti command the
    maximum decompressed size is 10 times this value. To decompress the Image
    straight to where it will run from (see below) this must be the exact
    size, e.g. ``$filesize`` after loading it.

kernel_comp_addr_r
    start of memory area used for decompression. This is not needed if the
    Image can be decompressed straight to where it will run from (see below).

Placement
---------

A Linux Image must be placed at text_offset bytes (a field in its header)
from a 2MB-aligned base. If it is loaded to such an address it is left where
it is; otherwise it is moved to a suitable region of free memory, with a
message like ``Moving Image from 8080000 to 8200000``. To avoid the copy, load
the Image to an address which is already correctly aligned.

A compressed Image is decompressed straight to a suitable address, so that it
is never moved, if its uncompressed size can be read from the compressed data.
That is the case for gzip and normally for zstd, but only for lzma and lz4 if
the encoder recorded the size (e.g. ``lz4 --content-size``) and never for
bzip2 or lzo. Otherwise it is decompressed to kernel_comp_addr_r first and
then moved if necessary.

If CONFIG_BOOTI_RANDOMIZE_BASE is enabled, the base is instead chosen at
random from the free memory on each boot, using the RNG uclass, so that the
kernel's physical address is not predictable. This is the physical part of
KASLR, as done by the kernel's EFI stub. It is skipped if the kernel command
line contains ``nokaslr``. A compressed Image is decompressed straight to the
random address where possible; an uncompressed one is moved there.

Example
-------

This is the boot log of an Odroid C2 board:

::

    => load mmc 0:1 $fdt_addr_r dtb-5.10.0-3-arm64
    27530 bytes read in 7 ms (3.7 MiB/s)
    => load mmc 0:1 $kernel_addr_r vmlinuz-5.10.0-3-arm64
    26990448 bytes read in 1175 ms (21.9 MiB/s)
    => load mmc 0:1 $ramdisk_addr_r initrd.img-5.10.0-3-arm64
    27421776 bytes read in 1209 ms (21.6 MiB/s)
    => booti $kernel_addr_r $ramdisk_addr_r:$filesize $fdt_addr_r
    Moving Image from 8080000 to 8200000, end 9c60000
    ## Flattened Device Tree blob at 08008000
       Booting using the fdt blob at 0x8008000
       Loading Ramdisk to 7a52a000, end 7bf50c50 ... OK
       Loading Device Tree to 000000007a520000, end 000000007a529b89 ... OK

    Starting kernel ...

The kernel can be compressed with gzip:

.. code-block:: bash

    cd /boot
    gzip -k vmlinuz-5.10.0-3-arm64

Here is the boot log for the compressed kernel:

::

    => load mmc 0:1 $fdt_addr_r dtb-5.10.0-3-arm64
    27530 bytes read in 6 ms (4.4 MiB/s)
    => load mmc 0:1 $kernel_addr_r vmlinuz-5.10.0-3-arm64.gz
    9267730 bytes read in 402 ms (22 MiB/s)
    => env set kernel_comp_size $filesize
    => load mmc 0:1 $ramdisk_addr_r initrd.img-5.10.0-3-arm64
    27421776 bytes read in 1181 ms (22.1 MiB/s)
    => booti $kernel_addr_r $ramdisk_addr_r:$filesize $fdt_addr_r
       Uncompressing Kernel Image to 7a600000
    ## Flattened Device Tree blob at 08008000
       Booting using the fdt blob at 0x8008000
       Loading Ramdisk to 7a52a000, end 7bf50c50 ... OK
       Loading Device Tree to 000000007a520000, end 000000007a529b89 ... OK

    Starting kernel ...

Configuration
-------------

The booti command is only available if CONFIG_CMD_BOOTI=y, which needs an
architecture that knows the Image format: arm64, RISC-V or sandbox.

Which compression types are supported depends on:

* CONFIG_BZIP2
* CONFIG_GZIP
* CONFIG_LZ4
* CONFIG_LZMA
* CONFIG_LZO
* CONFIG_ZSTD

Return value
------------

Normally this command does not return. If an error occurs, the return value $?
is set to 1 (false). If the operating system returns to U-Boot, the system is
reset.

See also
--------

* :doc:`bootz<bootz>` for booting a Linux kernel in the older zImage format
* :doc:`bootm<bootm>` for booting a kernel wrapped in a FIT or legacy image,
  which carries its own load address and compression type
* :doc:`bootefi<bootefi>` for booting a kernel through the UEFI sub-system
* :doc:`load<load>` for reading the Image, the device tree and the initial RAM
  disk into memory in the first place
