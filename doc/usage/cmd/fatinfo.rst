.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: fatinfo (command)

fatinfo command
===============

Synopsis
--------

::

    fatinfo <interface> [<dev[:part]>]

Description
-----------

The fatinfo command displays information about a FAT partition: the interface
and device it sits on, the device's own description, and the FAT type and
volume label read from the boot sector. The filesystem-generic
:doc:`fsinfo command <fsinfo>` shows the block counts instead.

interface
    interface for accessing the block device (mmc, sata, scsi, usb, ....)

dev
    device number, defaulting to the value of the environment variable
    bootdevice; without either the command reports that no device is
    specified

part
    partition number, defaulting to the first partition, or to the whole
    device where there is no partition table

Example
-------

This uses a 1MB FAT image bound to the sandbox host interface::

    => fatinfo host 0
    Interface:  host
      Device 0: Vendor: U-Boot Rev: 1.0 Prod: hostfile
                Type: Hard Disk
                Capacity: 1.0 MB = 0.0 GB (2048 x 512)
    Filesystem: FAT12 "NO NAME    "

A device which is not there is reported before the filesystem is touched::

    => fatinfo host 1
    ** Bad device specification host 1 **

A device which holds something other than a FAT filesystem is reported once
the partition has been found, with the partition the command settled on::

    => host bind 0 ext4.img
    => fatinfo host 0

    ** Unable to use host 0:0 for fatinfo **

Configuration
-------------

The fatinfo command is only available if CONFIG_CMD_FAT=y.

Return value
------------

The return value $? is set to 0 (true) if the information was shown,
1 (false) otherwise.

A command line which cannot be acted on at all, such as one with no
interface or with more arguments than the command takes, shows the usage
message instead.

See also
--------

* :doc:`fatload<fatload>` for reading a file from the filesystem
* :doc:`fatls<fatls>` for listing what the filesystem holds
* :doc:`fatsize<fatsize>` for finding a file's size without reading it
* :doc:`fatmkdir<fatmkdir>` for creating a directory on the filesystem
* :doc:`fsinfo<fsinfo>` for the block counts of the filesystem
* :doc:`fstypes<fstypes>` for listing the filesystem types U-Boot supports
