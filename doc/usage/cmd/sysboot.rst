.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: sysboot (command)

sysboot command
===============

Synopsis
--------

::

    sysboot [-p] <interface> <dev[:part]> <ext2|fat|any> [addr] [filename]

Description
-----------

The sysboot command reads a syslinux or extlinux configuration file from a
local filesystem and boots the images it names. It is the local-disk
counterpart of :doc:`pxe<pxe>`, which does the same job over TFTP, and it
understands the same file format.

interface
    Block device interface to read from, such as *mmc*, *scsi* or *usb*

dev[:part]
    Device number, with an optional partition number. A hardware partition
    may be given as *<dev>.<hw_part>:<part>*

ext2|fat|any
    Filesystem to read the file with. *ext2* covers ext2, ext3 and ext4,
    *fat* covers FAT and VFAT, and *any* lets U-Boot work the filesystem out
    for itself

addr
    Address to read the configuration file to. This defaults to the
    environment variable *pxefile_addr_r*, which must then be set

filename
    Path of the configuration file within the filesystem. This defaults to
    the environment variable *bootfile*; giving it here sets *bootfile* as
    well, since paths inside the file are taken relative to it

Each image the file names is read to the address held in the environment
variable the file format defines for it: *kernel_addr_r*, *ramdisk_addr_r*,
*fdt_addr_r* and *fdtoverlay_addr_r*.

Options
-------

-p
    Show the menu and wait for a choice, rather than booting the default
    label. The wait is *pxe_timeout* seconds, falling back to the *timeout*
    value in the file, which is in tenths of a second. With neither set the
    command waits for a choice indefinitely

Example
-------

This is on a BeagleBone Black, reading an extlinux configuration file from
the first partition of the eMMC::

    => sysboot mmc 1:1 ext2 ${pxefile_addr_r} /extlinux/extlinux.conf
    Retrieving file: /extlinux/extlinux.conf
    1:	Debian GNU/Linux
    Retrieving file: /vmlinuz-6.12.0-am335x
    Retrieving file: /dtbs/am335x-boneblack.dtb
    append: root=/dev/mmcblk1p1 ro quiet
    ## Flattened Device Tree blob at 88000000
       Booting using the fdt blob at 0x88000000
       Loading Device Tree to 8ffee000, end 8fffffff ... OK

    Starting kernel ...

Asking for a filesystem the command does not know about is an error::

    => sysboot mmc 1:1 ntfs ${pxefile_addr_r} /extlinux/extlinux.conf
    Invalid filesystem: ntfs

As is leaving out one of the three arguments it requires::

    => sysboot mmc 1:1
    sysboot - command to get and boot from syslinux files

    Usage:
    sysboot [-p] <interface> <dev[:part]> <ext2|fat|any> [addr] [filename]
        - load and parse syslinux menu file 'filename' from ext2, fat
          or any filesystem on 'dev' on 'interface' to address 'addr'

Return value
------------

The return value $? is 0 if the configuration file was read and parsed, and 1
otherwise. It is also 1 when fewer than three arguments are given, when
*pxefile_addr_r* holds something which is not a hex number, and when neither
a filename nor *bootfile* says which file to read.

Note that a file which names no label the command can boot still gives 0,
since reading and parsing the file is all the command undertakes to do.

See also
--------

* :doc:`pxe<pxe>` for the same file format fetched over TFTP
* :doc:`bootm<bootm>` for booting an image which is already in memory
* :doc:`ext4load<ext4load>` for reading a single file from an ext4
  filesystem by hand
