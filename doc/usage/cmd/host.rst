.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: host (command)

host command
============

Synopsis
--------

::

    host bind [-r] <label> <filename> [<blksz>]
    host unbind <label|seq>
    host info [<label|seq>]
    host info <label|seq> <varname>
    host dev [<label|seq>]
    host load hostfs - <addr> <filename> [<bytes> [<offset>]]
    host ls hostfs - [<filename>]
    host save hostfs - <addr> <filename> <bytes> [<offset>]
    host size hostfs - <filename>

Description
-----------

The host command provides a way to attach disk images on the host to U-Boot
sandbox. This can be useful for testing U-Boot's filesystem implementations.

A bound image appears as a block device of the 'host' uclass, so it can be
read and written with any command which takes one, such as
:doc:`fatls<fatls>` or :doc:`ext4ls<ext4ls>`.

The load, ls, save and size sub-commands are a different thing: they reach
the files of the host filesystem directly, with no binding involved, through
the 'hostfs' device.

Common arguments:

<label|seq>
    This is used to specify a host device. It can either be a label (a string)
    or the sequence number of the device. An invalid value causes the command
    to fail.


host bind
~~~~~~~~~

This creates a new host device and binds a file to it.

Arguments:

label
    Label to use to identify this binding. This can be any string.

filename:
    Host filename to bind to

blksz
    Logical block size of the device in bytes, in decimal. This must be at
    least 512 and a power of two. It defaults to 512.

Flags:

-r
    Mark the device as removable


host unbind
~~~~~~~~~~~

This unbinds a host device that was previously bound. The sequence numbers of
other devices remain unchanged.


host info
~~~~~~~~~

Provides information about a particular host binding, or all of them: the
sequence number, the size in blocks, the logical block size, the label and the
filename.

If a variable name <varname> is provided, the sequence number is written to
the variable and no output is written to the console.


host dev
~~~~~~~~

Allowing selecting a particular device, or (with no arguments) seeing which one
is selected. Selecting a device shows nothing.


host load, host ls, host save, host size
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

These work on the host filesystem, taking the same arguments as
:doc:`load<load>`, :doc:`ls<ls>`, :doc:`save<save>` and :doc:`size<size>` do,
with 'hostfs' as the interface and '-' as the device. The filename is a path
on the host, relative to the directory U-Boot was started in.

As with those commands, 'host load' sets the fileaddr and filesize variables
and 'host size' sets filesize, in hex.


Example
-------

These examples use the disk images which U-Boot's tests create in the sandbox
build directory.

Initially there are no devices::

    => host info
    dev       blocks  blksz label           path

Bind two images, the first of them removable::

    => host bind -r test2 /tmp/b/sandbox/persistent-data/2MB.ext2.img
    => host bind fat /tmp/b/sandbox/persistent-data/1MB.fat32.img
    => host info
    dev       blocks  blksz label           path
      0         4096    512 test2           /tmp/b/sandbox/persistent-data/2MB.ext2.img
      1         2048    512 fat             /tmp/b/sandbox/persistent-data/1MB.fat32.img

Select a device by label or sequence number. Nothing is shown until the
current device is asked for::

    => host dev
    No current host device
    => host dev fat
    => host dev
    Current host device: 1: fat
    => host dev 0
    => host dev
    Current host device: 0: test2

Write the sequence number of a device to a variable, for use in a script::

    => host info fat devnum
    => printenv devnum
    devnum=1

The images are block devices, so the ordinary filesystem commands reach their
contents. The other drivers which 'lsblk' lists are left out here::

    => lsblk
    Block Driver          Devices
    -----------------------------
    sandbox_host_blk    : host 0, host 1

Read a file of the host filesystem, which needs no binding::

    => host size hostfs - README
    => printenv filesize
    filesize=12446
    => host load hostfs - 1000 README
    74822 bytes read in 1 ms (71.4 MiB/s)
    => host save hostfs - 1000 /tmp/b/sandbox/persistent-data/dump.bin 100
    256 bytes written in 0 ms
    => host ls hostfs - test/py/tests/bootstd
    DIR       4096 ..
              1384 armbian.bmp.xz
    DIR       4096 .

Unbind a device::

    => host unbind test2
    => host info
    dev       blocks  blksz label           path
      1         2048    512 fat             /tmp/b/sandbox/persistent-data/1MB.fat32.img

Return value
------------

The return value $? is 0 (true) if the command succeeds. It is 1 (false) if a
named device does not exist, if 'host bind' is given a block size below 512 or
one which is not a power of two, if it cannot open the file or create the
device, if 'host unbind' cannot detach or unbind the device, if 'host dev' is
asked for a current device when none is selected, or if a file sub-command
cannot read or write its file.

See also
--------

* :doc:`lsblk<lsblk>` for the host devices listed with the block devices of
  every other driver
* :doc:`rkmtd<rkmtd>` for a command which binds and unbinds block devices the
  same way, on real Rockchip hardware
* :doc:`load<load>`, :doc:`ls<ls>`, :doc:`save<save>` and :doc:`size<size>`
  for the commands the file sub-commands follow
