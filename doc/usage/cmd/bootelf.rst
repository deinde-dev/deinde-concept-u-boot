.. SPDX-License-Identifier: GPL-2.0-or-later
.. Copyright 2024, Maxim Moskalets <maximmosk4@gmail.com>

.. index::
   single: bootelf (command)

bootelf command
===============

Synopsis
--------

::

    bootelf [-p|-s] [-d <fdt_addr>] [<image_addr> [<arg>]...]

Description
-----------

The *bootelf* command is used to launch a ELF binary at *image_addr*. If
*image_addr* is not specified, the bootelf command will try to find image in
*image_load_addr* variable (*CONFIG\_SYS\_LOAD\_ADDR* by default).

Args after *image_addr* will be passed to application in common *argc*, *argv*
format.

The image is loaded through its section headers unless *-p* asks for the
program headers instead. Either way each part of the image is copied to the
address the header gives, which is not where the image itself sits.

The *autostart* environment variable decides what happens next. Where it is
set to a true value the command jumps to the entry point in the ELF header
and returns only if the image returns; otherwise the image is left in memory
for something else to start, and the command prints nothing at all.

A command sequence to run a ELF image using FDT might look like

::

    load mmc 0:1 ${loadaddr} /kernel.elf
    load mmc 0:1 ${fdt_addr_r} /soc-board.dtb
    bootelf -d ${fdt_addr_r} ${loadaddr} ${loadaddr}

image_addr
    Address of the ELF binary.

fdt_addr
    Address of the device-tree. This argument in only needed if bootable
    application uses FDT that requires additional setup (like /memory node).

arg
    Any text arguments for bootable application. This is usually the address
    of the device-tree.

Flags:

-p
    Load ELF image via program headers.

-s
    Load ELF image via section headers.

-d
    Setup FDT by address.

Example
-------

This runs the standalone hello_world example on a BeagleBone Black, with
am335x_evm built with CONFIG_EXAMPLES. The image is linked at 0x82400000,
four megabytes above CONFIG_SYS_LOAD_ADDR, but is read from the SD card to
*loadaddr* (0x82000000), so the command copies its sections up to where
they belong before jumping to the entry point. Arguments after the address
reach the application as its *argc* and *argv*::

    => load mmc 0:1 ${loadaddr} hello_world
    10748 bytes read in 3 ms (3.4 MiB/s)
    => setenv autostart yes
    => bootelf ${loadaddr} one two
    Example expects ABI version 10
    Actual U-Boot ABI version 10
    Hello World
    argc = 2
    argv[0] = "one"
    argv[1] = "two"
    argv[2] = "<NULL>"
    Hit any key to exit ...

    => echo $?
    0

With *autostart* unset the same command only loads the image, printing
nothing, so something else must start it. Here the *go* command calls the
entry point from the ELF header, whose low bit selects Thumb mode::

    => setenv autostart
    => bootelf ${loadaddr}
    => go 82400001
    ## Starting application at 0x82400001 ...
    Example expects ABI version 10
    Actual U-Boot ABI version 10
    Hello World
    argc = 1
    argv[0] = "82400001"
    argv[1] = "<NULL>"
    Hit any key to exit ...

    ## Application terminated, rc = 0x0

A file which is not an ELF image, such as a Linux zImage, is refused::

    => load mmc 0:1 ${loadaddr} zImage
    4313600 bytes read in 279 ms (14.7 MiB/s)
    => bootelf ${loadaddr}
    ## No elf image at address 0x82000000
    => echo $?
    1

An image whose magic number is right but whose type is not ET_EXEC, such as
a relocatable object or a shared library, is refused with '## Not a 32-bit
elf image' instead, whatever class its header gives. Where *autostart* is
set, either refusal is followed by 'Invalid ELF image'.

Return value
------------

The return value $? is 0 (true) where the image is loaded, and, where it is
started as well, where the image itself returns 0. It is 1 (false) where
there is no valid ELF image at the address, where the devicetree setup fails,
or where the image returns something other than 0.

An image which does not return, as an operating system does not, leaves the
command with no value to report.

Configuration
-------------

The bootelf command is only available if CONFIG_CMD_ELF=y. FDT setup by flag -d
need CONFIG_CMD_ELF_FDT_SETUP=y.

See also
--------

* :doc:`bootvx<bootvx>` for starting a VxWorks image, which shares this
  command's file and its ELF loader but sets up a bootline first
* :doc:`go<go>` for calling code at an address which is already known
